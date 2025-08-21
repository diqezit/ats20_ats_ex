#pragma once

#include "Globals.h"

// ======================================================================
// Input.h - Input Handling Subsystem
// Handles rotary encoder, button presses, and command mode logic.
// ======================================================================

// throttle for BAND long-press repeat to avoid too fast cycling
// increase to slow down more (e.g. 240..320 ms gives ~2–4x slower)
// safe since interval << 65535 ms
static constexpr uint16_t BAND_LP_REPEAT_MS = 240;

// ==========================================
// ===== INPUT: ROTARY ENCODER & BUTTONS ====
// ==========================================

// Handle encoder rotation in interrupt
// any turn cancels active seek for instant user control
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        noInterrupts();
        if (encoderStatus == DIR_CW) {
            g_encoderCount++;
        } else {
            g_encoderCount--;
        }
        g_seekStop = true;
        interrupts();
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

// Helper function to manage display power and CPU speed
// Consolidates logic for turning the display on or off
static inline void setDisplayPower(bool on) {
    g_displayOn = on;
    if (on) {
        setCpuPrescaler(g_Settings[SettingsIndex::CPUSpeed].param);
        oled.setPower(true);
    } else {
        setCpuPrescaler(1);         // 8 MHz for responsive handler
        oled.setPower(false);
    }
    autoDisplayOff = false;         // always manual action or wake-up fo reset autoflag
}

// Wake display on user activity but only if it was turned off by timeout
static inline void wakeUpDisplayIfNeeded() {
    if (!g_displayOn && autoDisplayOff) {
        setDisplayPower(true);
        g_lastUserActivityTime = millis() / 1000;
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
    if (pin != MODE_SWITCH
        && pin != STEP_BUTTON
        && pin != AGC_BUTTON
        && pin != BANDWIDTH_BUTTON
        && event == BUTTONEVENT_FIRSTLONGPRESS) {
        return BUTTONEVENT_SHORTPRESS;
    }
    return event;
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
    g_lastAdjustmentTime = 0; // Reset auto-exit timer
    oled.clear();
    showStatus();
}

// Handle encoder rotation for favorites list
// enabling circular navigation
static void handleFavoritesMenu(int16_t movement) {
    if (movement) {
        g_lastAdjustmentTime = millis(); // Reset timer on encoder rotation
        if (g_totalFavorites > 0) {
            // Using doSwitchLogic for clean, wraparound navigation
            doSwitchLogic((int8_t&)g_favoriteSelected, 0, g_totalFavorites - 1, movement);
            showFavorites();
        }
    }
}

// Handles all button inputs when the favorites menu is active
// It acts as a simple dispatcher, calling other functions for complex actions
static void processFavoritesMenuControls() {
    // Encoder press - select favorite and tune to it
    if (BUTTONEVENT_SHORTPRESS == btn_Encoder.checkEvent(simpleEvent)) {
        tuneToSelectedFavorite();
        exitFavoritesMenu();
        return;
    }

    // Bandwidth press -  delete selected favorite
    if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(simpleEvent)) {
        deleteFavorite();
        showFavorites(true);
        g_lastAdjustmentTime = millis();
        return;
    }

    // Step press - exit menu without tuning
    if (BUTTONEVENT_SHORTPRESS == btn_Step.checkEvent(simpleEvent)) {
        exitFavoritesMenu();
        return;
    }
}
#endif

#if ENABLE_CW_DECODER
// ==========================================
// ===== CW VIEW GATE & LIMITED CONTROLS ====
// ==========================================

static inline void processCwViewButtons() {
    uint8_t evt;

    // Volume UP (short + long)
    evt = btn_VolumeUp.checkEvent(volumeEvent);
    if (evt == BUTTONEVENT_SHORTPRESS ||
        (BUTTONEVENT_ISLONGPRESS(evt) && BUTTONEVENT_LONGPRESSDONE != evt)) {
        // ensure actual change on short if needed
        if (evt == BUTTONEVENT_SHORTPRESS) doVolume(1);
        g_lastAdjustmentTime = millis();
    }

    // Volume DOWN: short -> mute/unmute; long -> volume down
    evt = btn_VolumeDn.checkEvent(volumeEvent);
    if (evt == BUTTONEVENT_SHORTPRESS) {
        handleVolumeDownShortPress();
        g_lastAdjustmentTime = millis();
    } else if (BUTTONEVENT_ISLONGPRESS(evt) && BUTTONEVENT_LONGPRESSDONE != evt) {
        doVolume(-1);
        g_lastAdjustmentTime = millis();
    }

    // MODE long -> exit CW view
    evt = btn_Mode.checkEvent(simpleEvent);
    if (evt == BUTTONEVENT_LONGPRESSDONE) {
        g_cwViewActive = false;
        cwViewExit();
    }
}

// Returns true if CW view handled this frame (caller should return from loop)
static inline bool handleCwViewGate() {
    if (!g_cwViewActive) return false;

    if (g_currentMode != CW) {
        g_cwViewActive = false;
        cwViewExit();
        return false; // continue normal loop
    }

    // Exclusive CW frame: decoder + limited controls only
    cwViewTask();
    processCwViewButtons();
    return true; // stop further processing this frame
}
#endif

// ==========================================
// ===== SPECIFIC BUTTON ACTION HANDLERS ====
// ==========================================

// Encoder button is multi-purpose
// action depends on context providing an enter or confirm
static void handleEncoderShortPress() {
    if (g_activeCommand != CMD_NONE) {
        resetCommandMode();
        return;
    }

    if (g_settingsActive) {
        g_SettingEditing = !g_SettingEditing;
        DrawSetting(g_SettingSelected, true);
        g_lastAdjustmentTime = millis();
        return;
    }

    (isSSB() || !g_Settings[ScanSwitch].param) ? switchCommand(CMD_STEP) : doSeek();
}

// Button has dual roles for hardware efficiency
// cycles bands or switches settings pages
static void handleBandUpShortPress() {
    if (g_settingsActive) {
        switchSettingsPage();
        g_lastAdjustmentTime = millis();
    } else {
        switchCommand(CMD_BAND);
    }
}

static void handleBandDownShortPress() {
    resetCommandMode();
    g_settingsActive = !g_settingsActive;
    switchSettings();
    if (g_settingsActive) g_lastAdjustmentTime = millis();
}

// Primary action toggles mute
// saves current volume for seamless restore
static void handleVolumeDownShortPress() {
    RETURN_IF_SETTINGS_ACTIVE();
    if (g_activeCommand != CMD_VOLUME) {
        uint8_t vol = g_si4735.getCurrentVolume();
        if (vol && !g_muteVolume) {
            g_muteVolume = vol;
            g_si4735.setVolume(0);
        } else if (g_muteVolume) {
            g_si4735.setVolume(g_muteVolume);
            g_muteVolume = 0;
        }
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
    addFavorite();
    showSavedConfirmation();
#endif
}

// Long press on STEP opens or closes the favorites menu
static void handleStepLongDone() {
    RETURN_IF_SETTINGS_ACTIVE();
#if ENABLE_FAVORITES
    if (g_favoritesActive) {
        exitFavoritesMenu();
    } else {
        g_favoritesActive = true;
        g_favoriteSelected = 0;
        g_lastAdjustmentTime = millis();    // Start timer on entry
        showFavorites(true);                // Force a full redraw on entry
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

    if (g_currentMode == LSB || g_currentMode == USB) {
        g_currentMode = (g_currentMode == LSB) ? USB : LSB;
        applyBandConfiguration();
    } else if (g_currentMode == CW) {
        doCWSwitch();
    }
}

// Short press on Mode button cycles through AM/SSB/CW, but is disabled in FM
static void handleModeShortPress() {
    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) return;
    RETURN_IF_SETTINGS_ACTIVE();
    cycleAmSsbCwModes();
}

// Long press on Mode toggles SYNC in SSB mode
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

    if (isSSB()) doSync(0);
}

// ==========================================
// ===== COMMAND MODE & ENCODER LOGIC =======
// ==========================================

// Update all command icons on screen at once to match current state
void refreshCommandIndicators() {
    showVolume();
    showStep();
    showBandwidth();
    showModulation();
}

// Activate a specific command mode for the encoder
// pressing same button again deactivates it
void switchCommand(CommandMode mode) {

    if (mode == CMD_BW && g_currentMode == CW) return;

    RETURN_IF_SETTINGS_ACTIVE();
    g_activeCommand = (g_activeCommand != mode) ? mode : CMD_NONE;
    if (g_activeCommand != CMD_NONE) {
        g_lastAdjustmentTime = millis();
    } else {
        g_lastAdjustmentTime = 0;
    }
    refreshCommandIndicators();
}

// Exit any active command mode
// returning encoder to default frequency control
void resetCommandMode() {
    if (g_activeCommand != CMD_NONE) {
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
        refreshCommandIndicators();
    }
}

// Handle encoder for navigating settings items
// with wrap-around to cycle through options
static void navigateSettingsPage(int encoder_delta) {
    if (!encoder_delta) return;

    const uint8_t total = SettingsIndex::SETTINGS_MAX;
    uint8_t prev = (uint8_t)g_SettingSelected;

    int16_t t = (int16_t)prev + (int16_t)encoder_delta;
    while (t < 0)      t += total;
    while (t >= total) t -= total;

    g_SettingSelected = (int8_t)t;

    uint8_t newPage = (t < 6) ? 1 : (t < 12) ? 2 : (t < 18) ? 3 : (t < 24) ? 4 : 5;

    if (newPage != (uint8_t)g_SettingsPage) {
        g_SettingsPage = (int8_t)newPage;
        oled.clear();
        showSettingsTitle();
        showSettings();
    } else if (prev != (uint8_t)g_SettingSelected) {
        DrawSetting(prev, true);
        DrawSetting(g_SettingSelected, true);
    }
}

// Dispatch encoder actions in settings menu
// rotation navigates list or edits a value
static inline void processEncoderForSettings(int encoder_delta) {
    if (!g_SettingEditing) {
        navigateSettingsPage(encoder_delta);
    } else {
        (*g_Settings[g_SettingSelected].manipulateCallback)(encoder_delta);
        DrawSetting(g_SettingSelected, false);
        delay(MIN_ELAPSED_TIME);
    }
}

// Dispatch encoder actions on main screen
// encoder controls whichever command is active
static inline bool processEncoderForCommands(int encoder_delta) {
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
        noInterrupts();
        g_encoderCount = encoder_delta;
        interrupts();
        if (isSSB()) {
            doFrequencyTuneSSB();
        } else {
            doFrequencyTune();
        }
        return true;
    }
    return false;
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

// Central dispatcher for button presses
// giving priority to favorites menu with its unique control scheme
void processButtonEvents() {
#if ENABLE_FAVORITES
    if (g_favoritesActive) {
        processFavoritesMenuControls();
        return;
    }
#endif

    for (auto& action : buttonActions) {
        uint8_t evt = action.btn.checkEvent(action.checkFn);

        if (evt && !g_displayOn && autoDisplayOff) {
            wakeUpDisplayIfNeeded();
            return;
        }

        if (evt == BUTTONEVENT_SHORTPRESS) {
            if (action.onShortPress) {
                action.onShortPress();
            } else if (action.cmdToSwitch != CMD_NONE) {
                switchCommand(action.cmdToSwitch);
            }
        } else if (evt == BUTTONEVENT_LONGPRESSDONE && action.onLongDone) {
            action.onLongDone();
        }
    }
}

// Main orchestrator for encoder actions
// determines context and calls correct handler
bool processEncoderActions(int16_t movement) {
    if (g_activeCommand != CMD_NONE || g_settingsActive)
        g_lastAdjustmentTime = millis();

    bool was_tuning_event = false;

    wakeUpDisplayIfNeeded();

    if (g_settingsActive) {
        processEncoderForSettings(movement);

    } else {
        was_tuning_event = processEncoderForCommands(movement);
    }

    resetEepromDelay();
    return was_tuning_event;
}
