// ----------------------------------------------------------------------
// ATS_EX (Extended) Firmware for ATS-20 and ATS-20+ receivers
// For more information, see the README at:
// https://github.com/goshante/ats20_ats_ex
// ----------------------------------------------------------------------
//
// Main development by Goshante
// https://github.com/goshante
//
// MOD_NO_RDS branch by diqezit
// https://github.com/diqezit/ats20_ats_ex
//
// --- Acknowledgments & Credits ---
//
// This project is built upon the foundational work of:
// - PU2CLR (https://github.com/pu2clr)
//
// Inspired by the work of:
// - swling.ru (closed-source firmware)
// - esp32-si4732/ats-mini (https://github.com/esp32-si4732/ats-mini)
// - G8PTN/ATS_MINI (https://github.com/G8PTN/ATS_MINI)
//
// Special thanks for testing, ideas, and contributions to:
// - d3n3rats
// - jh4vaj
//
// --- Technical Reference ---
// For in-depth Si473x programming details, see Skyworks AN332:
// https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN332.pdf
//
// ----------------------------------------------------------------------

#include <microWire.h>
#include <avr/wdt.h>
#include "Defines.h"
#include "SI4735_fixed.h"
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "SSD1306_OLED.h"

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

#include "Rotary.h"
#include "SimpleButton.h"
#include "patch_ssb_compressed.h"

#include "Globals.h"
#include "Utils.h"
#include "Memory.h"
#include "Battery.h"
#include "Game.h"
#include "Input.h"
#include <avr/interrupt.h>
#include "UI.h"
#include "RDS.h"
#include "CW_decoder.h"
#include "RadioControl.h"

#include "Boot.h"
#include "SettingsLogic.h"
#include "Favorites.h"
#include "SMeter.h"

// ==========================================
// ===== CORE UTILITIES & STATE SYNC ========
// ==========================================

// read-modify-write encoder counter under a short critical section
// avoids ISR races and returns consumed delta in one shot
int16_t __attribute__((noinline)) getAndResetEncoderCount(volatile int16_t& counter) {
    int16_t value;
    uint8_t oldSREG = SREG;
    cli();
    value = counter;
    counter = 0;
    SREG = oldSREG;
    return value;
}

// sync previous freq tracker with current freq
static inline __attribute__((always_inline)) void syncPreviousFreq() {
    g_previousFrequency = g_currentFrequency;
}

// commit current freq to skip redundant EEPROM writes
static void __attribute__((noinline)) saveLastFreq() {
    g_lastSavedFrequency = g_currentFrequency;
}

// single place to touch user activity timers
// keeps UI timeouts and power-saving logic in sync
static inline void noteUserActivity() {
    uint32_t now = millis();
    g_lastAdjustmentTime = (uint16_t)now;               // ms resolution for UI/command timeouts
    storeUserActivitySecondsFromMillis(now);            // s resolution for display-off / save-on-idle
}

// Initialize all mode contexts with default values
// Called once after an EEPROM reset to populate all contexts in RAM
static inline void initModeSettingsDefaults(void) {
    for (uint8_t i = 0; i < MODE_CONTEXT_COUNT; i++) {
        g_modeSettings[MODE_SETTING_AGC][i] = DEFAULT_MODE_SETTINGS.agc;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][i] = DEFAULT_MODE_SETTINGS.soft_mute;
        g_modeSettings[MODE_SETTING_AVC][i] = DEFAULT_MODE_SETTINGS.avc;
    }
}

// This X-Macro defines the list of mode-dependent settings that need to be synced
// allows define list once and use it to generate code for both
// loading and saving
#define MODE_SETTINGS_MAP(APPLY) \
    APPLY(ATT,            MODE_SETTING_AGC) \
    APPLY(SoftMute,       MODE_SETTING_SOFT_MUTE) \
    APPLY(AutoVolControl, MODE_SETTING_AVC)

// Syncs mode-dependent settings between UI buffer (g_SettingsParams)
// and persistent storage (g_modeSettings)
// The 'load' flag is used inside the X-Macro expansion to set the data flow direction
void syncModeDependentSettings(bool load) {
    const uint8_t m = getModeContext();

    // Correctly check for an uninitialized state
    // Erased EEPROM is 0xFF which is -1 for int8_t
    // This distinguishes it from the valid user setting of 0
    if (load && g_modeSettings[MODE_SETTING_AVC][MODE_CONTEXT_AM] == -1)
        initModeSettingsDefaults();

    // Define operation for both loading and saving
    // preprocessor will expand this for each item in the map
#define SYNC_OPERATION(ui_idx, stored_idx) \
        if (load) { setSettingParam(ui_idx, g_modeSettings[stored_idx][m]); } \
        else      { g_modeSettings[stored_idx][m] = getSettingParam(ui_idx); }

    // Expand the map once to generate all sync operations
    MODE_SETTINGS_MAP(SYNC_OPERATION)

#undef SYNC_OPERATION // Clean up
}

// ==========================================
// ===== TUNING CONTROLS ====================
// ==========================================

// adjust tuning step for current mode and apply to SI4735
static __attribute__((noinline))
void doStep(int8_t v) {
    Band* band = currentBandPtr();
    int8_t* idx;
    int8_t  max;

    switch (g_currentMode) {
    case FM:
        idx = (int8_t*)&band->stepIdxFM;
        max = g_lastStepFM;
        break;

    case LSB: case USB: case CW:
        idx = (int8_t*)&band->stepIdxSSB;
        max = SSB_STEPS_COUNT - 1;
        break;

    default: // AM
        idx = (int8_t*)&band->stepIdxAM;
        max = IS_LW_MW(band->bandType) ? 3 : (AM_STEPS_COUNT - 1);
        break;
    }

    doSwitchLogic(*idx, 0, max, v);
    swLinkSyncCore(true);

    // SSB/CW tunes via BFO adjustments so no hardware step needed
    const uint8_t i = (uint8_t)*idx;
    if (g_currentMode == FM)
        g_si4735.setFrequencyStep((uint16_t)(uint8_t)g_tabStepFM[i]);
    else if (g_currentMode == AM)
        g_si4735.setFrequencyStep(g_tabStep[i]);

    showStep();
}

// Handles bandwidth adjustment based on current modulation
// Each mode has distinct hardware commands and bandwidth tables
static inline void doBandwidth(uint8_t v) {
    if (g_currentMode == CW) return;

    Band* band = currentBandPtr();

    switch (g_currentMode) {
    case LSB:
    case USB: {
        doSwitchLogic(band->bwIdxSSB, 0, MAX_INDEX(bw_ssb_map), v);
        uint8_t hwBw = g_bwSSBIdx[(uint8_t)band->bwIdxSSB];
        uint8_t cut = ssbCutoffForHwBw(hwBw);
        g_si4735.setSSBAudioBwAndCutoff(hwBw, cut);
        break;
    }

    case AM:
        doSwitchLogic(band->bwIdxAM, 0, MAX_INDEX(bw_am_map), v);
        g_si4735.setBandwidth(g_bwAMIdx[(uint8_t)band->bwIdxAM], 1);
        break;

    case FM:
        // invert step because FM map is ordered in reverse
        // this makes knob rotation feel consistent with other modes
        doSwitchLogic(band->bwIdxFM, 0, MAX_INDEX(bw_fm_map), (int8_t)-v);
        g_si4735.setFmBandwidth((uint8_t)band->bwIdxFM);
        break;

    default:
        break;
    }

    // Link SW bandwidth settings across all SW segments if enabled
    swLinkSyncCore(true);

    showBandwidth();
}

// ==========================================
// ===== SETTINGS MENU ORCHESTRATION ========
// ==========================================

// =-=-=-=-=-=-=-=-= Settings & Parameter Handlers =-=-=-=-=-=-=-=-=

// compute first item index for 6-per-page layout
static inline __attribute__((always_inline))
uint8_t settingsPageStart(uint8_t page) {
    return (uint8_t)(6 * (page - 1));
}

// load mode-scoped values into UI and reset cursor
inline static __attribute__((always_inline))
void settingsEnter() {
    syncModeDependentSettings(true);

    // Load current band BFO calibration into UI buffer (per-band calibration)
    setSettingParam(BFO, currentBandPtr()->bfoCal);

    if (g_SettingsPage == 0 || g_SettingsPage > g_SettingsMaxPages)
        g_SettingsPage = 1; // safeguard

    showSettingsTitle();
    g_SettingSelected = settingsPageStart(g_SettingsPage);
    g_SettingEditing = false;
    showSettings();
}

// save UI values back to storage and commit to EEPROM
inline static __attribute__((always_inline))
void settingsExitAndSave() {
    syncModeDependentSettings(false);
    g_settingsDirty = true;
    saveAllReceiverInformation();
    showStatus();
}

// wrap pages and reset edit mode to avoid accidental write
static void switchSettingsPage() {
    g_SettingsPage++;
    g_SettingsPage = (g_SettingsPage > g_SettingsMaxPages) ? 1 : g_SettingsPage;
    g_SettingSelected = settingsPageStart(g_SettingsPage);
    g_SettingEditing = false;
    oled.clear();
    showSettingsTitle();
    showSettings();
}

//Switch between main screen and settings mode
static void switchSettings() {
    oled.clear();
    if (g_settingsActive) {
        settingsEnter();
    } else {
        settingsExitAndSave();
    }
}

// ==========================================
// ===== PERIODIC & TIMED TASKS =============
// ==========================================

// =-=-=-=-=-=-=-=-= Freq update helpers =-=-=-=-=-=-=-=-=

// avoid 32-bit abs, compute 16-bit delta the way UI expects
inline static __attribute__((always_inline))
uint16_t freqDelta16(uint16_t a, uint16_t b) {
    return (a >= b) ? (a - b) : (b - a);
}

// rate limit protects I2C from flooding during fast turns
inline static __attribute__((always_inline))
bool freqRateLimitOk(uint32_t now) {
    return (now - g_lastSetFreqTime) >= MIN_SETFREQ_INTERVAL_MS;
}

// time gate prevents spamming setFrequency on micro moves
inline static __attribute__((always_inline))
bool freqTimeElapsed(uint32_t now) {
    return (now - g_lastFreqChange) >= FREQ_UPDATE_DELAY_MS;
}

// large delta is a user intent to jump, send early
inline static __attribute__((always_inline))
bool freqForceUpdate(uint16_t delta) {
    return delta >= FREQ_FORCE_UPDATE_THRESHOLD_KHZ;
}

// Helper for performing frequency update check
static inline void performFrequencyUpdateCheck(uint32_t now) {
    uint16_t delta = freqDelta16(g_currentFrequency, g_previousFrequency);

    bool time_elapsed = freqTimeElapsed(now);
    bool force_update = freqForceUpdate(delta);
    bool rate_limit_ok = freqRateLimitOk(now);

    // send command if timer elapsed or user made a large jump
    if ((time_elapsed || force_update) && rate_limit_ok) {
        g_si4735.setFrequency(g_currentFrequency);
        g_processFreqChange = false;
        g_lastSetFreqTime = now;

        // sync only after successful send
        syncPreviousFreq();
    }
}

// =-=-=-=-=-=-=-=-= Encoder coalesce helper =-=-=-=-=-=-=-=-=

// Handles the delayed frequency update for AM/FM to prevent flooding the chip
static void __attribute__((noinline)) handleDelayedFrequencyUpdate() {
    if (autoDisplayOff) return;              // only deep sleep mode
    if (!g_processFreqChange || isSSB()) return;

    performFrequencyUpdateCheck(millis());
}

// =-=-=-=-=-=-=-=-= RSSI helpers =-=-=-=-=-=-=-=-=

// skip AM polling when disabled or right after user action to avoid clicks
static inline __attribute__((always_inline))
bool amRssiPollingAllowed(uint16_t now_s) {
    return (uint16_t)(now_s - g_lastUserActivityTime) >= 1;
}

// Fetches signal quality (RSSI) using mode-specific commands
// SSB/CW poll RSQ (0x43) and return RSSI (RESP4) after SSB patch is loaded
static uint8_t getSignalQuality(uint16_t now_s) {
    // RSSI disabled for AM-family (AM/SSB/CW)
    if (g_currentMode != FM && getSettingParam(RSSI_AM_Off) == 1)
        return UI_SIGNAL_NO_VALUE;

    switch (g_currentMode) {
    case AM:
        if (!amRssiPollingAllowed(now_s))
            return g_signalQualityValue;

        g_si4735.getCurrentReceivedSignalQuality(0);
        return g_si4735.getCurrentRSSI();

    case FM: case LSB: case USB: case CW:
        g_si4735.getCurrentReceivedSignalQuality(1);
        return g_si4735.getCurrentRSSI();

    default:
        return UI_SIGNAL_NO_VALUE;
    }
}

// Polls for new signal quality and updates UI only on change
// also triggers squelch logic after each poll
static inline void updateSignalQuality(uint16_t now_s) {
    uint8_t new_value = getSignalQuality(now_s);
    updateIfChanged(g_signalQualityValue, new_value, showSignalQuality);
    handleSquelch();
}

// helper for FM stereo indicator logic
static inline void updateFmStereoIndicator(uint32_t now) {
    if (g_currentMode != FM || now <= 3000UL) return;
    bool new_stereo_status = g_si4735.getCurrentPilot();
    updateIfChanged(g_stereoStatus, new_stereo_status, updateStereoIndicator);
}

// gate status polling while user is tuning or menus are open
inline static __attribute__((always_inline)) bool uiBusy() {
#if ENABLE_FAVORITES
    return g_settingsActive || g_favoritesActive;
#else
    return g_settingsActive;
#endif
}

// Checks for and handles signal quality and stereo indicator updates
static void __attribute__((noinline))
handleSignalAndStereoUpdates(uint32_t now, uint16_t now_s) {
    if (uiBusy() || g_processFreqChange) return;

    if (now - g_lastFreqChange < RSSI_POLL_DELAY_AFTER_TUNE_MS) return;
    if (now - g_lastRSSIUpdate < RSSI_POLL_INTERVAL_MS) return;

    g_lastRSSIUpdate = now;
    updateSignalQuality(now_s);
    updateFmStereoIndicator(now);
}

// =-=-=-=-=-=-=-=-= Timeout helpers =-=-=-=-=-=-=-=-=

// command timeout depends on context so compute once
uint16_t currentCmdTimeoutMs() {
    return (uint16_t)(g_settingsActive ? SETTINGS_MENU_TIMEOUT : ADJUSTMENT_ACTIVE_TIMEOUT);
}

// provides auto-exit for both temporary adjustment modes and the main Settings menu
// now16 precomputed by handlePeriodicTasks
static inline void handleCommandTimeout(uint16_t now16) {
    // Timeout is relevant only when a mode is active
    if (!g_settingsActive && g_activeCommand == CMD_NONE) return;

    const uint16_t timeout = currentCmdTimeoutMs();

    if ((uint16_t)(now16 - g_lastAdjustmentTime) > timeout) {
        if (g_settingsActive) {
            g_settingsActive = false;
            switchSettings();
        }
        resetCommandMode();
    }
}

// =-=-=-=-=-=-=-=-= Save helpers =-=-=-=-=-=-=-=-=

// idle save protects EEPROM during active tuning and still captures last freq
inline static __attribute__((always_inline))
bool shouldSaveStateOnIdle(uint16_t now_s) {
    uint16_t idle_s = (uint16_t)(SAVE_ON_IDLE_TIMEOUT / 1000);
    return g_stateIsDirty && ((uint16_t)(now_s - g_lastUserActivityTime) > idle_s);
}

// Manages saving settings to EEPROM based on user activity
static inline void handleSettingsSave(uint16_t now_s) {
    // save menu settings immediately on exit for predictable behavior
    if (g_settingsDirty) {
        saveAllReceiverInformation(true);
        g_settingsDirty = false;
        g_stateIsDirty = false;             // settings include state, reset both flags
        return;
    }

    // save frequency state on idle to prevent EEPROM wear during active tuning
    // and to ensure last frequency is saved before a potential power-off
    if (shouldSaveStateOnIdle(now_s)) {
        saveAllReceiverInformation(false); // partial save for frequency only
        g_stateIsDirty = false;            // reset flag only after successful save
    }
}

// =-=-=-=-=-=-=-=-= Display sleep helpers =-=-=-=-=-=-=-=-=

// read timeout from PROGMEM table so logic stays data-driven
inline static __attribute__((always_inline))
uint16_t displayTimeoutS(uint8_t p) {
    return pgm_read_word(&T[p]);
}

// Enter low-power mode turn off OLED, reduce CPU multiplier
inline static __attribute__((always_inline)) void engageDisplaySleep() {
    if (!g_displayOn) return;
    g_displayOn = false;
    autoDisplayOff = true;
    oled.setPower(false);
    setCpuPrescaler(CPU_PRESCALER_DEEP_SLEEP);
}

// Handles auto display-off timer
// tracks time in seconds to keep math in 16-bit
static inline void checkDisplayTimeout() {
#if ENABLE_GAME
    if (gameIsActive()) return;
#endif

    uint8_t p = (uint8_t)getSettingParam(DisplayOff);

    if (!g_displayOn || p == 0) return;

    uint16_t timeout_s = displayTimeoutS(p);

    if ((uint16_t)(millis() / 1000) - g_lastUserActivityTime > timeout_s)
        engageDisplaySleep();
}

// for all time-based tasks
static void __attribute__((noinline)) handlePeriodicTasks() {
    const uint32_t now = millis();
    const uint16_t now_s = (uint16_t)(now / 1000);

    if (g_displayOn) {
        handleSignalAndStereoUpdates(now, now_s);
#if ENABLE_BATTERY_MONITOR
        updateAndShowBattery(false);
#endif
        rdsMiniTask((uint16_t)now);
    }
    handleCommandTimeout((uint16_t)now);
    handleSettingsSave(now_s);

    // pending SSB chip update
    if (isSSB()) ssbChipRate();
}

// ==========================================
// ===== MAIN APPLICATION ENTRY POINTS ======
// ==========================================

// Probe battery pin after EEPROM load (getBatteryPin depends on BATT_PIN setting)
static inline void initBatteryProbe() {
#if ENABLE_BATTERY_MONITOR
    g_voltagePinConnected = (uint16_t)adcReadAx(getBatteryPin()) > ADC_CONNECTED_THRESHOLD;
#endif
}

// Initialize controller
void __attribute__((noinline)) setup() {
#if DEBUG_MODE
    initDebugUART();
    debugPrint_P(PSTR("\n\n--- ATS_EX DEBUG START ---\n"));
#endif
    initHardwarePins();
    oled.init();
    handleEEPROMReset();
    initSi4735();
    loadReceiverConfig();
    applyBrightness();

    swLinkNormalizeAllSwBands();

    initBatteryProbe();
    applyInitialConfiguration();
    setAmpState(true);
    syncPreviousFreq();
}

#if ENABLE_CW_DECODER
static inline bool handleCWViewMode() {
    if (!g_cwViewActive) return false;

    cwViewTask();
    processButtonEvents();
    return true;
}
#endif

#if ENABLE_GAME
static inline bool handleGameMode(int16_t encDelta) {
    if (!gameIsActive()) return false;

    gameTask(encDelta);

    if (btn_Mode.checkEvent(simpleEvent) == BUTTONEVENT_LONGPRESSDONE) {
        gameToggle();
        showStatus(true);
    }
    return true;
}
#endif

// main loop program in process order
void __attribute__((noinline)) loop() { // no iline to less bloated main func this is must be 

    updateEncoderState();
    checkDisplayTimeout();

#if ENABLE_CW_DECODER
    if (handleCWViewMode()) return;
#endif

    int16_t safe_encoder_delta = getAndResetEncoderCount(g_safeEncoderMovement);

#if ENABLE_GAME
    if (handleGameMode(safe_encoder_delta)) return;
#endif

#if ENABLE_FAVORITES
    if (handleFavoritesMode(safe_encoder_delta)) return;
#endif

    handleDelayedFrequencyUpdate();

    bool frequencyTuned = false;
    if (safe_encoder_delta)
        frequencyTuned = processEncoderActions(safe_encoder_delta);

    if (!frequencyTuned)
        processButtonEvents();

    handlePeriodicTasks();
}

// Overriding original main to save some space and reduce register pressure
int __attribute__((OS_main, used)) main(void) {

    // Kill any bootloader-residual WDT that may cause spurious resets
    MCUSR = 0;
    wdt_disable();

    initFast();
    setup();

    while (1) {
        loop();
    }
}
