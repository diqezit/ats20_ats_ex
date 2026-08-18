// ----------------------------------------------------------------------
// SettingsLogic.h - Settings handler business logic
// ----------------------------------------------------------------------

#pragma once

// =================================================================================================
// Applicability predicates (used by g_SettingsMeta[] to grey-out irrelevant items)
// =================================================================================================

static bool isAlwaysActive() { return true; }

// noinline to keep one shared body for FM check
static bool __attribute__((noinline)) isFMActive() {
    return g_currentMode == FM;
}

static bool isAMFamilyActive() { return !isFMActive(); }

static bool isSSBActive() { return isSSB(); }

// =================================================================================================
// Helper: persist mode-dependent setting to g_modeSettings[][]
// =================================================================================================

// save setting value specific to current mode
// links temporary UI state to persistent storage
inline static __attribute__((always_inline))
void persistModeSetting(ModeSettingType type, SettingsIndex index) {
    ModeContext m = getModeContext();
    g_modeSettings[type][m] = getSettingParam(index);
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

        if (g_currentMode == FM) {
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
void doAttenuation(int8_t v) {
    uint8_t max_att_value = (g_currentMode == FM) ? MAX_ATTENUATION_FM_DB : MAX_ATTENUATION_AM_DB;
    doSwitchLogic(settingRef(ATT), 0, max_att_value, v);

    setAgcHardware(getSettingParam(ATT));
    persistModeSetting(MODE_SETTING_AGC, ATT);
}

// Settings: Automatic Volume Control (AVC)
// Adjusts maximum gain for the AVC system to normalize volume levels
// between strong and weak stations
// Higher values give more aggressive leveling making quiet stations louder
// Maps simple user index (0-10) to non-linear hardware gain value (12-90)
void doAvc(int8_t v) {
    if (g_currentMode == FM) return;

    doSwitchLogic(settingRef(AutoVolControl), AVC_MIN_INDEX, AVC_MAX_INDEX, v);

    persistModeSetting(MODE_SETTING_AVC, AutoVolControl);

    // re-apply value to hardware immediately
    applyAvcGainHW((uint8_t)getSettingParam(AutoVolControl));
}

// Settings: Squelch Threshold
// Handles user input for the Squelch (SQL) setting in the menu
// Adjusts the RSSI threshold from 0 (OFF) to 60
// As a safety measure if the squelch is manually disabled (set to 0) while it is actively muting the audio,
// this function immediately un-mutes receiver
void doSquelch(int8_t v) {
    doSwitchLogic(settingRef(SQL), 0, SQUELCH_MAX_LEVEL, v);

    if (getSettingParam(SQL) == 0 && g_squelchCutoff)
        unmuteAndClearSquelchCutoff();
}

// Settings: Soft Mute Attenuation
// controls HOW MUCH the volume is reduced when a signal becomes weak
// A higher value means stronger muting, making the receiver almost silent on noisy frequencies
// Setting it to 0 - disables soft mute feature
void doSoftMute(int8_t v) {
    doSwitchLogic(settingRef(SoftMute), 0, SOFT_MUTE_MAX_ATTENUATION, v);

    // persist per modulation (AM, LSB, USB, CW)
    persistModeSetting(MODE_SETTING_SOFT_MUTE, SoftMute);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(getSettingParam(SoftMute));
}

// Settings: Soft Mute Threshold
// controls WHEN the soft mute feature activates
// It sets a minimum signal quality (SNR) threshold
// If the signal drops below this level, the audio will be muted by the amount set in 'SMA'
void doSoftMuteThreshold(int8_t v) {
    doSwitchLogic(settingRef(SoftMuteThr), 0, SOFT_MUTE_MAX_SNR_THRESHOLD, v);
    if (!g_si4735.isCurrentTuneFM())
        g_si4735.setAMSoftMuteSnrThreshold(getSettingParam(SoftMuteThr));
}

// Settings: AM Noise Blanker
void doAMNoiseBlanker(int8_t v) {
    toggleSetting(AMNoiseBlanker);
    if (g_currentMode != FM) applyAMNoiseBlankerSettings();
}

// =================================================================================================
// Settings: Page 2 - SSB & CW
// =================================================================================================

// Settings: BFO Offset calibration
void doBFOCalibration(int8_t v) {

    // BFO calibration is not applicable in FM
    if (currentBandType() == FM_BAND_TYPE) return;

    // Expanded range to -25..+25. With a x100 multiplier in updateBFO(),
    // this provides a +/- 2.5kHz calibration range in 100Hz step
    doSwitchLogic(settingRef(BFO), BFO_CALIBRATION_MIN, BFO_CALIBRATION_MAX, v);

    // Per-band BFO calibration: store in current band
    currentBandPtr()->bfoCal = getSettingParam(BFO);

    markStateAsDirty();

    if (isSSB()) updateBFO();
}

//Settings: SSB Soft Mute Mode
void doSSBSoftMuteMode(int8_t v) {
    toggleSetting(SSM);
    if (isSSB())
        g_si4735.setSSBSoftMute(getSettingParam(SSM));
}

// Common tail for SSB-related toggle settings (Sync, SVC)
// reduce code duplication
static void __attribute__((noinline)) toggleSettingAndSyncSSB(uint8_t idx) {
    toggleSetting(idx);

    if (isSSB()) applyBandConfiguration(false);
}

//Settings: SSB AVC Switch
void doSSBAVC(int8_t v) {
    toggleSettingAndSyncSSB(SVC);
}

//Settings: SSB Cutoff filter
void doCutoffFilter(int8_t v) {
    doSwitchLogic(settingRef(CutoffFilter), 0, CUTOFF_FILTER_MAX_VALUE, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

//Settings: Sync switch
void doSync(int8_t v) {
    // Sync is not need in CW mode
    if (g_currentMode == CW) return;

    toggleSettingAndSyncSSB(Sync);
}

// Settings: CW Pitch
// 5..8 meaning 500..800 Hz
void doCWPitch(int8_t v) {
    doSwitchLogic(settingRef(CWPitch), 5, 8, v);
    markStateAsDirty();
    if (g_currentMode == CW) updateBFO();
}

// =================================================================================================
// Settings: Page 3 - FM & Advanced Audio
// =================================================================================================

// Settings: FM De-Emphasis (DE)
// sets de-emphasis time constant for FM reception
// matches the pre-emphasis used by broadcasters in different regions
// 75 µs is standard for America, 50 µs for Europe and rest of
void doDeEmp(int8_t v) {
    toggleSetting(DeEmp);
    if (g_currentMode == FM)
        applyFmDeEmphasisFromSetting();
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

// Settings: FM Soft Mute Attenuation (FSA)
// Controls how much the volume is reduced (in dB) when soft mute activates
// Higher values result in a deeper, more noticeable mute
// Range: 0 (disabled) to 31 (max)
void doFmSoftMuteAtt(int8_t v) {
    doSwitchLogic(settingRef(FmSmAtt), 0, FM_SOFT_MUTE_MAX_ATTN_LEVEL, v);
    if (g_currentMode == FM)
        siSetProperty(FM_PROP_SOFTMUTE_MAX_ATTN_ADDR, (uint16_t)(uint8_t)getSettingParam(FmSmAtt));
}

// Settings: FM Soft Mute Threshold (FST)
// Sets the minimum signal quality (SNR) required to keep audio at full volume
// If SNR drops below this, soft mute engages. Higher values are more aggressive
// Range: 0 to 15
void doFmSoftMuteThr(int8_t v) {
    doSwitchLogic(settingRef(FmSmThr), 0, FM_SOFT_MUTE_MAX_SNR_LEVEL, v);
    if (g_currentMode == FM)
        siSetProperty(FM_PROP_SOFTMUTE_SNR_THRESH_ADDR, (uint16_t)(uint8_t)getSettingParam(FmSmThr));
}

// Settings: Toggle handler for SW AFC menu item (SWA)
// 0=OFF, 1=PPM, 2=Hz Normal, 3=Hz Aggressive
void doSwAfcProfile(int8_t v) {
    doSwitchLogic(settingRef(SWAFC),
        SW_AFC_PROFILE_OFF,
        SW_AFC_PROFILE_HZ_AGGR,
        v);
    applySwAfc();
}

// =================================================================================================
// Settings: Page 4 - Display & UI
// =================================================================================================

//Settings: Brightness
void doBrightness(int8_t v) {
    int8_t new_setting = getSettingParam(Brightness) + v;

    // clamp the value of to the [0, 9]
    new_setting = constrain(new_setting, 0, BRIGHTNESS_MAX_LEVEL);

    setSettingParam(Brightness, new_setting);
    applyBrightness();
}

// Settings: switcher - S-Point display to RSSI display
// 0=RSSI, 1=SPT, 2=RSSI+BAR, 3=SPT+BAR
void doSMeter(int8_t v) {
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

//Settings: SW Units
void doSWUnits(int8_t v) {
    toggleSetting(SWUnits);
}

//Settings: Display timeout switch
void doDisplayOff(int8_t v) {
    doSwitchLogic(settingRef(DisplayOff), 0, DISPLAY_OFF_TIMER_MAX_LEVEL, v);
}

//Settings: RSSI AM Off switch
void doRSSIAMOff(int8_t v) {
    toggleSetting(RSSI_AM_Off);
}

// Settings: Navigation Style
// Toggles between row-first and column-first cursor movement
void doNavStyle(int8_t v) {
    toggleSetting(NAV);
}

// =================================================================================================
// Settings: Page 5 - Hardware Configuration
// =================================================================================================

//Settings: Auto Antenna Capacitor
void doAntennaCapacitor(int8_t v) {
    toggleSetting(AntennaCap);
    applyBandAntennaCap(currentBandType() == FM_BAND_TYPE); // in menu
}

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v) {
    toggleSetting(CPUSpeed);
    setCpuPrescaler(getSettingParam(CPUSpeed));
}

// Settings: Toggles the battery voltage pin between A1 and A2.
void doBatteryPinSelect(int8_t v) {
    toggleSetting(BATT_PIN);
}

//Settings: Scan button switch
void doScanSwitch(int8_t v) {
    toggleSetting(ScanSwitch);
}

// Settings: FM Volume Adjust
// Fine-tunes the software volume reduction for FM mode to match AM/SSB levels
void doFmVolAdjust(int8_t v) {
    doSwitchLogic(settingRef(FmVolAdjust), 0, 15, v);
    if (g_currentMode == FM) applyCompensatedVolume();
}

// Settings SW Link
// Links step and bandwidth across all SW sub bands when enabled
void doSwLink(int8_t v) {
    toggleSetting(SWLink);

    if (swLinkEnabled()) {
        swLinkSyncCore(true);
        swLinkNormalizeAllSwBands();
    }
}
