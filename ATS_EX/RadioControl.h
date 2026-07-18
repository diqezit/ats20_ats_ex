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
    const Band* b = currentBandPtr();
    return (f >= b->minimumFreq) && (f <= b->maximumFreq);
}

// most state is already in the band list
// only need to sync the single live frequency variable
void syncActiveStateToBand() {
    Band* b = currentBandPtr();
    const uint16_t f = g_currentFrequency;

    // only accept live freq if it belongs to current band
    if (f >= b->minimumFreq && f <= b->maximumFreq)
        b->currentFreq = f;
}

// functions will read step/bw directly from the band list
// only need to load the frequency into the single live variable
void loadActiveStateFromBand() {
    g_currentFrequency = currentBandPtr()->currentFreq;
}

// Wrapper for SI4735 setProperty bound to global g_si4735
// reduce flash size by avoiding repeated load of the g_si4735 this pointer
void __attribute__((noinline))
siSetProperty(uint16_t prop_addr, uint16_t prop_val) {
    g_si4735.setProperty(prop_addr, prop_val);
}

// apply a list of Si4735 properties from PROGMEM
// single pass table keeps call sites small and easy to audit
static void applyProperties(const uint16_t props[][2]) {
    const uint16_t* p = &props[0][0];  // walk pairs [addr,val] PROGMEM
    for (;;) {
        uint16_t prop_addr = pgm_read_word(p++);
        if (prop_addr == 0) break;
        uint16_t prop_val = pgm_read_word(p++);
        siSetProperty(prop_addr, prop_val);
    }
}

// SW link helpers
static inline bool swLinkEnabled() {
    return (uint8_t)getSettingParam(SWLink) != 0;
}

// Copy AM and SSB step and bandwidth from src to dst for SW bands
static void __attribute__((noinline))
swLinkCopySwParams(Band* dst, const Band* src) {
    dst->stepIdxAM = src->stepIdxAM;
    dst->bwIdxAM = src->bwIdxAM;
    dst->stepIdxSSB = src->stepIdxSSB;
    dst->bwIdxSSB = src->bwIdxSSB;
}

// Unified guard logic for SW link sync in both directions
// toMaster=true: current band → master; false: master → current band
static void __attribute__((noinline)) swLinkSyncCore(bool toMaster) {
    if (!swLinkEnabled()) return;
    Band* b = currentBandPtr();
    if (b->bandType != SW_BAND_TYPE) return;
    Band* m = &g_bandList[SW_MASTER_BAND_INDEX];
    if (m->bandType != SW_BAND_TYPE) return;

    if (toMaster) swLinkCopySwParams(m, b);
    else          swLinkCopySwParams(b, m);
}

// Normalize all SW bands from SW master
static void __attribute__((noinline)) swLinkNormalizeAllSwBands() {
    if (!swLinkEnabled()) return;

    const Band* m = &g_bandList[SW_MASTER_BAND_INDEX];
    if (m->bandType != SW_BAND_TYPE) return;

    for (uint8_t i = 0; i < g_bandCount; ++i) {
        Band* b = &g_bandList[i];
        if (b->bandType == SW_BAND_TYPE) {
            swLinkCopySwParams(b, m);
        }
    }
}

// =-=-=-=-=-=-=-=-= Seek stop helpers =-=-=-=-=-=-=-=-=

// raw, no-debounce read of encoder button from PINC
// used inside critical section while seek is in progress
inline static __attribute__((always_inline)) bool encBtnPressedRaw() {
    return !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

// callback polled by seek process
// latches stop on encoder button press and returns stop state
// uses SREG save/restore so we dont accidentally force-enable interrupts (unlike interrupts())
static inline bool checkStopSeeking() {
    uint8_t oldSREG = SREG;
    cli();

    if (encBtnPressedRaw())
        g_seekStop = 1;

    uint8_t stop = g_seekStop;

    SREG = oldSREG;
    return stop;
}

// =-=-=-=-=-=-=-=-= SW AFC helpers =-=-=-=-=-=-=-=-=

// Convert fixed-Hz window to 16-bit AFC register (clamped 1..0xFFFF)
// add half-window for rounding so user windows map predictably
static uint16_t __attribute__((noinline))
swAfcRegFromHzK(uint32_t fk1000, uint16_t winHz) {
    if (!winHz) return AM_AFC_SW_PULL_IN_RANGE_VAL;
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
    siSetProperty(AM_AFC_SW_PULL_IN_RANGE_PROP, rPull);
    siSetProperty(AM_AFC_SW_LOCK_IN_RANGE_PROP, rLock);
}

// Entry: 0=OFF, 1=PPM, 2=Hz Normal, 3=Hz Aggressive
// Force default on non-SW bands to prevent state leakage; merge OFF/PPM to ensure chip reset
static void applySwAfc() {
    if (g_currentMode != AM) return;

    uint8_t target = (currentBandType() == SW_BAND_TYPE)
        ? getSettingParam(SWAFC)
        : SW_AFC_PROFILE_PPM;

    switch (target) {
    case SW_AFC_PROFILE_HZ_NORMAL:
        applySwAfcProfileHz(SW_AFC_PULL_HZ_NORMAL, SW_AFC_LOCK_HZ_NORMAL);
        break;

    case SW_AFC_PROFILE_HZ_AGGR:
        applySwAfcProfileHz(SW_AFC_PULL_HZ_AGGR, SW_AFC_LOCK_HZ_AGGR);
        break;

    default: // Covers OFF (0) and PPM (1)
        applySwAfcProfilePpm();
        break;
    }
}

// ==========================================
// ===== LOW-LEVEL HARDWARE CONTROL =========
// ==========================================

// drive amp shutdown via MCU pin so mode switches do not pop the speaker
// pin direction (DDR) is configured once at startup in initHardwarePins()
static inline void __attribute__((always_inline)) setAmpState(bool on) {
    // Fast PORT-only toggle (saves Flash vs repeating DDR writes at each call site)
    if (on) {
        AMP_PORT &= ~(1 << AMP_BIT);  // LOW  (amp ON)
    } else {
        AMP_PORT |= (1 << AMP_BIT);   // HIGH (amp OFF / shutdown)
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

    if (index > 10) index = 5;
    return pgm_read_byte(&avc_table[index]);
}

// Applies AVC gain to hardware from user index (0..10)
// Kept out-of-line to deduplicate inlined setAvcAmMaxGain sequences under -Os + LTO
static void __attribute__((noinline)) applyAvcGainHW(uint8_t avcIndex) {
    uint8_t avcValue = getAvcValueFromIndex(avcIndex);
    g_si4735.setAvcAmMaxGain(avcValue);
}

// Applies mode-specific AVC gain to hardware
// Ensures correct gain is restored on mode switch
static void applyAvcGain(ModeContext modeCtx) {
    uint8_t avcIndex = (uint8_t)g_modeSettings[MODE_SETTING_AVC][modeCtx];
    applyAvcGainHW(avcIndex);
}

// =-=-=-=-=-=-=-=-= BFO helpers =-=-=-=-=-=-=-=-=

// user calibration compensates crystal drift
// invert for USB to keep tuning natural
static inline __attribute__((always_inline)) int16_t bfoCalibrationHz(uint8_t sideband) {
    // Per-band BFO calibration (stored in Band::bfoCal)
    const int8_t cal = currentBandPtr()->bfoCal;
    int16_t v = (int16_t)cal * (int16_t)BFO_CALIBRATION_MULTIPLIER;
    return (sideband == USB) ? (int16_t)-v : v;
}

// Sets BFO with user calibration and CW pitch offset (CWP)
// Si4735 requires an inverted BFO value for sideband selection (* -1)
static void updateBFO() {
    // Take a consistent snapshot (also reduces repeated volatile reads under -Os + LTO)
    const uint8_t mode = g_currentMode;
    const uint8_t cwSideband = g_lastCWMode;

    const uint8_t sideband = (mode == CW) ? cwSideband : mode;
    const int16_t calibration = bfoCalibrationHz(sideband);

    int16_t cwOffset = 0;
    if (mode == CW) {
        // CWPitch param is 5..8 -> 500..800 Hz
        int16_t pitch = (int16_t)getSettingParam(CWPitch) * 100;
        cwOffset = (cwSideband == USB) ? (int16_t)-pitch : pitch;
    }

    const int16_t finalBfo = (int16_t)(g_currentBFO + calibration + cwOffset);

    // Si4735 expects inverted BFO value for sideband selection (* -1)
    // Use wrapper to avoid repeated g_si4735 "this" load at callsite.
    siSetProperty(SSB_BFO, (uint16_t)(int16_t)(-finalBfo));
}

// =-=-=-=-=-=-=-=-= SSB cutoff helpers =-=-=-=-=-=-=-=-=

// auto cutoff pick for common widths
static inline __attribute__((always_inline))
uint8_t ssbAutoCutoffFromBwIdx(uint8_t hwBw) {
    return (hwBw == 0 || hwBw == 4 || hwBw == 5) ? 0 : 1;
}

// compute SSB cutoff value (0/1) from current HW BW + user setting
static uint8_t __attribute__((noinline))
ssbCutoffForHwBw(uint8_t hwBw) {
    uint8_t cf = (uint8_t)getSettingParam(CutoffFilter);

    uint8_t mode = (uint8_t)g_currentMode;

    if (cf == 0 || mode == CW)
        return ssbAutoCutoffFromBwIdx(hwBw);

    return (uint8_t)(cf - 1);
}

static inline void updateSSBCutoffFilter() {
    const uint8_t bwIdx = (uint8_t)currentBandPtr()->bwIdxSSB;
    const uint8_t hwBw = g_bwSSBIdx[bwIdx];
    g_si4735.setSSBSidebandCutoffFilter(ssbCutoffForHwBw(hwBw));
}

// =-=-=-=-=-=-=-=-= Squelch helpers =-=-=-=-=-=-=-=-=

// apply mute only on AM when RSSI drops below user threshold to hide weak noise
static inline __attribute__((always_inline)) bool squelchShouldCut() {
    uint8_t lvl = getSettingParam(SQL);
    if (g_signalQualityValue == INVALID_RSSI_VALUE) return false;
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
    const Band* band = currentBandPtr();

    g_si4735.setSSBConfig(
        g_bwSSBIdx[(uint8_t)band->bwIdxSSB],
        1, 0, 1, 0, 1
    );
    g_si4735.setI2CStandardMode();
    applyI2CSpeed(); // restore OLED speed after SSB patch
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

    siSetProperty(0x1302, (uint16_t)(uint8_t)getSettingParam(FmSmAtt));
    siSetProperty(0x1303, (uint16_t)(uint8_t)getSettingParam(FmSmThr));
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
    applyProperties(getSettingParam(AMNoiseBlanker) ? am_nb_on_props : am_nb_off_props);
}

// Applies user setting for forcing mono or allowing auto-stereo in FM mode
// mono can reduce hiss on weak indoor speaker reception
static void applyFMStereoSettings() {
    g_si4735.setFmStereoMode(getSettingParam(ForceMono));
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
static inline __attribute__((always_inline))
void applyFmHiCutProfile(bool enabled) {

    // AN332 mapping:
    // 0x1A00 FM_HICUT_SNR_HIGH_THRESHOLD
    // 0x1A01 FM_HICUT_SNR_LOW_THRESHOLD
    // 0x1A02 FM_HICUT_ATTACK_RATE
    // 0x1A03 FM_HICUT_RELEASE_RATE
    // 0x1A04 FM_HICUT_MULTIPATH_TRIGGER_THRESHOLD
    // 0x1A05 FM_HICUT_MULTIPATH_END_THRESHOLD
    // 0x1A06 FM_HICUT_CUTOFF_FREQUENCY
    //      - Hi-Cut disabled when FREQ[2:0] == 0

    static const uint16_t hicut_speaker_eq_props[][2] PROGMEM = {
        // Speaker EQ profile: keep Hi-Cut engaged (static “warm” sound)
        { FM_HICUT_SNR_HIGH_THRESHOLD_PROP, 127 },
        { FM_HICUT_SNR_LOW_THRESHOLD_PROP,  127 },

        { FM_HICUT_ATTACK_RATE_PROP,        FM_HICUT_ATTACK_DEFAULT },
        { FM_HICUT_RELEASE_RATE_PROP,       FM_HICUT_RELEASE_DEFAULT },

        { FM_HICUT_MP_TRIGGER_PROP,         FM_HICUT_MP_TRIGGER_DEFAULT },
        { FM_HICUT_MP_END_PROP,             FM_HICUT_MP_END_DEFAULT },

        { FM_HICUT_CUTOFF_PROP,             FM_PROP_HICUT_CUTOFF },

        { 0, 0 } // terminator
    };

    static const uint16_t hicut_default_props[][2] PROGMEM = {
        // Restore AN332 defaults and disable Hi-Cut
        { FM_HICUT_SNR_HIGH_THRESHOLD_PROP, FM_HICUT_SNR_HIGH_DEFAULT },
        { FM_HICUT_SNR_LOW_THRESHOLD_PROP,  FM_HICUT_SNR_LOW_DEFAULT  },

        { FM_HICUT_ATTACK_RATE_PROP,        FM_HICUT_ATTACK_DEFAULT   },
        { FM_HICUT_RELEASE_RATE_PROP,       FM_HICUT_RELEASE_DEFAULT  },

        { FM_HICUT_MP_TRIGGER_PROP,         FM_HICUT_MP_TRIGGER_DEFAULT },
        { FM_HICUT_MP_END_PROP,             FM_HICUT_MP_END_DEFAULT     },

        { FM_HICUT_CUTOFF_PROP,             0x0000 }, // Hi-Cut disabled

        { 0, 0 } // terminator
    };

    applyProperties(enabled ? hicut_speaker_eq_props : hicut_default_props);
}

// Applies all FM-specific audio enhancements
// Orchestrates all FM audio tweak
static void FMAudioConfigure() {
    applyFmSoftMuteSettings();
    applyFmNoiseBlankerProps();
    applyFmHiCutProfile(getSettingParam(FMAudioProfile));
}

// Helper to set seek thresholds for both AM and FM
// lower thresholds improve find rate on weak stations
static void setSeekThresholds(bool isFM) {
    static const uint16_t fm_seek[][2] PROGMEM = {
        {FM_SEEK_TUNE_SNR_THRESHOLD_PROP, FM_SEEK_SNR_THRESHOLD_VAL},
        {FM_SEEK_TUNE_RSSI_THRESHOLD_PROP, FM_SEEK_RSSI_THRESHOLD_VAL},
        {0, 0}
    };
    static const uint16_t am_seek[][2] PROGMEM = {
        {AM_SEEK_SNR_THRESHOLD_PROP, AM_SEEK_SNR_THRESHOLD_VAL},
        {AM_SEEK_RSSI_THRESHOLD_PROP, AM_SEEK_RSSI_THRESHOLD_VAL},
        {0, 0}
    };
    applyProperties(isFM ? fm_seek : am_seek);
}

// Helper to apply soft mute settings
// shared between AM and SSB modes
static void applySoftMuteSettings(ModeContext modeCtx) {
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx]);
    g_si4735.setAMSoftMuteSnrThreshold(getSettingParam(SoftMuteThr));
}

// Apply FM de-emphasis from the current UI setting
// DeEmp menu stores 0/1, chip expects 1/2
static void __attribute__((noinline)) applyFmDeEmphasisFromSetting() {
    siSetProperty((uint16_t)FM_DEEMPHASIS, (uint16_t)(uint8_t)(getSettingParam(DeEmp) + 1));
}

// Helper to configure FM seek limits and spacing
static void __attribute__((noinline)) applyFmSeekConfig(
    uint16_t minLimit, uint16_t maxLimit) {
    g_si4735.setSeekFmLimits(minLimit, maxLimit);
    g_si4735.setSeekFmSpacing(10);
}

// Orchestrates complete Si4735 setup for FM mode
// set limits + spacing + thresholds
// then apply audio profile and stereo mode
static void __attribute__((noinline)) configureFMMode(const Band& current_band) {
    g_currentMode = FM;
    g_stereoStatus = false;

    g_si4735.setFM(
        current_band.minimumFreq,
        current_band.maximumFreq,
        current_band.currentFreq,
        g_tabStepFM[current_band.stepIdxFM]
    );

    applyFmSeekConfig(current_band.minimumFreq, current_band.maximumFreq);

    setSeekThresholds(true);

    g_ssbLoaded = false;

    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    applyFmDeEmphasisFromSetting();

    FMAudioConfigure();
    applyFMStereoSettings();
}

// Configures chip for standard AM reception
// send critical audio and gain right after setAM to avoid muted audio on cold start
static void configureAMMode(const Band& current_band, uint16_t minFreq,
    uint16_t maxFreq, ModeContext modeCtx) {
    g_currentMode = AM;
    g_ssbLoaded = false;

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
    siSetProperty(AM_SOFT_MUTE_SLOPE_PROP, AM_SOFT_MUTE_SLOPE_RECOMMENDED);
    applySoftMuteSettings(modeCtx);
}

// Centralizes setup for properties shared between AM and SSB to avoid duplication
// ensures AVC and seek thresholds are consistent across AM family
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq, ModeContext modeCtx) {
    applyAvcGain(modeCtx);

    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    setSeekThresholds(false);
    applyAMNoiseBlankerSettings();
}

// Orchestrates Si4735 setup for SSB and CW modes
// optional patch reload + CW disables sync AFC + apply user filters and soft mute
static void configureSSBMode(
    Band& current_band,
    uint16_t minFreq,
    uint16_t maxFreq,
    bool extraSSBReset,
    ModeContext modeCtx) {

    if (current_band.bwIdxSSB > g_bwSSBMaxIdx)
        current_band.bwIdxSSB = g_bwSSBMaxIdx;

    // reload patch only when requested to save time
    if (!g_ssbLoaded || extraSSBReset)
        loadSSBPatch();

    bool isCW = (g_currentMode == CW);
    uint8_t sync = isCW ? 0 : getSettingParam(Sync);

    g_si4735.setSSB(
        minFreq, maxFreq,
        current_band.currentFreq,
        1, // Base step for the chip (1 kHz)
        isCW ? g_lastCWMode : g_currentMode
    );

    g_si4735.configureSSBModeBatch(
        getSettingParam(SVC),                                      // AVCEN
        isCW ? 1 : (1 - sync),                                     // DSP_AFCDIS
        isCW ? 0 : (sync * 3),                                     // AVC_DIVIDER
        isCW ? g_bwSSBIdx[0] : g_bwSSBIdx[current_band.bwIdxSSB],  // AUDIOBW
        getSettingParam(SSM)                                       // SMUTESEL
    );

    updateSSBCutoffFilter();

    // soft mute and SNR gate from storage for SSB
    applySoftMuteSettings(modeCtx);

    updateBFO();
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
    uint8_t cap_value = isFmBand ? 1 : getSettingParam(AntennaCap);
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);
}

// Top-level orchestrator for all band and mode changes
// mute around FM<->AM + load state + set RF cap + configure mode then refresh UI
static void applyBandConfiguration(bool extraSSBReset) {
    // Get band once
    Band& band = *currentBandPtr();

    // detects a major mode switch (FM <-> non-FM) to safely toggle amp
    bool isFmBand = (band.bandType == FM_BAND_TYPE);
    bool switchingBetweenFMandAM = (g_currentMode == FM) != isFmBand;

    applyBandAmpMute(switchingBetweenFMandAM, true);

    // disable squelch if active to avoid stuck mute across reconfig
    if (g_squelchCutoff) unmuteAndClearSquelchCutoff();

    loadActiveStateFromBand();
    g_signalQualityValue = INVALID_RSSI_VALUE;

    if (isFmBand) {
        configureFMMode(band);
    } else {
        uint16_t minFreq = band.minimumFreq;
        uint16_t maxFreq = band.maximumFreq;

        applyBandAntennaCap(false);

        const ModeContext modeCtx = getModeContext();

        if (isSSB()) {
            configureSSBMode(band, minFreq, maxFreq, extraSSBReset, modeCtx);
        } else {
            configureAMMode(band, minFreq, maxFreq, modeCtx);
        }

        configureAMCommon(minFreq, maxFreq, modeCtx);

        applySwAfc();
    }

    applyAgcSettings();

#if ENABLE_FAVORITES
    if (!g_settingsActive && !g_favoritesActive) {
#else
    if (!g_settingsActive) {
#endif
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

// Global clamp to AM-family hardware limits [LW_min, 10m_max]
static inline __attribute__((always_inline))
void clampFreqLimits(int32_t& khz, int32_t& bfo) {
    if (khz < SSB_MODE_MIN_FREQ) khz = SSB_MODE_MIN_FREQ;
    if (khz >= SSB_MODE_MAX_FREQ) { khz = SSB_MODE_MAX_FREQ; bfo = 0; }
}

// Converts absolute frequency in Hz into (kHz, bfo_hz) using floor division
// Guarantees bfo_hz in [0..999] and kHz adjusted accordingly
static inline __attribute__((always_inline))
void hzToKHzBfoFloor(int32_t abs_hz, int32_t& out_khz, int32_t& out_bfo_hz) {
    out_khz = abs_hz / HZ_PER_KHZ;
    out_bfo_hz = abs_hz % HZ_PER_KHZ;

    // Corrects negative BFO back into positive range 0-999
    // and adjusts main frequency down by 1 kHz to compensate
    if (out_bfo_hz < 0) {
        out_bfo_hz += HZ_PER_KHZ;
        out_khz -= 1;
    }

    clampFreqLimits(out_khz, out_bfo_hz);
}

// Collapse whole kHz from BFO into main frequency when |BFO| exceeds ±13 kHz threshold
// Normalizes negative BFO remainder to keep BFO in stable 0..999 Hz range
static inline void bfoFastRollover(uint16_t * freq, int32_t * bfo) {

    // Extract kHz steps and Hz remainder from BFO (C division truncates toward zero)
    int32_t steps_khz = *bfo / HZ_PER_KHZ;
    int32_t rem_hz    = *bfo % HZ_PER_KHZ;

    // Convert to floor-style remainder: rem_hz in [0..999], adjust steps accordingly
    if (rem_hz < 0) {
        rem_hz += HZ_PER_KHZ;
        --steps_khz;
    }

    // Apply whole-kHz part to the base frequency, keep remainder as the new BFO
    int32_t khz = (int32_t)(*freq) + steps_khz;
    int32_t bfo_hz = rem_hz;

    clampFreqLimits(khz, bfo_hz);

    *freq = (uint16_t)khz;
    *bfo  = bfo_hz;

    snapToNewStep(freq, steps_khz > 0);
}

// convert absolute Hz back to kHz + BFO after band edge decision
// keeps BFO in 0..999 Hz
static inline void absHzToFreqBfo(long absolute_freq_hz, uint16_t * freq, int32_t * bfo) {
    int32_t k, b;
    hzToKHzBfoFloor((int32_t)absolute_freq_hz, k, b);
    *freq = (uint16_t)k;
    *bfo = b;
}

// Re-anchor base kHz when absolute frequency exits current band boundaries
// Converts absolute Hz back to kHz + BFO pair for seamless tuning across band edges
static void bfoPreciseRollover(uint16_t* freq, int32_t* bfo) {
    long absolute_freq_hz = ((long)(*freq) * HZ_PER_KHZ) + *bfo;

    const Band* band = currentBandPtr();
    long min_freq_hz = (long)band->minimumFreq * HZ_PER_KHZ;
    long max_freq_hz = (long)band->maximumFreq * HZ_PER_KHZ;

    if (absolute_freq_hz >= max_freq_hz || absolute_freq_hz < min_freq_hz) {
        absHzToFreqBfo(absolute_freq_hz, freq, bfo);
    }
}

// performs bfo rollover with integrated boundary checks and max bfo limit
// this is core of stability system for ssb tuning
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static void performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {

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
    const Band* band = currentBandPtr();

    if (band->bandType != FM_BAND_TYPE) {
        // for AM/SW seek step is tied to current manual step
        uint16_t current_step = g_tabStep[(uint8_t)band->stepIdxAM];
        uint8_t seek_spacing = normalizeAmSeekSpacing(current_step);

        g_si4735.setSeekAmLimits(minLimit, maxLimit);
        g_si4735.setSeekAmSpacing(seek_spacing);
    } else {
        // FM spacing fixed to 10 kHz so scan grid stays standard
        applyFmSeekConfig(minLimit, maxLimit);
    }
}

// sets up the station seek boundaries and step
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(DEFAULT_SEEK_DELAY_MS);

    const Band* band = currentBandPtr();

    // for limits (strict for LW/MW, full for SW)
    uint16_t minLimit = (band->bandType == SW_BAND_TYPE)
        ? SW_MIN_FREQ : band->minimumFreq;
    uint16_t maxLimit = (band->bandType == SW_BAND_TYPE)
        ? SW_MAX_FREQ : band->maximumFreq;

    setupSeekParameters(minLimit, maxLimit);

    noInterrupts();
    g_seekStop = false;
    interrupts();

    // Return final frequency directly to avoid redundant status/frequency query
    return g_si4735.seekStationProgressGetFrequency(showFrequencySeek, checkStopSeeking, g_seekDirection);
}

// map found SW frequency to owning sub-band so limits, step and labels stay correct
static inline void swMapSeekToBand(uint16_t f) {
    for (uint8_t i = 2; i < g_lastBand; ++i) {
        const Band* current_band_ptr = &g_bandList[i];
        if (f >= current_band_ptr->minimumFreq &&
            f <= current_band_ptr->maximumFreq) {
            g_bandIndex = i;
            break;
        }
    }

    // Keep step and bandwidth linked across SW segments if enabled
    swLinkSyncCore(false);
}

// apply DSP and UI after seek so audio and filters follow the new station
static inline void finalizeSeekUpdate(bool bandChanged) {

    // BW does not depend on frequency - reapply only when band changed (SW remap)
    if (bandChanged) {
        doBandwidth(0);
        applyAgcSettings();
    }

    applySwAfc(); // Recalculate AFC window for SW (AM) after seek
    syncActiveStateToBand();
    showStatus(true);
    markStateAsDirty();
    g_previousFrequency = g_currentFrequency;
}

// manages hardware seek result and syncs state
// SW remaps to sub-band for correct limits/labels
static void __attribute__((noinline)) doSeek() {
    uint8_t oldBand = g_bandIndex;

    uint16_t f = executeHardwareSeek();
    if (!f) return;

    g_currentFrequency = f;

    // SW seek can land in a different sub-band — remap for correct limits/labels
    if (currentBandPtr()->bandType == SW_BAND_TYPE)
        swMapSeekToBand(f);

    finalizeSeekUpdate(g_bandIndex != oldBand);
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

    // Per-band BFO calibration must be applied immediately on band switch in SSB/CW
    if (isSSB()) updateBFO();

    bool clearUnits =
        getSettingParam(SWUnits) &&
        ((oldType == SW_BAND_TYPE) != (newType == SW_BAND_TYPE));

    applySwAfc();
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

    // Cache old band type BEFORE changing g_bandIndex
    BandType oldType = currentBandPtr()->bandType;

    g_currentBFO = 0;

    if (up) {
        g_bandIndex++;
        if (g_bandIndex >= g_bandCount) g_bandIndex = 0;
    } else {
        if (g_bandIndex == 0) g_bandIndex = g_bandCount - 1;
        else g_bandIndex--;
    }

    // Keep step and bandwidth linked across SW segments if enabled
    swLinkSyncCore(false);

    if (loadStoredFreq) loadActiveStateFromBand();

    g_lastSavedFrequency = g_currentFrequency;

    // Read new band type AFTER changing g_bandIndex
    BandType newType = currentBandPtr()->bandType;

    if (isMajorFmSwitch(oldType, newType)) {
        applyBandConfiguration();
        return;
    }

    applySameFamilySwitchUI(oldType, newType);
}

// =-=-=-=-=-=-=-=-= Tune helpers =-=-=-=-=-=-=-=-=

static inline __attribute__((always_inline))
uint16_t tuneStepForBand(BandType bandType, const Band* b) {
    return (bandType == FM_BAND_TYPE)
        ? (uint16_t)(uint8_t)g_tabStepFM[(uint8_t)b->stepIdxFM]
        : (uint16_t)g_tabStep[(uint8_t)b->stepIdxAM];
}

static inline __attribute__((always_inline))
uint16_t tuneSnapInBand(uint16_t f, uint16_t step, bool dirUp) {
    uint16_t rem = f % step;
    return rem ? (f - rem + (dirUp ? step : 0)) : f;
}

static inline __attribute__((always_inline))
uint16_t tuneResolveCrossBandFreq(BandType oldType, const Band* b, bool dirUp, int32_t tmp) {
    // FM boundary: snap to target edge by direction
    if (oldType == FM_BAND_TYPE || b->bandType == FM_BAND_TYPE)
        return dirUp ? b->minimumFreq : b->maximumFreq;

    // AM-family: clamp into the new band
    if (tmp < (int32_t)b->minimumFreq) return b->minimumFreq;
    if (tmp > (int32_t)b->maximumFreq) return b->maximumFreq;
    return (uint16_t)tmp;
}

// crossed-band apply: switch band, then clamp tmp into the new band limits
static inline __attribute__((always_inline))
void tuneApplyCrossed(BandType oldType, bool dirUp, int32_t tmp) {
    bandSwitch(dirUp, false);
    g_currentFrequency = tuneResolveCrossBandFreq(
        oldType, currentBandPtr(), dirUp, tmp);
}

// encoder tuning with snap-to-grid and safe band crossing
static void __attribute__((noinline)) doFrequencyTune(int16_t delta) {
    if (!delta) return;

    const bool dirUp = (delta > 0);
    g_seekDirection = dirUp;

    const Band* band = currentBandPtr();
    const BandType oldType = band->bandType;
    const uint16_t step = tuneStepForBand(oldType, band);

    int32_t tmp = (int32_t)g_currentFrequency + (int32_t)step * (int32_t)delta;

    if (tmp < (int32_t)band->minimumFreq || tmp >(int32_t)band->maximumFreq) {
        tuneApplyCrossed(oldType, dirUp, tmp);
    } else {
        g_currentFrequency = tuneSnapInBand((uint16_t)tmp, step, dirUp);
    }

    g_processFreqChange = true;
    g_lastFreqChange = millis();
    showFrequency();
    syncActiveStateToBand();
    markStateAsDirty();
}

// =-=-=-=-=-=-=-=-= SSB Tune Helpers =-=-=-=-=-=-=-=-=

// Returns current SSB step in Hz from user settings
static inline __attribute__((always_inline)) int32_t ssbStepHz() {
    return (int32_t)g_tabStep[SSB_STEP_OFFSET + (uint8_t)currentBandPtr()->stepIdxSSB];
}

// Rate-limited I2C update to Si4735
// Prevents chip lockup when encoder is turned very fast
static inline void ssbChipRate() {
    static uint16_t last_ms16 = 0;
    static uint16_t last_sent_freq = 0xFFFF;
    static uint8_t  last_mode = 0xFF;

    // Invalidate cache on mode change
    if (last_mode != (uint8_t)g_currentMode) {
        last_mode = (uint8_t)g_currentMode;
        last_sent_freq = 0xFFFF;
    }

    // Rate limit gate
    uint16_t now16 = (uint16_t)millis();
    if ((uint16_t)(now16 - last_ms16) < (uint16_t)MIN_SETFREQ_INTERVAL_MS) return;

    // Update base frequency only when kHz changed
    if (g_currentFrequency != last_sent_freq) {
        g_si4735.setFrequency(g_currentFrequency);
        last_sent_freq = g_currentFrequency;
    }

    updateBFO();
    last_ms16 = now16;
}

// Main SSB tuning handler
// UI updates immediately, chip updates are rate-limited
static void __attribute__((noinline)) doFrequencyTuneSSB(int16_t encoder_delta) {
    if (encoder_delta == 0) return;

    // Apply encoder movement to BFO (int32_t prevents overflow on fast turns)
    uint16_t temp_freq = g_currentFrequency;
    int32_t  temp_bfo = g_currentBFO;
    temp_bfo += ssbStepHz() * (int32_t)encoder_delta;

    uint8_t old_band = (uint8_t)g_bandIndex;

    // Normalize BFO and handle band boundaries
    performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo);

    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    // Safety guard for FM mode
    if (g_currentMode == FM) {
        loadActiveStateFromBand();
        return;
    }

    // Find correct band for new frequency
    uint8_t new_band = old_band;
    for (uint8_t i = 0; i < g_bandCount; ++i) {
        if (g_bandList[i].bandType == FM_BAND_TYPE) continue;
        if (g_currentFrequency >= g_bandList[i].minimumFreq &&
            g_currentFrequency <= g_bandList[i].maximumFreq) {
            new_band = i;
            break;
        }
    }

    // Update UI on band change
    if (new_band != old_band) {
        g_bandIndex = new_band;
        showBandTag();
        showStep();

        // apply DSP filter only on band change to avoid redundant I2C traffic
        // during SSB tuning (BW does not depend on frequency)
        doBandwidth(0);
    }

    // Send to chip (rate-limited)
    ssbChipRate();

    // Update UI and state
    syncActiveStateToBand();
    g_lastFreqChange = millis();
    g_previousFrequency = 0;
    showFrequency();
    markStateAsDirty();
}

// =-=-=-=-=-=-=-=-= Mode switch helpers =-=-=-=-=-=-=-=-=

// fold bfo into frequency before mode switch
// user expects tuned station to remain centered
// keeps state valid during SSB to AM transition
static inline void normalizeSsbBeforeSwitch(Band & band) {
    if (!isSSB()) return;

    // use 32bit math to prevent overflow with large frequencies
    int32_t freq_khz = g_currentFrequency;
    int32_t bfo_hz = g_currentBFO;

    // use absolute Hz in 32bit to prevent overflow and simplify math
    int32_t total_freq_hz = (freq_khz * HZ_PER_KHZ) + bfo_hz;

    // floor-division normalization
    hzToKHzBfoFloor(total_freq_hz, freq_khz, bfo_hz);

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
    Band& band = *currentBandPtr();

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
    Band& current_band = *currentBandPtr();

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
    const Band* current_band = currentBandPtr();
    g_si4735.setSSB(
        current_band->minimumFreq,
        current_band->maximumFreq,
        g_currentFrequency,
        1, // Hardware step
        g_lastCWMode
    );

    updateBFO();
    applyAgcSettings();
    showFrequency(true);
    updateStereoIndicator();
}
