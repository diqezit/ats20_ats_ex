#pragma once

// ----------------------------------------------------------------------
// SettingsLogic.h - Settings handler business logic
// ----------------------------------------------------------------------

#define HANDLER(name) void name(int8_t v)
#define MODE_PRED(name, mode) \
    static inline ALWAYS_INLINE bool name() { return g_currentMode == (mode); }
#define SWITCH_TO(idx, lo, hi) \
    doSwitchLogic(settingRef(idx), (lo), (hi), v)
#define TOGGLE_ONLY(name, idx) \
    HANDLER(name) { toggleSetting(idx); }
#define TOGGLE_WHEN(name, idx, cond, fn) \
    HANDLER(name) { toggleSetting(idx); if (cond) fn(); }
#define SWITCH_PROP_WHEN(name, idx, maxv, cond, prop) \
    HANDLER(name) { SWITCH_TO(idx, 0, maxv); if (cond) siSetSettingProp(prop, idx); }

// =================================================================================================
// Applicability predicates (used by g_SettingsMeta[] to grey-out irrelevant items)
// =================================================================================================

MODE_PRED(isFm, FM)
MODE_PRED(isCw, CW)

// noinline to keep one shared body for FM check
static bool NOINLINE isFMActive() {
    return isFm();
}

static bool isAMFamilyActive() { return !isFMActive(); }

static bool isSSBActive() { return isSSB(); }

#undef MODE_PRED

// =================================================================================================
// Helper: persist mode-dependent setting to g_modeSettings[][]
// =================================================================================================

// save setting value specific to current mode
// links temporary UI state to persistent storage
inline static ALWAYS_INLINE
void persistModeSetting(ModeSettingType type, SettingsIndex index) {
    ModeContext m = getModeContext();
    g_modeSettings[type][m] = getSettingParam(index);
}

static inline void siSetSettingProp(uint16_t prop, SettingsIndex idx) {
    siSetProperty(prop, (uint16_t)(uint8_t)getSettingParam(idx));
}

// =================================================================================================
// Volume & Audio helpers
// =================================================================================================

// FM signals sound louder than other audio sources
// Apply a user-set offset for consistent volume feel
// Caches last HW value to skip redundant I2C writes
static void applyCompensatedVolume() {
    static uint8_t s_last = 0xFF;
    uint8_t t = 0;

    if (g_muteVolume) {
        t = 0;
    } else {
        t = g_volume;

        if (isFm()) {
            uint8_t o = getSettingParam(FmVolAdjust);
            t = (o >= t) ? 0 : (uint8_t)(t - o);
        }
    }

    if (t != s_last) {
        s_last = t;
        g_si4735.setVolume(t);
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

// =================================================================================================
// Settings: Page 1 - Core Audio & RF
// =================================================================================================

// Settings: Attenuation (ATT)
// manual control over the receiver front-end gain, which handled by the Automatic Gain Control (AGC)
// 'AUT' (Auto) is the standard mode.
// can be useful to prevent overload from very strong local stations
// (by increasing attenuation)
HANDLER(doAttenuation) {
    uint8_t max_att_value = isFm() ? MAX_ATTENUATION_FM_DB : MAX_ATTENUATION_AM_DB;
    SWITCH_TO(ATT, 0, max_att_value);

    setAgcHardware(getSettingParam(ATT));
    persistModeSetting(MODE_SETTING_AGC, ATT);
}

// Settings: Automatic Volume Control (AVC)
// Adjusts maximum gain for the AVC system to normalize volume levels
// between strong and weak stations
// Higher values give more aggressive leveling making quiet stations louder
// Maps simple user index (0-10) to non-linear hardware gain value (12-90)
HANDLER(doAvc) {
    if (isFm()) return;

    SWITCH_TO(AutoVolControl, AVC_MIN_INDEX, AVC_MAX_INDEX);

    persistModeSetting(MODE_SETTING_AVC, AutoVolControl);

    // re-apply value to hardware immediately
    applyAvcGainHW((uint8_t)getSettingParam(AutoVolControl));
}

// Settings: Squelch Threshold
// Handles user input for the Squelch (SQL) setting in the menu
// Adjusts the RSSI threshold from 0 (OFF) to 60
// As a safety measure if the squelch is manually disabled (set to 0) while it is actively muting the audio,
// this function immediately un-mutes receiver
HANDLER(doSquelch) {
    SWITCH_TO(SQL, 0, SQUELCH_MAX_LEVEL);

    if (getSettingParam(SQL) == 0 && g_squelchCutoff)
        unmuteAndClearSquelchCutoff();
}

// Settings: Soft Mute Attenuation
// controls HOW MUCH the volume is reduced when a signal becomes weak
// A higher value means stronger muting, making the receiver almost silent on noisy frequencies
// Setting it to 0 - disables soft mute feature
HANDLER(doSoftMute) {
    SWITCH_TO(SoftMute, 0, SOFT_MUTE_MAX_ATTENUATION);

    // persist per modulation (AM, LSB, USB, CW)
    persistModeSetting(MODE_SETTING_SOFT_MUTE, SoftMute);

    if (!isFm())
        siSetSettingProp(AM_SOFT_MUTE_MAX_ATTENUATION, SoftMute);
}

// Settings: Soft Mute Threshold
// controls WHEN the soft mute feature activates
// It sets a minimum signal quality (SNR) threshold
// If the signal drops below this level, the audio will be muted by the amount set in 'SMA'
SWITCH_PROP_WHEN(doSoftMuteThreshold, SoftMuteThr, SOFT_MUTE_MAX_SNR_THRESHOLD,
                 !g_si4735.isCurrentTuneFM(), AM_SOFT_MUTE_SNR_THRESHOLD)

// Settings: AM Noise Blanker
TOGGLE_WHEN(doAMNoiseBlanker, AMNoiseBlanker, !isFm(), applyAMNoiseBlankerSettings)

// =================================================================================================
// Settings: Page 2 - SSB & CW
// =================================================================================================

// Settings: BFO Offset calibration
HANDLER(doBFOCalibration) {

    // BFO calibration is not applicable in FM
    if (currentBandType() == FM_BAND_TYPE) return;

    // Expanded range to -25..+25. With a x100 multiplier in updateBFO(),
    // this provides a +/- 2.5kHz calibration range in 100Hz step
    SWITCH_TO(BFO, BFO_CALIBRATION_MIN, BFO_CALIBRATION_MAX);

    // Per-band BFO calibration: store in current band
    currentBandPtr()->bfoCal = getSettingParam(BFO);

    markStateAsDirty();

    if (isSSB()) updateBFO();
}

// Settings: SSB Soft Mute Mode
HANDLER(doSSBSoftMuteMode) {
    toggleSetting(SSM);
    if (isSSB())
        g_si4735.setSSBSoftMute(getSettingParam(SSM));
}

// Common tail for SSB-related toggle settings (Sync, SVC)
// reduce code duplication
static void NOINLINE toggleSettingAndSyncSSB(uint8_t idx) {
    toggleSetting(idx);

    if (isSSB()) applyBandConfiguration(false);
}

// Settings: SSB AVC Switch
HANDLER(doSSBAVC) {
    toggleSettingAndSyncSSB(SVC);
}

// Settings: SSB Cutoff filter
HANDLER(doCutoffFilter) {
    SWITCH_TO(CutoffFilter, 0, CUTOFF_FILTER_MAX_VALUE);

    if (isSSB())
        updateSSBCutoffFilter();
}

// Settings: Sync switch
HANDLER(doSync) {
    // Sync is not need in CW mode
    if (isCw()) return;

    toggleSettingAndSyncSSB(Sync);
}

// Settings: CW Pitch
// 5..8 meaning 500..800 Hz
HANDLER(doCWPitch) {
    SWITCH_TO(CWPitch, 5, 8);
    markStateAsDirty();
    if (isCw()) updateBFO();
}

// =================================================================================================
// Settings: Page 3 - FM & Advanced Audio
// =================================================================================================

// Settings: FM De-Emphasis (DE)
// sets de-emphasis time constant for FM reception
// matches the pre-emphasis used by broadcasters in different regions
// 75 µs is standard for America, 50 µs for Europe and rest of
TOGGLE_WHEN(doDeEmp, DeEmp, isFm(), applyFmDeEmphasisFromSetting)

// Settings: FM Audio Profile (Speaker EQ)
// Toggles a curated audio profile designed to improve sound on the small internal speaker.
// When disabled, it restores default chip settings for pure audio output, ideal for headphones.
TOGGLE_WHEN(doFMAudioProfile, FMAudioProfile, isFm(), FMAudioConfigure)

// Settings: Force FM Mono
// Toggles between automatic stereo/mono blend and forced mono reception
TOGGLE_WHEN(doForceMono, ForceMono, isFm(), applyFMStereoSettings)

// Settings: FM Soft Mute Attenuation (FSA)
// Controls how much the volume is reduced (in dB) when soft mute activates
// Higher values result in a deeper, more noticeable mute
// Range: 0 (disabled) to 31 (max)
SWITCH_PROP_WHEN(doFmSoftMuteAtt, FmSmAtt, FM_SOFT_MUTE_MAX_ATTN_LEVEL,
                 isFm(), FM_SOFT_MUTE_MAX_ATTENUATION)

// Settings: FM Soft Mute Threshold (FST)
// Sets the minimum signal quality (SNR) required to keep audio at full volume
// If SNR drops below this, soft mute engages. Higher values are more aggressive
// Range: 0 to 15
SWITCH_PROP_WHEN(doFmSoftMuteThr, FmSmThr, FM_SOFT_MUTE_MAX_SNR_LEVEL,
                 isFm(), FM_PROP_SOFTMUTE_SNR_THRESH_ADDR)

// Settings: Toggle handler for SW AFC menu item (SWA)
// 0=OFF, 1=PPM, 2=Hz Normal, 3=Hz Aggressive
HANDLER(doSwAfcProfile) {
    SWITCH_TO(SWAFC, SW_AFC_PROFILE_OFF, SW_AFC_PROFILE_HZ_AGGR);
    applySwAfc();
}

// =================================================================================================
// Settings: Page 4 - Display & UI
// =================================================================================================

// Settings: Brightness
HANDLER(doBrightness) {
    int8_t new_setting = getSettingParam(Brightness) + v;

    // clamp the value of to the [0, 9]
    new_setting = constrain(new_setting, 0, BRIGHTNESS_MAX_LEVEL);

    setSettingParam(Brightness, new_setting);
    applyBrightness();
}

// Settings: switcher - S-Point display to RSSI display
// 0=RSSI, 1=SPT, 2=RSSI+BAR, 3=SPT+BAR
HANDLER(doSMeter) {
    enum : uint8_t { SM_UI_BAR = 2 };

    int8_t& sm = settingRef(SMeter);
    const uint8_t prev = (uint8_t)sm;

    doSwitchLogic(sm, 0, 3, v);

#if defined(ENABLE_SIGNAL_BAR) && ENABLE_SIGNAL_BAR
    // Clear bar once when user turns it off
    if ((prev & SM_UI_BAR) && !(((uint8_t)sm) & SM_UI_BAR))
        smDrawSignalBar(255);
#endif
}

// Settings: SW Units
TOGGLE_ONLY(doSWUnits, SWUnits)

// Settings: Display timeout switch
HANDLER(doDisplayOff) {
    SWITCH_TO(DisplayOff, 0, DISPLAY_OFF_TIMER_MAX_LEVEL);
}

// Settings: RSSI AM Off switch
TOGGLE_ONLY(doRSSIAMOff, RSSI_AM_Off)

// Settings: Navigation Style
// Toggles between row-first and column-first cursor movement
TOGGLE_ONLY(doNavStyle, NAV)

// =================================================================================================
// Settings: Page 5 - Hardware Configuration
// =================================================================================================

// Settings: Auto Antenna Capacitor
HANDLER(doAntennaCapacitor) {
    toggleSetting(AntennaCap);
    applyBandAntennaCap(currentBandType() == FM_BAND_TYPE); // in menu
}

// Settings: CPU Frequency divider
HANDLER(doCPUSpeed) {
    toggleSetting(CPUSpeed);
    setCpuPrescaler(getSettingParam(CPUSpeed));
}

// Settings: Toggles the battery voltage pin between A1 and A2.
TOGGLE_ONLY(doBatteryPinSelect, BATT_PIN)

// Settings: Scan button switch
TOGGLE_ONLY(doScanSwitch, ScanSwitch)

// Settings: FM Volume Adjust
// Fine-tunes the software volume reduction for FM mode to match AM/SSB levels
HANDLER(doFmVolAdjust) {
    SWITCH_TO(FmVolAdjust, 0, 15);
    if (isFm()) applyCompensatedVolume();
}

// Settings: SW Link
// Links step and bandwidth across all SW sub bands when enabled
HANDLER(doSwLink) {
    toggleSetting(SWLink);

    if (swLinkEnabled()) {
        swLinkSyncCore(true);
        swLinkNormalizeAllSwBands();
    }
}

// =================================================================================================
// Settings: Page 6 - Advanced RF
// =================================================================================================

// Settings: SSB AGC Speed
// Fast / Normal / Slow, SSB/CW only
HANDLER(doSsbAgcSpeed) {
    SWITCH_TO(SsbAgcSpeed, 0, 2);
    if (isSSB()) applySsbAgcSpeed();
}

// Settings: AM/SSB Soft Mute Rate
// Fast / Normal / Slow, shared 0x3300
HANDLER(doAmSmRate) {
    SWITCH_TO(AmSmRate, 0, 2);
    if (!isFm())
        applySoftMuteSettings(getModeContext());
}

#undef SWITCH_PROP_WHEN
#undef TOGGLE_WHEN
#undef TOGGLE_ONLY
#undef SWITCH_TO
#undef HANDLER
