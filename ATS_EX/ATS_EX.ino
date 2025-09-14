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

// To resolve the conflict of definitions(wire->microWire),
// you will need to manually edit the SI4735.h header file,
// which is part of the PU2CLR library, if the library is updated automatically
#include <microWire.h> // #include <Wire.h>

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
#include "Input.h"
#include "UI.h"
#include "CW_decoder.h"
#include "RadioControl.h"

// ==========================================
// ===== CORE UTILITIES & STATE SYNC ========
// ==========================================

// read-modify-write encoder counter under a short critical section
// avoids ISR races and returns consumed delta in one shot
inline int16_t getAndResetEncoderCount(volatile int16_t& counter) {
    int16_t value;
    cli();
    value = counter;
    counter -= value;
    sei();
    return value;
}

// single place to touch user activity timers
// keeps UI timeouts and power-saving logic in sync
static inline void noteUserActivity() {
    uint32_t now = millis();
    g_lastAdjustmentTime = now;                         // ms resolution for UI/command timeouts
    g_lastUserActivityTime = (uint16_t)(now / 1000);    // s resolution for display-off / save-on-idle
}

// Initialize all mode contexts with default values
// Called once after an EEPROM reset to populate all contexts in RAM
static inline void initModeSettingsDefaults(void) {
    for (uint8_t i = 0; i < MODE_CONTEXT_COUNT; i++) {
        g_modeSettings[MODE_SETTING_AGC][i] = defaultModeSettings[i].agc;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][i] = defaultModeSettings[i].soft_mute;
        g_modeSettings[MODE_SETTING_AVC][i] = defaultModeSettings[i].avc;
    }
}

// This X-Macro defines the list of mode-dependent settings that need to be synced
// allows define list once and use it to generate code for both
// loading and saving
#define MODE_SETTINGS_MAP(APPLY) \
    APPLY(ATT,            MODE_SETTING_AGC) \
    APPLY(SoftMute,       MODE_SETTING_SOFT_MUTE) \
    APPLY(AutoVolControl, MODE_SETTING_AVC)

// Syncs mode-dependent settings between UI buffer (g_Settings)
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
        if (load) { g_Settings[ui_idx].param = g_modeSettings[stored_idx][m]; } \
        else      { g_modeSettings[stored_idx][m] = g_Settings[ui_idx].param; }

    // Expand the map once to generate all sync operations
    MODE_SETTINGS_MAP(SYNC_OPERATION)

#undef SYNC_OPERATION // Clean up
}

// Settings: CPU Frequency divider helper
// touch prescaler atomically as required by AVR
static void setCpuPrescaler(uint8_t prescaler) {
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = prescaler;
    interrupts();
}

// ==========================================
// ===== USER ACTION HANDLERS (SETTINGS) ====
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

    // Load current band BFO calibration
    // into the temporary UI setting
    g_Settings[BFO].param = g_bandList[g_bandIndex].bfoCal;

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

// save setting value specific to current mode
// links temporary UI state to persistent storage
inline static __attribute__((always_inline))
void persistModeSetting(ModeSettingType type, SettingsIndex index) {
    ModeContext m = getModeContext();
    g_modeSettings[type][m] = g_Settings[index].param;
}

#if ENABLE_FAVORITES
// check duplicate to keep list useful
inline static __attribute__((always_inline))
bool favoriteExists(uint16_t f, uint8_t m) {
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        if (g_favorites[i].frequency == f && g_favorites[i].modulation == m)
            return true;
    }
    return false;
}

// compact list after removal
inline static __attribute__((always_inline))
void compactFavoritesFrom(uint8_t start) {
    for (uint8_t i = start; i < g_totalFavorites - 1; i++) {
        g_favorites[i] = g_favorites[i + 1];
    }
}

// keep selection valid after delete
inline static __attribute__((always_inline))
void fixFavoriteSelectionAfterDelete() {
    if (g_totalFavorites && g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites - 1;
    }
}

// require full reconfig on FM<->AM or SSB patch need
inline static __attribute__((always_inline))
bool favoriteNeedsFullReset(
    BandType prevType, BandType newType, bool wantSSB, bool hadSSB) {
    return (prevType != newType) || (wantSSB && !hadSSB);
}
#endif

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
        // Entering settings menu
        settingsEnter();
    } else {
        // Exiting settings menu
        settingsExitAndSave();
    }
}

#if ENABLE_FAVORITES
// Add current station details to RAM and set dirty flag
static void addFavorite() {
    if (g_totalFavorites >= MAX_FAVORITES) return;

    // Prevent adding a station if the same frequency and mode already exist
    if (favoriteExists(g_currentFrequency, g_currentMode)) return;

    // Frequencies are saved as-is in kHz (FM 107.0 MHz is saved as 10700)
    g_favorites[g_totalFavorites] = {
        g_currentFrequency,
        g_currentMode,
        (int16_t)g_currentBFO
    };

    g_totalFavorites++;
    g_favoritesDirty = true;
}

// Delete selected favorite from RAM and set dirty flag
static void deleteFavorite() {
    if (!g_totalFavorites) return;

    // Shift all subsequent items one position to the left to fill the gap
    compactFavoritesFrom(g_favoriteSelected);

    g_totalFavorites--;

    // If the last item was deleted, move the selection to the new last item
    fixFavoriteSelectionAfterDelete();

    g_favoritesDirty = true;
}

// Finds the band index that matches favorites frequency and modulation
static inline uint8_t findBandForFavorite(const FavoriteStation& fav) {
    for (uint8_t i = 0; i < g_bandCount; ++i) {
        // Band must match both frequency range and modulation type (AM/SSB vs FM)
        bool isFmMod = (fav.modulation == FM);
        bool isFmBand = (g_bandList[i].bandType == FM_BAND_TYPE);

        if (isFmMod == isFmBand &&
            fav.frequency >= g_bandList[i].minimumFreq &&
            fav.frequency <= g_bandList[i].maximumFreq) {
            return i;   // Found a matching band
        }
    }
    return g_bandIndex; // Fallback to current band if no match is found
}

// Applies settings from selected favorite
// Must handle AM to SSB mode switch, which requires a full SSB patch reload
// to enable sideband reception
void tuneToSelectedFavorite() {
    if (!g_totalFavorites) return;

    // Capture receiver state before any changes
    BandType previousBandType = g_bandList[g_bandIndex].bandType;
    bool ssbWasLoaded = g_ssbLoaded;

    const FavoriteStation& fav = g_favorites[g_favoriteSelected];
    setAmpState(false);

    // Update global state to match favorite station target
    g_currentMode = fav.modulation;
    g_ssbLoaded = isSSB();

    uint8_t targetBand = findBandForFavorite(fav);

    if (g_bandIndex != targetBand) {
        syncActiveStateToBand();
        g_bandIndex = targetBand;
    }

    g_bandList[g_bandIndex].currentFreq = fav.frequency;
    g_currentBFO = fav.bfo;

    // Force full reconfig for FM/AM type switch or to load required SSB patch
    bool forceReset =
        favoriteNeedsFullReset(
            previousBandType,
            g_bandList[g_bandIndex].bandType,
            g_ssbLoaded,
            ssbWasLoaded
        );

    applyBandConfiguration(forceReset);

    setAmpState(true);
}
#endif

// handles tuning step adjustment
// updates the current band state and applies it to the IC
static __attribute__((noinline))
void doStep(int8_t v) {
    Band& band = g_bandList[g_bandIndex];

    int8_t* idx;
    int8_t     max;
    const int16_t* table = nullptr;

    switch (g_currentMode) {
    case FM:
        // cast address of unsigned index to a signed pointer
        // tricks the type system allowing unified processing in doSwitchLogic
        idx = (int8_t*)&band.stepIdxFM;
        max = g_lastStepFM;
        table = (const int16_t*)g_tabStepFM;
        break;

    case LSB:
    case USB:
    case CW: // CW shares the same step settings as SSB
        idx = (int8_t*)&band.stepIdxSSB;
        max = SSB_STEPS_COUNT - 1;
        // for SSB/CW step is not sent to IC step register,
        // as tuning is done via BFO adjustments
        // table pointer remains null
        break;

    default: // AM
        idx = (int8_t*)&band.stepIdxAM;
        max = IS_LW_MW(band.bandType) ? 3 : (AM_STEPS_COUNT - 1);
        table = (const int16_t*)g_tabStep;
        break;
    }

    // idx is an int8_t pointer - dereferencing it provides a value
    // that can be correctly passed by reference to doSwitchLogic
    doSwitchLogic(*idx, 0, max, v);

    // if step table was assigned (i.e., not for SSB/CW) update IC
    if (table) g_si4735.setFrequencyStep((uint16_t)table[*idx]);

    showStep();
}

// FM signals sound louder than other audio sources
// Apply a user-set offset for consistent volume feel
static void applyCompensatedVolume() {
    if (g_currentMode == FM) {
        int8_t offset = g_Settings[FmVolAdjust].param;
        g_si4735.setVolume(constrain(g_volume - offset, 0, 63));
    } else {
        g_si4735.setVolume(g_volume);
    }
}

// Volume control
// User expects volume buttons to cancel mute state
// This provides immediate auditory feedback on first press
static void doVolume(int8_t v) {
    if (!g_muteVolume) {
        g_volume = constrain(g_volume + v, 0, 63);
    } else {
        g_muteVolume = 0;
    }
    applyCompensatedVolume();
    showVolume();
}

// Handles bandwidth adjustment based on current modulation
// Each mode has distinct hardware commands and bandwidth tables
static inline void doBandwidth(uint8_t v) {
    if (g_currentMode == CW) return;

    Band& band = g_bandList[g_bandIndex];

    switch (g_currentMode) {
    case LSB:
    case USB:
        doSwitchLogic(band.bwIdxSSB, 0, MAX_INDEX(bw_ssb_map), v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[band.bwIdxSSB]);
        updateSSBCutoffFilter();
        break;

    case AM:
        doSwitchLogic(band.bwIdxAM, 0, MAX_INDEX(bw_am_map), v);
        g_si4735.setBandwidth(g_bwAMIdx[band.bwIdxAM], 1);
        break;

    case FM:
        // invert step because FM map is ordered in reverse
        // this makes knob rotation feel consistent with other modes
        doSwitchLogic(band.bwIdxFM, 0, MAX_INDEX(bw_fm_map), -v);
        g_si4735.setFmBandwidth(band.bwIdxFM);
        break;

        // for any unexpected modes do nothing
    default: break;
    }

    showBandwidth();
}

// Settings: FM Volume Adjust
// Fine-tunes the software volume reduction for FM mode to match AM/SSB levels
void doFmVolAdjust(int8_t v) {
    doSwitchLogic(g_Settings[FmVolAdjust].param, 0, 15, v);
    if (g_currentMode == FM) applyCompensatedVolume();
}

// Settings: Attenuation (ATT)
// manual control over the receiver front-end gain, which handled by the Automatic Gain Control (AGC)
// 'AUT' (Auto) is the standard mode.
// can be useful to prevent overload from very strong local stations
// (by increasing attenuation)
void doAttenuation(int8_t v) {
    uint8_t max_att_value = (g_currentMode == FM) ? MAX_ATTENUATION_FM_DB : MAX_ATTENUATION_AM_DB;
    doSwitchLogic(g_Settings[ATT].param, 0, max_att_value, v);

    setAgcHardware(g_Settings[ATT].param);
    persistModeSetting(MODE_SETTING_AGC, ATT);
}

// Settings: Soft Mute Attenuation
// controls HOW MUCH the volume is reduced when a signal becomes weak
// A higher value means stronger muting, making the receiver almost silent on noisy frequencies
// Setting it to 0 - disables soft mute feature
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SoftMute].param, 0, SOFT_MUTE_MAX_ATTENUATION, v);

    // persist per modulation (AM, LSB, USB, CW)
    persistModeSetting(MODE_SETTING_SOFT_MUTE, SoftMute);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SoftMute].param);
}

// Settings: Soft Mute Threshold
// controls WHEN the soft mute feature activates
// It sets a minimum signal quality (SNR) threshold
// If the signal drops below this level, the audio will be muted by the amount set in 'SMA'
void doSoftMuteThreshold(int8_t v) {
    doSwitchLogic(g_Settings[SoftMuteThr].param, 0, SOFT_MUTE_MAX_SNR_THRESHOLD, v);
    if (!g_si4735.isCurrentTuneFM())
        g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);
}

//Settings: Brightness
void doBrightness(int8_t v) {
    int8_t new_setting = g_Settings[Brightness].param + v;

    // clamp the value of to the [0, 9]
    new_setting = constrain(new_setting, 0, BRIGHTNESS_MAX_LEVEL);

    g_Settings[Brightness].param = new_setting;
    applyBrightness();
}

//Settings: SSB AVC Switch
void doSSBAVC(int8_t v) {
    toggleSetting(SVC);
    if (isSSB()) {
        g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);
        applyBandConfiguration(true);
    }
}

// Settings: Automatic Volume Control (AVC)
// Adjusts maximum gain for the AVC system to normalize volume levels
// between strong and weak stations
// Higher values give more aggressive leveling making quiet stations louder
// Maps simple user index (0-10) to non-linear hardware gain value (12-90)
void doAvc(int8_t v) {
    if (g_currentMode == FM) return;

    doSwitchLogic(g_Settings[AutoVolControl].param, AVC_MIN_INDEX, AVC_MAX_INDEX, v);

    persistModeSetting(MODE_SETTING_AVC, AutoVolControl);

    // re-apply value to hardware immediately
    uint8_t avcValue = getAvcValueFromIndex(g_Settings[AutoVolControl].param);
    g_si4735.setAvcAmMaxGain(avcValue);
}

//Settings: Sync switch
void doSync(int8_t v) {
    // Sync is not need in CW mode
    if (g_currentMode == CW) return;

    toggleSetting(Sync);

    if (isSSB()) {
        uint8_t p = g_Settings[Sync].param; // p ∈ {0,1}
        g_si4735.setSSBDspAfc(1 - p);
        g_si4735.setSSBAvcDivider(3 * p);
        applyBandConfiguration(true);
    }
}

// Settings: FM De-Emphasis (DE)
// sets de-emphasis time constant for FM reception
// matches the pre-emphasis used by broadcasters in different regions
// 75 µs is standard for America, 50 µs for Europe and rest of
void doDeEmp(int8_t v) {
    toggleSetting(DeEmp);
    if (g_currentMode == FM)
        g_si4735.setFMDeEmphasis(g_Settings[DeEmp].param + 1);
}

//Settings: SW Units
void doSWUnits(int8_t v) {
    toggleSetting(SWUnits);
}

//Settings: SW Units
void doSSBSoftMuteMode(int8_t v) {
    toggleSetting(SSM);
    if (isSSB())
        g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

//Settings: SSB Cutoff filter
void doCutoffFilter(int8_t v) {
    doSwitchLogic(g_Settings[CutoffFilter].param, 0, CUTOFF_FILTER_MAX_VALUE, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v) {
    toggleSetting(CPUSpeed);
    setCpuPrescaler(g_Settings[CPUSpeed].param);
}

// Settings: BFO Offset calibration
void doBFOCalibration(int8_t v) {

    // BFO calibration is not applicable in FM
    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) return;

    // Expanded range to -25..+25. With a x100 multiplier in updateBFO(),
    // this provides a +/- 2.5kHz calibration range in 100Hz step
    doSwitchLogic(g_Settings[BFO].param, BFO_CALIBRATION_MIN, BFO_CALIBRATION_MAX, v);

    // Write the temporary UI value to the current band persistent field
    g_bandList[g_bandIndex].bfoCal = g_Settings[BFO].param;
    markStateAsDirty();

    if (isSSB()) updateBFO();
}

//Settings: Scan button switch
void doScanSwitch(int8_t v) {
    toggleSetting(ScanSwitch);
}

// Settings: CW Pitch
// Selects the audible tone frequency for CW reception
void doCWPitch(int8_t v) {
    doSwitchLogic(g_Settings[CWPitch].param, 0, MAX_INDEX(cw_pitch_options_hz), v);
    if (g_currentMode == CW) updateBFO();
}

// Settings: Toggles the battery voltage pin between A1 and A2.
void doBatteryPinSelect(int8_t v) {
    toggleSetting(BATT_PIN);
}

//Settings: Auto Antenna Capacitor
void doAntennaCapacitor(int8_t v) {
    toggleSetting(AntennaCap);
}

//Settings: RSSI AM Off switch
void doRSSIAMOff(int8_t v) {
    toggleSetting(RSSI_AM_Off);
}

//Settings: Display timeout switch
void doDisplayOff(int8_t v) {
    doSwitchLogic(g_Settings[DisplayOff].param, 0, DISPLAY_OFF_TIMER_MAX_LEVEL, v);
}

// Settings: FM Audio Profile (Speaker EQ)
// Toggles a curated audio profile designed to improve sound on the small internal speaker.
// When disabled, it restores default chip settings for pure audio output, ideal for headphones.
void doFMAudioProfile(int8_t v) {
    toggleSetting(FMAudioProfile);
    if (g_currentMode == FM) FMAudioConfigure();
}

// Settings: Force FM Mono
// Toggles between automatic stereo/mono blend and forced mono reception
void doForceMono(int8_t v) {
    toggleSetting(ForceMono);
    if (g_currentMode == FM) applyFMStereoSettings();
}

// Settings: AM Noise Blanker
void doAMNoiseBlanker(int8_t v) {
    toggleSetting(AMNoiseBlanker);
    if (g_currentMode != FM) applyAMNoiseBlankerSettings();
}

// Settings: Squelch Threshold
// Handles user input for the Squelch (SQL) setting in the menu
// Adjusts the RSSI threshold from 0 (OFF) to 60
// As a safety measure if the squelch is manually disabled (set to 0) while it is actively muting the audio,
// this function immediately un-mutes receiver
void doSquelch(int8_t v) {
    doSwitchLogic(g_Settings[SQL].param, 0, SQUELCH_MAX_LEVEL, v);

    if (g_Settings[SQL].param == 0 && g_squelchCutoff) {
        g_si4735.setAudioMute(false);
        g_squelchCutoff = false;
    }
}

// Settings: FM Soft Mute Attenuation (FSA)
// Controls how much the volume is reduced (in dB) when soft mute activates
// Higher values result in a deeper, more noticeable mute
// Range: 0 (disabled) to 31 (max)
void doFmSoftMuteAtt(int8_t v) {
    doSwitchLogic(g_Settings[FmSmAtt].param, 0, FM_SOFT_MUTE_MAX_ATTN_LEVEL, v);
    if (g_currentMode == FM)
        g_si4735.setProperty(FM_PROP_SOFTMUTE_MAX_ATTN_ADDR, g_Settings[FmSmAtt].param);
}

// Settings: FM Soft Mute Threshold (FST)
// Sets the minimum signal quality (SNR) required to keep audio at full volume
// If SNR drops below this, soft mute engages. Higher values are more aggressive
// Range: 0 to 15
void doFmSoftMuteThr(int8_t v) {
    doSwitchLogic(g_Settings[FmSmThr].param, 0, FM_SOFT_MUTE_MAX_SNR_LEVEL, v);
    if (g_currentMode == FM)
        g_si4735.setProperty(FM_PROP_SOFTMUTE_SNR_THRESH_ADDR, g_Settings[FmSmThr].param);
}

// Settings: Toggle handler for SW AFC menu item (SWA)
// 0=OFF, 1=PPM, 2=Hz Normal, 3=Hz Aggressive
void doSwAfcProfile(int8_t v) {
    doSwitchLogic(g_Settings[SWAFC].param,
        SW_AFC_PROFILE_OFF,
        SW_AFC_PROFILE_HZ_AGGR,
        v);
    applySwAfc();
}

// Settings: switcher - S-Point display to RSSI display
void doSMeter(int8_t v) {
    toggleSetting(SMeter);
}

// Settings: Navigation Style
// Toggles between row-first and column-first navigation in the settings menu
void doNavStyle(int8_t v) {
    toggleSetting(NAV);
    // Changing navigation style requires a full redraw of the settings page
    oled.clear();
    showSettingsTitle();
    showSettings();
}

// ==========================================
// ========== S-METER LOGIC =================
// ==========================================

// linear scan saves flash on avr for small progmem tables
// CREAD helper abstracts required pgm_read_byte call
static uint8_t scanThreshold(
    uint8_t rssi,
    const uint8_t* thr,
    uint8_t len) {
    uint8_t i = 0;
    while (i < len && rssi > CREAD(thr, i)) ++i;
    return i;
}

// map index to S-point based on receiver mode
// fm uses custom low-end map for better squelch feel
// hf follows classic S scale for SWL convention
static inline void mapIdxToSAndPlus(
    uint8_t idx,
    uint8_t fm,
    uint8_t* s,
    uint8_t* plus) {
    if (fm) {
        if (idx < 4) {
            *s = CREAD(FM_S4, idx);
            *plus = 0;
            return;         // fast path for weak fm
        }
        *s = 9;
        *plus = (idx > 4);  // show S9+ only above index 4
        return;
    }
    if (idx < 9) {
        *s = idx;
        *plus = 0;
        return;
    }
    *s = 9;
    *plus = 1;              // classic S9+
}

// format fixed-width string to keep UI columns aligned
// ui shows a simple plus indicator not a numeric dB value
static void formatSMeter(
    char* buf,
    uint8_t s,
    uint8_t plus) {
    buf[0] = 'S';
    buf[1] = (s < 9) ? ('0' + s) : '9';
    buf[2] = plus ? '+' : ' ';
    buf[3] = '\0';
}

// orchestrate s-meter display from raw rssi value
// blanks output on no signal to prevent stale readings
// splits path for fm/hf to use mode-specific rules
void rssiToSLevel(
    char* buffer,
    uint8_t rssi) {
    if (rssi == UI_SIGNAL_NO_VALUE) {
        // write "   \0" in one go
        *((uint32_t*)buffer) = 0x00202020;
        return;
    }

    uint8_t fm = (g_currentMode == FM);
    const uint8_t* thr = fm ? THR_FM : THR_HF;
    uint8_t len = fm ? LEN_FM : LEN_HF;

    uint8_t idx = scanThreshold(rssi, thr, len);

    uint8_t s, plus;
    mapIdxToSAndPlus(idx, fm, &s, &plus);
    formatSMeter(buffer, s, plus);
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
        g_previousFrequency = g_currentFrequency;
    }
}

// =-=-=-=-=-=-=-=-= Encoder coalesce helper =-=-=-=-=-=-=-=-=

// fold safe coalesced movement back to main counter and tune once
inline static __attribute__((always_inline))
bool applySafeEncoderDeltaAndTune() {
    int16_t safe = getAndResetEncoderCount(g_safeEncoderMovement);
    if (!safe) return false;

    noInterrupts();
    g_encoderCount += safe;
    interrupts();

    doFrequencyTune();
    return true;
}

// Handles the delayed frequency update for AM/FM to prevent flooding the chip
static void handleDelayedFrequencyUpdate() {
    if (!g_processFreqChange || isSSB()) return;

    if (applySafeEncoderDeltaAndTune()) return;

    performFrequencyUpdateCheck(millis());
}

// =-=-=-=-=-=-=-=-= RSSI helpers =-=-=-=-=-=-=-=-=

// skip AM polling when disabled or right after user action to avoid clicks
inline static __attribute__((always_inline))
bool amRssiPollingAllowed(uint32_t now_ms) {
    if (g_Settings[RSSI_AM_Off].param == 1) return false;
    uint16_t now_s = (uint16_t)(now_ms / 1000);
    return (uint16_t)(now_s - g_lastUserActivityTime) >= 1;
}

// Fetches signal quality (RSSI) using mode-specific commands
// SSB/CW is unsupported by this patch query method
static uint8_t getSignalQuality() {
    switch (g_currentMode) {
    case FM:
        g_si4735.getCurrentReceivedSignalQuality(1);
        return g_si4735.getCurrentRSSI();

    case AM:
        if (!amRssiPollingAllowed(millis())) return g_signalQualityValue;
        // soft update prevents audio clicks
        g_si4735.softAmRssiUpdate();
        return g_si4735.getReceivedSignalStrengthIndicator();

    case LSB: case USB: case CW:
    default: return INVALID_RSSI_VALUE;
    }
}

// Polls for new signal quality and updates UI only on change
// also triggers squelch logic after each poll
static inline void updateSignalQuality() {
    uint8_t new_value = getSignalQuality();
    updateIfChanged(g_signalQualityValue, new_value, showSignalQuality);
    handleSquelch();
}

// helper for FM stereo indicator logic
static inline void updateFmStereoIndicator() {
    if (g_currentMode != FM || millis() <= 3000) return;
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
static inline void handleSignalAndStereoUpdates() {
    // 500ms debounce after last frequency change to prevent polling while actively tuning
    if (millis() - g_lastFreqChange < RSSI_POLL_DELAY_AFTER_TUNE_MS) return;

    if (uiBusy()) return;

    if (millis() - g_lastRSSIUpdate >= RSSI_POLL_INTERVAL_MS) {
        g_lastRSSIUpdate = millis();

        updateSignalQuality();
        updateFmStereoIndicator();
    }
}

// =-=-=-=-=-=-=-=-= Timeout helpers =-=-=-=-=-=-=-=-=

// command timeout depends on context so compute once
inline static __attribute__((always_inline))
uint32_t currentCmdTimeoutMs() {
    return g_settingsActive ? SETTINGS_MENU_TIMEOUT : ADJUSTMENT_ACTIVE_TIMEOUT;
}

// provides auto-exit for both temporary adjustment modes and the main Settings menu
static inline void handleCommandTimeout() {
    if (!g_lastAdjustmentTime) return;

    uint32_t now = millis();
    uint32_t timeout = currentCmdTimeoutMs();

    if (now - g_lastAdjustmentTime > timeout) {
        if (g_settingsActive) {
            g_settingsActive = false;
            switchSettings();
        }
        resetCommandMode();
    }
}

#if ENABLE_FAVORITES
// Provides auto-exit for the favorites menu on inactivity
static inline void handleFavoritesTimeout() {
    if (g_favoritesActive && (millis() - g_lastAdjustmentTime > SETTINGS_MENU_TIMEOUT))
        exitFavoritesMenu();
}
#endif

// =-=-=-=-=-=-=-=-= Save helpers =-=-=-=-=-=-=-=-=

// idle save protects EEPROM during active tuning and still captures last freq
inline static __attribute__((always_inline))
bool shouldSaveStateOnIdle(uint16_t now_s) {
    uint16_t idle_s = (uint16_t)(SAVE_ON_IDLE_TIMEOUT / 1000);
    return g_stateIsDirty && ((uint16_t)(now_s - g_lastUserActivityTime) > idle_s);
}

// Manages saving settings to EEPROM based on user activity
static inline void handleSettingsSave() {
    // save menu settings immediately on exit for predictable behavior
    if (g_settingsDirty) {
        saveAllReceiverInformation(true);
        g_settingsDirty = false;
        g_stateIsDirty = false;             // settings include state, reset both flags
        return;
    }

    uint16_t now_s = (uint16_t)(millis() / 1000);

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

// enter deep sleep for display to extend battery life
inline static __attribute__((always_inline)) void engageDisplaySleep() {
    g_displayOn = false;
    // on auto-timeout engage deep power save mode at 2 MHz to maximize battery life
    setCpuPrescaler(CPU_PRESCALER_DEEP_SLEEP); // 3 = 2 MHz , 2 = 4 MHz , 1 = 8 MHz
    oled.setPower(false);
    autoDisplayOff = true;
}


// Handles auto display-off timer
// tracks time in seconds to keep math in 16-bit
static inline void checkDisplayTimeout() {
    uint8_t p = g_Settings[DisplayOff].param;

    if (!g_displayOn || p == 0) return;

    uint16_t timeout_s = displayTimeoutS(p);

    if ((uint16_t)(millis() / 1000) - g_lastUserActivityTime > timeout_s)
        engageDisplaySleep();
}

// for all time-based tasks
static void handlePeriodicTasks() {
    handleSignalAndStereoUpdates();
    handleCommandTimeout();
    handleSettingsSave();
#if ENABLE_BATTERY_MONITOR
    updateAndShowBattery(false);
#endif
}

// ==========================================
// ===== MAIN APPLICATION ENTRY POINTS ======
// ==========================================

// Helper to initialize hardware pins and battery check
static inline void initHardwarePins() {
    setAmpState(false);

    DDRB |= (1 << DDB5);
    DDRD &= ~((1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B));
    PORTD |= (1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B);

    // get the correct pin for the initial connection check (lf in Battery.h)
    g_voltagePinConnnected = (uint16_t)analogRead(getBatteryPin()) > ADC_CONNECTED_THRESHOLD;
}

// Helper to initialize OLED display
static inline void initOLED() {
    oled.init();
    oled.clear();
    oled.setPower(true);
    oled.setScale(1);
}

// Helper to handle EEPROM reset on button press
static inline void handleEEPROMReset() {
#if DEBUG_MODE
    initDebugUART();
    debugPrint_P(PSTR("Debug started\n"));
#endif

    // Force EEPROM reset if specific buttons are held on startup
    if (!(PINC & (1 << (ENCODER_BUTTON - 14))) || !(PINB & (1 << (AGC_BUTTON - 8)))) {
        // Invalidate version to trigger reset logic
        eeprom_update_byte((uint8_t*)EEPROM_VERSION_ADDRESS, 0);
    } else {
#if ENABLE_SPLASH_SCREEN
        showSplashScreen();
#endif
    }
}

// Helper to initialize interrupts and Si4735 chip
static inline void initSi4735() {
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);
    g_si4735.setMaxSeekTime(SEEK_TIME);

    delay(SYSTEM_INIT_DELAY_MS);
}

// Helper to load receiver configuration from EEPROM
static inline void loadReceiverConfig() {
    // Load configuration from EEPROM or initialize with defaults
    readAllReceiverInformation();

#if ENABLE_FAVORITES
    loadFavorites();
#endif
}

// Helper to apply initial configuration and show status
static inline void applyInitialConfiguration() {
    setCpuPrescaler(g_Settings[SettingsIndex::CPUSpeed].param);

    applyBandConfiguration(false);
    g_currentFrequency = g_si4735.getFrequency();
    g_si4735.setVolume(g_volume);

    oled.clear();
    showStatus();
}

// Initialize controller
void setup() {
#if DEBUG_MODE
    initDebugUART();
    debugPrint_P(PSTR("\n\n--- ATS_EX DEBUG START ---\n"));
#endif
    initHardwarePins();
    initOLED();
    handleEEPROMReset();
    initSi4735();
    loadReceiverConfig();
    applyInitialConfiguration();

    setAmpState(true);
    g_previousFrequency = g_currentFrequency;
}

#if ENABLE_FAVORITES
static inline bool handleFavoritesMode(int16_t safe_encoder_delta) {
    if (!g_favoritesActive) return false;

    if (safe_encoder_delta)
        handleFavoritesMenu(safe_encoder_delta);

    processButtonEvents();
    handleFavoritesTimeout();
    return true;
}
#endif

#if ENABLE_CW_DECODER
static inline bool handleCWViewMode() {
    if (!g_cwViewActive) return false;

    cwViewTask();
    processButtonEvents();
    return true;
}
#endif

// main loop program in process order
void loop() {
    updateEncoderState();
    checkDisplayTimeout();

#if ENABLE_CW_DECODER
    if (handleCWViewMode()) return;
#endif

    int16_t safe_encoder_delta = getAndResetEncoderCount(g_safeEncoderMovement);

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

//Overriding original main to save some space
int main(void) {
    init();
    setup();
    while (1)
        loop();
    return 0;
}
