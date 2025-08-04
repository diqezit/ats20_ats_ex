#pragma once

#include "Globals.h"

// ======================================================================
// Input.h - Input Handling Subsystem
// Handles rotary encoder, button presses, and command mode logic.
// ======================================================================

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
// ===== INPUT: ROTARY ENCODER & BUTTONS ====
// ==========================================

// Handle encoder rotation in interrupt
// any turn cancels active seek for instant user control
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        noInterrupts();  // for race protection!
        g_encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
        g_seekStop = true;
        interrupts();
    }
}

// Safely read encoder counts from interrupt and filter rapid turns into a single action
void updateEncoderState() {
    static uint32_t lastEncoderTime = 0;
    if (g_encoderCount) {
        if (millis() - lastEncoderTime < 10) return;
        lastEncoderTime = millis();
        noInterrupts();
        g_safeEncoderMovement += g_encoderCount;
        g_encoderCount = 0;
        interrupts();
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
// prevents accidental changes
static uint8_t bandEvent(uint8_t event, uint8_t pin) {
#if (0 != BAND_DELAY)
    if (BUTTONEVENT_ISLONGPRESS(event)
        && !g_settingsActive) {
        if (BUTTONEVENT_LONGPRESSDONE != event) {
            bandSwitch(pin == BAND_BUTTON);
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
    if (isSSB()) doSync(0);
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

#if ENABLE_FAVORITES
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

#if ENABLE_FAVORITES
// Handle encoder rotation for favorites list
// enabling circular navigation
static void handleFavoritesMenu() {
    if (g_safeEncoderMovement) {
        g_lastAdjustmentTime = millis(); // Reset timer on encoder rotation
        if (g_totalFavorites > 0) {
            // Using doSwitchLogic for clean, wraparound navigation
            doSwitchLogic((int8_t&)g_favoriteSelected, 0, g_totalFavorites - 1, g_safeEncoderMovement);
            showFavorites();
        }
        g_safeEncoderMovement = 0;
    }
}
#endif

// Handle encoder for navigating settings items
// with wrap-around to cycle through options
static inline void navigateSettingsPage(int encoder_delta) {
    int8_t prev = g_SettingSelected;
    g_SettingSelected += encoder_delta;
    uint8_t page = g_SettingsPage - 1;

    uint8_t a = (page * 6) + 5;
    uint8_t b = SettingsIndex::SETTINGS_MAX - 1;
    uint8_t max = (a < b) ? a : b;

    if (g_SettingSelected < page * 6) g_SettingSelected = max;
    else if (g_SettingSelected > max) g_SettingSelected = page * 6;

    DrawSetting(prev, true);
    DrawSetting(g_SettingSelected, true);
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
        g_encoderCount = encoder_delta;
        if (isSSB()) {
            doFrequencyTuneSSB();
        } else {
            doFrequencyTune();
        }
        return true;
    }
    return false;
}

// Main orchestrator for encoder actions
// determines context and calls correct handler
bool processEncoderActions() {
    if (g_activeCommand != CMD_NONE || g_settingsActive)
        g_lastAdjustmentTime = millis();

    bool was_tuning_event = false;

    wakeUpDisplayIfNeeded();

    if (g_settingsActive) {
        processEncoderForSettings(g_safeEncoderMovement);
    }
#if ENABLE_FAVORITES
    else if (g_favoritesActive) {
        handleFavoritesMenu();
    }
#endif
    else {
        was_tuning_event = processEncoderForCommands(g_safeEncoderMovement);
    }

    g_safeEncoderMovement = 0;
    g_encoderCount = 0;
    resetEepromDelay();
    return was_tuning_event;
}
