// ----------------------------------------------------------------------
// ATS_EX (Extended) Firmware for ATS-20 and ATS-20+ receivers.
// Based on PU2CLR sources.
// Inspired by closed-source swling.ru firmware.
// For more information check README file in my github repository:
// http://github.com/goshante/ats20_ats_ex
// ----------------------------------------------------------------------
// By Goshante
// 02.2024
// http://github.com/goshante
// ----------------------------------------------------------------------
// MOD_NO_RDS by diqezit
// More info for this mod you can get below
// https://github.com/diqezit/ats20_ats_ex
// ----------------------------------------------------------------------
// Si4704/05/06/3x FM Receiver Programming:
// – Hardware interface control (I2C signal mappings, GPIO functions)
// – Software command set (register definitions, status reads/writes)
// – Configuration workflows (tuning, volume, seek, power modes and more..)
// Ref here https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN332.pdf
// ----------------------------------------------------------------------
// Using the work of
// https://github.com/esp32-si4732/ats-mini
// https://github.com/G8PTN/ATS_MINI
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

// =-=-=-=-=-=-=-=-= Band state helpers =-=-=-=-=-=-=-=-=

// only accept live freq if it belongs to current band
// protects band memory from off-range writes
inline static __attribute__((always_inline)) bool freqInCurrentBand(uint16_t f) {
    const Band& b = g_bandList[g_bandIndex];
    return (f >= b.minimumFreq) && (f <= b.maximumFreq);
}

// most state is already in the band list
// only need to sync the single live frequency variable
void syncActiveStateToBand() {
    if (freqInCurrentBand(g_currentFrequency))
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
}

// functions will read step/bw directly from the band list
// only need to load the frequency into the single live variable
void loadActiveStateFromBand() {
    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
}

// Syncs mode-dependent settings between UI buffer (g_Settings)
// and persistent storage (g_modeSettings)
// when loading use current mode context + when saving propagate UI values
// to all contexts for factory reset
void syncModeDependentSettings(bool load) {
    if (load) {
        const uint8_t m = getModeContext();
        g_Settings[ATT].param = g_modeSettings[MODE_SETTING_AGC][m];
        g_Settings[SoftMute].param = g_modeSettings[MODE_SETTING_SOFT_MUTE][m];
        g_Settings[AutoVolControl].param = g_modeSettings[MODE_SETTING_AVC][m];
    } else {
        for (uint8_t m = 0; m < MODE_CONTEXT_COUNT; m++) {
            g_modeSettings[MODE_SETTING_AGC][m] = g_Settings[ATT].param;
            g_modeSettings[MODE_SETTING_SOFT_MUTE][m] = g_Settings[SoftMute].param;
            g_modeSettings[MODE_SETTING_AVC][m] = g_Settings[AutoVolControl].param;
        }
    }
}

// Settings: CPU Frequency divider helper
// touch prescaler atomically as required by AVR
static void setCpuPrescaler(uint8_t prescaler) {
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = prescaler;
    interrupts();
}

// apply a list of Si4735 properties from PROGMEM
// single pass table keeps call sites small and easy to audit
static void applyProperties(const uint16_t props[][2]) {
    const uint16_t* p = &props[0][0];  // walk pairs [addr,val] PROGMEM
    for (;;) {
        uint16_t prop_addr = pgm_read_word(p++);
        if (prop_addr == 0) break;
        uint16_t prop_val = pgm_read_word(p++);
        g_si4735.setProperty(prop_addr, prop_val);
    }
}

// =-=-=-=-=-=-=-=-= Seek stop helpers =-=-=-=-=-=-=-=-=

// raw, no-debounce read of encoder button from PINC
// used inside critical section while seek is in progress
inline static __attribute__((always_inline)) bool encBtnPressedRaw() {
    return !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

// callback polled by seek process, sets stop flag on any user action
// single critical section avoids torn reads while ISR may toggle the flag
static inline bool checkStopSeeking() {
    noInterrupts();
    bool pressed = encBtnPressedRaw();
    if (pressed) g_seekStop = true;
    bool stop = g_seekStop;
    interrupts();
    return stop;
}

// =-=-=-=-=-=-=-=-= SW AFC helpers =-=-=-=-=-=-=-=-=

// Convert fixed-Hz window to 16-bit AFC register (clamped 1..0xFFFF)
// add half-window for rounding so user windows map predictably
static uint16_t swAfcRegFromHzK(uint32_t fk1000, uint16_t winHz) {
    if (!winHz) return 1;
    uint32_t v = (fk1000 + (winHz / 2)) / winHz;
    return (v > 0xFFFF) ? 0xFFFF : (uint16_t)v;
}

// PPM defaults (115/85 ppm) via applyProperties
// simple path to restore datasheet defaults
static void applySwAfcProfilePpm() {
    static const uint16_t props[][2] PROGMEM = {
        { AM_AFC_SW_PULL_IN_RANGE_PROP, AM_AFC_SW_PULL_IN_RANGE_VAL },
        { AM_AFC_SW_LOCK_IN_RANGE_PROP, AM_AFC_SW_LOCK_IN_RANGE_VAL },
        { 0, 0 }
    };
    applyProperties(props);
}

// Fixed-Hz windows at current frequency
// compute registers per live kHz to keep window constant in Hz at any dial
static void applySwAfcProfileHz(uint16_t pullHz, uint16_t lockHz) {
    const uint32_t fk1000 = (uint32_t)g_currentFrequency * 1000UL;
    const uint16_t rPull = swAfcRegFromHzK(fk1000, pullHz);
    const uint16_t rLock = swAfcRegFromHzK(fk1000, lockHz);
    g_si4735.setProperty(AM_AFC_SW_PULL_IN_RANGE_PROP, rPull);
    g_si4735.setProperty(AM_AFC_SW_LOCK_IN_RANGE_PROP, rLock);
}

// Entry: 0=OFF, 1=PPM, 2=Hz Normal, 3=Hz Aggressive
// enable only on SW in AM so broadcast bands can auto-center without touching SSB/CW
static void applySwAfc() {
    if (g_bandList[g_bandIndex].bandType != SW_BAND_TYPE || g_currentMode != AM) return;

    switch (g_Settings[SWAFC].param) {
    case SW_AFC_PROFILE_PPM:
        applySwAfcProfilePpm();
        break;
    case SW_AFC_PROFILE_HZ_NORMAL:
        applySwAfcProfileHz(SW_AFC_PULL_HZ_NORMAL, SW_AFC_LOCK_HZ_NORMAL);
        break;
    case SW_AFC_PROFILE_HZ_AGGR:
        applySwAfcProfileHz(SW_AFC_PULL_HZ_AGGR, SW_AFC_LOCK_HZ_AGGR);
        break;
    default: break; // OFF
    }
}

// ==========================================
// ===== LOW-LEVEL HARDWARE CONTROL =========
// ==========================================

// drive amp shutdown via MCU pin so mode switches do not pop the speaker
// always set pin direction before write to survive random boot states
static inline void __attribute__((always_inline)) setAmpState(bool on) {
    AMP_DDR |= (1 << AMP_BIT);        // Set as OUTPUT
    if (on) {
        AMP_PORT &= ~(1 << AMP_BIT);  // LOW (on)
    } else {
        AMP_PORT |= (1 << AMP_BIT);   // HIGH (off)
    }
}

// let user ATT override AGC when >0, otherwise leave AGC on
// chip expects index shifted by 1 for manual mode, map here to hide detail
static inline void setAgcHardware(int8_t att_val) {
    bool disableAgc = att_val > 0;
    // attenuation index for the chip is one less than the parameter valu
    // if att_val is 0 (auto) or 1 (manual, 0dB), the index sent to the chip is 0
    uint8_t agcNdx = (att_val > 1) ? (att_val - 1) : 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

// =-=-=-=-=-=-=-=-= BFO helpers =-=-=-=-=-=-=-=-=

// CW uses a fixed audio pitch so apply tone offset here instead of touching carrier
static inline __attribute__((always_inline)) int16_t bfoCwOffsetHz() {
    if (g_currentMode != CW) return 0;
    uint16_t pitch = pgm_read_word(&cw_pitch_options_hz[g_Settings[CWPitch].param]);
    return (g_lastCWMode == USB) ? -(int16_t)pitch : (int16_t)pitch;
}

// user calibration compensates crystal drift
// invert for USB to keep tuning natural
static inline __attribute__((always_inline)) int16_t bfoCalibrationHz(uint8_t sideband) {
    int16_t v = (int16_t)g_Settings[BFO].param * BFO_CALIBRATION_MULTIPLIER;
    return (sideband == USB) ? (int16_t)-v : v;
}

// Sets BFO with user calibration and automatic CW pitch offset
// Si4735 requires an inverted BFO value for sideband selection
static void updateBFO() {
    int16_t cwOffset = bfoCwOffsetHz();
    uint8_t sideband = (g_currentMode == CW) ? g_lastCWMode : g_currentMode;
    int16_t calibration = bfoCalibrationHz(sideband);
    int16_t finalBfo = g_currentBFO + calibration + cwOffset;

    g_si4735.setSSBBfo(finalBfo * -1);
}

// =-=-=-=-=-=-=-=-= SSB cutoff helpers =-=-=-=-=-=-=-=-=

// auto cutoff pick for common widths so default sound stays natural without menu tweaks
static inline __attribute__((always_inline)) uint8_t ssbAutoCutoffFromBwIdx(uint8_t idx) {
    return (idx == 0 || idx == 4 || idx == 5) ? 0 : 1;
}

//Saves more flash image size
static void updateSSBCutoffFilter() {
    uint8_t idx = g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB];

    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter(ssbAutoCutoffFromBwIdx(idx));
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

// =-=-=-=-=-=-=-=-= Squelch helpers =-=-=-=-=-=-=-=-=

// apply mute only on AM when RSSI drops below user threshold to hide weak noise
static inline __attribute__((always_inline)) bool squelchShouldCut() {
    uint8_t lvl = g_Settings[SQL].param;
    return (lvl && g_currentMode == AM && g_signalQualityValue < lvl);
}

// change mute state only on edge to avoid unnecessary I2C audio toggles
static inline __attribute__((always_inline)) void squelchApply(bool cut) {
    if (cut != g_squelchCutoff) {
        g_si4735.setAudioMute(cut);
        g_squelchCutoff = cut;
    }
}

// Manages the audio mute state based on the user-defined Squelch level and current RSSI
// The function compares the signal strength against the SQL threshold (for non-FM modes)
static void handleSquelch(void) {
    squelchApply(squelchShouldCut());
}

// ==========================================
// ===== HARDWARE CONFIGURATION =============
// ==========================================

// =-=-=-=-=-=-=-=-= SSB patch helpers =-=-=-=-=-=-=-=-=

// mute amp and switch to fast I2C before patch
// avoids speaker pop and speeds up the large transfer
static inline __attribute__((always_inline)) void ssbPatchEnter() {
    setAmpState(false);
    g_si4735.setI2CFastModeCustom(I2C_SSB_PATCH_SPEED_HZ);
    g_si4735.queryLibraryId();
    g_si4735.patchPowerUp();
    delay(PATCH_LOAD_DELAY_MS);
}

// keep patch selection centralized so build flag picks format
// keeps code size in check
static inline __attribute__((always_inline)) void ssbPatchDownload() {
#if PATCH_EX_SSB
    // compact path - compressed patch + 0x15 offset table to reduce flash
    g_si4735.downloadCompressedPatch(
        compressed_ssb_patch_content,  // PROGMEM data
        cutoff_places_offsets,         // PROGMEM table
        cutoff_nonzero_lengths,        // PROGMEM table
        cmd_0x15_offsets               // PROGMEM table
    );
#else
    // legacy patch + absolute 0x15 line list
    g_si4735.downloadCompressedPatch(
        ssb_patch_content,
        sizeof(ssb_patch_content),
        cmd_0x15,
        sizeof(cmd_0x15)
    );
#endif
}

// restore normal I2C and apply SSB defaults before unmute
// prevents clicks and ensures DSP is in a safe state
static inline __attribute__((always_inline)) void ssbPatchFinalize() {
    g_si4735.setSSBConfig(
        g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB],
        1, 0, 1, 0, 1
    );
    g_si4735.setI2CStandardMode();
    g_ssbLoaded = true;
    setAmpState(true);
}

// load SSB patch at runtime so SSB mode is available
// mute amp during patch + use fast I2C for throughput + restore band BW then unmute
static void loadSSBPatch() {
    ssbPatchEnter();
    ssbPatchDownload();
    ssbPatchFinalize();
}

// Applies user-defined FM soft mute parameters
// split constants from knobs to avoid re-sending fixed timing each time
static void applyFmSoftMuteSettings() {

    static const uint16_t fixed_soft_mute_props[][2] PROGMEM = {
        {0x1300, FM_PROP_SOFTMUTE_RATE},
        {0x1301, FM_PROP_SOFTMUTE_SLOPE},
        {0x1304, FM_PROP_SOFTMUTE_REL_RATE},
        {0x1305, FM_PROP_SOFTMUTE_ATT_RATE},
        {0, 0} // terminator
    };

    applyProperties(fixed_soft_mute_props);

    g_si4735.setProperty(0x1302, g_Settings[FmSmAtt].param);
    g_si4735.setProperty(0x1303, g_Settings[FmSmThr].param);
}

// Applies or disables AM Noise Blanker based on user settings
// threshold 0 disables per datasheet which is safer than toggling bits
static void applyAMNoiseBlankerSettings() {
    static const uint16_t am_nb_on_props[][2] PROGMEM = {
        {AM_NB_DETECT_THRESHOLD_PROP, AM_NB_THRESHOLD_DEFAULT},
        {AM_NB_INTERVAL_PROP,         AM_NB_INTERVAL_DEFAULT},
        {AM_NB_RATE_PROP,             AM_NB_RATE_DEFAULT},
        {AM_NB_IIR_FILTER_PROP,       AM_NB_IIR_FILTER_DEFAULT},
        {AM_NB_DELAY_PROP,            AM_NB_DELAY_DEFAULT},
        {0, 0} // terminator
    };
    static const uint16_t am_nb_off_props[][2] PROGMEM = {
        // setting threshold to 0 disables feature per datasheet
        {AM_NB_DETECT_THRESHOLD_PROP, 0},
        {0, 0} // terminator
    };

    // apply appropriate set of properties based on user setting
    if (g_Settings[AMNoiseBlanker].param) {
        applyProperties(am_nb_on_props);
    } else {
        applyProperties(am_nb_off_props);
    }
}

// Applies user setting for forcing mono or allowing auto-stereo in FM mode
// mono can reduce hiss on weak indoor speaker reception
static void applyFMStereoSettings() {
    g_si4735.setFmStereoMode(g_Settings[ForceMono].param);
}

// =-=-=-=-=-=-=-=-= FM audio helpers =-=-=-=-=-=-=-=-=

// group NB and blend defaults so profile stays coherent
// keeps tuning quiet between stations and limits multipath artifacts
static inline __attribute__((always_inline)) void applyFmNoiseBlankerProps() {
    static const uint16_t noise_blanker_props[][2] PROGMEM = {
        // Noise blanker
        {0x1900, FM_PROP_NB_REJ_THRESH},
        {0x1901, FM_PROP_NB_ATT_RATE},
        {0x1902, FM_PROP_NB_REL_RATE},
        {0x1903, FM_PROP_NB_ADC_OVER_THRESH},
        {0x1904, FM_PROP_NB_ADC_OVER_DELAY},

        // Multipath blend
        {0x1808, FM_MP_STEREO_THR_DEFAULT},
        {0x1809, FM_MP_MONO_THR_DEFAULT},
        {0x180A, FM_MP_ATTACK_DEFAULT},
        {0x180B, FM_MP_RELEASE_DEFAULT},

        {0, 0} // terminator
    };
    applyProperties(noise_blanker_props);
}

// switchable Hi-Cut profile to match small speaker vs headphones
// keeps code size low by driving both paths from tables
static inline __attribute__((always_inline)) void applyFmHiCutProfile(bool enabled) {
    static const uint16_t hicut_speaker_eq_props[][2] PROGMEM = {
        {0x1A00, FM_PROP_HICUT_ENABLE},
        {0x1A01, FM_PROP_HICUT_WINDOW},
        {0x1A02, FM_PROP_HICUT_SNR_THRESH},
        {0x1A03, FM_HICUT_RELEASE_DEFAULT},
        {0x1A04, FM_HICUT_MP_TRIGGER_DEFAULT},
        {0x1A05, FM_HICUT_MP_END_DEFAULT},
        {0x1A06, FM_PROP_HICUT_CUTOFF},
        {0, 0} // terminator
    };
    static const uint16_t hicut_default_props[][2] PROGMEM = {
        {0x1A00, 0},
        {0x1A06, 0x0000},
        {0, 0}
    };
    if (enabled) applyProperties(hicut_speaker_eq_props);
    else applyProperties(hicut_default_props);
}

// Applies all FM-specific audio enhancements
// Orchestrates all FM audio tweak
static void FMAudioConfigure() {
    applyFmSoftMuteSettings();
    applyFmNoiseBlankerProps();
    applyFmHiCutProfile(g_Settings[FMAudioProfile].param);
}

// Orchestrates complete Si4735 setup for FM mode
// set limits + spacing + thresholds
// then apply audio profile and stereo mode
static void configureFMMode() {
    g_currentMode = FM;
    g_stereoStatus = false;

    // Get all parameters from the current band's state
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setFM(
        current_band.minimumFreq,
        current_band.maximumFreq,
        current_band.currentFreq,
        g_tabStepFM[current_band.stepIdxFM]
    );

    g_si4735.setSeekFmLimits(
        current_band.minimumFreq,
        current_band.maximumFreq
    );

    g_si4735.setSeekFmSpacing(10);

    // lower thresholds improve find rate on weak stations
    g_si4735.setProperty(
        FM_SEEK_TUNE_SNR_THRESHOLD_PROP,
        FM_SEEK_SNR_THRESHOLD_VAL
    );
    g_si4735.setProperty(
        FM_SEEK_TUNE_RSSI_THRESHOLD_PROP,
        FM_SEEK_RSSI_THRESHOLD_VAL
    );

    g_ssbLoaded = false;

    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    g_si4735.setFMDeEmphasis(g_Settings[DeEmp].param + 1);

    FMAudioConfigure();
    applyFMStereoSettings();
}

// Configures chip for standard AM reception
// send critical audio and gain right after setAM to avoid muted audio on cold start
static void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;
    const Band& current_band = g_bandList[g_bandIndex];
    ModeContext modeCtx = getModeContext();

    // Set primary mode and frequency
    g_si4735.setAM(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        g_tabStep[current_band.stepIdxAM]
    );

    // Bandwidth
    g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);

    // Soft Mute settings
    g_si4735.setProperty(
        AM_SOFT_MUTE_SLOPE_PROP,
        AM_SOFT_MUTE_SLOPE_RECOMMENDED
    );
    g_si4735.setAmSoftMuteMaxAttenuation(
        g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx]
    );
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);

    // AGC settings first to stabilize audio level
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];
    setAgcHardware(att_val);

    // AVC Gain next to normalize loud vs weak stations
    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);
}

// Centralizes setup for properties shared between AM and SSB to avoid duplication
// ensures AVC and seek thresholds are consistent across AM family
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);

    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    // Custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(AM_SEEK_SNR_THRESHOLD_PROP, AM_SEEK_SNR_THRESHOLD_VAL);
    g_si4735.setProperty(AM_SEEK_RSSI_THRESHOLD_PROP, AM_SEEK_RSSI_THRESHOLD_VAL);

    applyAMNoiseBlankerSettings();
}

// Orchestrates Si4735 setup for SSB and CW modes
// optional patch reload + CW disables sync AFC + apply user filters and soft mute
static void configureSSBMode(
    uint16_t minFreq,
    uint16_t maxFreq,
    bool extraSSBReset) {

    Band& current_band = g_bandList[g_bandIndex];

    if (current_band.bwIdxSSB > g_bwSSBMaxIdx)
        current_band.bwIdxSSB = 4;

    // reload patch only when requested to save time
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);

    g_si4735.setSSB(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? g_lastCWMode : g_currentMode
    );

    updateSSBCutoffFilter();

    // CW uses tone offset, not DSP AFC
    if (g_currentMode == CW) {
        g_si4735.setSSBDspAfc(1);
        g_si4735.setSSBAvcDivider(0);
    } else { // LSB or USB
        uint8_t p = g_Settings[Sync].param;  // p ∈ {0,1}
        g_si4735.setSSBDspAfc(1 - p);
        g_si4735.setSSBAvcDivider(3 * p);
    }

    // soft mute and SNR gate from storage for SSB
    g_si4735.setAmSoftMuteMaxAttenuation(
        g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_SSB]
    );
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);

    // CW uses narrow fixed bandwidth, SSB uses user index
    g_si4735.setSSBAudioBandwidth(
        (g_currentMode == CW)
        ? g_bwSSBIdx[0]
        : g_bwSSBIdx[current_band.bwIdxSSB]
    );
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

// Applies AGC settings based on current mode and stored values
// keeps front-end under control when switching bands or modes
static void applyAgcSettings() {
    ModeContext modeCtx = getModeContext();
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];

    setAgcHardware(att_val);
}

// =-=-=-=-=-=-=-=-= Band config helpers =-=-=-=-=-=-=-=-=

// mute on FM<->AM transitions to avoid pop + unmute after config
// before=true means we are about to reconfigure
static inline __attribute__((always_inline)) void applyBandAmpMute(
    bool switchingBetweenFMandAM,
    bool before) {
    if (!switchingBetweenFMandAM) return;
    if (before) setAmpState(false);
    else setAmpState(true);
}

// select antenna capacitor per band so input match fits RF path
static inline __attribute__((always_inline)) void applyBandAntennaCap(
    bool isFmBand) {
    uint8_t cap_value = isFmBand ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);
}

// Top-level orchestrator for all band and mode changes
// mute around FM<->AM + load state + set RF cap + configure mode then refresh UI
static void applyBandConfiguration(bool extraSSBReset) {
    // detects a major mode switch (FM <-> non-FM) to safely toggle amp
    bool isFmBand = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    bool switchingBetweenFMandAM = (g_currentMode == FM) != isFmBand;

    // disable squelch if active to avoid stuck mute across reconfig
    if (g_squelchCutoff) {
        g_si4735.setAudioMute(false);
        g_squelchCutoff = false;
    }

    applyBandAmpMute(switchingBetweenFMandAM, true);

    loadActiveStateFromBand();

    g_signalQualityValue = INVALID_RSSI_VALUE;

    applyBandAntennaCap(isFmBand);

    if (isFmBand) {
        configureFMMode();
    } else {
        uint16_t minFreq = g_bandList[g_bandIndex].minimumFreq;
        uint16_t maxFreq = g_bandList[g_bandIndex].maximumFreq;

        if (g_ssbLoaded) {
            configureSSBMode(minFreq, maxFreq, extraSSBReset);
        } else {
            configureAMMode(minFreq, maxFreq);
        }
        configureAMCommon(minFreq, maxFreq);

        applySwAfc();
    }

    applyAgcSettings();

    if (!g_settingsActive) {
        oled.clear();
        showStatus(true);
    }

    applyBandAmpMute(switchingBetweenFMandAM, false);

    g_previousFrequency = g_currentFrequency;
}

// =-=-=-=-=-=-=-=-= Seek setup helpers =-=-=-=-=-=-=-=-=

// normalize AM seek spacing to allowed HW steps
// fallback to 5 kHz when user step is unsupported
static inline __attribute__((always_inline)) uint8_t normalizeAmSeekSpacing(
    uint16_t current_step
) {
    uint8_t seek_spacing = (current_step > 10) ? 10 : (uint8_t)current_step;
    if (seek_spacing != 1 && seek_spacing != 5 &&
        seek_spacing != 9 && seek_spacing != 10) {
        seek_spacing = 5;
    }
    return seek_spacing;
}

// Configures hardware seek parameters before starting a scan
// in AM tie seek spacing to current manual step so scan follows user intent
static inline void setupSeekParameters(
    uint16_t minLimit,
    uint16_t maxLimit
) {
    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        // for AM/SW seek step is tied to current manual step
        uint16_t current_step =
            g_tabStep[g_bandList[g_bandIndex].stepIdxAM];

        uint8_t seek_spacing = normalizeAmSeekSpacing(current_step);

        g_si4735.setSeekAmLimits(minLimit, maxLimit);
        g_si4735.setSeekAmSpacing(seek_spacing);
    } else {
        // FM spacing fixed to 10 kHz so scan grid stays standard
        g_si4735.setSeekFmLimits(minLimit, maxLimit);
        g_si4735.setSeekFmSpacing(10);
    }
}

// ==========================================
// ===== STATE & ACTION MANAGEMENT ==========
// ==========================================

// =-=-=-=-=-=-=-=-= BFO rollover =-=-=-=-=-=-=-=-=

// fast path gate when BFO leaves a small window
// large BFO usually means user spun fast so switch to coarse kHz moves to keep tuning smooth
static inline bool bfoNeedsFastRollover(int32_t bfo) {
    return (bfo >= BFO_ROLLOVER_MAX_HZ) || (bfo <= -BFO_ROLLOVER_MAX_HZ);
}

// collapse whole kHz from BFO into main frequency
// keeps BFO small for stable SSB tuning
// handles band edge jump then snaps to current step grid
static inline void bfoFastRollover(uint16_t* freq, int32_t* bfo) {
    int16_t steps_khz = (int16_t)(*bfo / HZ_PER_KHZ);
    *freq += steps_khz;
    *bfo %= HZ_PER_KHZ;

    if (*freq >= g_bandList[g_bandIndex].maximumFreq ||
        *freq < g_bandList[g_bandIndex].minimumFreq) {
        bandSwitch(steps_khz > 0, false);
    }

    snapToNewStep(freq, steps_khz > 0);
}

// convert absolute Hz back to kHz + BFO after band edge decision
// keeps BFO in 0..999 Hz
static inline void absHzToFreqBfo(long absolute_freq_hz, uint16_t* freq, int32_t* bfo) {
    int32_t new_freq_khz = (int32_t)(absolute_freq_hz / HZ_PER_KHZ);
    int32_t new_bfo_hz = (int32_t)(absolute_freq_hz % HZ_PER_KHZ);

    // corrects negative BFO back into  positive range 0-999
    // and adjusts main frequency down by 1 kHz to compensate
    if (new_bfo_hz < 0) {
        new_bfo_hz += HZ_PER_KHZ;
        new_freq_khz -= 1;
    }

    *freq = (uint16_t)new_freq_khz;
    *bfo = new_bfo_hz;
}

// precise path near limits checks absolute Hz against band in Hz
// direction comes from BFO sign so wrap matches user motion then re-snap to the step grid
static inline void bfoPreciseRollover(uint16_t* freq, int32_t* bfo) {
    long absolute_freq_hz = ((long)(*freq) * HZ_PER_KHZ) + *bfo;
    long min_freq_hz = (long)g_bandList[g_bandIndex].minimumFreq * HZ_PER_KHZ;
    long max_freq_hz = (long)g_bandList[g_bandIndex].maximumFreq * HZ_PER_KHZ;

    if (absolute_freq_hz >= max_freq_hz || absolute_freq_hz < min_freq_hz) {
        bool direction_is_up = (*bfo > 0);
        bandSwitch(direction_is_up, false);

        // after band switch recalculate freq/bfo from the absolute Hz value
        absHzToFreqBfo(absolute_freq_hz, freq, bfo);

        snapToNewStep(freq, direction_is_up);
    }
}

// performs bfo rollover with integrated boundary checks and max bfo limit
// this is core of stability system for ssb tuning
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline void performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {

    if (bfoNeedsFastRollover(*bfo)) {
        // fast - for large jumps work directly with kHz steps
        bfoFastRollover(freq, bfo);
    } else {
        // precise - for fine-tuning near the rollover point
        bfoPreciseRollover(freq, bfo);
    }
}

// sets up the station seek boundaries and step
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(DEFAULT_SEEK_DELAY_MS);

    //  for limits (strict for LW/MW, full for SW)
    uint16_t minLimit = (g_bandList[g_bandIndex].bandType == SW_BAND_TYPE)
        ? SW_MIN_FREQ : g_bandList[g_bandIndex].minimumFreq;
    uint16_t maxLimit = (g_bandList[g_bandIndex].bandType == SW_BAND_TYPE)
        ? SW_MAX_FREQ : g_bandList[g_bandIndex].maximumFreq;

    setupSeekParameters(minLimit, maxLimit);

    noInterrupts();
    g_seekStop = false;
    interrupts();

    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);

    return g_si4735.getFrequency();
}

// =-=-=-=-=-=-=-=-= Seek helpers =-=-=-=-=-=-=-=-=

// map found SW frequency to owning sub-band so limits, step and labels stay
// correct
static inline void swMapSeekToBand(uint16_t f) {
    for (uint8_t i = 2; i <= g_lastBand; ++i) {
        const Band* current_band_ptr = &g_bandList[i];
        if (f >= current_band_ptr->minimumFreq &&
            f <= current_band_ptr->maximumFreq) {
            g_bandIndex = i;
            break;
        }
    }
}

// align FM to 10 kHz grid so UI and spacing match what user expects
static inline uint16_t fmAlign10k(uint16_t f) {
    return (uint16_t)(f - (f % 10));
}

// apply DSP and UI after seek so audio and filters follow the new station
static inline void finalizeSeekUpdate() {
    g_si4735.setFrequency(g_currentFrequency);
    applySwAfc(); // Recalculate AFC window for SW (AM) after seek
    doBandwidth(0);
    syncActiveStateToBand();
    showStatus(true);
    resetEepromDelay();
    g_previousFrequency = g_currentFrequency;
}

// manages hardware seek result and syncs state
// SW remaps to sub-band for correct limits/labels
// FM aligns to 10 kHz grid
static void doSeek() {
    uint16_t f = executeHardwareSeek();
    if (!f) return;

    g_currentFrequency = f;

    switch (g_bandList[g_bandIndex].bandType) {
    case SW_BAND_TYPE:
        swMapSeekToBand(f);
        break;
    case FM_BAND_TYPE:
        g_currentFrequency = fmAlign10k(f);
        break;
    default:
        break;
    }

    finalizeSeekUpdate();
}

// =-=-=-=-=-=-=-=-= Band switch helpers =-=-=-=-=-=-=-=-=

// detect FM <-> AM-family jump which needs full reconfig to avoid pops and wrong
// props
static inline bool isMajorFmSwitch(BandType oldType, BandType newType) {
    return (oldType == FM_BAND_TYPE) != (newType == FM_BAND_TYPE);
}

// fast path for AM-family switch keeps motion seamless and refreshes UI without
// full reconfig
static inline void applySameFamilySwitchUI(BandType oldType, BandType newType) {
    g_si4735.setFrequency(g_currentFrequency);
    applyAgcSettings();
    doBandwidth(0);

    bool clearUnits =
        g_Settings[SettingsIndex::SWUnits].param &&
        ((oldType == SW_BAND_TYPE) != (newType == SW_BAND_TYPE));

    showFrequency(clearUnits);
    showBandTag();
    showStep();
    g_previousFrequency = g_currentFrequency;
}

// switches band and applies radio state
// when loadStoredFreq=false keep seamless edge crossing
// when true recall band memory
static void bandSwitch(bool up, bool loadStoredFreq) {
    syncActiveStateToBand();
    markStateAsDirty();

    uint8_t oldBandIndex = g_bandIndex;
    g_currentBFO = 0;

    int8_t delta = up ? 1 : -1;
    g_bandIndex = (g_bandIndex + delta + g_bandCount) % g_bandCount;

    if (loadStoredFreq) loadActiveStateFromBand();

    g_lastSavedFrequency = g_currentFrequency;

    BandType oldType = g_bandList[oldBandIndex].bandType;
    BandType newType = g_bandList[g_bandIndex].bandType;

    if (isMajorFmSwitch(oldType, newType)) {
        applyBandConfiguration();
        return;
    }

    applySameFamilySwitchUI(oldType, newType);
}

// =-=-=-=-=-=-=-=-= Tune helpers =-=-=-=-=-=-=-=-=

// pick current step for band so snap matches user setting
static inline uint16_t currentBandStep(const Band& b) {
    return (b.bandType == FM_BAND_TYPE)
        ? g_tabStepFM[b.stepIdxFM]
        : g_tabStep[b.stepIdxAM];
}

// for cross-band motion wrap at FM edges
// keep temp freq for AM-family to preserve seamless motion
static inline uint16_t resolveCrossBandFreq(
    const Band& old_band,
    const Band& new_band,
    bool dir_up,
    int32_t temp_freq) {
    bool wrap = (old_band.bandType == FM_BAND_TYPE) ||
        (new_band.bandType == FM_BAND_TYPE);
    return wrap
        ? (dir_up ? new_band.minimumFreq : new_band.maximumFreq)
        : (uint16_t)temp_freq;
}

// snap to grid only inside band to avoid distortion on boundary
static inline uint16_t snapIntraBand(
    uint16_t newFreq,
    uint16_t step,
    bool dir_up) {
    uint16_t rem = newFreq % step;
    return rem
        ? (uint16_t)(newFreq - rem + (dir_up ? step : 0))
        : newFreq;
}

// encoder tuning with snap-to-grid and safe band crossing
// 32-bit math prevents underflow near edges
static void doFrequencyTune() {
    int16_t encoder_delta = getAndResetEncoderCount(g_encoderCount);
    if (encoder_delta == 0) return;

    g_seekDirection = encoder_delta > 0;
    const Band& old_band = g_bandList[g_bandIndex];
    uint16_t step = currentBandStep(old_band);

    // 32-bit integer is needed here for calculations to prevent underflow on
    // band edges
    int32_t temp_freq = g_currentFrequency +
        (int16_t)step * encoder_delta;

    // > for the upper bound to include the maximum frequency value within the
    // band
    bool needs_switch =
        (temp_freq > old_band.maximumFreq) ||
        (temp_freq < old_band.minimumFreq);

    if (needs_switch) {
        // band boundary has been crossed
        bandSwitch(g_seekDirection, false);

        const Band& new_band = g_bandList[g_bandIndex];
        g_currentFrequency = resolveCrossBandFreq(
            old_band,
            new_band,
            g_seekDirection,
            temp_freq);
    } else {
        // standard intra-band tuning path
        g_currentFrequency = snapIntraBand(
            (uint16_t)temp_freq,
            step,
            g_seekDirection);
    }

    g_processFreqChange = true;
    g_lastFreqChange = millis();
    showFrequency();
    syncActiveStateToBand();
    markStateAsDirty();
}

// =-=-=-=-=-=-=-=-= SSB tune helpers =-=-=-=-=-=-=-=-=

// centralize step pick so BFO delta follows user SSB step setting
static inline int32_t ssbStepHz() {
    return (int32_t)g_tabStep[SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB];
}

// after rollover guard against landing on FM band by accident
// FM has no BFO so reset SSB state to sane defaults
static inline void ssbGuardFmAfterRollover() {
    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
        g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
        g_currentBFO = 0;
    }
}

// update chip only if base kHz changed to avoid unnecessary I2C traffic
static inline void ssbApplyChipUpdateIfNeeded(uint16_t old_freq) {
    if (g_currentFrequency != old_freq) {
        g_si4735.setFrequency(g_currentFrequency);
        applyAgcSettings();
    }
}

// prepare SSB tune by checking count and calculating temp values
static inline bool SSBTune(uint16_t& temp_freq, int32_t& temp_bfo) {
    int16_t encoder_delta = getAndResetEncoderCount(g_encoderCount);
    if (encoder_delta == 0) return false;

    // store frequency before changes to detect a rollover event
    temp_freq = g_currentFrequency;

    // 32-bit integer to prevent overflow during fast encoder spins
    temp_bfo = g_currentBFO;

    temp_bfo += ssbStepHz() * (int32_t)encoder_delta;

    return true;
}

// performs SSB rollover and chip update
static inline void SSBRollover(uint16_t& temp_freq, int32_t& temp_bfo, uint16_t old_freq) {
    performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo);

    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    // if the base frequency changed, the chip must be updated
    // this is critical fix
    ssbApplyChipUpdateIfNeeded(old_freq);
}

// finalize SSB tune - updating BFO, state, and display
static inline void SSBTuneFinalize() {
    updateBFO();
    syncActiveStateToBand();
    g_lastFreqChange = millis();
    g_previousFrequency = 0;
    showFrequency();
    markStateAsDirty();
}

// handles ssb tuning using the definitive "atomic step with integrated checks" architecture
static void doFrequencyTuneSSB() {
    uint16_t temp_freq;
    int32_t temp_bfo;
    uint16_t old_freq = g_currentFrequency;

    if (SSBTune(temp_freq, temp_bfo)) {
        SSBRollover(temp_freq, temp_bfo, old_freq);

        // post-rollover sanity check
        // its fixes invalid SSB to FM state transition (e.g 30000.00 to 1.45MHz etc.)
        ssbGuardFmAfterRollover();

        SSBTuneFinalize();
    }
}

// =-=-=-=-=-=-=-=-= Mode switch helpers =-=-=-=-=-=-=-=-=

// fold BFO remainder into kHz before leaving SSB/CW so carrier stays exact
static inline void normalizeSsbBeforeSwitch(Band& band) {
    if (!isSSB()) return;

    int32_t f = g_currentFrequency;
    int16_t b = g_currentBFO;

    int16_t k = (b >= 0)
        ? (b / HZ_PER_KHZ)
        : -((-b + (HZ_PER_KHZ - 1)) / HZ_PER_KHZ);

    f += k;
    b -= k * HZ_PER_KHZ;

    const bool out_of_bounds = (f < band.minimumFreq || f > band.maximumFreq);
    if (out_of_bounds) {
        f = (f < band.minimumFreq) ? band.minimumFreq : band.maximumFreq;
        b = 0;
    }

    g_currentFrequency = (uint16_t)f;
    g_currentBFO = b;
}

// cache BFO when leaving LSB/USB so fine tune restores on return
static inline void cacheSsbBfoIfLeaving(uint8_t mode_before) {
    if (mode_before == LSB || mode_before == USB) {
        g_savedSsbBfo[g_bandIndex] = g_currentBFO;
    }
}

// clamp bandwidth index to valid range for target map
static inline int8_t clampBwIdx(int8_t bw, bool toAm) {
    int8_t maxIdx = toAm
        ? (int8_t)MAX_INDEX(bw_am_map)
        : (int8_t)MAX_INDEX(bw_ssb_map);
    return (bw > maxIdx) ? maxIdx : bw;
}

// Prepare AM <-> SSB/CW switch
// Before saving state - normalize SSB frequency
// Ensures seamless frequency transition when switching from SSB to other modes like AM
// Save BFO to cache ONLY when exiting LSB/USB (not CW)
static inline void prepareModeSwitch(int8_t& bw) {
    Band& band = g_bandList[g_bandIndex];

    bw = (g_currentMode == AM) ? band.bwIdxAM : band.bwIdxSSB;

    const uint8_t mode_before = g_currentMode;

    // Normalization (gluing whole kHz and remainder in BFO) for SSB/CW
    normalizeSsbBeforeSwitch(band);

    // Refresh the cache only when exiting LSB/USB
    cacheSsbBfoIfLeaving(mode_before);

    syncActiveStateToBand();
    markStateAsDirty();

    if (g_currentMode == CW)
        setAmpState(false);
}

// Manages modulation state transitions AM -> SSB -> CW -> AM
// When returning from AM to SSB, restore the saved BFO from the cache
static inline void performModeCycle(int8_t bw) {
    Band& current_band = g_bandList[g_bandIndex];

    switch (g_currentMode) {
    case LSB:
    case USB:
        g_lastSsbMode = g_currentMode;
        g_currentMode = CW;
        g_currentBFO = 0; // Reset BFO for a clean start in CW
        break;

    case CW:
        g_currentMode = AM;
        g_ssbLoaded = false;
        bw = clampBwIdx(bw, true);
        current_band.bwIdxAM = bw;
        break;

    case AM:
        g_currentMode = g_lastSsbMode;
        loadSSBPatch();
        bw = clampBwIdx(bw, false);
        current_band.bwIdxSSB = bw;
        g_currentBFO = g_savedSsbBfo[g_bandIndex];
        break;
    }
}

// handles the complex logic of cycling through AM, LSB, USB, and CW modes
static inline void cycleAmSsbCwModes() {
    resetCommandMode();
    int8_t bw;
    prepareModeSwitch(bw);
    performModeCycle(bw);
    applyBandConfiguration();

    if (!g_ssbLoaded && g_currentMode == AM)
        setAmpState(true);
}

// ==========================================
// ===== USER ACTION HANDLERS (SETTINGS) ====
// ==========================================

// =-=-=-=-=-=-=-=-= Settings & Parameter Handlers =-=-=-=-=-=-=-=-=

// compute first item index for 6-per-page layout
inline static __attribute__((always_inline)) uint8_t settingsPageStart(uint8_t page) {
    return (uint8_t)(6 * (page - 1));
}

// load mode-scoped values into UI and reset cursor
inline static __attribute__((always_inline)) void settingsEnter() {
    syncModeDependentSettings(true);
    g_SettingsPage = 1;
    showSettingsTitle();
    g_SettingSelected = 0;
    g_SettingEditing = false;
    showSettings();
}

// save UI values back to storage and commit to EEPROM
inline static __attribute__((always_inline)) void settingsExitAndSave() {
    syncModeDependentSettings(false);
    g_settingsDirty = true;
    saveAllReceiverInformation();
    showStatus();
}

#if ENABLE_FAVORITES
// check duplicate to keep list useful
inline static __attribute__((always_inline)) bool favoriteExists(uint16_t f, uint8_t m) {
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        if (g_favorites[i].frequency == f && g_favorites[i].modulation == m)
            return true;
    }
    return false;
}

// compact list after removal
inline static __attribute__((always_inline)) void compactFavoritesFrom(uint8_t start) {
    for (uint8_t i = start; i < g_totalFavorites - 1; i++) {
        g_favorites[i] = g_favorites[i + 1];
    }
}

// keep selection valid after delete
inline static __attribute__((always_inline)) void fixFavoriteSelectionAfterDelete() {
    if (g_totalFavorites && g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites - 1;
    }
}

// require full reconfig on FM<->AM or SSB patch need
inline static __attribute__((always_inline)) bool favoriteNeedsFullReset(
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

// handles tuning step adjustment, updates the current band state and applies it to the IC
static void doStep(int8_t v) __attribute__((noinline));
static void doStep(int8_t v) {
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

//Volume control
static void doVolume(int8_t v) {
    int8_t vol;
    g_si4735.setVolume(
        vol = g_muteVolume
        ? (vol = g_muteVolume, g_muteVolume = 0, vol)
        : ((vol = g_si4735.getCurrentVolume() + v),
            vol < 0 ? 0 : (vol > 63 ? 63 : vol))
    );
    showVolume();
}

// handles bandwidth adjustment
static void doBandwidth(uint8_t v) {

    if (g_currentMode == CW) return;

    Band& band = g_bandList[g_bandIndex];

    // SSB mode
    if (isSSB()) {
        doSwitchLogic(band.bwIdxSSB, 0, MAX_INDEX(bw_ssb_map), v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[band.bwIdxSSB]);
        updateSSBCutoffFilter();
    }
    // AM and FM modes
    else {
        const bool is_am = (g_currentMode == AM);

        // pointer to an int8_t to target the correct index variable
        // (bwIdxAM or bwIdxFM)
        int8_t* idx = is_am ? (int8_t*)&band.bwIdxAM : (int8_t*)&band.bwIdxFM;
        int8_t  max = is_am ? MAX_INDEX(bw_am_map) : MAX_INDEX(bw_fm_map);

        int8_t step = is_am ? v : -v;

        doSwitchLogic(*idx, 0, max, step);

        // сall hardware func
        if (is_am)
            g_si4735.setBandwidth(g_bwAMIdx[*idx], 1);
        else
            g_si4735.setFmBandwidth(*idx);
    }
    showBandwidth();
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

    ModeContext m = getModeContext();
    g_modeSettings[MODE_SETTING_AGC][m] = g_Settings[ATT].param;
}

// Settings: Soft Mute Attenuation
// controls HOW MUCH the volume is reduced when a signal becomes weak
// A higher value means stronger muting, making the receiver almost silent on noisy frequencies
// Setting it to 0 - disables soft mute feature
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SoftMute].param, 0, SOFT_MUTE_MAX_ATTENUATION, v);

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
// adjusts maximum gain for the AVC system helps to normalize volume levels
// between strong and weak stations
// higher value allows for more aggressive leveling, making quiet stations louder
void doAvc(int8_t v) {
    doSwitchLogic(g_Settings[AutoVolControl].param, AVC_MAX_GAIN_MIN, AVC_MAX_GAIN_MAX, v);

    if (g_currentMode != FM)
        g_si4735.setAvcAmMaxGain(g_Settings[AutoVolControl].param);
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
    // Expanded range to -25..+25. With a x100 multiplier in updateBFO(),
    // this provides a +/- 2.5kHz calibration range in 100Hz step
    doSwitchLogic(g_Settings[BFO].param, BFO_CALIBRATION_MIN, BFO_CALIBRATION_MAX, v);

    if (isSSB()) {
        updateBFO();
    }
}

//Settings: Scan button switch
void doScanSwitch(int8_t v) {
    toggleSetting(ScanSwitch);
}

// Toggles CW sideband between LSB and USB
// It re-issues the tune command with the new sideband to avoid audio gaps
// The BFO is then reapplied, triggering auto-compensation in updateBFO
static void doCWSwitch() {
    if (g_currentMode != CW) return;

    g_lastCWMode = SIDEBAND_TOGGLE_LSB_USB - g_lastCWMode;

    // The Si4735 requires re-sending the TUNE command to change sideband
    // preventing audio gaps
    const Band& current_band = g_bandList[g_bandIndex];
    g_si4735.setSSB(
        current_band.minimumFreq,
        current_band.maximumFreq,
        g_currentFrequency,
        1, // Hardware step
        g_lastCWMode
    );

    updateBFO();
    showFrequency(true);
    updateStereoIndicator();
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

// ==========================================
// ===== PERIODIC & TIMED TASKS =============
// ==========================================

// =-=-=-=-=-=-=-=-= Freq update helpers =-=-=-=-=-=-=-=-=

// avoid 32-bit abs, compute 16-bit delta the way UI expects
inline static __attribute__((always_inline)) uint16_t freqDelta16(uint16_t a, uint16_t b) {
    return (a >= b) ? (a - b) : (b - a);
}

// rate limit protects I2C from flooding during fast turns
inline static __attribute__((always_inline)) bool freqRateLimitOk(uint32_t now) {
    return (now - g_lastSetFreqTime) >= MIN_SETFREQ_INTERVAL_MS;
}

// time gate prevents spamming setFrequency on micro moves
inline static __attribute__((always_inline)) bool freqTimeElapsed(uint32_t now) {
    return (now - g_lastFreqChange) >= FREQ_UPDATE_DELAY_MS;
}

// large delta is a user intent to jump, send early
inline static __attribute__((always_inline)) bool freqForceUpdate(uint16_t delta) {
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
inline static __attribute__((always_inline)) bool applySafeEncoderDeltaAndTune() {
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
inline static __attribute__((always_inline)) bool amRssiPollingAllowed(uint32_t now_ms) {
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

    if (g_signalQualityValue != new_value) {
        g_signalQualityValue = new_value;
        showSignalQuality();
    }

    handleSquelch();
}

// helper for FM stereo indicator logic
static inline void updateFmStereoIndicator() {
    if (g_currentMode != FM || millis() <= 3000) return;

    bool stereo = g_si4735.getCurrentPilot();

    if (stereo != g_stereoStatus) {
        g_stereoStatus = stereo;
        updateStereoIndicator();
    }
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
inline static __attribute__((always_inline)) uint32_t currentCmdTimeoutMs() {
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
inline static __attribute__((always_inline)) bool shouldSaveStateOnIdle(uint16_t now_s) {
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
inline static __attribute__((always_inline)) uint16_t displayTimeoutS(uint8_t p) {
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

// main loop program in process order
void loop() {
    updateEncoderState();
    checkDisplayTimeout();

#if ENABLE_CW_DECODER
    if (g_cwViewActive) {
        cwViewTask();
        processButtonEvents();
    } else {
#endif
        int16_t safe_encoder_delta = getAndResetEncoderCount(g_safeEncoderMovement);

#if ENABLE_FAVORITES
        if (g_favoritesActive) {
            if (safe_encoder_delta) {
                handleFavoritesMenu(safe_encoder_delta);
            }
            processButtonEvents();
            handleFavoritesTimeout();
            return;
        }
#endif

        handleDelayedFrequencyUpdate();

        bool frequencyTuned = false;
        if (safe_encoder_delta)
            frequencyTuned = processEncoderActions(safe_encoder_delta);

        if (!frequencyTuned)
            processButtonEvents();
#if ENABLE_CW_DECODER
    }
#endif

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
