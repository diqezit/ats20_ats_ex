#pragma once

#include "Globals.h"

// ====================================================================================
//
// Input.h - User Input Handling and Action Dispatcher
//
// This file translates physical user actions (button presses, encoder turns)
// into logical commands for the receiver
// It acts as the central hub that connects hardware inputs to the functions in RadioControl.h and UI.h
//
// Manages different input contexts
// An encoder turn might change frequency adjust a setting or navigate a menu
// depending on the current application state (e.g., `g_settingsActive`)
//
// Implements logic for short - long - double presses to maximize the utility
// of limited physical buttons
//
// ====================================================================================

// throttle for BAND long-press repeat to avoid too fast cycling
// increase to slow down more (e.g. 240..320 ms gives ~2–4x slower)
// safe since interval << 65535 ms
static constexpr uint16_t BAND_LP_REPEAT_MS = 240;

// ==========================================
// ===== INPUT: ROTARY ENCODER & BUTTONS ====
// ==========================================

// Handle encoder rotation in interrupt
// No manual interrupts() call prevents Stack Overflow from nested ISRs during fast rotation
// g_seekStop set on ANY change gives instant response, stopping seek even on contact bounce
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    g_seekStop = true;
    if (encoderStatus) {
        g_encoderCount += (encoderStatus == DIR_CW) ? 1 : -1;
    }
}

// Safely read encoder counts from interrupt and filter rapid turns into a single action
void updateEncoderState() {
    // Atomically get all steps accumulated in the interrupt
    int16_t count_delta = getAndResetEncoderCount(g_encoderCount);

    if (count_delta)
        g_safeEncoderMovement += count_delta;
}

// ==========================================
// =============== INPUT: HELPERS ===========
// ==========================================

static void __attribute__((noinline))
displayPowerOn() {
    g_displayOn = true;
    setCpuPrescaler(getSettingParam(CPUSpeed));
    oled.setPower(true);
    autoDisplayOff = false;
}

static void __attribute__((noinline))
displayPowerOff() {
    g_displayOn = false;
    setCpuPrescaler(1);         // 8 MHz for responsive handler
    oled.setPower(false);
    autoDisplayOff = false;
}

// Helper function to manage display power and CPU speed
// Consolidates logic for turning the display on or off
static inline void setDisplayPower(bool on) {
    if (on) displayPowerOn();
    else    displayPowerOff();
}

// Wake display on user activity but only if it was turned off by timeout
static inline void wakeUpDisplayIfNeeded() {
    if (!g_displayOn && autoDisplayOff) {
        setDisplayPower(true);
        noteUserActivity();
    }
}

// ==========================================
// ===== BUTTON EVENT FILTERS ===============
// ==========================================

// If receiver is muted, any volume press unmutes first to avoid user confusion
static uint8_t volumeEvent(uint8_t event, uint8_t pin) {
    if (g_muteVolume) {
        if (!BUTTONEVENT_ISDONE(event)) {
            if ((BUTTONEVENT_SHORTPRESS != event)
                || (VOLUME_BUTTON == pin))
                doVolume(1);
        }
    } else if (
        BUTTONEVENT_ISLONGPRESS(event)
        && (BUTTONEVENT_LONGPRESSDONE != event)) {
        doVolume(VOLUME_BUTTON == pin ? 1 : -1);
    }
    return event;
}

// Disable long press for most buttons
// treat as short press to simplify user interaction
static uint8_t simpleEvent(uint8_t event, uint8_t pin) {
    if (event != BUTTONEVENT_FIRSTLONGPRESS) return event;

    switch (pin) {
    case MODE_SWITCH:
    case STEP_BUTTON:
    case AGC_BUTTON:
    case BANDWIDTH_BUTTON:
        return event;                  // long press allowed
    default:
        return BUTTONEVENT_SHORTPRESS; // long press disabled
    }
}

// Allow continuous band cycling on long press only if a delay is configured
// throttle long-press repeat rate to avoid overshoot when holding BAND+/BAND−
static uint8_t bandEvent(uint8_t event, uint8_t pin) {
#if (0 != BAND_DELAY)

    static uint16_t lastRepeatUp = 0;
    static uint16_t lastRepeatDn = 0;

    if (BUTTONEVENT_ISLONGPRESS(event) && !g_settingsActive) {
        if (BUTTONEVENT_LONGPRESSDONE != event) {
            uint16_t now16 = (uint16_t)millis();
            uint16_t& last = (pin == BAND_BUTTON) ? lastRepeatUp : lastRepeatDn;

            // fire bandSwitch only if repeat interval elapsed
            if ((uint16_t)(now16 - last) >= BAND_LP_REPEAT_MS) {
                bandSwitch(pin == BAND_BUTTON);
                last = now16;
            }
        }
    }
#else
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
#endif
    return event;
}


#if ENABLE_FAVORITES
// =======================================================================
// ===== FAVORITES MENU LOGIC ============================================
// =======================================================================

// Save favorites to EEPROM only on exit and only if changed to reduce wear
static inline void exitFavoritesMenu() {
    if (g_favoritesDirty) {
        saveFavorites();
        g_favoritesDirty = false;
    }
    g_favoritesActive = false;
    oled.clear();
    showStatus(true);
}

// Handle encoder rotation for favorites list with circular navigation
static void handleFavoritesMenu(int16_t movement) {
    if (movement) {
        noteUserActivity(); // reset inactivity timer on encoder rotation
        if (g_totalFavorites > 0) {
            // Using doSwitchLogic for clean, wraparound navigation
            doSwitchLogic((int8_t&)g_favoriteSelected, 0, g_totalFavorites - 1, movement);
            showFavorites();
        }
    }
}

// Handles all button inputs when the favorites menu is active
// It acts as a dispatcher and updates inactivity timer on each action
static void processFavoritesMenuControls() {
    // Encoder press: select favorite and tune to it
    if (BUTTONEVENT_SHORTPRESS == btn_Encoder.checkEvent(simpleEvent)) {
        noteUserActivity();
        tuneToSelectedFavorite();
        exitFavoritesMenu();
        return;
    }

    // Bandwidth press: delete selected favorite
    if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(simpleEvent)) {
        noteUserActivity();
        deleteFavorite();
        showFavorites(true);
        return;
    }

    // Step press: exit menu without tuning
    if (BUTTONEVENT_SHORTPRESS == btn_Step.checkEvent(simpleEvent)) {
        noteUserActivity();
        exitFavoritesMenu();
        return;
    }
}
#endif

#if ENABLE_CW_DECODER
// ==========================================
// ===== CW VIEW GATE =======================
// ==========================================

// Returns true if CW view is active (caller should return from loop)
// This function acts as a simple gate, delegating all work to cwViewTask
// and blocking the rest of the main loop.
static inline bool handleCwViewGate() {
    if (!g_cwViewActive) return false;

    // In CW mode, run the decoder task and stop further processing in the main loop
    cwViewTask();
    return true;
}
#endif

// ==========================================
// ===== SPECIFIC BUTTON ACTION HANDLERS ====
// ==========================================

// Encoder button is multi-purpose
// Action depends on context: enter/confirm or switch to step/seek
static void handleEncoderShortPress() {
    if (g_activeCommand != CMD_NONE) {
        resetCommandMode();
        return;
    }

    if (g_settingsActive) {
        g_SettingEditing = !g_SettingEditing;
        DrawSetting(g_SettingSelected, true);
        noteUserActivity();
        return;
    }

    (isSSB() || !getSettingParam(ScanSwitch)) ? switchCommand(CMD_STEP) : doSeek();
}

// BAND+ short press: in settings → next page; on main → switch to band command
static void handleBandUpShortPress() {
    if (g_settingsActive) {
        switchSettingsPage();
        noteUserActivity();
    } else {
        switchCommand(CMD_BAND);
    }
}

// BAND− short press: toggle settings mode
static void handleBandDownShortPress() {
    resetCommandMode();
    g_settingsActive = !g_settingsActive;
    switchSettings();
    if (g_settingsActive) noteUserActivity();
}

// Primary action toggles mute
// saves current volume for seamless restore
static void handleVolumeDownShortPress() {
    RETURN_IF_SETTINGS_ACTIVE();
    if (g_activeCommand == CMD_VOLUME) return;

    // If currently muted - restore audio
    if (g_muteVolume) {
        g_muteVolume = 0;
        applyCompensatedVolume();
        showVolume();
        return;
    }

    // If not muted - store current user volume and mute
    if (g_volume) {
        g_muteVolume = g_volume;
        applyCompensatedVolume();
        showVolume();
    }
}

// Toggles display power or wakes it from sleep
// On main screen a short press toggles the display ON/OFF
// If display has turned off by timeout, the first press will only wake it
static void handleAgcShortPress() {

    if (!g_displayOn) {
        setDisplayPower(true);
        return;
    }

    RETURN_IF_SETTINGS_ACTIVE();

    setDisplayPower(false);
}

// handler for long press on AGC (Save Favorite)
static void handleAgcLongDone() {
    RETURN_IF_SETTINGS_ACTIVE();
#if ENABLE_FAVORITES
    if (addFavorite()) {
        showSavedConfirmation();
    }
#endif
}

// STEP long press: open/close favorites menu
static void handleStepLongDone() {
    RETURN_IF_SETTINGS_ACTIVE();
#if ENABLE_FAVORITES
    if (g_favoritesActive) {
        exitFavoritesMenu();
    } else {
        g_favoritesActive = true;
        g_favoriteSelected = 0;
        noteUserActivity();          // start menu inactivity timer and mark user activity
        showFavorites(true);         // force a full redraw on entry
    }
#endif
}

// Handles sideband switching on long press
static void handleBandwidthLongDone() {
    // This action should only be available on the main screen
    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    switch (g_currentMode) {
    case LSB:
        g_currentMode = USB;
        applyBandConfiguration();
        break;

    case USB:
        g_currentMode = LSB;
        applyBandConfiguration();
        break;

    case CW:
        doCWSwitch();
        break;

    default:
        break;
    }
}

// Short press on Mode button cycles through AM/SSB/CW, but is disabled in FM
static void handleModeShortPress() {
    if (currentBandType() == FM_BAND_TYPE) return;
    RETURN_IF_SETTINGS_ACTIVE();
    cycleAmSsbCwModes();
}

// Long press on Mode toggles SYNC in SSB mode OR enters/exits CW Decoder
static void handleModeLongDone() {
    RETURN_IF_SETTINGS_ACTIVE();

#if ENABLE_CW_DECODER
    if (g_currentMode == CW) {
        g_cwViewActive = !g_cwViewActive;
        if (g_cwViewActive) cwViewEnter();
        else cwViewExit();
        return;
    }
#endif

    if (g_currentMode == FM) {
#if ENABLE_RDS_MINI
        // RDS takes priority over Game in FM mode
        // Long-press MODE toggles RDS on/off
        rdsMiniToggleUi();
        showRfHints();
        return;
#elif ENABLE_GAME
        // Game only available when RDS is compile-time disabled
        gameToggle();
        if (!gameIsActive()) showStatus(true);
        return;
#endif
    }

    if (isSSB()) doSync(0);
}

// ==========================================
// ===== COMMAND MODE & ENCODER LOGIC =======
// ==========================================

// Update all command icons on screen at once to match current state
void __attribute__((noinline)) refreshCommandIndicators() {
    showVolume();
    showStep();
    showBandwidth();
    showModulation();
}

// Activate a specific command mode for the encoder
// Pressing the same button again deactivates it
void switchCommand(CommandMode mode) {
    if (mode == CMD_BW && g_currentMode == CW) return;

    RETURN_IF_SETTINGS_ACTIVE();

    CommandMode newMode = (g_activeCommand != mode) ? mode : CMD_NONE;
    g_activeCommand = newMode;

    // only update activity timestamp when entering a mode
    if (newMode != CMD_NONE) noteUserActivity();

    refreshCommandIndicators();
}

// Exit any active command mode
// returning encoder to default frequency control
void resetCommandMode() {
    if (g_activeCommand != CMD_NONE) {
        g_activeCommand = CMD_NONE;
        refreshCommandIndicators();
    }
}

// Calculate which settings page the index belongs to pages are 1-based
// Each page holds 6 items
// This uses a small math trick instead of index / 6
// Works correctly while SETTINGS_MAX is 131 or less
inline uint8_t calculateSettingsPage(uint8_t index) {
    static_assert(SETTINGS_MAX <= 131, "calculateSettingsPage requires SETTINGS_MAX <= 131");
    return (uint8_t)((((uint16_t)index * 43u) >> 8) + 1u);
}

// Draw updated settings on screen
inline void updateSettingDisplay(uint8_t prev, uint8_t current) {
    if (prev != current) {
        DrawSetting(prev, true);
        DrawSetting(current, true);
    }
}

// Navigate settings with cursor order
// NAV=0: linear order (0→1→2→3→4→5→6...)
// NAV=1: column-first (0→2→4→1→3→5→6→8→10→7→9→11...)
static void navigateSettingsPage(int16_t encoder_delta) {
    if (!encoder_delta) return;

    uint8_t prev = g_SettingSelected;

    // nav table in SettingsData.h
    g_SettingSelected = getNextSettingIndex(prev, encoder_delta);

    uint8_t newPage = calculateSettingsPage(g_SettingSelected);

    // Redraw
    if (newPage != g_SettingsPage) {
        g_SettingsPage = newPage;
        oled.clear();
        showSettingsTitle();
        showSettings();
    } else {
        updateSettingDisplay(prev, g_SettingSelected);
    }
}

// Dispatch encoder actions in settings menu
// rotation navigates list or edits a value
static inline void processEncoderForSettings(int16_t encoder_delta) {
    const uint8_t idx = (uint8_t)g_SettingSelected;

    if (g_SettingEditing) {
        // user expects inactive settings to be non-editable
        if (isSettingActive(idx)) {
            callSettingCallback(idx, encoder_delta);
            DrawSetting(idx, false);
        }
    } else {
        navigateSettingsPage(encoder_delta);
    }
}

// Dispatch encoder actions on main screen
// encoder controls whichever command is active
static inline bool processEncoderForCommands(int16_t encoder_delta) {
    switch (g_activeCommand) {
    case CMD_VOLUME:
        doVolume(encoder_delta);
        break;
    case CMD_STEP:
        doStep(encoder_delta);
        break;
    case CMD_BW:
        doBandwidth(encoder_delta);
        break;
    case CMD_BAND:
        bandSwitch(encoder_delta > 0);
        return true;
    case CMD_SLEEP:
        break;
    case CMD_NONE:
        if (isSSB()) {
            doFrequencyTuneSSB(encoder_delta);
        } else {
            doFrequencyTune(encoder_delta);
        }
        return true; // frequency change is a tuning event
    default: break;
    }
    return false;
}

// Main orchestrator for all encoder actions
// Selects handler based on UI context (main vs settings)
bool processEncoderActions(int16_t encoder_delta) {
    noteUserActivity();

    bool was_tuning_event = false;

    wakeUpDisplayIfNeeded();

    if (g_settingsActive) {
        processEncoderForSettings(encoder_delta);
    } else {
        was_tuning_event = processEncoderForCommands(encoder_delta);
    }

    return was_tuning_event;
}

// ==========================================
// ===== TABLE-DRIVEN BUTTON DISPATCHER =====
// ==========================================

// This table-driven approach centralizes all button logic in one place
// It avoids a large, hard-to-follow if/else block in the main processing loop
// To change a button purpose or add a new one, you only edit or add a line here
// This keeps the button-to-action mapping clear and separate from the action functions themselves
using CheckFn = uint8_t(*)(uint8_t, uint8_t);
using ActionFn = void (*)();

struct ButtonAction {
    SimpleButton& btn;
    CheckFn  checkFn;
    ActionFn onShortPress;
    ActionFn onLongDone;
    CommandMode cmdToSwitch; // For simple actions that just switch command mode
};

// Simple actions (like switching to CMD_VOLUME) now use cmdToSwitch
// which removes the need for many small, single-purpose handler functions
static ButtonAction buttonActions[] = {
    { btn_Encoder,   simpleEvent, handleEncoderShortPress,    nullptr,                  CMD_NONE },
    { btn_Bandwidth, simpleEvent, nullptr,                    handleBandwidthLongDone,  CMD_BW },
    { btn_BandUp,    bandEvent,   handleBandUpShortPress,     nullptr,                  CMD_NONE },
    { btn_BandDn,    bandEvent,   handleBandDownShortPress,   nullptr,                  CMD_NONE },
    { btn_VolumeUp,  volumeEvent, nullptr,                    nullptr,                  CMD_VOLUME },
    { btn_VolumeDn,  volumeEvent, handleVolumeDownShortPress, nullptr,                  CMD_NONE },
    { btn_AGC,       simpleEvent, handleAgcShortPress,        handleAgcLongDone,        CMD_NONE },
    { btn_Step,      simpleEvent, nullptr,                    handleStepLongDone,       CMD_STEP },
    { btn_Mode,      simpleEvent, handleModeShortPress,       handleModeLongDone,       CMD_NONE },
};

// Wake up the display if it was off due to timeout
// Returns true if the event only woke the display
inline bool handleDisplayWake(const SimpleButton& btn) {
    if (!g_displayOn && autoDisplayOff) {
        wakeUpDisplayIfNeeded();
        return true;    // event only woke the display
    }
    noteUserActivity(); // normal user activity
    return false;
}

// Handle short press for a button
inline void handleShortPress(const ButtonAction& action) {
    if (action.onShortPress) {
        action.onShortPress();
    } else if (action.cmdToSwitch != CMD_NONE) {
        switchCommand(action.cmdToSwitch);
    }
}

// Handle long press completion for a button
inline void handleLongPressDone(const ButtonAction& action) {
    if (action.onLongDone) action.onLongDone();
}

// Central dispatcher for button presses
void processButtonEvents() {
#if ENABLE_CW_DECODER
    if (g_cwViewActive) {
        if (btn_Mode.checkEvent(simpleEvent) == BUTTONEVENT_LONGPRESSDONE)
            handleModeLongDone();
        return;
    }
#endif

#if ENABLE_FAVORITES
    if (g_favoritesActive) {
        processFavoritesMenuControls();
        return;
    }
#endif

    for (auto& action : buttonActions) {
        const uint8_t evt = action.btn.checkEvent(action.checkFn);
        if (evt == 0) continue;

        const bool wokeDisplay = handleDisplayWake(action.btn);
        if (wokeDisplay && &action.btn == &btn_AGC) continue;

        switch (evt) {
        case BUTTONEVENT_SHORTPRESS:
            handleShortPress(action);
            break;
        case BUTTONEVENT_LONGPRESSDONE:
            handleLongPressDone(action);
            break;
        default:
            break;
        }
    }
}

