#pragma once

// ====================================================================================
//
// RadioControl.h - Hardware Abstraction Layer for Si4735
//
// This file isolates application logic hardware specifics
// All direct chip interaction and radio state management occurs here
//
// Manages Si4735 commands SSB patch loading and the external audio amplifier
// It orchestrates complex state transitions for modes (AM/FM/SSB) and bands
// Func applyBandConfiguration is the primary entry point for these changes
//
// Contains core tuning algorithms - including BFO rollover for seamless SSB
//
// ====================================================================================

// ==========================================
// ===== CORE UTILITIES & STATE SYNC ========
// ==========================================

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

// =-=-=-=-=-=-=-=-= AVC gain helpers =-=-=-=-=-=-=-=-=

// Scales user index (0-10) to the chip gain range using a lookup table
// This provides manually configured response curve for optimal listening
// values represent gain in dB
static inline uint8_t getAvcValueFromIndex(uint8_t index) {
    static const uint8_t avc_table[11] PROGMEM = {
        12,  // Index 0  (MIN 12)
        16,  // Index 1
        20,  // Index 2
        24,  // Index 3
        28,  // Index 4
        32,  // Index 5
        37,  // Index 6
        43,  // Index 7
        50,  // Index 8
        55,  // Index 9
        60   // Index 10 (MAX 90)
    };

    return pgm_read_byte(&avc_table[index]);
}

// Applies mode-specific AVC gain to hardware
// Ensures correct gain is restored on mode switch
static void applyAvcGain(ModeContext modeCtx) {
    uint8_t avcIndex = g_modeSettings[MODE_SETTING_AVC][modeCtx];
    uint8_t avcValue = getAvcValueFromIndex(avcIndex);
    g_si4735.setAvcAmMaxGain(avcValue);
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
    // Read calibration value from the current band struct field
    int16_t v = (int16_t)g_bandList[g_bandIndex].bfoCal * BFO_CALIBRATION_MULTIPLIER;
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
        cutoff_nonzero_lengths
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
    applyProperties(g_Settings[AMNoiseBlanker].param ? am_nb_on_props : am_nb_off_props);
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
    applyProperties(enabled ? hicut_speaker_eq_props : hicut_default_props);
}

// Applies all FM-specific audio enhancements
// Orchestrates all FM audio tweak
static void FMAudioConfigure() {
    applyFmSoftMuteSettings();
    applyFmNoiseBlankerProps();
    applyFmHiCutProfile(g_Settings[FMAudioProfile].param);
}

// Helper to set seek thresholds for both AM and FM
// lower thresholds improve find rate on weak stations
static void setSeekThresholds(bool isFM) {
    if (isFM) {

        g_si4735.setProperty(
            FM_SEEK_TUNE_SNR_THRESHOLD_PROP,
            FM_SEEK_SNR_THRESHOLD_VAL);

        g_si4735.setProperty(
            FM_SEEK_TUNE_RSSI_THRESHOLD_PROP,
            FM_SEEK_RSSI_THRESHOLD_VAL);

    } else {
        g_si4735.setProperty(
            AM_SEEK_SNR_THRESHOLD_PROP,
            AM_SEEK_SNR_THRESHOLD_VAL);

        g_si4735.setProperty(
            AM_SEEK_RSSI_THRESHOLD_PROP,
            AM_SEEK_RSSI_THRESHOLD_VAL);
    }
}

// Helper to apply soft mute settings
// shared between AM and SSB modes
static void applySoftMuteSettings(ModeContext modeCtx) {
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx]);
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);
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

    g_si4735.setSeekFmLimits(current_band.minimumFreq, current_band.maximumFreq);
    g_si4735.setSeekFmSpacing(10);

    setSeekThresholds(true);

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
    g_si4735.setProperty(AM_SOFT_MUTE_SLOPE_PROP, AM_SOFT_MUTE_SLOPE_RECOMMENDED);
    applySoftMuteSettings(modeCtx);

    // AGC settings first to stabilize audio level
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];
    setAgcHardware(att_val);
}

// Centralizes setup for properties shared between AM and SSB to avoid duplication
// ensures AVC and seek thresholds are consistent across AM family
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    applyAvcGain(modeCtx);

    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    setSeekThresholds(false);
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
    ModeContext modeCtx = getModeContext();
    applySoftMuteSettings(modeCtx);

    // CW uses narrow fixed bandwidth, SSB uses user index
    g_si4735.setSSBAudioBandwidth(
        (g_currentMode == CW) ? g_bwSSBIdx[0] : g_bwSSBIdx[current_band.bwIdxSSB]);

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
static inline __attribute__((always_inline))
void applyBandAmpMute(bool switchingBetweenFMandAM, bool before) {
    if (!switchingBetweenFMandAM) return;
    setAmpState(!before);
}

// select antenna capacitor per band so input match fits RF path
static inline __attribute__((always_inline))
void applyBandAntennaCap(bool isFmBand) {
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

    applyCompensatedVolume();
    applyBandAmpMute(switchingBetweenFMandAM, false);

    g_previousFrequency = g_currentFrequency;
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

// =-=-=-=-=-=-=-=-= Seek helpers =-=-=-=-=-=-=-=-=

// normalize AM seek spacing to allowed HW steps
// fallback to 5 kHz when user step is unsupported
static inline __attribute__((always_inline)) uint8_t normalizeAmSeekSpacing(uint16_t current_step) {
    uint8_t seek_spacing = (current_step > 10) ? 10 : (uint8_t)current_step;
    if (seek_spacing != 1 && seek_spacing != 5 &&
        seek_spacing != 9 && seek_spacing != 10) {
        seek_spacing = 5;
    }
    return seek_spacing;
}

// Configures hardware seek parameters before starting a scan
// in AM tie seek spacing to current manual step so scan follows user intent
static inline void setupSeekParameters(uint16_t minLimit, uint16_t maxLimit) {
    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        // for AM/SW seek step is tied to current manual step
        uint16_t current_step = g_tabStep[g_bandList[g_bandIndex].stepIdxAM];
        uint8_t seek_spacing = normalizeAmSeekSpacing(current_step);

        g_si4735.setSeekAmLimits(minLimit, maxLimit);
        g_si4735.setSeekAmSpacing(seek_spacing);
    } else {
        // FM spacing fixed to 10 kHz so scan grid stays standard
        g_si4735.setSeekFmLimits(minLimit, maxLimit);
        g_si4735.setSeekFmSpacing(10);
    }
}

// sets up the station seek boundaries and step
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(DEFAULT_SEEK_DELAY_MS);

    // for limits (strict for LW/MW, full for SW)
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

// map found SW frequency to owning sub-band so limits, step and labels stay correct
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

// detect FM <-> AM-family jump which needs full reconfig
// to avoid pops and wrong props
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

    // 32-bit integer is needed here for calculations to prevent underflow
    // on band edges
    int32_t temp_freq = g_currentFrequency
        + (int16_t)step
        * encoder_delta;

    // > for the upper bound to include the maximum frequency value within the band
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
static inline bool ssbTunePrepare(uint16_t& temp_freq, int32_t& temp_bfo) {
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
static inline void ssbRolloverAndUpdate(
    uint16_t& temp_freq,
    int32_t& temp_bfo,
    uint16_t old_freq) {
    performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo);

    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    // if the base frequency changed, the chip must be updated
    // this is critical fix
    ssbApplyChipUpdateIfNeeded(old_freq);
}

// finalize SSB tune - updating BFO, state, and display
static inline void ssbTuneFinalize() {
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

    if (ssbTunePrepare(temp_freq, temp_bfo)) {
        ssbRolloverAndUpdate(temp_freq, temp_bfo, old_freq);

        // post-rollover sanity check
        // its fixes invalid SSB to FM state transition (e.g 30000.00 to 1.45MHz etc.)
        ssbGuardFmAfterRollover();

        ssbTuneFinalize();
    }
}

// =-=-=-=-=-=-=-=-= Mode switch helpers =-=-=-=-=-=-=-=-=

// fold bfo into frequency before mode switch
// user expects tuned station to remain centered
// keeps state valid during SSB to AM transition
static inline void normalizeSsbBeforeSwitch(Band& band) {
    if (!isSSB()) return;

    // use 32bit math to prevent overflow with large frequencies
    int32_t freq_khz = g_currentFrequency;
    int32_t bfo_hz = g_currentBFO;

    // use absolute Hz in 32bit to prevent overflow and simplify math
    int32_t total_freq_hz = (freq_khz * HZ_PER_KHZ) + bfo_hz;

    freq_khz = total_freq_hz / HZ_PER_KHZ;
    bfo_hz = total_freq_hz % HZ_PER_KHZ;

    // correct negative modulo to implement floor division
    if (bfo_hz < 0) {
        bfo_hz += HZ_PER_KHZ;
        freq_khz--;
    }

    // clamp to band edges to keep state valid before the mode switch
    if (freq_khz < band.minimumFreq || freq_khz > band.maximumFreq) {
        freq_khz = (freq_khz < band.minimumFreq) ? band.minimumFreq : band.maximumFreq;
        // reset bfo when clamped to avoid ambiguous state
        bfo_hz = 0;
    }

    g_currentFrequency = (uint16_t)freq_khz;
    g_currentBFO = (int16_t)bfo_hz;
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
