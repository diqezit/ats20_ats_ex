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
// ===== CORE UTILITIES & DEFINITIONS =======
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

static inline bool checkStopSeeking() {
    bool result;
    noInterrupts();  // race protection
    result = g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
    interrupts();
    return result;
}

// performs bfo rollover with integrated boundary checks and a max bfo limit
// this is the core of the stability system for ssb tuning
// see: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline void performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {

    const int32_t BFOMax = 13000;
    const int16_t KHZ = 1000;

    if (abs(*bfo) >= BFOMax) {
        // fast - for large jumps work directly with kHz steps
        int16_t steps_khz = *bfo / KHZ;
        *freq += steps_khz;
        *bfo %= KHZ;

        if (*freq >= g_bandList[g_bandIndex].maximumFreq ||
            *freq < g_bandList[g_bandIndex].minimumFreq) {
            bandSwitch(steps_khz > 0, false);
        }

        snapToNewStep(freq, steps_khz > 0);
    } else {
        // precise - for fine-tuning near the rollover point
        long absolute_freq_hz = ((long)(*freq) * KHZ) + *bfo;
        long min_freq_hz = (long)g_bandList[g_bandIndex].minimumFreq * KHZ;
        long max_freq_hz = (long)g_bandList[g_bandIndex].maximumFreq * KHZ;

        if (absolute_freq_hz >= max_freq_hz || absolute_freq_hz < min_freq_hz) {
            bool direction_is_up = (*bfo > 0);
            bandSwitch(direction_is_up, false);

            // after band switch recalculate freq/bfo from the absolute Hz value
            int32_t new_freq_khz = absolute_freq_hz / KHZ;
            int32_t new_bfo_hz = absolute_freq_hz % KHZ;

            // corrects negative BFO back into  positive range 0-999
            // and adjusts main frequency down by 1 kHz to compensate
            if (new_bfo_hz < 0) {
                new_bfo_hz += KHZ;
                new_freq_khz -= 1;
            }

            *freq = (uint16_t)new_freq_khz;
            *bfo = new_bfo_hz;

            snapToNewStep(freq, direction_is_up);
        }
    }
}

// ==========================================
// ===== HARDWARE CONTROL SUBSYSTEM =========
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

// AGC hardware control
static inline void setAgcHardware(int8_t att_val) {
    bool disableAgc = att_val > 0;
    // attenuation index for the chip is one less than the parameter valu
    // if att_val is 0 (auto) or 1 (manual, 0dB), the index sent to the chip is 0
    uint8_t agcNdx = (att_val > 1) ? (att_val - 1) : 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

//Saves more flash image size
static void updateSSBCutoffFilter() {
    uint8_t idx = g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB];
    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter((idx == 0 || idx == 4 || idx == 5) ? 0 : 1);
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

// This function is required for using SSB. Si473x controllers do not support SSB by-default.
// But we can patch internal RAM of Si473x with special patch to make it work in SSB mode.
// Patch must be applied every time we enable SSB after AM or FM.
static void loadSSBPatch() {
    setAmpState(false);

    g_si4735.setI2CFastModeCustom(500000);

    g_si4735.queryLibraryId();

    g_si4735.patchPowerUp();
    delay(50);
    g_si4735.downloadCompressedPatch(ssb_patch_content, sizeof(ssb_patch_content), cmd_0x15, sizeof(cmd_0x15));

    // use bw from the current band's state
    g_si4735.setSSBConfig(g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB], 1, 0, 1, 0, 1);
    g_si4735.setI2CStandardMode();

    g_ssbLoaded = true;

    // line that reset the step here with index has been removed
    // allows the step setting for SSB to persist for each band individually for now

    setAmpState(true);
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

// sets up the station seek boundaries and step
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(30);

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

// Applies curated audio profile for FM band
// Profile is permanently active in FM mode and combines key enhancements
// - Enables aggressive soft mute for quiet tuning between stations
// - Repurposes Hi-Cut filter as a static EQ creating warmer sound on small speaker
// - Activates an experimental noise blanker to reduce impulse noise
// - Delegates mono/stereo control to dedicated handler
static void FMAudioConfigure() {

    // --- Aggressive Soft Mute ---
    // properties make the soft mute react instantly and attenuate deeply when SNR drops
    // silences static hiss when tuning between stations
    g_si4735.setProperty(0x1300, FM_PROP_SOFTMUTE_RATE);
    g_si4735.setProperty(0x1301, FM_PROP_SOFTMUTE_SLOPE);
    g_si4735.setProperty(0x1302, FM_PROP_SOFTMUTE_MAX_ATTN);
    g_si4735.setProperty(0x1303, FM_PROP_SOFTMUTE_REL_RATE);
    g_si4735.setProperty(0x1304, FM_PROP_SOFTMUTE_ATT_RATE);
    g_si4735.setProperty(0x1305, FM_PROP_SOFTMUTE_DEC_RATE);


    // --- Hi-Cut Filter as Audio Equalizer ---
    // The code below re-purposes the hi-cut filter as a static EQ to create a warmer sound

    // dynamic hi-cut filter is re-purposed as a static audio filter
    // forced active to tailor the audio output for the small speaker,
    // reducing high-frequency harshness
    g_si4735.setProperty(0x1A01, FM_PROP_HICUT_WINDOW);
    g_si4735.setProperty(0x1A02, FM_PROP_HICUT_SNR_THRESH);
    g_si4735.setProperty(0x1A03, FM_PROP_HICUT_ATT_RATE);
    g_si4735.setProperty(0x1A04, FM_PROP_HICUT_REL_RATE);
    g_si4735.setProperty(0x1A05, FM_PROP_HICUT_MPX_THRESH);
    g_si4735.setProperty(0x1A06, FM_PROP_HICUT_CUTOFF);
    g_si4735.setProperty(FM_PROP_HICUT_ENABLE, 1); // Always enable Hi-Cut for this profile

    // --- Experimental Noise Blanker ---
    // configure a digital filter to detect and suppress short noise spikes
    // this feature is undocumented for Si473x but present in related chips
    // testing shows no audio degradation when enabled
    g_si4735.setProperty(0x1900, FM_PROP_NB_REJ_THRESH);
    g_si4735.setProperty(0x1901, FM_PROP_NB_ATT_RATE);
    g_si4735.setProperty(0x1902, FM_PROP_NB_REL_RATE);
    g_si4735.setProperty(0x1903, FM_PROP_NB_ADC_OVER_THRESH);
    g_si4735.setProperty(0x1904, FM_PROP_NB_ADC_OVER_DELAY);
}

// Corrected CW BFO offset logic to match standard radio behavior
// The Si4735 IC requires an inverted BFO value, so the math is reversed here to compensate
// To get a positive BFO offset for USB, the value must be negative before the final inversion
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static void updateBFO() {

    int16_t finalBfo = g_currentBFO + (g_Settings[BFO].param * 100);

    if (g_currentMode == CW) {
        if (g_lastCWMode == USB) { // 1 = USB
            finalBfo -= CW_PITCH_OFFSET_HZ;
        } else { // 0 = LSB
            finalBfo += CW_PITCH_OFFSET_HZ;
        }
    }

    g_si4735.setSSBBfo(finalBfo * -1);
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
    g_si4735.setProperty(0x1403, 2);  // FM_SEEK_TUNE_SNR_THRESHOLD (Default: 3)
    g_si4735.setProperty(0x1404, 5);  // FM_SEEK_TUNE_RSSI_THRESHOLD (Default: 20)

    g_ssbLoaded = false;

    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[DeEmp].param == 0) ? 1 : 2);

    // after basic FM configuration is done - apply permanent audio enhancement profile
    FMAudioConfigure();
}

// Orchestrates Si4735 setup for SSB and CW modes
// - Handles optional SSB patch reload on major mode changes
// - Differentiates between SSB and CW disabling DSP AFC for CW reception
// - Applies all user-defined settings for filters audio and soft mute
static void configureSSBMode(
    uint16_t minFreq,
    uint16_t maxFreq,
    bool extraSSBReset
) {
    Band& current_band = g_bandList[g_bandIndex];

    if (current_band.bwIdxSSB >= g_bwSSBMaxIdx)
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
// - Reads all parameters like frequency step and bandwidth from current band state
// - Applies mode-specific settings for audio properties like soft mute
static void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setAM(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        g_tabStep[current_band.stepIdxAM]);

    // Use SoftMute setting from storage for AM
    g_si4735.setAmSoftMuteMaxAttenuation(
        g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_AM]
    );
    g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);
    g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);
}

// Centralizes setup for properties shared between AM and SSB to avoid code duplication
// - Conditionally applies AVC max gain only when AGC is in automatic mode
// - Sets custom seek thresholds for improved weak station performance
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    // Apply AVC MAX GAIN from storage for the current mode
    // but only if the AGC enabled (ATT setting is in AUT mode)
    if (g_modeSettings[MODE_SETTING_AGC][modeCtx] == 0)
        g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);

    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    // Custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(AM_SEEK_SNR_THRESHOLD, 3);     // AM_SEEK_TUNE_SNR_THRESHOLD (Default: 5)
    g_si4735.setProperty(AM_SEEK_RSSI_THRESHOLD, 10);   // AM_SEEK_TUNE_RSSI_THRESHOLD (Default: 25)
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

    if (switchingBetweenFMandAM)
        setAmpState(false);

    loadActiveStateFromBand();

    g_signalQualityValue = 255; // not keeping old value on screen temporarily (save 8 bytes)

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

    if (switchingBetweenFMandAM)
        setAmpState(true);

    g_previousFrequency = g_currentFrequency;
}

// ==========================================
// ===== ACTION & STATE MANAGEMENT ==========
// ==========================================

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

// prepare mode switch by storing bandwidth and handling initial state
static inline void prepareModeSwitch(int8_t& bw) {
    Band& current_band = g_bandList[g_bandIndex];
    // store the bandwidth index to carry it over between am/ssb
    bw = (g_currentMode == AM) ? current_band.bwIdxAM : current_band.bwIdxSSB;
    syncActiveStateToBand();

    markStateAsDirty();

    if (g_currentMode == CW) setAmpState(false);
}

// mode cycling logic (AM -> SSB -> CW -> AM)
static inline void performModeCycle(int8_t bw) {
    Band& current_band = g_bandList[g_bandIndex];

    switch (g_currentMode) {
    case LSB:
    case USB:
        g_lastSsbMode = g_currentMode; // remember sideband
        g_currentMode = CW;
        break;

    case CW:
        g_currentMode = AM;
        g_ssbLoaded = false;
        current_band.bwIdxAM = bw;
        break;

    case AM:
        g_currentMode = g_lastSsbMode; // restore sideband
        loadSSBPatch();
        current_band.bwIdxSSB = bw;
        g_processFreqChange = false;   // prevent frequency jump
        break;
    }
}

// handles the complex logic of cycling through AM, LSB, USB, and CW modes
static inline void cycleAmSsbCwModes() {
    int8_t bw;
    prepareModeSwitch(bw);
    performModeCycle(bw);
    applyBandConfiguration();

    if (!g_ssbLoaded && g_currentMode == AM)
        setAmpState(true);
}

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

// Settings: Attenuation (ATT)
// manual control over the receiver front-end gain, which handled by the Automatic Gain Control (AGC)
// 'AUT' (Auto) is the standard mode.
// can be useful to prevent overload from very strong local stations
// (by increasing attenuation)
void doAttenuation(int8_t v) {
    uint8_t max_att_value = (g_currentMode == FM) ? 26 : 37;
    doSwitchLogic(g_Settings[ATT].param, 0, max_att_value, v);

    setAgcHardware(g_Settings[ATT].param);
}

// Settings: Soft Mute Attenuation
// controls HOW MUCH the volume is reduced when a signal becomes weak
// A higher value means stronger muting, making the receiver almost silent on noisy frequencies
// Setting it to 0 - disables soft mute feature
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SoftMute].param, 0, 32, v);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SoftMute].param);
}

// Settings: Soft Mute Threshold
// controls WHEN the soft mute feature activates
// It sets a minimum signal quality (SNR) threshold
// If the signal drops below this level, the audio will be muted by the amount set in 'SMA'
void doSoftMuteThreshold(int8_t v) {
    doSwitchLogic(g_Settings[SoftMuteThr].param, 0, 63, v);
    if (!g_si4735.isCurrentTuneFM())
        g_si4735.setAMSoftMuteSnrThreshold(g_Settings[SoftMuteThr].param);
}

//Settings: Brightness
void doBrightness(int8_t v) {
    int8_t new_setting = g_Settings[Brightness].param + v;

    // clamp the value of to the [0, 9]
    new_setting = constrain(new_setting, 0, 9);

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
    doSwitchLogic(g_Settings[AutoVolControl].param, 12, 90, v);

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
    doSwitchLogic(g_Settings[CutoffFilter].param, 0, 2, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

// Settings: CPU Frequency divider helper
static void setCpuPrescaler(uint8_t prescaler) {
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = prescaler;
    interrupts();
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
    doSwitchLogic(g_Settings[BFO].param, -25, 25, v);

    if (isSSB()) {
        updateBFO();
    }
}

//Settings: Scan button switch
void doScanSwitch(int8_t v) {
    toggleSetting(ScanSwitch);
}

//Settings: CW sideband mode switch (LSB/USB)
// provides a seamless sideband switch by calculating the required
// frequency shift to keep the audible CW tone stable
static inline void doCWSwitch() {
    if (g_currentMode != CW) return;

    constexpr int16_t COMPENSATION_KHZ = (2 * CW_PITCH_OFFSET_HZ) / 1000;
    uint16_t original_freq = g_currentFrequency;

    // Toggles g_lastCWMode between LSB (1) and USB (2)
    g_lastCWMode = 3 - g_lastCWMode;
    // Calculates direction: -1 for LSB (1), +1 for USB (2)
    int8_t direction = (g_lastCWMode << 1) - 3; // (mode * 2) - 3

    g_currentFrequency += direction * COMPENSATION_KHZ;
    g_si4735.setFrequency(g_currentFrequency);
    updateBFO();
    g_currentFrequency = original_freq;
    showFrequency(true);
    updateStereoIndicator();
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
    doSwitchLogic(g_Settings[DisplayOff].param, 0, 4, v);
}

// handles bandwidth adjustment
static void doBandwidth(uint8_t v) {
    Band& band = g_bandList[g_bandIndex];

    // SSB mode
    if (isSSB()) {
        doSwitchLogic(band.bwIdxSSB, 0, LEN(bw_ssb_map), v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[band.bwIdxSSB]);
        updateSSBCutoffFilter();
    }
    // AM and FM modes
    else {
        const bool is_am = (g_currentMode == AM);

        // pointer to an int8_t to target the correct index variable
        // (bwIdxAM or bwIdxFM)
        int8_t* idx = is_am ? (int8_t*)&band.bwIdxAM : (int8_t*)&band.bwIdxFM;
        int8_t  max = is_am ? LEN(bw_am_map) : LEN(bw_fm_map);

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

// designated path for polling AM signal strength
// get RSSI in AM mode using non-interrupting "soft update"
static inline uint8_t getAmSignalValue() {
    if (g_Settings[RSSI_AM_Off].param == 1)
        return 255;

    // 1sec quiet after interaction
    // prevents RSSI from flickering while the encoder is actively being turned
    if (((uint16_t)(millis() / 1000) - g_lastUserActivityTime < 1))
        return g_signalQualityValue;

    // perform "soft update" after checks passed.. 
    g_si4735.softAmRssiUpdate();
    return g_si4735.getReceivedSignalStrengthIndicator();
}

static inline uint8_t getFmSignalValue() {
    g_si4735.getCurrentReceivedSignalQuality(1);
    return g_si4735.getCurrentRSSI();
}

// logic for updating the signal quality indicator RSSI value
static inline void updateSignalQuality() {
    uint8_t new_value = (g_currentMode == FM)
        ? getFmSignalValue()
        : ((g_currentMode == AM)
            ? getAmSignalValue()
            : 255);

    if (g_signalQualityValue != new_value) {
        g_signalQualityValue = new_value;
        showSignalQuality();
    }
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

// settings saved to EEPROM if they have been marked as changed
static inline void handleSettingsSave() {
    if (g_settingsDirty || (g_stateIsDirty && (millis() - g_lastUserActivityTime > SAVE_ON_IDLE_TIMEOUT))) {
        saveAllReceiverInformation(g_settingsDirty); // full_save if settings changed, partial if idle
        g_settingsDirty = false;
        g_stateIsDirty = false;
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
        setCpuPrescaler(3); // 3 = 2 MHz , 2 = 4 MHz , 1 = 8 MHz

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
    g_voltagePinConnnected = analogRead(getBatteryPin()) > 300;
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

    delay(500);
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
