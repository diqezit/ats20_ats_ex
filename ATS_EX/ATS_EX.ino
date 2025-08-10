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

// ==========================================
// ===== CORE UTILITIES & STATE SYNC ========
// ==========================================

// most state is already in the band list
// only need to sync the single live frequency variable
void syncActiveStateToBand() {
    const Band& current_band = g_bandList[g_bandIndex];

    // update only band stored frequency if the current live frequency
    // is within the valid range for this band
    if (g_currentFrequency >= current_band.minimumFreq && g_currentFrequency <= current_band.maximumFreq) {
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    }
}

// functions will read step/bw directly from the band list
// only need to load the frequency into the single live variable
void loadActiveStateFromBand() {
    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
}

// Syncs mode-dependent settings between UI buffer (g_Settings)
// and persistent storage (g_modeSettings)
// When loading (true) it uses current mode context
// When saving (false) it populates ALL mode contexts for a full factory reset
void syncModeDependentSettings(bool load) {
    if (load) {
        // LOAD from storage into UI, based on current mode context
        const uint8_t m = getModeContext();
        g_Settings[ATT].param = g_modeSettings[MODE_SETTING_AGC][m];
        g_Settings[SoftMute].param = g_modeSettings[MODE_SETTING_SOFT_MUTE][m];
        g_Settings[AutoVolControl].param = g_modeSettings[MODE_SETTING_AVC][m];
    } else {
        // SAVE from UI into storage, for ALL mode contexts
        for (uint8_t m = 0; m < MODE_CONTEXT_COUNT; m++) {
            g_modeSettings[MODE_SETTING_AGC][m] = g_Settings[ATT].param;
            g_modeSettings[MODE_SETTING_SOFT_MUTE][m] = g_Settings[SoftMute].param;
            g_modeSettings[MODE_SETTING_AVC][m] = g_Settings[AutoVolControl].param;
        }
    }
}

static inline bool checkStopSeeking() {
    bool result;
    noInterrupts();  // race protection
    result = g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
    interrupts();
    return result;
}

// Settings: CPU Frequency divider helper
static void setCpuPrescaler(uint8_t prescaler) {
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = prescaler;
    interrupts();
}

// Applies a series of properties to the Si4735 from a PROGMEM array
// This helper function simplifies applying multiple settings by using a declarative data table
// reducing repetitive setProperty calls from SI473z lib
// Property list must be terminated with a {0, 0} pair to mark its end
static void applyProperties(const uint16_t props[][2]) {
    for (int i = 0; ; ++i) {
        uint16_t prop_addr = pgm_read_word(&props[i][0]);
        if (prop_addr == 0) break;
        uint16_t prop_val = pgm_read_word(&props[i][1]);
        g_si4735.setProperty(prop_addr, prop_val);
    }
}

// ==========================================
// ===== LOW-LEVEL HARDWARE CONTROL =========
// ==========================================

// Controls the MD8002A amplifier state (on/off)
// Always sets the pin to OUTPUT mode for safety
// If TRUE = on, FALSE = off
static inline void __attribute__((always_inline)) setAmpState(bool on) {
    AMP_DDR |= (1 << AMP_BIT);        // Set as OUTPUT
    if (on) {
        AMP_PORT &= ~(1 << AMP_BIT);  // LOW (on)
    } else {
        AMP_PORT |= (1 << AMP_BIT);   // HIGH (off)
    }
}

// Manages the audio mute state based on the user-defined Squelch level and current RSSI
// The function compares the signal strength against the SQL threshold (for non-FM modes)
static void handleSquelch(void) {
    uint8_t lvl = g_Settings[SQL].param;
    bool cut = (lvl && g_currentMode == AM && g_signalQualityValue < lvl);

    if (cut != g_squelchCutoff) {
        g_si4735.setAudioMute(cut);
        g_squelchCutoff = cut;
    }
}

// AGC hardware control
static inline void setAgcHardware(int8_t att_val) {
    bool disableAgc = att_val > 0;
    // attenuation index for the chip is one less than the parameter valu
    // if att_val is 0 (auto) or 1 (manual, 0dB), the index sent to the chip is 0
    uint8_t agcNdx = (att_val > 1) ? (att_val - 1) : 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

// Sets BFO with user calibration and automatic CW pitch offset
// Si4735 requires an inverted BFO value for sideband selection
static void updateBFO() {

    // get selected pitch from settings
    uint16_t selected_pitch = pgm_read_word(&cw_pitch_options_hz[g_Settings[CWPitch].param]);

    // Determine automatic CW offset - USB uses a negative offset / LSB a positive one
    int16_t cwOffset = (g_currentMode == CW)
        ? ((g_lastCWMode == USB) ? -selected_pitch : selected_pitch)
        : 0;

    // Determine sideband context for calibration (LSB or USB)
    uint8_t current_sideband = (g_currentMode == CW) ? g_lastCWMode : g_currentMode;

    // Invert calibration sign for USB modes to match observed hardware response
    int8_t sign_multiplier = (current_sideband == USB) ? -1 : 1;
    int16_t bfo_calibration_offset = g_Settings[BFO].param * BFO_CALIBRATION_MULTIPLIER * sign_multiplier;

    // Combine manual tuning, calibration, and pitch offset for the final BFO value
    int16_t finalBfo = g_currentBFO + bfo_calibration_offset + cwOffset;

    g_si4735.setSSBBfo(finalBfo * -1);
}

//Saves more flash image size
static void updateSSBCutoffFilter() {
    uint8_t idx = g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB];
    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter((idx == 0 || idx == 4 || idx == 5) ? 0 : 1);
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

// ==========================================
// ===== HARDWARE CONFIGURATION =============
// ==========================================

// This function is required for using SSB. Si473x controllers do not support SSB by-default.
// But we can patch internal RAM of Si473x with special patch to make it work in SSB mode.
// Patch must be applied every time we enable SSB after AM or FM.
static void loadSSBPatch() {
    setAmpState(false);

    g_si4735.setI2CFastModeCustom(I2C_SSB_PATCH_SPEED_HZ);

    g_si4735.queryLibraryId();

    g_si4735.patchPowerUp();
    delay(PATCH_LOAD_DELAY_MS);
    g_si4735.downloadCompressedPatch(ssb_patch_content, sizeof(ssb_patch_content), cmd_0x15, sizeof(cmd_0x15));

    // use bw from the current band's state
    g_si4735.setSSBConfig(g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB], 1, 0, 1, 0, 1);
    g_si4735.setI2CStandardMode();

    g_ssbLoaded = true;

    // line that reset the step here with index has been removed
    // allows the step setting for SSB to persist for each band individually for now

    setAmpState(true);
}

// Applies all FM-specific audio enhancements
// This function acts as a master controller, dispatching to specialized handlers
// for soft mute, noise blanking, and speaker equalization
static void FMAudioConfigure() {
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

    applyFmSoftMuteSettings();
    applyProperties(noise_blanker_props);

    if (g_Settings[FMAudioProfile].param == 1) {
        applyProperties(hicut_speaker_eq_props);
    } else {
        applyProperties(hicut_default_props);
    }
}

// Applies user-defined FM soft mute parameters
// Separates fixed timing values from adjustable thresholds
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
    if (g_Settings[AMNoiseBlanker].param == 1) {
        applyProperties(am_nb_on_props);
    } else {
        applyProperties(am_nb_off_props);
    }
}

// Applies user setting for forcing mono or allowing auto-stereo in FM mode
static void applyFMStereoSettings() {
    g_si4735.setFmStereoMode(g_Settings[ForceMono].param == 1);
}

// Orchestrates complete Si4735 setup for FM mode
// Main entry point when switching to any FM band
// - Sets essential parameters like frequency limits and step from band data
// - Applies custom seek thresholds for improved weak station performance
// - Activates curated audio profile via FMAudioConfigure for enhanced sound
static void configureFMMode() {
    g_currentMode = FM;
    g_stereoStatus = false;

    // Get all parameters from the current band's state
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setFM(
        current_band.minimumFreq,
        current_band.maximumFreq,
        current_band.currentFreq,
        g_tabStepFM[current_band.stepIdxFM]);

    g_si4735.setSeekFmLimits(
        current_band.minimumFreq,
        current_band.maximumFreq);

    g_si4735.setSeekFmSpacing(10);

    // Set custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(FM_SEEK_TUNE_SNR_THRESHOLD_PROP, FM_SEEK_SNR_THRESHOLD_VAL);
    g_si4735.setProperty(FM_SEEK_TUNE_RSSI_THRESHOLD_PROP, FM_SEEK_RSSI_THRESHOLD_VAL);

    g_ssbLoaded = false;

    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[DeEmp].param == 0) ? 1 : 2);

    // after basic FM configuration is done - apply permanent audio enhancement profile
    FMAudioConfigure();

    applyFMStereoSettings();
}

// Orchestrates Si4735 setup for SSB and CW modes
// - Handles optional SSB patch reload on major mode changes
// - Differentiates between SSB and CW disabling DSP AFC for CW reception
// - Applies all user-defined settings for filters audio and soft mute
static void configureSSBMode(
    uint16_t minFreq,
    uint16_t maxFreq,
    bool extraSSBReset) {

    Band& current_band = g_bandList[g_bandIndex];

    if (current_band.bwIdxSSB > g_bwSSBMaxIdx)
        current_band.bwIdxSSB = 4;

    // g_currentBFO = 0;
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);

    g_si4735.setSSB(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? g_lastCWMode : g_currentMode);

    updateSSBCutoffFilter();

    // disable Sync (DSP AFC) functionality when in CW mode
    if (g_currentMode == CW) {
        g_si4735.setSSBDspAfc(1);
        g_si4735.setSSBAvcDivider(0);
    } else { // LSB or USB
        g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);
    }

    // Use SoftMute setting from storage for SSB
    g_si4735.setAmSoftMuteMaxAttenuation(
        g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_SSB]
    );
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);

    // Use bandwidth index from the current band state
    g_si4735.setSSBAudioBandwidth(
        (g_currentMode == CW)
        ? g_bwSSBIdx[0]
        : g_bwSSBIdx[current_band.bwIdxSSB]
    );
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

// Configures chip for standard AM reception
// consolidating all critical audio and gain settings into single block immediately
// following setAM command it resolves
static void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;
    const Band& current_band = g_bandList[g_bandIndex];
    ModeContext modeCtx = getModeContext();

    // Set primary mode and frequency
    g_si4735.setAM(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        g_tabStep[current_band.stepIdxAM]);

    // Bandwidth
    g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);

    // Soft Mute settings
    g_si4735.setProperty(AM_SOFT_MUTE_SLOPE_PROP, AM_SOFT_MUTE_SLOPE_RECOMMENDED); // new
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx]);
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);

    // AGC settings - deliberate duplication
    // block is critical for timing on cold start
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];
    setAgcHardware(att_val);

    // AVC Gain - must be set unconditionally
    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);
}

// Centralizes setup for properties shared between AM and SSB to avoid code duplication
// - Unconditionally applies AVC max gain to ensure correct audio levels
// - Sets custom seek thresholds for improved weak station performance
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);

    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    // Custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(AM_SEEK_SNR_THRESHOLD_PROP, AM_SEEK_SNR_THRESHOLD_VAL);
    g_si4735.setProperty(AM_SEEK_RSSI_THRESHOLD_PROP, AM_SEEK_RSSI_THRESHOLD_VAL);

    applyAMNoiseBlankerSettings();
}

// Applies AGC settings based on current mode and stored values
static void applyAgcSettings() {
    ModeContext modeCtx = getModeContext();
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];

    setAgcHardware(att_val);
}

// Top-level orchestrator for all band and mode changes
// Manages amplifier state safely preventing audio pops during major mode switches (FM <-> AM)
// Loads new band data then dispatches to correct configuration handler
// Applies shared settings like AGC and refreshes display to reflect new state
static void applyBandConfiguration(bool extraSSBReset) {
    // detects a major mode switch (FM <-> non-FM) to safely toggle amp
    bool isFmBand = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    bool switchingBetweenFMandAM = (g_currentMode == FM) != isFmBand;

    // Forcefully disable Squelch if it was active before changing the configuration
    if (g_squelchCutoff) {
        g_si4735.setAudioMute(false);
        g_squelchCutoff = false;
    }

    if (switchingBetweenFMandAM) setAmpState(false);

    loadActiveStateFromBand();

    g_signalQualityValue = INVALID_RSSI_VALUE;

    uint8_t cap_value = isFmBand ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);

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
    }

    applyAgcSettings();

    if (!g_settingsActive) {
        oled.clear();
        showStatus(true);
    }

    // resetEepromDelay() // redundant

    if (switchingBetweenFMandAM) setAmpState(true);

    g_previousFrequency = g_currentFrequency;
}

// Configures hardware seek parameters before starting a scan
// dynamic seek step feature for AM bands, where the scan step matches the user selected manual tuning step
static inline void setupSeekParameters(uint16_t minLimit, uint16_t maxLimit) {
    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        // for AM/SW seek step is dynamically tied to the user current manual step setting
        uint16_t current_step = g_tabStep[g_bandList[g_bandIndex].stepIdxAM];
        uint8_t seek_spacing = (current_step > 10) ? 10 : current_step;

        // Si4735 has specific limitations on supported seek steps
        // we always send a valid value, defaulting to 5kHz if the user step isnt supported hardware
        if (seek_spacing != 1 && seek_spacing != 5 && seek_spacing != 9 && seek_spacing != 10)
            seek_spacing = 5;

        g_si4735.setSeekAmLimits(minLimit, maxLimit);
        g_si4735.setSeekAmSpacing(seek_spacing);
    } else {
        // For FM seek parameters fixed
        g_si4735.setSeekFmLimits(minLimit, maxLimit);
        g_si4735.setSeekFmSpacing(10);
    }
}


// ==========================================
// ===== STATE & ACTION MANAGEMENT ==========
// ==========================================

// performs bfo rollover with integrated boundary checks and a max bfo limit
// this is the core of the stability system for ssb tuning
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline void performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {

    if (abs(*bfo) >= BFO_ROLLOVER_MAX_HZ) {
        // fast - for large jumps work directly with kHz steps
        int16_t steps_khz = *bfo / HZ_PER_KHZ;
        *freq += steps_khz;
        *bfo %= HZ_PER_KHZ;

        if (*freq >= g_bandList[g_bandIndex].maximumFreq ||
            *freq < g_bandList[g_bandIndex].minimumFreq) {
            bandSwitch(steps_khz > 0, false);
        }

        snapToNewStep(freq, steps_khz > 0);
    } else {
        // precise - for fine-tuning near the rollover point
        long absolute_freq_hz = ((long)(*freq) * HZ_PER_KHZ) + *bfo;
        long min_freq_hz = (long)g_bandList[g_bandIndex].minimumFreq * HZ_PER_KHZ;
        long max_freq_hz = (long)g_bandList[g_bandIndex].maximumFreq * HZ_PER_KHZ;

        if (absolute_freq_hz >= max_freq_hz || absolute_freq_hz < min_freq_hz) {
            bool direction_is_up = (*bfo > 0);
            bandSwitch(direction_is_up, false);

            // after band switch recalculate freq/bfo from the absolute Hz value
            int32_t new_freq_khz = absolute_freq_hz / HZ_PER_KHZ;
            int32_t new_bfo_hz = absolute_freq_hz % HZ_PER_KHZ;

            // corrects negative BFO back into  positive range 0-999
            // and adjusts main frequency down by 1 kHz to compensate
            if (new_bfo_hz < 0) {
                new_bfo_hz += HZ_PER_KHZ;
                new_freq_khz -= 1;
            }

            *freq = (uint16_t)new_freq_khz;
            *bfo = new_bfo_hz;

            snapToNewStep(freq, direction_is_up);
        }
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

// Manages the seek process and updates the application state.
static void doSeek() {
    uint16_t f = executeHardwareSeek();
    if (!f) return;

    g_currentFrequency = f;

    switch (g_bandList[g_bandIndex].bandType) {
    case SW_BAND_TYPE:
        for (uint8_t i = 2; i <= g_lastBand; ++i) {
            // Cache pointer to current element
            // avoid re-calculating g_bandList + i * sizeof(Band) multiple times
            const Band* current_band_ptr = &g_bandList[i];
            if (f >= current_band_ptr->minimumFreq &&
                f <= current_band_ptr->maximumFreq) {
                g_bandIndex = i;
                break;
            }
        }
        break;

    case FM_BAND_TYPE:
        g_currentFrequency -= f % 10;
        break;

    default: break;
    }

    g_si4735.setFrequency(g_currentFrequency);
    doBandwidth(0);
    syncActiveStateToBand();
    showStatus(true);
    resetEepromDelay();

    g_previousFrequency = g_currentFrequency;
}

// switches band index and immediately applies the new band's default state
static void bandSwitch(bool up, bool loadStoredFreq) {
    syncActiveStateToBand(); // Save current frequency to RAM
    markStateAsDirty();

    uint8_t oldBandIndex = g_bandIndex;
    g_currentBFO = 0;

    int8_t delta = up ? 1 : -1;
    g_bandIndex = (g_bandIndex + delta + g_bandCount) % g_bandCount;

    // load stored frequency ONLY if requested (for manual band switching BAND+)
    if (loadStoredFreq) loadActiveStateFromBand();

    g_lastSavedFrequency = g_currentFrequency;

    BandType oldType = g_bandList[oldBandIndex].bandType;
    BandType newType = g_bandList[g_bandIndex].bandType;

    g_previousFrequency = g_currentFrequency;

    if (oldType != FM_BAND_TYPE && newType != FM_BAND_TYPE) {
        // fast for seamless transitions within AM/SW bands
        g_si4735.setFrequency(g_currentFrequency);
        applyAgcSettings();
        doBandwidth(0);

        // clear at SW<->MW/LW transition if MHz mode is enabled
        bool clean = g_Settings[SettingsIndex::SWUnits].param == 1 &&
            ((oldType == SW_BAND_TYPE) != (newType == SW_BAND_TYPE));

        showFrequency(clean);
        showBandTag();
        showStep();
    } else {
        // long for major mode changes (like to/from FM - in AM/LW/MW (SSB too)
        applyBandConfiguration();
    }
}

// handles frequency tuning for am/fm
static void doFrequencyTune() {
    g_seekDirection = g_encoderCount > 0;
    const Band& old_band = g_bandList[g_bandIndex];
    uint16_t step = (old_band.bandType == FM_BAND_TYPE)
        ? g_tabStepFM[old_band.stepIdxFM]
        : g_tabStep[old_band.stepIdxAM];

    // 32-bit integer is needed here for calculations to prevent underflow on band edges
    int32_t temp_freq = g_currentFrequency + (int16_t)step * g_encoderCount;
    g_encoderCount = 0;

    // > for the upper bound to include the maximum frequency value within the band
    bool needs_switch = (temp_freq > old_band.maximumFreq || temp_freq < old_band.minimumFreq);

    if (needs_switch) {
        // band boundary has been crossed
        bandSwitch(g_seekDirection, false);

        // This block differentiates between two types of band transitions:
        // - Seamless Crossover (e.g., AM<->SW) - keep temp_freq for smooth tuning
        // - Wrap-Around (involving FM) - reset frequency to the new band edge
        // Presence of FM_BAND_TYPE is a proxy for wrap-around behavior
        bool is_wrap_around = (old_band.bandType == FM_BAND_TYPE);
        const Band& new_band = g_bandList[g_bandIndex];
        is_wrap_around |= (new_band.bandType == FM_BAND_TYPE);

        g_currentFrequency = is_wrap_around
            ? (g_seekDirection ? new_band.minimumFreq : new_band.maximumFreq)
            : (uint16_t)temp_freq;
    } else {
        // standard intra-band tuning path
        uint16_t newFreq = (uint16_t)temp_freq;

        // snap frequency to the current step grid
        // intentionally skipped during a band switch to prevent frequency distortion
        uint16_t remainder = newFreq % step;
        g_currentFrequency = remainder
            ? (newFreq - remainder + (g_seekDirection ? step : 0))
            : newFreq;
    }

    g_processFreqChange = true;
    g_lastFreqChange = millis();
    showFrequency();
    syncActiveStateToBand();
    markStateAsDirty();
}

// prepare SSB tune by checking count and calculating temp values
static inline bool SSBTune(uint16_t& temp_freq, int32_t& temp_bfo) {
    if (g_encoderCount == 0) return false;

    // store frequency before changes to detect a rollover event
    temp_freq = g_currentFrequency;

    // 32-bit integer to prevent overflow during fast encoder spins
    temp_bfo = g_currentBFO;

    temp_bfo += (int32_t)g_tabStep[SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB] * g_encoderCount;
    g_encoderCount = 0;

    return true;
}

// performs SSB rollover and chip update
static inline void SSBRollover(uint16_t& temp_freq, int32_t& temp_bfo, uint16_t old_freq) {
    performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo);

    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    // if the base frequency changed, the chip must be updated
    // this is critical fix!
    if (g_currentFrequency != old_freq) {
        g_si4735.setFrequency(g_currentFrequency);
        applyAgcSettings();
    }
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
        if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
            g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
            g_currentBFO = 0;
        }

        SSBTuneFinalize();
    }
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
    if (isSSB()) {
        int16_t b = g_currentBFO;
        int16_t k = (b >= 0) ? b / HZ_PER_KHZ : -((-b + HZ_PER_KHZ - 1) / HZ_PER_KHZ);
        uint16_t f = g_currentFrequency + k;
        b -= k * HZ_PER_KHZ;

        if (f < band.minimumFreq) {
            f = band.minimumFreq;
            b = 0;
        } else if (f > band.maximumFreq) {
            f = band.maximumFreq;
            b = 0;
        }

        g_currentFrequency = f;
        g_currentBFO = b;
    }

    // Refresh the cache only when exiting LSB/USB
    if (mode_before == LSB || mode_before == USB)
        g_savedSsbBfo[g_bandIndex] = g_currentBFO;

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
        current_band.bwIdxAM = bw;
        break;

    case AM:
        g_currentMode = g_lastSsbMode;
        loadSSBPatch();
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

// --- Settings & Parameter Handlers ---

static void switchSettingsPage() {
    g_SettingsPage++;
    g_SettingsPage = (g_SettingsPage > g_SettingsMaxPages) ? 1 : g_SettingsPage;
    g_SettingSelected = 6 * (g_SettingsPage - 1);
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
        syncModeDependentSettings(true);

        g_SettingsPage = 1;
        showSettingsTitle();
        g_SettingSelected = 0;
        g_SettingEditing = false;
        showSettings();
    } else {
        // Exiting settings menu
        syncModeDependentSettings(false);

        g_settingsDirty = true;

        // Commit all changes to EEPROM and return to the main screen
        saveAllReceiverInformation();
        showStatus();
    }
}

#if ENABLE_FAVORITES
// Add current station details to RAM and set dirty flag
static void addFavorite() {
    if (g_totalFavorites >= MAX_FAVORITES) return;

    // Prevent adding a station if the same frequency and mode already exist
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        if (g_favorites[i].frequency == g_currentFrequency &&
            g_favorites[i].modulation == g_currentMode) {
            return;
        }
    }

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
    for (uint8_t i = g_favoriteSelected; i < g_totalFavorites - 1; i++) {
        g_favorites[i] = g_favorites[i + 1];
    }

    g_totalFavorites--;

    // If the last item was deleted, move the selection to the new last item
    if (g_totalFavorites &&
        g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites - 1;
    }

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
        (previousBandType != g_bandList[g_bandIndex].bandType) ||
        (g_ssbLoaded && !ssbWasLoaded);

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
    if (g_muteVolume) {
        vol = g_muteVolume;
        g_muteVolume = 0;
    } else {
        vol = g_si4735.getCurrentVolume() + v;
        if (vol < 0) vol = 0;
        else if (vol > 63) vol = 63;
    }
    g_si4735.setVolume(vol);
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
        g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);
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
        g_si4735.setFMDeEmphasis(g_Settings[DeEmp].param == 0 ? 1 : 2);
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

// ==========================================
// ===== PERIODIC & TIMED TASKS =============
// ==========================================

// Helper for performing frequency update check
static inline void performFrequencyUpdateCheck(uint32_t now) {
    // calculate delta from the LAST frequency sent to the chip!
    int32_t freq_delta = abs((int32_t)g_currentFrequency - g_previousFrequency);

    bool time_elapsed = (now - g_lastFreqChange >= FREQ_UPDATE_DELAY_MS);
    bool force_update = (freq_delta >= FREQ_FORCE_UPDATE_THRESHOLD_KHZ);
    bool rate_limit_ok = (now - g_lastSetFreqTime >= MIN_SETFREQ_INTERVAL_MS);

    // send command if the tuning timer has elapsed OR the frequency delta is large
    if ((time_elapsed || force_update) && rate_limit_ok) {
        g_si4735.setFrequency(g_currentFrequency);
        g_processFreqChange = false;
        g_lastSetFreqTime = now;

        // sync previous frequency ONLY after a successful command send
        g_previousFrequency = g_currentFrequency;
    }
}

// Handles the delayed frequency update for AM/FM to prevent flooding the chip
static void handleDelayedFrequencyUpdate() {
    if (!g_processFreqChange || isSSB()) return;

    uint32_t now = millis();

    if (g_safeEncoderMovement) {
        g_encoderCount = g_safeEncoderMovement;
        g_safeEncoderMovement = 0;
        doFrequencyTune();
        return;
    }

    performFrequencyUpdateCheck(now);
}

// Fetches signal quality (RSSI) using mode-specific commands
// SSB/CW is unsupported by this patch query method
static uint8_t getSignalQuality() {
    switch (g_currentMode) {
    case FM:
        g_si4735.getCurrentReceivedSignalQuality(1);
        return g_si4735.getCurrentRSSI();

    case AM:
        // Return last value if disabled or user is actively tuning
        if (g_Settings[RSSI_AM_Off].param == 1 || ((uint16_t)(millis() / 1000) - g_lastUserActivityTime < 1)) {
            return g_signalQualityValue;
        }
        // Soft update prevents audio clicks
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

// Checks for and handles signal quality and stereo indicator updates
static inline void handleSignalAndStereoUpdates() {
    // 500ms debounce after last frequency change to prevent polling while actively tuning
    if (millis() - g_lastFreqChange < RSSI_POLL_DELAY_AFTER_TUNE_MS) return;

    // updates prevent while in any menu
    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    if (millis() - g_lastRSSIUpdate >= RSSI_POLL_INTERVAL_MS) {
        g_lastRSSIUpdate = millis();

        updateSignalQuality();
        updateFmStereoIndicator();
    }
}

// provides auto-exit for both temporary adjustment modes
// (e.g., Volume) and the main Settings menu.
static inline void handleCommandTimeout() {
    if (g_lastAdjustmentTime) {
        uint32_t timeout = g_settingsActive ? SETTINGS_MENU_TIMEOUT : ADJUSTMENT_ACTIVE_TIMEOUT;

        if (millis() - g_lastAdjustmentTime > timeout) {
            if (g_settingsActive) {
                g_settingsActive = false;
                switchSettings();
            }
            resetCommandMode();
        }
    }
}

#if ENABLE_FAVORITES
// Provides auto-exit for the favorites menu on inactivity
static inline void handleFavoritesTimeout() {
    if (g_favoritesActive && (millis() - g_lastAdjustmentTime > SETTINGS_MENU_TIMEOUT))
        exitFavoritesMenu();
}
#endif

// Manages saving settings to EEPROM based on user activity
static inline void handleSettingsSave() {
    // save menu settings immediately on exit for predictable behavior
    if (g_settingsDirty) {
        saveAllReceiverInformation(true);
        g_settingsDirty = false;
        g_stateIsDirty = false;             // settings include state, reset both flags
        return;
    }

    // save frequency state on idle to prevent EEPROM wear during active tuning
    // and to ensure last frequency is saved before a potential power-off
    if (g_stateIsDirty && ((uint16_t)(millis() / 1000) - g_lastUserActivityTime > (SAVE_ON_IDLE_TIMEOUT / 1000))) {
        saveAllReceiverInformation(false); // partial save for frequency only
        g_stateIsDirty = false;            // reset flag only after successful save
    }
}

// Handles auto display-off timer
// using a data-driven PROGMEM lookup instead of branching logic
// and tracks time in seconds to keep all math within 16-bit operations
static inline void checkDisplayTimeout() {
    uint8_t p = g_Settings[DisplayOff].param;

    if (!g_displayOn || p == 0) return;

    uint16_t timeout_s = pgm_read_word(&T[p]);

    if ((uint16_t)(millis() / 1000) - g_lastUserActivityTime > timeout_s) {
        g_displayOn = false;

        // on auto-timeout engage deep power save mode at 2 MHz to maximize battery life
        setCpuPrescaler(CPU_PRESCALER_DEEP_SLEEP); // 3 = 2 MHz , 2 = 4 MHz , 1 = 8 MHz

        oled.setPower(false);
        autoDisplayOff = true;
    }
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
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();

    applyBandConfiguration();
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

#if ENABLE_FAVORITES
    if (g_favoritesActive) {
        handleFavoritesMenu();
        processButtonEvents();
        handleFavoritesTimeout();
        return;
    }
#endif

    handleDelayedFrequencyUpdate();

    bool frequencyTuned = false;
    if (g_safeEncoderMovement)
        frequencyTuned = processEncoderActions();

    // process buttons only if the encoder was not used for a major tuning event
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
