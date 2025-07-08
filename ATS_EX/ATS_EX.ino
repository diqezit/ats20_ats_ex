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
// MOD_NO_RDS_v4.5 by diqezit
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

#include <SI4735.h>
#include <EEPROM.h>
#include <Tiny4kOLED.h>
#include <PixelOperatorBold.h>

#include "font14x24sevenSeg.h"
#include "Rotary.h"
#include "SimpleButton.h"
#include "patch_ssb_compressed.h"

#include "defs.h"
#include "globals.h"
#include "Utils.h"

constexpr auto APP_VERSION = 45;

// ------------------------------------------
// ------- Utility & Helper Functions -------
// ------------------------------------------

static bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// Helper function to get the current context (AM or SSB/CW)
static ModeContext getModeContext() {
    if (isSSB() || g_currentMode == CW) {
        return MODE_CONTEXT_SSB;
    }
    return MODE_CONTEXT_AM;
}

// gets the band name from the Band structure into a C-string
static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, 4);
    buffer[4] = '\0'; // ensure null
}

// formats a raw step value into a human-readable string for the display
// conversions to "kHz", "Hz", and a special "1M" case for 1000 kHz
static void formatStepValue(char* buf, int stepValue, bool isKhz) {
    if (isKhz && stepValue == 1000) {
        strcpy_P(buf, PSTR("  1M"));
    } else if (isKhz) {
        convertToChar(buf, stepValue, 3);
        buf[3] = 'k';
        buf[4] = '\0';
    } else { // Hz for SSB
        if (stepValue >= 1000) {
            convertToChar(buf, stepValue / 1000, 3);
            buf[3] = 'k';
            buf[4] = '\0';
        } else {
            convertToChar(buf, stepValue, 4);
        }
    }
}

// most state is already in the band list
// only need to sync the single live frequency variable
static void syncActiveStateToBand() {
    const Band& current_band = g_bandList[g_bandIndex];

    // update only band stored frequency if the current live frequency
    // is within the valid range for this band
    if (g_currentFrequency >= current_band.minimumFreq && g_currentFrequency <= current_band.maximumFreq) {
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    }
}

// functions will read step/bw directly from the band list
// only need to load the frequency into the single live variable
static void loadActiveStateFromBand() {
    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
}

//For saving features
static void resetEepromDelay() {
    g_storeTime = millis();
    g_previousFrequency = 0;
}

// register user activity and mark the state as dirty before save in EEPROM
static inline void markStateAsDirty() {
    g_lastUserActivityTime = millis();
    g_stateIsDirty = true;
    g_forceRssiUpdate = true;
}

static bool checkStopSeeking() {
    return g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

// performs bfo rollover with integrated boundary checks and a max bfo limit
// this is the core of the stability system for ssb tuning
// returns true if a band switch occurred, false otherwise
// see: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline bool performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {
    const int32_t BFOMax = 13000;
    Band& current_band = g_bandList[g_bandIndex];

    // clamp bfo to the maximum allowed range first for stability
    if (*bfo > BFOMax) *bfo = BFOMax;
    if (*bfo < -BFOMax) *bfo = -BFOMax;

    // then perform the reliable rollover with integrated "emergency brake" checks
    while (*bfo >= 1000) {
        (*freq)++;
        if (*freq >= current_band.maximumFreq) {
            bandSwitch(true);
            return true;
        }
        *bfo -= 1000;
    }

    while (*bfo <= -1000) {
        (*freq)--;
        if (*freq < current_band.minimumFreq) {
            bandSwitch(false);
            return true;
        }
        *bfo += 1000;
    }

    return false;
}

// ------------------------------------------
// ------- EEPROM Data I/O Subsystem --------
// ------------------------------------------

// Initializes mode-dependent settings to their default values
static void initializeDefaultModeSettings() {
    for (uint8_t i = 0; i < MODE_CONTEXT_COUNT; i++) {
        g_modeSettings[MODE_SETTING_AGC][i] = defaultModeSettings[i].agc;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][i] = defaultModeSettings[i].soft_mute;
        g_modeSettings[MODE_SETTING_AVC][i] = defaultModeSettings[i].avc;
    }
}

// writes the state of a single band's variable data to a specific eeprom address
static void writeBandStateToEEPROM(uint16_t addr, const Band& band) {
    EEPROM.update(addr + 0, band.currentFreq >> 8);
    EEPROM.update(addr + 1, band.currentFreq & 0xFF);
    EEPROM.update(addr + 2, band.stepIdxAM);
    EEPROM.update(addr + 3, band.stepIdxSSB);
    EEPROM.update(addr + 4, band.stepIdxFM);
    EEPROM.update(addr + 5, band.bwIdxAM);
    EEPROM.update(addr + 6, band.bwIdxSSB);
    EEPROM.update(addr + 7, band.bwIdxFM);
}

// reads the state of a single band's variable data from a specific eeprom address
static void readBandStateFromEEPROM(uint16_t addr, Band& band) {
    band.currentFreq = (EEPROM.read(addr + 0) << 8) | EEPROM.read(addr + 1);
    band.stepIdxAM = EEPROM.read(addr + 2);
    band.stepIdxSSB = EEPROM.read(addr + 3);
    band.stepIdxFM = EEPROM.read(addr + 4);
    band.bwIdxAM = EEPROM.read(addr + 5);
    band.bwIdxSSB = EEPROM.read(addr + 6);
    band.bwIdxFM = EEPROM.read(addr + 7);
}

// Save favorites FM stations to EEPROM
static void saveFMFav() {
    EEPROM.update(EEPROM_FM_FAVORITES_COUNT, g_totalFavorites);

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < MAX_FM_FAVORITES; i++) {
        if (i < g_totalFavorites) {
            uint16_t freq = g_fmFavorites[i].frequency;
            EEPROM.update(addr++, freq >> 8);
            EEPROM.update(addr++, freq & 0xFF);
        } else {
            // Fill unused slots with 0xFFFF
            EEPROM.update(addr++, 0xFF);
            EEPROM.update(addr++, 0xFF);
        }
    }
}

// Load favorites from EEPROM with data validation
static void loadFMFav() {
    g_totalFavorites = EEPROM.read(EEPROM_FM_FAVORITES_COUNT);

    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FM_FAVORITES) {
        g_totalFavorites = 0;
        saveFMFav();
        return;
    }

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        uint16_t freq = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
        if (freq >= 6400 && freq <= 10800) {
            g_fmFavorites[i].frequency = freq;
        } else {
            g_totalFavorites = 0;
            saveFMFav();
            return;
        }
    }
}

// Low-level helper to write a block of data and advance the address pointer
static inline void writeEepromBlock(uint16_t& addr, const void* src, uint16_t size) {
    const uint8_t* p = (const uint8_t*)src;
    for (uint16_t i = 0; i < size; i++) {
        EEPROM.update(addr + i, p[i]);
    }
    addr += size;
}

// Low-level helper to read a block of data and advance the address pointer
static inline void readEepromBlock(uint16_t& addr, void* dst, uint16_t size) {
    uint8_t* p = (uint8_t*)dst;
    for (uint16_t i = 0; i < size; i++) {
        p[i] = EEPROM.read(addr + i);
    }
    addr += size;
}

// Writes the main configuration header (volume, mode, etc.)
static inline void writeEepromHeader(uint16_t& addr) {
    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0xFF);
    EEPROM.update(addr++, g_prevMode);
}

// Reads the main configuration header and performs sanity checks
static inline void readEepromHeader(uint16_t& addr) {
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    if (g_bandIndex > g_lastBand) g_bandIndex = 1; // Sanity check
    g_currentMode = EEPROM.read(addr++);
    g_currentBFO = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
    g_prevMode = EEPROM.read(addr++);
}

// Writes all band data from RAM to EEPROM
static inline void writeAllBandsToEeprom(uint16_t& addr) {
    // The size of the variable part of the Band struct
    const uint8_t band_state_size = sizeof(Band) - offsetof(Band, currentFreq);
    for (uint8_t i = 0; i <= g_lastBand; i++) {
        writeBandStateToEEPROM(addr + (i * band_state_size), g_bandList[i]);
    }
    addr += (uint16_t)(g_lastBand + 1) * band_state_size;
}

// Reads all band data from EEPROM to RAM
static inline void readAllBandsFromEeprom(uint16_t& addr) {
    // part of the Band struct
    const uint8_t band_state_size = sizeof(Band) - offsetof(Band, currentFreq);
    for (uint8_t i = 0; i <= g_lastBand; i++) {
        readBandStateFromEEPROM(addr + (i * band_state_size), g_bandList[i]);
    }
    addr += (uint16_t)(g_lastBand + 1) * band_state_size;
}

// Orchestrator for saving all receiver state to EEPROM.
static void saveAllReceiverInformation(bool full_save = true) {
    syncActiveStateToBand();

    if (!full_save && g_currentFrequency == g_lastSavedFrequency) {
        g_stateIsDirty = false;
        return;
    }

    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);
    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);

    uint16_t addr = EEPROM_DATA_START_ADDRESS;
    writeEepromHeader(addr);

    if (full_save) {
        writeAllBandsToEeprom(addr);

        // g_Settings params one-by-one to avoid struct padding issues
        for (uint8_t i = 0; i < SETTINGS_MAX; i++) {
            EEPROM.update(addr++, g_Settings[i].param);
        }

        // g_modeSettings as a block
        writeEepromBlock(addr, g_modeSettings, sizeof(g_modeSettings));

        saveFMFav();
    } else {
        const uint8_t band_state_size = sizeof(Band) - offsetof(Band, currentFreq);
        uint16_t band_addr = addr + (g_bandIndex * band_state_size);
        writeBandStateToEEPROM(band_addr, g_bandList[g_bandIndex]);
    }

    g_lastSavedFrequency = g_currentFrequency;
}

// for loading all receiver state from EEPROM
static void readAllReceiverInformation() {
    if (EEPROM.read(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID || EEPROM.read(EEPROM_VERSION_ADDRESS) != APP_VERSION) {
        oled.clear();
        oled.setFont(DEFAULT_FONT);
        oled.setCursor(0, 2);
        oled.print(F("  EEPROM RESET"));
        delay(2000);
        initializeDefaultModeSettings();
        g_totalFavorites = 0;
        saveAllReceiverInformation(true);
        loadActiveStateFromBand();
        applyBandConfiguration();
        return;
    }

    uint16_t addr = EEPROM_DATA_START_ADDRESS;

    readEepromHeader(addr);
    readAllBandsFromEeprom(addr);

    // g_Settings params one-by-one to avoid struct padding issues
    for (uint8_t i = 0; i < SETTINGS_MAX; i++) {
        g_Settings[i].param = EEPROM.read(addr++);
    }

    // check for CPU Speed must happen immediately after read
    if (g_Settings[SettingsIndex::CPUSpeed].param > 1) {
        g_Settings[SettingsIndex::CPUSpeed].param = 0;
    }

    // g_modeSettings as a block
    readEepromBlock(addr, g_modeSettings, sizeof(g_modeSettings));

    // brightness setting, which depends on a value in g_Settings
    applyBrightness();

    loadActiveStateFromBand();
    g_previousFrequency = g_currentFrequency;
    if (isSSB()) {
        loadSSBPatch();
    }
    applyBandConfiguration();

    g_lastSavedFrequency = g_currentFrequency;
}


// ------------------------------------------
// ------- Battery Monitoring Subsystem -----
// ------------------------------------------

uint8_t g_stableBatteryPercent = 100;
static uint8_t g_percentChangeCounter = 0;
static int16_t g_averageADC = -1;

// Calculates the "raw" battery percentage from an ADC value using interpolation.
// It uses the original lookup table for consistency.
static uint8_t calculateRawPercent(uint16_t adc_value) {
    constexpr const uint8_t rows = 10;
    // Original voltage table from Goshante's firmware.
    static const PROGMEM uint16_t voltages[rows] = { 643, 620, 604, 581, 573, 558, 542, 503, 496, 488 };
    static const PROGMEM uint8_t percents[rows] = { 100, 95, 90, 80, 60, 40, 20, 15, 5, 0 };

    if (adc_value >= pgm_read_word(&voltages[0])) return 100;
    if (adc_value <= pgm_read_word(&voltages[rows - 1])) return 0;

    for (uint8_t i = 0; i < rows - 1; ++i) {
        uint16_t v_upper = pgm_read_word(&voltages[i]);
        uint16_t v_lower = pgm_read_word(&voltages[i + 1]);
        if (adc_value >= v_lower && adc_value <= v_upper) {
            uint8_t p_upper = pgm_read_byte(&percents[i]);
            uint8_t p_lower = pgm_read_byte(&percents[i + 1]);
            return p_lower + ((uint32_t)(adc_value - v_lower) * (p_upper - p_lower)) / (v_upper - v_lower);
        }
    }
    return 0;
}

// Updates the internal stable battery percentage. Applies an IIR filter to ADC
// readings and uses hysteresis logic to prevent display flicker.
static void updateStablePercent() {
    if (!g_voltagePinConnnected) return;

    int sample = analogRead(BATTERY_VOLTAGE_PIN);
    if (g_averageADC == -1) {
        g_averageADC = sample > 0 ? sample : 550;
    }
    g_averageADC = (3 * g_averageADC + sample) >> 2;

    uint8_t currentRawPercent = calculateRawPercent(g_averageADC);

    // Hysteresis threshold to prevent flicker between adjacent percentage values.
    const uint8_t PERCENT_HYSTERESIS_THRESHOLD = 2;
    const uint8_t CONFIRMATION_COUNT = 5;

    int8_t diff = currentRawPercent - g_stableBatteryPercent;

    // сheck if the change is significant (outside hysteresis threshold)
    if ((diff > 0 ? diff : -diff) > PERCENT_HYSTERESIS_THRESHOLD) {
        g_stableBatteryPercent = currentRawPercent;
        g_percentChangeCounter = 0;

    } else if (diff != 0) {
        if (++g_percentChangeCounter >= CONFIRMATION_COUNT) {
            g_stableBatteryPercent = currentRawPercent;
            g_percentChangeCounter = 0;
        }
    } else {
        g_percentChangeCounter = 0;
    }
}

// Public interface for the battery monitoring subsystem. Updates the internal
// state and shows it on the display if the timer has elapsed or if forced.
void updateAndShowBattery(bool forceShow) {
    if (!g_voltagePinConnnected) return;

    updateStablePercent();

    static uint32_t lastChargeShow = 0;

    if ((millis() - lastChargeShow) > 10000 || forceShow) {
        showChargeOnDisplay();
        lastChargeShow = millis();
    }
}

// ----------------------------------------------
// ---- Hardware & Receiver Control Subsystem ---
// ---- Sensitive logic is here -----------------
// ----------------------------------------------

// off amplifier md8002a
static void safeAmpOff() {
    AMP_DDR |= (1 << AMP_BIT);   // OUTPUT
    AMP_PORT |= (1 << AMP_BIT);  // HIGH
}

// on amplifier md8002a
static void safeAmpOn() {
    AMP_PORT &= ~(1 << AMP_BIT); // LOW
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
    safeAmpOff();

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

    safeAmpOn();
}

// Handles the low-level interaction with the Si4735 chip to perform a seek
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(30);

    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        g_si4735.setSeekAmLimits(150, 30000);
    }

    g_seekStop = false;
    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);

    delay(50);
    return g_si4735.getFrequency();
}

// Set up FM radio parameters including frequency limits, bandwidth, and de-emphasis
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
    g_si4735.setProperty(0x1404, 9);  // FM_SEEK_TUNE_RSSI_THRESHOLD (Default: 20)

    g_ssbLoaded = false;
    g_si4735.setFifoCount(1);
    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[DeEmp].param == 0) ? 1 : 2);
}

// Corrected CW BFO offset logic to match standard radio behavior
// The Si4735 IC requires an inverted BFO value, so the math is reversed here to compensate
// To get a positive BFO offset for USB, the value must be negative before the final inversion
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static void updateBFO() {

    int16_t finalBfo = g_currentBFO + (g_Settings[BFO].param * 100);

    if (g_currentMode == CW) {
        if (g_Settings[CWSwitch].param == 1) { // 1 = USB
            finalBfo -= CW_PITCH_OFFSET_HZ;
        } else { // 0 = LSB
            finalBfo += CW_PITCH_OFFSET_HZ;
        }
    }

    g_si4735.setSSBBfo(finalBfo * -1);
}

// Initialize SSB mode with patch loading, BFO setup, filters, and audio bandwidth configuration
static void configureSSBMode(uint16_t minFreq, uint16_t maxFreq, bool extraSSBReset) {
    Band& current_band = g_bandList[g_bandIndex];

    if (current_band.bwIdxSSB >= g_bwSSBMaxIdx)
        current_band.bwIdxSSB = 4;

    g_currentBFO = 0;
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);

    g_si4735.setSSB(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? (g_Settings[CWSwitch].param + 1) : g_currentMode);

    updateSSBCutoffFilter();
    g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
    g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);

    // Use SoftMute setting from storage for SSB
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_SSB]);

    // Use bandwidth index from the current band state
    g_si4735.setSSBAudioBandwidth((g_currentMode == CW) ? g_bwSSBIdx[0] : g_bwSSBIdx[current_band.bwIdxSSB]);
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

// Switch to AM mode and configure bandwidth, soft mute, and frequency parameters
static void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setAM(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        g_tabStep[current_band.stepIdxAM]);

    // Use SoftMute setting from storage for AM
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_AM]);
    g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);
}

// Set AGC, AVC gain, and seek parameters shared between AM and SSB modes
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    // Apply AVC MAX GAIN from storage for the current mode
    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);
    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    // Set custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(AM_SEEK_SNR_THRESHOLD, 2);     // AM_SEEK_TUNE_SNR_THRESHOLD (Default: 5)
    g_si4735.setProperty(AM_SEEK_RSSI_THRESHOLD, 12);   // AM_SEEK_TUNE_RSSI_THRESHOLD (Default: 25)
}

// AGC hardware control
static inline void setAgcHardware(int8_t att_val) {
    bool disableAgc = att_val > 0;
    // attenuation index for the chip is one less than the parameter valu
    // if att_val is 0 (auto) or 1 (manual, 0dB), the index sent to the chip is 0
    uint8_t agcNdx = (att_val > 1) ? (att_val - 1) : 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

// Applies AGC settings based on current mode and stored values
static void applyAgcSettings() {
    ModeContext modeCtx = getModeContext();
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];

    setAgcHardware(att_val);
}

// Main band switching logic that coordinates mode transitions and amplifier control
void applyBandConfiguration(bool extraSSBReset) {
    bool prevWasFM = (g_currentMode == FM);
    bool nextIsFM = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    bool nextIsPureAM = !nextIsFM && !g_ssbLoaded;
    bool switchingBetweenFMandAM = (prevWasFM && nextIsPureAM) || (!prevWasFM && nextIsFM);

    if (switchingBetweenFMandAM)
        safeAmpOff();

    loadActiveStateFromBand();

    // g_signalQualityValue = 255; // keeping the old value on screen temporarily
    g_forceRssiUpdate = true;

    uint8_t cap_value = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);

    if (nextIsFM) {
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

    resetEepromDelay();

    if (switchingBetweenFMandAM)
        safeAmpOn();
}

// ------------------------------------------
// ------- UI: Main Screen Drawing ----------
// ------------------------------------------

// helper for brightness calculating the value by using integer (low flash consume)
// on edit have a white display, so don`t know what it will look like for you
static void applyBrightness() {
    uint8_t s = g_Settings[Brightness].param;

    // f(s) = 0.75*s^2 + 2.0*s. coefficients are scaled by 256 here
    uint8_t contrast_value = ((uint32_t)s * ((uint16_t)s * 192 + 512)) >> 8;
    // add the base value of 1 to map to the final contrast range [1, 80]
    oled.setContrast(contrast_value + 1);
}

// Startup screen
void showSplashScreen() {
    oled.clear();
    oled.setFont(DEFAULT_FONT);

    oled.setCursor(26, 1);
    oled.print(F("ATS-20+ v4.5"));

    oled.setCursor(32, 3);
    oled.print(F("Mod No RDS"));

    for (int i = 0; i < 21; i++) {
        oled.setCursor(i * 6, 6);
        oled.print('-');
        delay(70);
    }

    delay(2000);
    oled.clear();
}

//Draw frequency.
static void showFrequency(bool cleanDisplay = false) {
    if (g_settingsActive)
        return;

    char freqDisplay[7];
    static uint8_t prevLen = 0;
    uint16_t khzBFO, tailBFO;
    bool ssbMode = isSSB();
    uint8_t off = (ssbMode ? -5 : 4) + 8;
    const char* unit = "kHz";
    uint8_t displayMode = 0;
    BandType currentBandType = g_bandList[g_bandIndex].bandType;

    if (ssbMode) {
        displayMode = 2;
    } else if (currentBandType == FM_BAND_TYPE) {
        displayMode = 1;
    }

    switch (displayMode) {
    case 2: // SSB
        splitFreq(khzBFO, tailBFO);
        convertToChar(freqDisplay, khzBFO, ilen(khzBFO), 0, '.', ' ');
        break;

    case 1: // FM
        convertToChar(freqDisplay, g_currentFrequency, 5, 3, '.', '/');
        unit = "MHz";
        break;

    case 0: // AM-like modes SW, MW, LW
    default:
        uint8_t dot_pos = 0;

        // applies MHz format ONLY to the SW band
        // MW/LW, this check is always false, leaving dot_pos at 0
        if (currentBandType == SW_BAND_TYPE && g_Settings[SettingsIndex::SWUnits].param == 1) {
            dot_pos = 2;
            unit = "MHz";
        }
        convertToChar(freqDisplay, g_currentFrequency, 5, dot_pos, '.', '/');
        break;
    }

    uint8_t len = ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);

    if (cleanDisplay) {
        oledPrint("/////////", 0, 3, FONT14X24SEVENSEG);
    } else if (ssbMode && len > prevLen && len == 5) 
        oledPrint("   ", 102, 4, DEFAULT_FONT);
    
    oledPrint(freqDisplay, off, 3, FONT14X24SEVENSEG);

    if (ssbMode) {
        oled.print('.');
        if (tailBFO < 10)
            oled.print('0');
        oled.print(tailBFO);

        if (len != prevLen && len < prevLen)
            oledPrint("/");
    }

    if (g_Settings[SettingsIndex::UnitsSwitch].param == 1 && (!ssbMode || len < 5))
        oledPrint(unit, 102, 4, DEFAULT_FONT);

    prevLen = len;
}

//This function is called by station seek logic
static void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;

    if (g_currentMode == FM) {
        g_currentFrequency = (freq / 10) * 10;
    }
    showFrequency();
}

//Draw current band tag (e.g., "40m")
static void showBandTag() {
    if (g_settingsActive) return;

    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    oledPrint(name_buffer, 0, 0, DEFAULT_FONT, invert);
}

//Draw current modulation (AM/LSB/USB/CW/FM) and stereo indicator
static void showModulation() {
    oledPrint(g_bandModeDesc[g_currentMode], 0, 6, DEFAULT_FONT,
        g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);

    oled.print(' ');
    updateStereoIndicator();
    showBandTag();
}

//Draw volume level or mute status
static void showVolume() {
    if (g_settingsActive)
        return;

    char buf[3];
    if (g_muteVolume == 0)
        convertToChar(buf, g_si4735.getCurrentVolume(), 2, 0, 0);
    else {
        buf[0] = ' '; buf[1] = 'M'; buf[2] = 0;
    }
    oledPrint(buf, (128 - (8 * 2) + 2 - 6), 0, DEFAULT_FONT, g_activeCommand == CMD_VOLUME);
}

// Displays the current signal quality value (RSSI)
static void showSignalQuality() {
    if (g_settingsActive || g_favoritesActive) return;
    oled.setCursor(78, 6);
    if (g_signalQualityValue == 255) {
        oled.print("   ");
    } else {
        if (g_signalQualityValue < 10) oled.print(' ');
        oled.print(g_signalQualityValue);
        oled.print('|');
    }
}

// Renders the stable battery percentage value on the display.
static void showChargeOnDisplay() {
    if (g_settingsActive) return;

    oled.setCursor(102, 6);
    oled.setFont(DEFAULT_FONT);

    if (g_stableBatteryPercent >= 100) {
        oled.print("100");
    } else {
        // Add padding for single-digit numbers to maintain alignment
        if (g_stableBatteryPercent < 10) oled.print(' ');
        oled.print(g_stableBatteryPercent);
        oled.print('%');
    }
}

// displays the current tuning step
static void showStep() {
    char buf[5];
    int stepValue;
    bool isKhz = true;

    const Band& current_band = g_bandList[g_bandIndex];

    if (g_currentMode == FM) {
        stepValue = (g_tabStepFM[current_band.stepIdxFM] == 100) ? 1000 : g_tabStepFM[current_band.stepIdxFM] * 10;
    } else if (isSSB()) {
        stepValue = g_tabStep[SSB_STEP_OFFSET + current_band.stepIdxSSB];
        isKhz = false;
    } else { // AM
        stepValue = g_tabStep[current_band.stepIdxAM];
    }

    formatStepValue(buf, stepValue, isKhz);

    oledSetFont(DEFAULT_FONT);
    bool invert = (g_activeCommand == CMD_STEP);
    if (invert) oled.invertOutput(true);

    oled.setCursor(34, 0);
    oled.print("Step:");
    oled.print(buf);

    if (invert) oled.invertOutput(false);
}

// displays the current bandwidth
static void showBandwidth() {
    char bw[5];
    bw[4] = '\0';
    const Band& current_band = g_bandList[g_bandIndex];
    const uint8_t* table_ptr = nullptr;
    uint8_t index;
    switch (g_currentMode) {
    case LSB:
    case USB:
        table_ptr = bw_ssb_map;
        index = current_band.bwIdxSSB;
        break;
    case AM:
        table_ptr = bw_am_map;
        index = current_band.bwIdxAM;
        break;
    case FM:
        table_ptr = bw_fm_map;
        index = current_band.bwIdxFM;
        break;
    }

    if (table_ptr) {
        uint8_t offset = pgm_read_byte(&table_ptr[index]);
        for (uint8_t i = 0; i < 4; i++) {
            bw[i] = pgm_read_byte(&bw_all_data[offset + i]);
        }
    } else { // for CW
        bw[0] = '\0';
    }
    oledPrint(bw, 40, 6, DEFAULT_FONT, g_activeCommand == CMD_BW);
}

void updateStereoIndicator() {
    char c = (isSSB() && g_Settings[SettingsIndex::Sync].param == 1) ? 'S' :
        ((g_currentMode == FM && g_stereoStatus) ? '*' : ' ');

    oled.setCursor(24, 6);
    oled.print(c);
}

// Orchestrator for drawing the main status screen
void showStatus(bool cleanFreq) {
    showFrequency(cleanFreq);
    showModulation();
    showStep();
    showBandwidth();
    updateAndShowBattery(true);
    showVolume();
    showSignalQuality();
}

// ------------------------------------------
// --- UI: Favorites Menu Drawing -----------
// ------------------------------------------

// Draws a single item in the favorites list, called by showFav
static inline void drawFavItem(uint8_t index, uint8_t y_pos, bool selected) {
    oled.setCursor(0, y_pos);
    oled.print(selected ? '>' : ' ');
    oled.print('0');
    oled.print(index + 1);
    oled.print(':');

    uint16_t f_copy = g_fmFavorites[index].frequency;
    uint8_t megahertz = sw_div(f_copy, 100);
    if (megahertz < 100) oled.print(' ');
    if (megahertz < 10) oled.print(' ');
    oled.print(megahertz);
    oled.print('.');
    uint8_t first_decimal = sw_div(f_copy, 10);
    oled.print(first_decimal);
    oled.print(F(" MHz  "));
}

// Display favorites menu
static void showFav() {
    oled.setCursor(0, 0);
    oled.invertOutput(true);
    oled.print(F("  FM FAVORITES  "));
    oled.invertOutput(false);

    if (!g_totalFavorites) {
        oled.setCursor(30, 3);
        oled.print(F("NO SAVED"));
        return;
    }

    uint8_t start = (g_favoriteSelected >> 1) << 1;
    uint8_t end = start + 2;
    if (end > g_totalFavorites) end = g_totalFavorites;

    for (uint8_t i = start; i < end; i++) {
        drawFavItem(i, 2 + ((i - start) << 1), i == g_favoriteSelected);
    }

    if (end == start + 1) {
        oled.setCursor(0, 4);
        for (uint8_t j = 16; j; j--) oled.print(' ');
    }

    oled.setCursor(0, 6);
    oled.print(' ');
    oled.print(g_favoriteSelected + 1);
    oled.print('/');
    oled.print(g_totalFavorites);
    oled.print(F("DEL:BW"));
}

// ------------------------------------------
// --- UI: Settings Menu Drawing ------------
// ------------------------------------------

// Converts setting parameter value to UI display string
static void SettingParamToUI(char* buf, uint8_t idx) {
    int8_t param = g_Settings[idx].param;
    uint8_t type = g_Settings[idx].type;
    uint8_t textIdx;

    if (type == SettingType::Switch || type == SettingType::SwitchAuto) {
        if (type == SettingType::SwitchAuto) {
            textIdx = param;
        } else {
            uint8_t base = pgm_read_byte(&switch_setting_map[idx].baseIndex);
            bool inv = pgm_read_byte(&switch_setting_map[idx].inverted);
            textIdx = inv ? (base - param) : (base + param);
        }
        strcpy_P(buf, paramTexts[textIdx]);
        return;
    }

    if (type == SettingType::ZeroAuto && param == 0) {
        strcpy_P(buf, paramTexts[0]); // "AUT"
    } else {
        uint8_t val_to_convert = abs(param);
        if (idx == SettingsIndex::Brightness) {
            val_to_convert += 1;
        }

        convertToChar(buf, val_to_convert, 3);
        if (param < 0) {
            buf[0] = '-';
        }
        buf[3] = '\0';
    }
}

// Draw a single setting item in the settings menu
static void DrawSetting(uint8_t idx, bool full) {
    if (!g_settingsActive)
        return;

    char buf[5];
    uint8_t place = idx - ((g_SettingsPage - 1) * 6);
    uint8_t yOffset = place > 2 ? (place - 3) * 2 : place * 2;
    uint8_t xOffset = place > 2 ? 60 : 0;
    if (full)
        oledPrint(g_Settings[idx].name, 5 + xOffset, 2 + yOffset, DEFAULT_FONT, idx == g_SettingSelected && !g_SettingEditing);
    SettingParamToUI(buf, idx);
    oledPrint(buf, 35 + xOffset, 2 + yOffset, DEFAULT_FONT, idx == g_SettingSelected && g_SettingEditing);
}

// Draw the title of the settings menu
static void showSettingsTitle() {
    oledPrint("   SETTINGS  ", 0, 0, DEFAULT_FONT, true);
    oled.invertOutput(true);
    oled.print(uint8_t(g_SettingsPage));
    oled.print("/");
    oled.print(uint8_t(g_SettingsMaxPages));
    oled.invertOutput(false);
}

// Draw the complete settings screen (all visible items)
static void showSettings() {
    for (uint8_t i = 0; i < 6 && i + ((g_SettingsPage - 1) * 6) < SettingsIndex::SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * 6), true);
}

// ------------------------------------------
// --- State & Band Management Subsystem --==
// ------------------------------------------

// switches band index and immediately applies the new band's default state
static void bandSwitch(bool up) {
    syncActiveStateToBand(); // Save current frequency to RAM
    markStateAsDirty();

    uint8_t oldBandIndex = g_bandIndex;
    g_currentBFO = 0;

    if (up) {
        g_bandIndex = (g_bandIndex + 1) % g_bandCount;
    } else {
        g_bandIndex = (g_bandIndex == 0) ? (g_bandCount - 1) : (g_bandIndex - 1);
    }

    loadActiveStateFromBand();

    g_lastSavedFrequency = g_currentFrequency;

    BandType oldType = g_bandList[oldBandIndex].bandType;
    BandType newType = g_bandList[g_bandIndex].bandType;

    if (oldType != FM_BAND_TYPE && newType != FM_BAND_TYPE) {
        // fast for seamless transitions within AM/SW bands
        g_si4735.setFrequency(g_currentFrequency);

        // clear at SW<->MW/LW transition if MHz mode is enabled
        bool clean = g_Settings[SettingsIndex::SWUnits].param == 1 &&
            ((oldType == SW_BAND_TYPE) != (newType == SW_BAND_TYPE));

        showFrequency(clean);
        showBandTag();
        showStep();
        showBandwidth();
    } else {
        // long for major mode changes (like to/from FM - in AM/LW/MW (SSB too)
        applyBandConfiguration();
    }
}

// Manages the seek process and updates the application state.
static void doSeek() {
    g_currentFrequency = executeHardwareSeek();

    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        for (uint8_t i = 0; i < g_bandCount; i++) {
            if (g_bandList[i].bandType != FM_BAND_TYPE &&
                g_currentFrequency >= g_bandList[i].minimumFreq &&
                g_currentFrequency <= g_bandList[i].maximumFreq) {
                g_bandIndex = i;
                break;
            }
        }
        const Band& new_band = g_bandList[g_bandIndex];
        g_si4735.setSeekAmLimits(new_band.minimumFreq, new_band.maximumFreq);
    } else { // FM band
        // round down the frequency to the nearest 10khz step (e.g. 102.57 -> 102.50)
        uint16_t rounded = g_currentFrequency % 10;
        if (rounded != 0) {
            g_currentFrequency -= rounded;
            g_si4735.setFrequency(g_currentFrequency);
        }
    }

    syncActiveStateToBand();
    showStatus(true);
    resetEepromDelay();
}

// handles frequency tuning for am/fm
static void doFrequencyTune() {
    g_seekDirection = g_encoderCount > 0;
    Band& current_band = g_bandList[g_bandIndex];
    uint16_t step;

    if (current_band.bandType == FM_BAND_TYPE) {
        step = g_tabStepFM[current_band.stepIdxFM];
    } else { // am
        step = g_tabStep[current_band.stepIdxAM];
    }

    // 32 integer need here for calculations to prevent underflow on band edges!
    int32_t temp_freq = g_currentFrequency;
    temp_freq += (int16_t)step * g_encoderCount;
    g_encoderCount = 0;

    // calculate then check
    if (temp_freq > current_band.maximumFreq) {
        bandSwitch(true);
        return;
    }
    if (temp_freq < current_band.minimumFreq) {
        bandSwitch(false);
        return;
    }

    // if safe, commit the new frequency and align it to the grid (snap)
    g_currentFrequency = (uint16_t)temp_freq;
    uint16_t remainder = g_currentFrequency % step;
    if (remainder != 0) {
        if (g_seekDirection) {
            g_currentFrequency += (step - remainder);
        } else {
            g_currentFrequency -= remainder;
        }
    }

    g_processFreqChange = true;
    g_lastFreqChange = millis();
    showFrequency();
    syncActiveStateToBand();
    markStateAsDirty();
}

// handles ssb tuning using the definitive "atomic step with integrated checks" architecture
static void doFrequencyTuneSSB() {
    if (g_encoderCount == 0) return;

    // store frequency before changes to detect a rollover event
    uint16_t old_freq = g_currentFrequency;
    uint16_t temp_freq = g_currentFrequency;

    // 32-bit integer to prevent overflow during fast encoder spins
    int32_t temp_bfo = g_currentBFO;

    temp_bfo += (int32_t)g_tabStep[SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB] * g_encoderCount;
    g_encoderCount = 0;

    if (performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo)) {
        return;
    }

    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    // if the base frequency changed, the chip must be updated
    // this is critical fix!
    if (g_currentFrequency != old_freq) {
        g_si4735.setFrequency(g_currentFrequency);
        applyAgcSettings();
    }

    updateBFO();
    syncActiveStateToBand();
    g_lastFreqChange = millis();
    g_previousFrequency = 0;
    showFrequency();
    markStateAsDirty();
}

// handles the complex logic of cycling through AM, LSB, USB, and CW modes
static inline void cycleAmSsbCwModes() {
    Band& current_band = g_bandList[g_bandIndex];
    // store the bandwidth index to carry it over between am/ssb
    int8_t bw = (g_currentMode == AM) ? current_band.bwIdxAM : current_band.bwIdxSSB;
    syncActiveStateToBand();

    //saveAllReceiverInformation(false); // Save the state of the departing mode to EEPROM
    markStateAsDirty();

    if (g_currentMode == CW) safeAmpOff();

    // when switching from am to ssb for the first time, the patch must be loaded
    if (g_currentMode == AM) {
        loadSSBPatch();
        current_band.bwIdxSSB = bw;
        g_processFreqChange = false;
    }

    // cycle through am -> lsb -> usb -> cw -> am
    g_currentMode = (g_currentMode + 1) % 4;

    if (g_currentMode == AM) {
        g_ssbLoaded = false;
        current_band.bwIdxAM = bw;
    }

    applyBandConfiguration();

    if (!g_ssbLoaded && g_currentMode == AM) safeAmpOn();
}

// ------------------------------------------
// --- Settings & Parameter Subsystem -------
// ------------------------------------------

static void switchSettingsPage() {
    g_SettingsPage++;
    g_SettingsPage = (g_SettingsPage > g_SettingsMaxPages) ? 1 : g_SettingsPage;
    g_SettingSelected = 6 * (g_SettingsPage - 1);
    g_SettingEditing = false;
    oled.clear();
    showSettingsTitle();
    showSettings();
}

// Helper to sync settings between the g_Settings buffer and g_modeSettings storage.
static inline void syncModeDependentSettings(bool load) {
    ModeContext modeCtx = getModeContext();
    if (load) {
        g_Settings[ATT].param = g_modeSettings[MODE_SETTING_AGC][modeCtx];
        g_Settings[SoftMute].param = g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx];
        g_Settings[AutoVolControl].param = g_modeSettings[MODE_SETTING_AVC][modeCtx];
    } else {
        g_modeSettings[MODE_SETTING_AGC][modeCtx] = g_Settings[ATT].param;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx] = g_Settings[SoftMute].param;
        g_modeSettings[MODE_SETTING_AVC][modeCtx] = g_Settings[AutoVolControl].param;
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

// Add current frequency to RAM and set dirty flag
static void addFav() {
    if (g_totalFavorites >= MAX_FM_FAVORITES) return;
    for (uint8_t i = 0; i < g_totalFavorites; i++)
        if (g_fmFavorites[i].frequency == g_currentFrequency) return;

    g_fmFavorites[g_totalFavorites++].frequency = g_currentFrequency;
    g_favoritesDirty = true;
}

// Delete selected favorite from RAM and set dirty flag
static void delFav() {
    if (!g_totalFavorites) return;

    for (uint8_t i = g_favoriteSelected; i < g_totalFavorites - 1; i++)
        g_fmFavorites[i] = g_fmFavorites[i + 1];

    g_totalFavorites--;
    if (g_totalFavorites && g_favoriteSelected >= g_totalFavorites)
        g_favoriteSelected = g_totalFavorites - 1;

    g_favoritesDirty = true;
}

// handles tuning step adjustment, updates the current band's state, and applies it to the chip
static void doStep(int8_t v) {
    Band& current_band = g_bandList[g_bandIndex];

    if (g_currentMode == FM) {
        doSwitchLogic(current_band.stepIdxFM, 0, g_lastStepFM, v);
        g_si4735.setFrequencyStep(g_tabStepFM[current_band.stepIdxFM]);
        g_si4735.setSeekFmSpacing(10);
    } else if (isSSB()) {
        doSwitchLogic(current_band.stepIdxSSB, 0, SSB_STEPS_COUNT - 1, v);
    } else { // AM
        const uint8_t max_am_idx = (current_band.bandType == LW_BAND_TYPE || current_band.bandType == MW_BAND_TYPE) ? 3 : (AM_STEPS_COUNT - 1);
        doSwitchLogic(current_band.stepIdxAM, 0, max_am_idx, v);
        g_si4735.setFrequencyStep(g_tabStep[current_band.stepIdxAM]);
        g_si4735.setSeekAmSpacing(g_tabStep[current_band.stepIdxAM]);
    }
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

//Settings: Attenuation
void doAttenuation(int8_t v) {
    uint8_t max_att_value = (g_currentMode == FM) ? 26 : 37;
    doSwitchLogic(g_Settings[ATT].param, 0, max_att_value, v);

    setAgcHardware(g_Settings[ATT].param);
}

//Settings: Soft Mute
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SoftMute].param, 0, 32, v);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SoftMute].param);
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

//Settings: Automatic Volume Control
void doAvc(int8_t v) {
    doSwitchLogic(g_Settings[AutoVolControl].param, 12, 90, v);

    if (g_currentMode != FM)
        g_si4735.setAvcAmMaxGain(g_Settings[AutoVolControl].param);
}

//Settings: Sync switch
void doSync(int8_t v) {
    bool wasSettingsActive = g_settingsActive;
    toggleSetting(Sync);
    if (isSSB()) {
        g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);
        applyBandConfiguration(true);
        if (wasSettingsActive) {
            showSettingsTitle();
            showSettings();
        }
    }
}

//Settings: FM DeEmp switch (50 or 75)
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

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v = 0) {
    toggleSetting(CPUSpeed);
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[CPUSpeed].param;
    interrupts();
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

//Settings: Tune Frequency Antenna Capacitor
void doUnitsSwitch(int8_t v) {
    toggleSetting(UnitsSwitch);
}

//Settings: Scan button switch
void doScanSwitch(int8_t v) {
    toggleSetting(ScanSwitch);
}

//Settings: CW sideband mode switch (LSB/USB)
// provides a seamless sideband switch by calculating the required
// frequency shift to keep the audible CW tone stable
void doCWSwitch(int8_t v) {
    constexpr int16_t COMPENSATION_KHZ = (2 * CW_PITCH_OFFSET_HZ) / 1000; // 1 kHz if CW_PITCH_OFFSET_HZ = 500
    const int8_t old_param = g_Settings[CWSwitch].param;

    toggleSetting(CWSwitch);
    if (g_currentMode != CW) return;

    const int8_t actual_direction = g_Settings[CWSwitch].param - old_param;
    if (actual_direction == 0) return;

    g_currentFrequency += actual_direction * COMPENSATION_KHZ;
    g_si4735.setFrequency(g_currentFrequency);
    updateBFO();
    showFrequency(true);
}

//Settings: Auto Antenna Capacitor
void doAntennaCapacitor(int8_t v) {
    toggleSetting(AntennaCap);
}

//Settings: RSSI AM Off switch
void doRSSIAMOff(int8_t v) {
    toggleSetting(RSSI_AM_Off);
}

// handles bandwidth adjustment and updates the current band's state
static void doBandwidth(uint8_t v) {
    Band& current_band = g_bandList[g_bandIndex];

    if (isSSB()) {
        doSwitchLogic(current_band.bwIdxSSB, 0, sizeof(bw_ssb_map) - 1, v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[current_band.bwIdxSSB]);
        updateSSBCutoffFilter();
    } else if (g_currentMode == AM) {
        doSwitchLogic(current_band.bwIdxAM, 0, sizeof(bw_am_map) - 1, v);
        g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);
    } else { // FM
        doSwitchLogic(current_band.bwIdxFM, 0, sizeof(bw_fm_map) - 1, -v);
        g_si4735.setFmBandwidth(current_band.bwIdxFM);
    }
    showBandwidth();
}

// ------------------------------------------
// --- Input: Button & Event Handling -----
// ------------------------------------------

// Handle encoder direction (ISR context)
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        g_encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
        g_seekStop = true;
    }
}

// Safely reads the accumulated encoder value from the interrupt context.
static void updateEncoderState() {
    if (g_encoderCount) {
        noInterrupts();
        g_safeEncoderMovement += g_encoderCount;
        g_encoderCount = 0;
        interrupts();
    }
}

static uint8_t volumeEvent(uint8_t event, uint8_t pin) {
    if (g_muteVolume) {
        if (!BUTTONEVENT_ISDONE(event)) {
            if ((BUTTONEVENT_SHORTPRESS != event) || (VOLUME_BUTTON == pin))
                doVolume(1);
        }
    }

    if (!g_muteVolume) {
#if (0 != VOLUME_DELAY)
#if (VOLUME_DELAY > 1)
        static uint8_t count;
        if (BUTTONEVENT_FIRSTLONGPRESS == event) {
            count = 0;
        }
#endif
        if (BUTTONEVENT_ISLONGPRESS(event))
            if (BUTTONEVENT_LONGPRESSDONE != event) {
#if (VOLUME_DELAY > 1)
                if (count++ == 0)
#endif
                    doVolume(VOLUME_BUTTON == pin ? 1 : -1);
#if (VOLUME_DELAY > 1)
                count = count % VOLUME_DELAY;
#endif
            }
#else
        if (BUTTONEVENT_FIRSTLONGPRESS == event)
            event = BUTTONEVENT_SHORTPRESS;
#endif
    }
    return event;
}

static uint8_t simpleEvent(uint8_t event, uint8_t pin) {
    // If the event is from the Mode button in FM mode, pass it through unmodified.
    if (pin == MODE_SWITCH && g_currentMode == FM) {
        return event;
    }

    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
    return event;
}

// This function handles the button events for band switching.
static uint8_t bandEvent(uint8_t event, uint8_t pin) {
#if (0 != BAND_DELAY)
    static uint8_t count;
    if (BUTTONEVENT_ISLONGPRESS(event) && !g_settingsActive) {
        if (BUTTONEVENT_LONGPRESSDONE != event) {
            if (BUTTONEVENT_FIRSTLONGPRESS == event) {
                count = 0;
            }
            // Process only for the designated band buttons
            if ((pin == BAND_BUTTON || pin == SOFTMUTE_BUTTON) && count++ == 0) {
                bandSwitch(pin == BAND_BUTTON);
            }
            count = count % BAND_DELAY;
        }
    }
#else
    // This logic handles the case where long press is disabled in defs.h
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
#endif
    return event;
}

// helper function for processbuttonevents, handles encoder button presses
static inline void handleEncoderButton() {
    uint8_t evt = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_activeCommand != CMD_NONE) {
        resetCommandMode();
        return;
    }

    if (g_settingsActive) {
        g_SettingEditing = !g_SettingEditing;
        DrawSetting(g_SettingSelected, true);
        return;
    }

    (isSSB() || !g_Settings[ScanSwitch].param) ? switchCommand(CMD_STEP) : doSeek();
}

static inline void handleBandwidthButton() {
    uint8_t evt = btn_Bandwidth.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (!g_settingsActive && g_currentMode != CW) {
        switchCommand(CMD_BW);
    }
}

static inline void handleBandUpButton() {
    uint8_t evt = btn_BandUp.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_settingsActive) {
        switchSettingsPage();
    } else {
        switchCommand(CMD_BAND);
    }
}

static inline void handleBandDownButton() {
    uint8_t evt = btn_BandDn.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    resetCommandMode();
    g_settingsActive = !g_settingsActive;
    switchSettings();
}

static inline void handleVolumeUpButton() {
    // volume up button handler
    uint8_t evt = btn_VolumeUp.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive && !g_muteVolume) {
        switchCommand(CMD_VOLUME);
    }
}

static inline void handleVolumeDownButton() {
    // volume down button (mute) handler
    uint8_t evt = btn_VolumeDn.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_favoritesActive && g_activeCommand != CMD_VOLUME) {
        // toggles mute, saves the current volume to restore it later
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

// AGC button press for display and CPU speed toggle
static inline void handleAgcButton() {
    uint8_t evt = btn_AGC.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    // not allow toggling OFF while in settings menu
    if (g_settingsActive && g_displayOn) return;

    g_displayOn = !g_displayOn;
    uint8_t new_prescaler = g_displayOn ? g_Settings[SettingsIndex::CPUSpeed].param : 1;

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = new_prescaler;
    interrupts();

    g_displayOn ? oled.on() : oled.off();
}

static inline void handleStepButton() {
    // step button handler
    uint8_t evt = btn_Step.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive) {
        switchCommand(CMD_STEP);
    }
}

static inline void processModeButtonShortPress() {
    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
        g_favoritesActive = true;
        g_favoriteSelected = 0;
        oled.clear();
        showFav();
        return;
    }

    cycleAmSsbCwModes();
}

// handles mode button presses, dispatching tasks based on the current context
static inline void handleModeButton() {
    uint8_t evt = btn_Mode.checkEvent(simpleEvent);
    if (g_settingsActive) return;

    if (BUTTONEVENT_SHORTPRESS == evt)
        processModeButtonShortPress();

    else if (BUTTONEVENT_LONGPRESSDONE == evt &&
        g_bandList[g_bandIndex].bandType == FM_BAND_TYPE && !g_favoritesActive) {
        // long press on FM adds the current station to favorites
        addFav();
        oled.setCursor(45, 3);
        oled.print(F("SAVED"));
        delay(500);
        showFrequency(true);
    }
}

// exit the favorites menu and return to the main status screen
static inline void exitFavoritesMenu() {
    if (g_favoritesDirty) {
        saveFMFav();
        g_favoritesDirty = false;
    }

    g_favoritesActive = false;
    oled.clear();
    showStatus();
}

// Centralized button handler for the Favorites menu
static void handleFavoritesMenuButtons() {
    if (BUTTONEVENT_SHORTPRESS == btn_Encoder.checkEvent(simpleEvent)) {
        if (g_totalFavorites) {
            g_currentFrequency = g_fmFavorites[g_favoriteSelected].frequency;
            g_si4735.setFrequency(g_currentFrequency);
        }
        exitFavoritesMenu();
        return;
    }

    if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(simpleEvent)) {
        delFav();
        oled.clear();
        showFav();
        return;
    }

    if ((BUTTONEVENT_SHORTPRESS == btn_BandUp.checkEvent(bandEvent)) ||
        (BUTTONEVENT_SHORTPRESS == btn_BandDn.checkEvent(bandEvent)) ||
        (BUTTONEVENT_SHORTPRESS == btn_Mode.checkEvent(simpleEvent))) {
        exitFavoritesMenu();
    }
}

// key process for all keys. Acts as a dispatcher based on the current UI mode
static void processButtonEvents() {

    // process buttons for FM favorites
    if (g_favoritesActive) {
        handleFavoritesMenuButtons();
        return;
    }

    // process buttons for main screen / settings menu
    handleEncoderButton();
    handleBandwidthButton();
    handleBandUpButton();
    handleBandDownButton();
    handleVolumeUpButton();
    handleVolumeDownButton();
    handleAgcButton();
    handleStepButton();
    handleModeButton();
}

// ------------------------------------------
// --- Input: Encoder & Command Logic -----
// ------------------------------------------

// helper to refresh all command indicators on screen
static void refreshCommandIndicators() {
    showVolume();
    showStep();
    showBandwidth();
    showModulation();
}

// switch command modes
static void switchCommand(CommandMode mode) {
    if (g_activeCommand != mode) {
        g_activeCommand = mode;
        g_lastAdjustmentTime = millis();
    } else { // Pressing the same button again to deactivate
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
    }

    refreshCommandIndicators();
}

//  helper to reset any active command mode
static void resetCommandMode() {
    if (g_activeCommand != CMD_NONE) {
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
        refreshCommandIndicators();
    }
}

// Handles ONLY encoder input for the Favorites menu
static void handleFavoritesMenu() {
    if (g_safeEncoderMovement) {
        if (g_totalFavorites > 0) {
            g_favoriteSelected = (g_favoriteSelected + g_safeEncoderMovement + g_totalFavorites) % g_totalFavorites;
            showFav();
        }
        g_safeEncoderMovement = 0;
    }
}

// handles encoder movement within the settings menu
static inline void processEncoderForSettings(int encoder_delta) {
    if (!g_SettingEditing) {
        int8_t prev = g_SettingSelected;
        g_SettingSelected += encoder_delta;
        uint8_t page = g_SettingsPage - 1;

        // for flash savings
        uint8_t a = (page * 6) + 5;
        uint8_t b = SettingsIndex::SETTINGS_MAX - 1;
        uint8_t max = (a < b) ? a : b;

        if (g_SettingSelected < page * 6) g_SettingSelected = max;
        else if (g_SettingSelected > max) g_SettingSelected = page * 6;

        DrawSetting(prev, true);
        DrawSetting(g_SettingSelected, true);
    } else {
        (*g_Settings[g_SettingSelected].manipulateCallback)(encoder_delta);
        DrawSetting(g_SettingSelected, false);
        delay(MIN_ELAPSED_TIME);
    }
}

// handles encoder movement for main screen commands
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
        g_safeEncoderMovement = 0;
        g_encoderCount = 0;
        return true;
    case CMD_NONE:
        g_encoderCount = encoder_delta;
        if (isSSB()) {
            doFrequencyTuneSSB();
        } else {
            doFrequencyTune();
        }
        g_safeEncoderMovement = 0;
        g_encoderCount = 0;
        return true;
    }
    return false;
}

// Handles encoder actions by dispatching to the appropriate handler
static bool processEncoderActions() {
    if (g_activeCommand != CMD_NONE) {
        g_lastAdjustmentTime = millis();
    }

    bool was_tuning_event = false;

    if (g_settingsActive) {
        processEncoderForSettings(g_safeEncoderMovement);
    } else {
        was_tuning_event = processEncoderForCommands(g_safeEncoderMovement);
    }

    g_safeEncoderMovement = 0;
    g_encoderCount = 0;
    resetEepromDelay();
    return was_tuning_event;
}

// ------------------------------------------
// --- Timed & Periodic Tasks ---------------
// ------------------------------------------

// Handles the delayed frequency update for AM/FM to prevent flooding the chip
static void handleDelayedFrequencyUpdate() {
    if (!g_processFreqChange || isSSB()) return;

    // if there new encoder movement, process it immediately to stay responsive
    if (g_safeEncoderMovement) {
        g_encoderCount = g_safeEncoderMovement;
        g_safeEncoderMovement = 0;
        doFrequencyTune();
        return;
    }

    // if there no new movement and enough time has passed, send the last frequency to the chip
    bool recent = millis() - g_lastFreqChange < 70;
    if (!recent) {
        g_si4735.setFrequency(g_currentFrequency);
        g_processFreqChange = false;
    }
}

static inline uint8_t getAmSignalValue() {
    if (g_Settings[RSSI_AM_Off].param == 1)
        return 255;

    // last value if frozen or during 1-sec quiet period to keep display stable
    if (!g_forceRssiUpdate || (millis() - g_lastUserActivityTime < 1000))
        return g_signalQualityValue;

    g_forceRssiUpdate = false;
    g_si4735.setFrequency(g_currentFrequency);
    g_si4735.getStatus();
    return g_si4735.getReceivedSignalStrengthIndicator();
}

static inline uint8_t getFmSignalValue() {
    g_si4735.getCurrentReceivedSignalQuality(1);
    return g_si4735.getCurrentRSSI();
}

// logic for updating the signal quality indicator RSSI value
static inline void updateSignalQuality() {
    uint8_t new_value = 255;

    if (g_currentMode == FM) {
        new_value = getFmSignalValue();
    } else if (g_currentMode == AM) {
        new_value = getAmSignalValue();
    }

    // value cap for 2 symbol
    if (new_value > 99 && new_value != 255) {
        new_value = 99;
    }

    if (g_signalQualityValue != new_value) {
        g_signalQualityValue = new_value;
        showSignalQuality();
    }
}

// Checks for and handles signal quality and stereo indicator updates
static inline void handleSignalAndStereoUpdates() {
    // 500ms debounce after last frequency change to prevent polling while actively tuning
    if (millis() - g_lastFreqChange < 500) return;

    // updates prevent while in any menu
    if (g_settingsActive || g_favoritesActive) return;

    if (millis() - g_lastRSSIUpdate >= 1000) {
        g_lastRSSIUpdate = millis();

        // 10-sec periodic unfreeze for AM RSSI via countdown
        static uint8_t am_refresh_countdown = 0;
        if (g_currentMode != AM || g_forceRssiUpdate) {
            am_refresh_countdown = 10;
        } else if (g_Settings[RSSI_AM_Off].param == 0 && --am_refresh_countdown == 0) {
            g_forceRssiUpdate = true;
            am_refresh_countdown = 10;
        }

        updateSignalQuality();

        // Stereo indicator logic is specific to FM
        if (g_currentMode == FM && millis() > 3000) {
            bool stereo = g_si4735.getCurrentPilot();
            if (g_stereoStatus != stereo) {
                g_stereoStatus = stereo;
                updateStereoIndicator();
            }
        }
    }
}

// exits command modes (Volume, BW, etc.) after a period of inactivity
static inline void handleCommandTimeout() {
    if (g_lastAdjustmentTime && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT)
        resetCommandMode();
}

// settings saved to EEPROM if they have been marked as changed
static inline void handleSettingsSave() {
    if (g_settingsDirty || (g_stateIsDirty && (millis() - g_lastUserActivityTime > SAVE_ON_IDLE_TIMEOUT))) {
        saveAllReceiverInformation(g_settingsDirty); // full_save if settings changed, partial if idle
        g_settingsDirty = false;
        g_stateIsDirty = false;
    }
}

// for all time-based tasks
static void handlePeriodicTasks() {
    handleSignalAndStereoUpdates();
    handleCommandTimeout();
    handleSettingsSave();
    updateAndShowBattery(false);
}

// ------------------------------------------
// ------- Main Logic (setup and loop) ------
// ------------------------------------------

//Initialize controller
void setup() {
    safeAmpOff();

    DDRB |= (1 << DDB5);
    DDRD &= ~((1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B));
    PORTD |= (1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B);

    g_voltagePinConnnected = analogRead(BATTERY_VOLTAGE_PIN) > 300;

    oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
    oled.clear();
    oled.on();
    oled.setFont(DEFAULT_FONT);

    // Force EEPROM reset if specific buttons are held on startup
    if (!(PINC & (1 << (ENCODER_BUTTON - 14))) || !(PINB & (1 << (AGC_BUTTON - 8)))) {
        // Invalidate version to trigger reset logic
        EEPROM.write(EEPROM_VERSION_ADDRESS, 0);
    } else {
#if ENABLE_SPLASH_SCREEN
        showSplashScreen();
#endif
    }

    // The rest of the setup is common for both paths
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);

    delay(500);

    // Load configuration from EEPROM or initialize with defaults
    readAllReceiverInformation();

    loadFMFav();

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();

    applyBandConfiguration();
    g_currentFrequency = g_previousFrequency = g_si4735.getFrequency();
    g_si4735.setVolume(g_volume);

    oled.clear();
    showStatus();

    safeAmpOn();
}

// main loop program in process order
void loop() {
    updateEncoderState();

    if (g_favoritesActive) {
        handleFavoritesMenu();
        processButtonEvents();
        return;
    }

    handleDelayedFrequencyUpdate();

    bool frequencyTuned = false;
    if (g_safeEncoderMovement) {
        frequencyTuned = processEncoderActions();
    }

    // process buttons only if the encoder was not used for a major tuning event
    if (!frequencyTuned) {
        processButtonEvents();
    }

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
