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
// MOD_NO_RDS_v5.2 by diqezit
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

#include "CustomFonts.h"        // custom font data
#include "GyverOLED.h"
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

#if DEBUG_MODE
#include <avr/pgmspace.h>
#endif

#include "Rotary.h"
#include "SimpleButton.h"
#include "patch_ssb_compressed.h"

#include "defs.h"
#include "globals.h"
#include "Utils.h"

constexpr auto APP_VERSION = 52;

// ------------------------------------------
// ------- Utility & Helper Functions -------
// ------------------------------------------

static bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// Helper function to get the current context (AM or SSB/CW)
static ModeContext getModeContext() {
    if (isSSB()) {
        return MODE_CONTEXT_SSB;
    }
    return MODE_CONTEXT_AM;
}

// gets the band name from the Band structure into a C-string
static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, 4);
    buffer[4] = '\0'; // ensure null
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
    bool result;
    noInterrupts();  // race protection
    result = g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
    interrupts();
    return result;
}

// snaps frequency to the step grid after a band switch
// steps <1 kHz skipped to maintain smooth tuning
// for larger steps calculates in kHz
static inline void snapToNewStep(uint16_t* freq, bool isUp) {
    uint16_t new_step = g_tabStep[SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB];
    if (new_step < 1000) return;
    uint16_t step_khz = new_step / 1000;
    uint16_t remainder = *freq % step_khz;
    if (remainder) {
        if (isUp) *freq += step_khz - remainder;
        else if (*freq >= remainder) *freq -= remainder;
    }
}

// performs bfo rollover with integrated boundary checks and a max bfo limit
// this is the core of the stability system for ssb tuning
// see: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline void performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {
    const int32_t BFOMax = 13000;

    // Clamp bfo to the maximum allowed range first for stability
    if (*bfo > BFOMax) *bfo = BFOMax;
    if (*bfo < -BFOMax) *bfo = -BFOMax;

    // This loop "rolls over" BFO by subtracting 1000 Hz each time and adding 1 kHz to the base frequency
    // If we hit band upper limit, it switches to next band and resets frequency to the new minimum
    // Unlike downward tuning below, there no special "pre-check" here for small positive BFO at the band max
    // Based on tests and logs, any small delay (needing a full 1000 Hz buildup) is usually not noticeable
    // (e.g., it might take a few encoder turns, but sound stays smooth)
    // If you experience "stuck" tuning upward, consider adding a symmetric pre-check for perfect balance

    // Handle tuning upward
    while (*bfo >= 1000) {
        (*freq)++;
        if (*freq >= g_bandList[g_bandIndex].maximumFreq) {
            bandSwitch(true, false);
            *freq = g_bandList[g_bandIndex].minimumFreq;
            snapToNewStep(freq, true);
        }
        *bfo -= 1000;
    }

    // SSB band edge tuning - standard `while` loop below only triggers after a full 1 kHz BFO rollover,
    // causing a step delay.
    // Pre-check handles boundary crossing immediately when freq is at the band minimum and BFO becomes negative.
    // It forces the band switch and manually rolls over the BFO (e.g., 7000 kHz, BFO -100 -> 6999 kHz, BFO +900)
    // for a seamless transition
    if (*bfo < 0) {
        // First, handle the specific band edge case to prevent a one-step delay
        if (*freq == g_bandList[g_bandIndex].minimumFreq) {
            bandSwitch(false, false);
            *freq = g_bandList[g_bandIndex].maximumFreq;
            (*freq)--;
            *bfo += 1000;
        }

        // Then, handle any remaining standard rollovers
        while (*bfo <= -1000) {
            (*freq)--;
            if (*freq < g_bandList[g_bandIndex].minimumFreq) {
                bandSwitch(false, false);
                *freq = g_bandList[g_bandIndex].maximumFreq;
                snapToNewStep(freq, false);
            }
            *bfo += 1000;
        }
    }
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

#if ENABLE_FM_FAV
// Save favorites FM stations to EEPROM
static void saveFMFav() {
    EEPROM.update(EEPROM_FM_FAVORITES_COUNT, g_totalFavorites);

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        uint16_t freq = g_fmFavorites[i].frequency;
        EEPROM.update(addr++, freq >> 8);
        EEPROM.update(addr++, freq & 0xFF);
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
        g_fmFavorites[i].frequency = freq;  // now load without validation
    }
}
#endif

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

#if ENABLE_FM_FAV
        saveFMFav();
#endif
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
#if ENABLE_EEPROM_RESET_MSG
        oled.clear();
        oled.setCursor(0, 2);
        oled.print(F("  EEPROM RESET"));
        delay(2000);
#endif
        initializeDefaultModeSettings();
#if ENABLE_FM_FAV
        g_totalFavorites = 0;
#endif
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

#if ENABLE_FM_FAV
    loadFMFav();
#endif

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

// --- HARDWARE REQUIREMENT ---
// For battery level display, the BATTERY_VOLTAGE_PIN (look in defs.h) must be
// connected to the midpoint of a voltage divider made of two 10kΩ resistors
//
//   VCC (PREFER after power switch!) --- [10kΩ] --- (BATTERY_VOLTAGE_PIN) --- [10kΩ] --- GND
//
// The firmware is calibrated specifically for this 1:2 divider
// Using other resistor values will result in inaccurate battery readings


#if ENABLE_BATTERY_MONITOR
uint8_t g_stableBatteryPercent = 100;

// saving FLASH memory (1-byte offset)
struct BatteryPointCompressed {
    uint8_t voltage_offset;
    uint8_t percent;
};

// To save PROGMEM, values are 8-bit offsets from a base of 491,
// which corresponds to the safe 3.15V cutoff
static const BatteryPointCompressed battery_table[] PROGMEM = {
    // Offset, Percent, (ADC -> Approx. Voltage)
    {156, 100}, // 647 -> 4.15V
    {148, 90},  // 639 -> 4.10V
    {141, 85},  // 632 -> 4.05V
    {132, 80},  // 623 -> 4.00V
    {125, 75},  // 616 -> 3.95V
    {117, 70},  // 608 -> 3.90V
    {109, 65},  // 600 -> 3.85V
    {101, 60},  // 592 -> 3.80V
    { 93, 55},  // 584 -> 3.75V
    { 86, 50},  // 577 -> 3.70V
    { 78, 45},  // 569 -> 3.65V
    { 70, 40},  // 561 -> 3.60V
    { 62, 35},  // 553 -> 3.55V
    { 55, 30},  // 546 -> 3.50V
    { 47, 25},  // 538 -> 3.45V
    { 39, 20},  // 530 -> 3.40V
    { 31, 15},  // 522 -> 3.35V
    { 23, 10},  // 514 -> 3.30V
    { 15,  5},  // 506 -> 3.25V
    {  7,  2},  // 498 -> 3.20V
    {  0,  0}   // 491 -> 3.15V (Safe cut-off)
};

// Calculates raw battery percentage from an ADC value using interpolation
static uint8_t calculateRawPercent(uint16_t adc_value) {
    if (adc_value >= 647) return 100;
    if (adc_value <= 491) return 0;

    // The loop limit '21' is hardcoded as 'TABLE_POINTS - 1' for 22-point table
    for (uint8_t i = 0; i < 20; ++i) {
        // Reconstruct full ADC value from the stored 8-bit offset and base (491)
        uint16_t v_lower = pgm_read_byte(&battery_table[i + 1].voltage_offset) + 491;

        if (adc_value > v_lower) {
            uint16_t v_upper = pgm_read_byte(&battery_table[i].voltage_offset) + 491;
            uint8_t p_lower = pgm_read_byte(&battery_table[i + 1].percent);
            uint8_t p_upper = pgm_read_byte(&battery_table[i].percent);

            return p_lower + ((uint32_t)(adc_value - v_lower) * (p_upper - p_lower)) / (v_upper - v_lower);
        }
    }
    return 0;
}

#if ENABLE_ADVANCED_BATTERY_LOGIC
static uint8_t g_percentChangeCounter = 0;
static int16_t g_averageADC = -1;

static void applyPercentUpdate(uint8_t newPercent) {
    g_stableBatteryPercent = newPercent;
    g_percentChangeCounter = 0;
}
#endif

// Update internal stable battery percentage
static void updateStablePercent() {
    if (!g_voltagePinConnnected) return;

    int sample = analogRead(BATTERY_VOLTAGE_PIN);

    if (sample <= 0) sample = 491; // Default if disconnected

#if ENABLE_ADVANCED_BATTERY_LOGIC
    if (g_averageADC == -1) g_averageADC = sample;

    // simple IIR filter (3/4 old, 1/4 new)
    g_averageADC = (3 * g_averageADC + sample) >> 2;

    uint8_t currentRawPercent = calculateRawPercent(g_averageADC);
    int8_t diff = currentRawPercent - g_stableBatteryPercent;

    // hysteresis logic
    if (diff == 0) g_percentChangeCounter = 0;
    else if (diff > 2 || diff < -2) applyPercentUpdate(currentRawPercent);
    else if (++g_percentChangeCounter >= 5) applyPercentUpdate(currentRawPercent);
#else
    g_stableBatteryPercent = calculateRawPercent(sample);
#endif
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
#endif

// ----------------------------------------------
// ---- Hardware & Receiver Control Subsystem ---
// ---- Sensitive logic is here -----------------
// ----------------------------------------------

// Controls the MD8002A amplifier state (on/off)
// Always sets the pin to OUTPUT mode for safety
// If TRUE = on, FALSE = off
static void __attribute__((always_inline)) setAmpState(bool on) {
    AMP_DDR |= (1 << AMP_BIT);        // Set as OUTPUT
    if (on) {
        AMP_PORT &= ~(1 << AMP_BIT);  // LOW (on)
    } else {
        AMP_PORT |= (1 << AMP_BIT);   // HIGH (off)
    }
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

// Handles the low-level interaction with the Si4735 chip to perform a seek
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(30);

    //  for limits (strict for LW/MW, full for SW)
    uint16_t minLimit = (g_bandList[g_bandIndex].bandType == SW_BAND_TYPE)
        ? SW_MIN_FREQ : g_bandList[g_bandIndex].minimumFreq;
    uint16_t maxLimit = (g_bandList[g_bandIndex].bandType == SW_BAND_TYPE)
        ? SW_MAX_FREQ : g_bandList[g_bandIndex].maximumFreq;

    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        g_si4735.setSeekAmLimits(minLimit, maxLimit);
        g_si4735.setSeekAmSpacing((g_bandList[g_bandIndex].bandType == MW_BAND_TYPE
            || g_bandList[g_bandIndex].bandType == LW_BAND_TYPE)
            ? LW_MW_STEP_SPACING : SW_STEP_SPACING);
    } else {
        g_si4735.setSeekFmLimits(minLimit, maxLimit);
        g_si4735.setSeekFmSpacing(10);
    }

    noInterrupts();
    g_seekStop = false;
    interrupts();

    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);

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
    // g_si4735.setFifoCount(1);
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

    // g_currentBFO = 0;
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

    // disable Sync (DSP AFC) functionality when in CW mode
    if (g_currentMode == CW) {
        g_si4735.setSSBDspAfc(1);
        g_si4735.setSSBAvcDivider(0);
    } else { // LSB or USB
        g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);
    }

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

    // Custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(AM_SEEK_SNR_THRESHOLD, 0);     // AM_SEEK_TUNE_SNR_THRESHOLD (Default: 5)
    g_si4735.setProperty(AM_SEEK_RSSI_THRESHOLD, 25);   // AM_SEEK_TUNE_RSSI_THRESHOLD (Default: 25)
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

// Helper to detect a major mode switch (FM <-> AM/SSB)
// is needed for safely toggle amplifier off/on during the internal IC transition
static inline bool isSwitchingBetweenFmAndAm() {
    bool prevWasFM = (g_currentMode == FM);
    bool nextIsFM = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    bool nextIsPureAM = !nextIsFM && !g_ssbLoaded;
    return (prevWasFM && nextIsPureAM) || (!prevWasFM && nextIsFM);
}

// Main band switching logic that coordinates mode transitions and amplifier control
void applyBandConfiguration(bool extraSSBReset) {
    bool switchingBetweenFMandAM = isSwitchingBetweenFmAndAm();

    if (switchingBetweenFMandAM)
        setAmpState(false);

    loadActiveStateFromBand();

    g_signalQualityValue = 255; // not keeping old value on screen temporarily (save 8 bytes)
    g_forceRssiUpdate = true;

    uint8_t cap_value = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);

    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) { // better use this
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
        setAmpState(true);

    g_previousFrequency = g_currentFrequency;
}

// ------------------------------------------
// ------- UI: Main Screen Drawing ----------
// ------------------------------------------

// helper for brightness calculating the value by using integer (low flash consume)
// on edit have a white color display (non default blue), so don`t know what it will look like for you
static void applyBrightness() {
    uint8_t s = g_Settings[Brightness].param;

    // non-linear formula to map s=[0,9] to a contrast value of [1,255]
    uint8_t contrast_value = (((uint32_t)s * ((uint16_t)s * 130 + 6060)) >> 8);

    // add 1 to shift the final range to [1, 255]
    oled.setContrast(contrast_value + 1);
}

// Startup screen
void showSplashScreen() {
    oled.clear();

    oled.setCursor(26, 1);
    oled.print(F("ATS-20+ v5.2"));

    oled.setCursor(32, 3);
    oled.print(F("Mod No RDS"));

#if ANIMATE_SPLASH
    for (int i = 0; i < 21; i++) {
        oled.setCursor(i * 6, 6);
        oled.print('-');
        delay(70);
    }
#endif

    delay(2000);
    oled.clear();
}

// display mode, dot position, units based on current band
static void prepareDisplayConfig(bool ssbMode, BandType band, uint8_t& outMode, uint8_t& outDotPos, const char*& outUnit) {
    outMode = 0;
    outDotPos = 0;
    outUnit = "kHz";

    if (ssbMode) {
        outMode = 2;
    } else if (band == FM_BAND_TYPE) {
        outMode = 1;
        outDotPos = 3;
        outUnit = "MHz";
    } else if (band == SW_BAND_TYPE && g_Settings[SettingsIndex::SWUnits].param == 1) {
        outDotPos = 2;
        outUnit = "MHz";
    }
}

// format main part frequency string and get SSB value
static void prepareMainFreq(uint8_t displayMode, char* freqDisplay, uint16_t& khzBFO, uint16_t& tailBFO, uint8_t dotPos) {
    if (displayMode == 2) { // SSB
        splitFreq(khzBFO, tailBFO);
        convertToChar(freqDisplay, khzBFO, ilen(khzBFO), 0, '.', ' ');
    } else { // AM / FM
        convertToChar(freqDisplay, g_currentFrequency, 5, dotPos, '.', '/');
    }
}

// clean background when frequency length changes
static void renderClearOrBlink(bool cleanDisplay, bool ssbMode, uint8_t len, uint8_t prevLen, uint8_t off, int pixelY) {
    if (cleanDisplay) {
        oled.clear(0, pixelY, 128, pixelY + 23);
    } else if (len != prevLen) {
        // if frequency length changes - clear from its starting position to the end of the screen
        uint8_t maxW = 128 - off;
        oled.partialUpdate(off, pixelY, maxW, 24, NULL);
    }
}

// SSB tail with save flash mem
static void renderSSBTail(bool ssbMode, uint16_t tailBFO, uint8_t len, uint8_t prevLen, int mainEndX, int pixelY) {
    if (!ssbMode) return;

    // Tightly align and draw the decimal part. Font widths: '.'=6px, digit=16px
    int curX = mainEndX - 2;
    oled.drawDigit('.', curX, pixelY);                  curX += 6;
    oled.drawDigit('0' + (tailBFO / 10), curX, pixelY); curX += 16;
    oled.drawDigit('0' + (tailBFO % 10), curX, pixelY); curX += 16;

    // If main frequency shortens (e.g. 14MHz -> 7MHz), clear the now-empty space
    // left by the disappearing digit from the main part
    if (len < prevLen) oled.clear(curX, pixelY, curX + 13, pixelY + 23);
}

// renders measurement units (kHz/MHz)
static void renderUnit(bool ssbMode, uint8_t len, const char* unit) {
    if (g_Settings[SettingsIndex::UnitsSwitch].param == 1 && (!ssbMode || len < 5)) {
        oled.setCursor(108, 4);
        oled.print(unit);
    }
}

// Helper function to render each character in the frequency string using drawDigit
static int renderFrequencyString(const char* freqDisplay, int startX, int pixelY) {
    int curX = startX;
    for (uint8_t i = 0; freqDisplay[i] != '\0'; i++) {
        char ch = freqDisplay[i];
        oled.drawDigit(ch, curX, pixelY);
        curX += (ch == '.') ? 6 : 16;
    }
    return curX;
}

//Draw frequency on display.
static void showFrequency(bool cleanDisplay = false) {
    if (g_settingsActive) return;

    // previous frequency length for update
    static uint8_t prevLen = 0;

    char     freqDisplay[7];
    uint16_t khzBFO = 0, tailBFO = 0;
    bool     ssbMode = isSSB();
    BandType band = g_bandList[g_bandIndex].bandType;

    // offset for text alignment depending on mode
    uint8_t  off = (ssbMode ? 3 : 12);

    uint8_t displayMode, dotPos;
    const char* unit;
    prepareDisplayConfig(ssbMode, band, displayMode, dotPos, unit);
    prepareMainFreq(displayMode, freqDisplay, khzBFO, tailBFO, dotPos);

    uint8_t len = ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);

    // Сrutch that prevents optimization by forcing the compiler to calculate the value in runtime
    // Without it, the data is written with a shift, breaking pages. Don`t remove it!
    // Violatile here adds + 80 bytes!

    // Set cursor position for frequency display
    int pixelY = 24 * 1;

    renderClearOrBlink(cleanDisplay, ssbMode, len, prevLen, off, pixelY);

    // Render main frequency and get its end X position
    int mainEndX = renderFrequencyString(freqDisplay, off, pixelY);

    renderSSBTail(ssbMode, tailBFO, len, prevLen, mainEndX, pixelY);
    renderUnit(ssbMode, len, unit);

    prevLen = len;
}

//This function is called by station seek logic
static void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;
    showFrequency();
    delay(100);
}

//Draw current band tag (e.g., "40m")
static void showBandTag() {
    if (g_settingsActive) return;

    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    oled.setCursor(0, 0);
    printInverted(name_buffer, invert);
}

//Draw current modulation (AM/LSB/USB/CW/FM) and stereo indicator
static void showModulation() {
    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    oled.setCursor(0, 7);
    printInverted(g_bandModeDesc[g_currentMode], invert);

    oled.print(' ');
    updateStereoIndicator();
    showBandTag();
}

//Draw volume level or mute status
static void showVolume() {
    if (g_settingsActive) return;

    char buf[3];

    if (g_muteVolume == 0) {
        convertToChar(buf, g_si4735.getCurrentVolume(), 2, 0, 0);
    } else {
        buf[0] = ' ';
        buf[1] = 'M';
        buf[2] = '\0';
    }
    bool invert = (g_activeCommand == CMD_VOLUME);
    oled.setCursor(114, 0);
    printInverted(buf, invert);
}

// Displays the current signal quality value (RSSI)
static void showSignalQuality() {
    if (g_settingsActive
#if ENABLE_FM_FAV
        || g_favoritesActive
#endif
        ) return;

    oled.setCursor(90, 7);

    if (g_signalQualityValue == 255) {
        oled.print(F("   "));
        return;
    }

    if (g_signalQualityValue < 10) oled.print(' ');
    oled.print(g_signalQualityValue);
    oled.print('|');
}

// Renders the stable battery percentage value on the display.
static void showChargeOnDisplay() {
    if (g_settingsActive) return;
    int charge = min(g_stableBatteryPercent, 100);
    oled.setCursor(108, 7);
    oled.print(charge);
    if (charge < 100) oled.print('%');
}

// display the step on the screen
static void showStep() {
    bool invert = (g_activeCommand == CMD_STEP);

    oled.setCursor(34, 0);
    printInverted(F("STEP: "), invert);

    const Band& current_band = g_bandList[g_bandIndex];
    uint8_t index = (g_currentMode == FM)
        ? (4 + current_band.stepIdxFM)
        : (isSSB() ? (SSB_STEP_OFFSET + current_band.stepIdxSSB)
            : current_band.stepIdxAM);

    printInverted((__FlashStringHelper*)step_lookup_table[index], invert);
}

// displays the current bandwidth
static void showBandwidth() {
    char bw[8];
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
        for (uint8_t i = 0; i < 7; i++) {
            bw[i] = pgm_read_byte(&bw_all_data[offset + i]);
        }
        bw[7] = '\0';
    } else { // for CW
        bw[0] = '\0';
    }
    bool invert = (g_activeCommand == CMD_BW);
    oled.setCursor(40, 7);
    printInverted(bw, invert);
}

// determine the indicator character based on mode
void updateStereoIndicator() {
    char c = (g_currentMode == CW) ? (g_Settings[CWSwitch].param == 0 ? 'L' : 'U') :
        (isSSB() && g_Settings[Sync].param == 1) ? 'S' :
        (g_currentMode == FM && g_stereoStatus) ? '*' : ' ';

    oled.setCursor(24, 7);
    oled.print(c);
}

// Orchestrator for drawing the main status screen
void showStatus(bool cleanFreq) {
    showFrequency(cleanFreq);
    showModulation();
    showStep();
    showBandwidth();
#if ENABLE_BATTERY_MONITOR
    updateAndShowBattery(true);
#endif
    showVolume();
    showSignalQuality();
}

// ------------------------------------------
// --- UI: Favorites Menu Drawing -----------
// ------------------------------------------

#if ENABLE_FM_FAV
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
    printInverted(F("  FM FAVORITES"), true);
    printInverted(F("         "), true);

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
        oled.print(F("                "));
    }

    oled.setCursor(0, 6);
    oled.print(' ');
    oled.print(g_favoriteSelected + 1);
    oled.print('/');
    oled.print(g_totalFavorites);
    oled.print(F(" DEL:BW"));
}
#endif

// ------------------------------------------
// --- UI: Settings Menu Drawing ------------
// ------------------------------------------

static inline void handleSwitchParam(char* buf, uint8_t idx, int8_t param, uint8_t type) {
    uint8_t textIdx;
    if (type == SettingType::SwitchAuto) {
        textIdx = param;
    } else {
        uint8_t base = pgm_read_byte(&switch_setting_map[idx].baseIndex);
        bool inv = pgm_read_byte(&switch_setting_map[idx].inverted);
        textIdx = inv ? (base - param) : (base + param);
    }
    strcpy_P(buf, paramTexts[textIdx]);
}

// Converts setting parameter value to UI display string
static void SettingParamToUI(char* buf, uint8_t idx) {
    int8_t param = g_Settings[idx].param;
    uint8_t type = g_Settings[idx].type;

    if (type == SettingType::Switch || type == SettingType::SwitchAuto) {
        handleSwitchParam(buf, idx, param, type);
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
    if (!g_settingsActive) return;

    char buf[5];

    // Calculate position in the page (0-5) for 2-column layout
    uint8_t place = idx - ((g_SettingsPage - 1) * 6);

    // Determine row (yOffset) and column (xOffset)
    uint8_t yOffset = (place > 2) ? (place - 3) * 2 : place * 2;
    uint8_t xOffset = (place > 2) ? 68 : 0;

    // Draw setting name if full redraw requested
    if (full) {
        oled.setCursor(5 + xOffset, 2 + yOffset);
        oled.print((idx == g_SettingSelected && !g_SettingEditing) ? '>' : ' ');
        oled.print(g_Settings[idx].name);
    }

    // Convert param to string and draw value (aligned to name)
    SettingParamToUI(buf, idx);
    oled.setCursor(35 + xOffset, 2 + yOffset);
    oled.print((idx == g_SettingSelected && g_SettingEditing) ? '>' : ' ');
    oled.print(buf);
}

// Draw the title of the settings menu
static void showSettingsTitle() {
    oled.setCursor(0, 0);
    printInverted(F("  SETTINGS "), true);
    printInverted((uint8_t)g_SettingsPage, true);
    printInverted('/', true);
    printInverted((uint8_t)g_SettingsMaxPages, true);
    printInverted(F("         "), true);
}

// Draw the complete settings screen (all visible items)
static void showSettings() {
    for (uint8_t i = 0; i < 6 && i + ((g_SettingsPage - 1) * 6) < SettingsIndex::SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * 6), true);
}

// ------------------------------------------
// --- State & Band Management Subsystem ----
// ------------------------------------------

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
    uint16_t found_freq = executeHardwareSeek();
    if (found_freq == 0) return;

    g_currentFrequency = found_freq;

    if (g_bandList[g_bandIndex].bandType == SW_BAND_TYPE) {
        for (uint8_t i = 2; i <= g_lastBand; i++) {
            if (g_currentFrequency >= g_bandList[i].minimumFreq
                && g_currentFrequency <= g_bandList[i].maximumFreq) {
                g_bandIndex = i;
                break;
            }
        }
    } else if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
        uint16_t remainder = g_currentFrequency % 10;
        if (remainder != 0) g_currentFrequency -= remainder;
    }

    g_si4735.setFrequency(g_currentFrequency);
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
    bool needs_switch_up = (temp_freq > old_band.maximumFreq);
    bool needs_switch_down = (temp_freq < old_band.minimumFreq);

    if (needs_switch_up || needs_switch_down) {
        // band boundary has been crossed
        bandSwitch(needs_switch_up, false);

        // This block differentiates between two types of band transitions:
        // - Seamless Crossover (e.g., AM<->SW) - keep temp_freq for smooth tuning
        // - Wrap-Around (involving FM) - reset frequency to the new band edge
        // Presence of FM_BAND_TYPE is a proxy for wrap-around behavior
        bool is_wrap_around = (old_band.bandType == FM_BAND_TYPE || g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);

        if (is_wrap_around) {
            g_currentFrequency = needs_switch_up ? g_bandList[g_bandIndex].minimumFreq : g_bandList[g_bandIndex].maximumFreq;
        } else {
            g_currentFrequency = (uint16_t)temp_freq;
        }
    } else {
        // standard intra-band tuning path
        g_currentFrequency = (uint16_t)temp_freq;

        // snap frequency to the current step grid
        // intentionally skipped during a band switch to prevent frequency distortion
        uint16_t remainder = g_currentFrequency % step;
        if (remainder) g_currentFrequency += g_seekDirection
            ? (step - remainder)
            : -remainder;
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
    uint16_t old_freq = g_currentFrequency;  // not used here, but to keep structure
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

// finalize mode switch by applying configuration and handling amp
static inline void finalizeModeSwitch() {
    applyBandConfiguration();

    if (!g_ssbLoaded && g_currentMode == AM) setAmpState(true);
}

// handles the complex logic of cycling through AM, LSB, USB, and CW modes
static inline void cycleAmSsbCwModes() {
    int8_t bw;
    prepareModeSwitch(bw);
    performModeCycle(bw);
    finalizeModeSwitch();
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

#if ENABLE_FM_FAV
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
#endif

// handles tuning step adjustment, updates the current band's state, and applies it to the chip
static void doStep(int8_t v) {
    Band& current_band = g_bandList[g_bandIndex];

    if (g_currentMode == FM) {
        doSwitchLogic(current_band.stepIdxFM, 0, g_lastStepFM, v);
        g_si4735.setFrequencyStep(g_tabStepFM[current_band.stepIdxFM]);
    } else if (isSSB()) {
        doSwitchLogic(current_band.stepIdxSSB, 0, SSB_STEPS_COUNT - 1, v);
    } else { // AM
        // LW/MW maximum step index is limited
        // in SW full range of steps
        bool is_lw_mw = (current_band.bandType == LW_BAND_TYPE || current_band.bandType == MW_BAND_TYPE);
        const uint8_t max_am_idx = is_lw_mw ? 3 : (AM_STEPS_COUNT - 1);

        doSwitchLogic(current_band.stepIdxAM, 0, max_am_idx, v);
        g_si4735.setFrequencyStep(g_tabStep[current_band.stepIdxAM]);
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
    // Sync is not need in CW mode
    if (g_currentMode == CW) return;

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

// Settings: CPU Frequency divider helper
static void setCpuPrescaler(uint8_t prescaler) {
    noInterrupts();
    CLKPR = 0x80;
    CLKPR = prescaler;
    interrupts();
}

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v = 0) {
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
    uint16_t original_freq = g_currentFrequency;

    toggleSetting(CWSwitch);
    if (g_currentMode != CW) return;

    const int8_t actual_direction = g_Settings[CWSwitch].param - old_param;
    // if (actual_direction == 0) return;

    g_currentFrequency += actual_direction * COMPENSATION_KHZ;
    g_si4735.setFrequency(g_currentFrequency);
    updateBFO();
    g_currentFrequency = original_freq;
    showFrequency(true);
    updateStereoIndicator();
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
// --- Input: Button & Event Handling -------
// ------------------------------------------

// Handle encoder direction (ISR context)
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        noInterrupts();  // for race protection!
        g_encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
        g_seekStop = true;
        interrupts();
    }
}

// Safely reads accumulated encoder value from the interrupt context
// Since have a cheap encoder which am tired of replacing because of rattling
// I had to add a little code to it :)
// So  this function can remove some rattle of lamellae when rotating the encoder
// Added software debounce (10ms) here to avoid bloating ISR
// noInterrupts() for atomic access without requiring extra #includes
static void updateEncoderState() {
    static uint32_t lastEncoderTime = 0;                // uint32 for precise debounce timing (moved from ISR)
    if (g_encoderCount) {                               // process only if ISR detected a change
        if (millis() - lastEncoderTime < 10) return;    // debounce 10ms (ignore if too soon)
        lastEncoderTime = millis();
        noInterrupts();                                 //  disable interrupts for atomic access
        g_safeEncoderMovement += g_encoderCount;
        g_encoderCount = 0;
        interrupts();                                   // re-enable interrupts
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
    if (pin != MODE_SWITCH && pin != STEP_BUTTON && event == BUTTONEVENT_FIRSTLONGPRESS) {
        return BUTTONEVENT_SHORTPRESS;
    }
    return event;
}

// This function handles the button events for band switching.
static uint8_t bandEvent(uint8_t event, uint8_t pin) {
#if (0 != BAND_DELAY)
    if (BUTTONEVENT_ISLONGPRESS(event) && !g_settingsActive) {
        if (BUTTONEVENT_LONGPRESSDONE != event) {
            bandSwitch(pin == BAND_BUTTON);
        }
    }
#else
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
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive
#if ENABLE_FM_FAV
        && !g_favoritesActive
#endif
        && !g_muteVolume) {
        switchCommand(CMD_VOLUME);
    }
}

static inline void handleVolumeDownButton() {
    // volume down button (mute) handler
    uint8_t evt = btn_VolumeDn.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt
#if ENABLE_FM_FAV
        && !g_favoritesActive
#endif
        && g_activeCommand != CMD_VOLUME) {
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

    setCpuPrescaler(new_prescaler);

    g_displayOn ? oled.setPower(true) : oled.setPower(false);
}

// step button handler
static inline void handleStepButton() {
    uint8_t evt = btn_Step.checkEvent(simpleEvent);

    // do nothing if any menu is active
#if ENABLE_FM_FAV
    if (g_settingsActive || g_favoritesActive) return;
#else
    if (g_settingsActive) return;
#endif

    if (BUTTONEVENT_SHORTPRESS == evt) {
        switchCommand(CMD_STEP);
    } else if (BUTTONEVENT_LONGPRESSDONE == evt) {
        // long press to toggle sideband, also for CW
        if (g_currentMode == LSB || g_currentMode == USB) {
            g_currentMode = (g_currentMode == LSB) ? USB : LSB;
            applyBandConfiguration();
        } else if (g_currentMode == CW) {
            // in CW call handle with frequency compensation
            doCWSwitch(0);
        }
    }
}

static inline void processModeButtonShortPress() {
#if ENABLE_FM_FAV
    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
        g_favoritesActive = true;
        g_favoriteSelected = 0;
        oled.clear();
        showFav();
        return;
    }
#endif
    cycleAmSsbCwModes();
}

// handles mode button presses, dispatching tasks based on the current context
static inline void handleModeButton() {
    uint8_t evt = btn_Mode.checkEvent(simpleEvent);
    if (g_settingsActive) return;

    if (BUTTONEVENT_SHORTPRESS == evt) {
        processModeButtonShortPress();
    } else if (BUTTONEVENT_LONGPRESSDONE == evt) {
#if ENABLE_FM_FAV
        if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE && !g_favoritesActive) {
            // long press in FM mode add to favorites
            addFav();
            oled.setCursor(45, 3);
            oled.print(F("SAVED"));
            delay(500);
            showStatus(true);
        } else
#endif
            if (isSSB()) {
                // long press in SSB/CW modes now toggle Sync
                doSync(0);
                updateStereoIndicator();
            }
    }
}

#if ENABLE_FM_FAV
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
#endif

// key process for all keys. Acts as a dispatcher based on the current UI mode
static void processButtonEvents() {

#if ENABLE_FM_FAV
    // process buttons for FM favorites
    if (g_favoritesActive) {
        handleFavoritesMenuButtons();
        return;
    }
#endif

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

#if ENABLE_FM_FAV
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
#endif

// Helper for navigating settings page
static inline void navigateSettingsPage(int encoder_delta) {
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
}

// handles encoder movement within the settings menu
static inline void processEncoderForSettings(int encoder_delta) {
    if (!g_SettingEditing) {
        navigateSettingsPage(encoder_delta);
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
    uint8_t new_value = (g_currentMode == FM)
        ? getFmSignalValue()
        : ((g_currentMode == AM)
            ? getAmSignalValue()
            : 255);

    // value cap for 2 symbol
    if (new_value > 99 && new_value != 255) {
        new_value = 99;
    }

    if (g_signalQualityValue != new_value) {
        g_signalQualityValue = new_value;
        showSignalQuality();
    }
}

// helper for AM RSSI countdown logic
static inline void updateAmRssiCountdown() {
    static uint8_t am_refresh_countdown = 0;
    if (g_currentMode != AM || g_forceRssiUpdate) {
        am_refresh_countdown = 10;
    } else if (g_Settings[RSSI_AM_Off].param == 0 && --am_refresh_countdown == 0) {
        g_forceRssiUpdate = true;
        am_refresh_countdown = 10;
    }
}

// helper for FM stereo indicator logic
static inline void updateFmStereoIndicator() {
    if (g_currentMode == FM && millis() > 3000) {
        bool stereo = g_si4735.getCurrentPilot();
        if (g_stereoStatus != stereo) {
            g_stereoStatus = stereo;
            updateStereoIndicator();
        }
    }
}

// Checks for and handles signal quality and stereo indicator updates
static inline void handleSignalAndStereoUpdates() {
    // 500ms debounce after last frequency change to prevent polling while actively tuning
    if (millis() - g_lastFreqChange < 500) return;

    // updates prevent while in any menu
    if (g_settingsActive
#if ENABLE_FM_FAV
        || g_favoritesActive
#endif
        ) return;

    if (millis() - g_lastRSSIUpdate >= 1000) {
        g_lastRSSIUpdate = millis();

        updateAmRssiCountdown();
        updateSignalQuality();
        updateFmStereoIndicator();
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
#if ENABLE_BATTERY_MONITOR
    updateAndShowBattery(false);
#endif
}

// ------------------------------------------
// ------------ Main Init Logic  ------------
// ------------------------------------------

// Helper to initialize hardware pins and battery check
static inline void initHardwarePins() {
    setAmpState(false);

    DDRB |= (1 << DDB5);
    DDRD &= ~((1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B));
    PORTD |= (1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B);

    g_voltagePinConnnected = analogRead(BATTERY_VOLTAGE_PIN) > 300;
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
        EEPROM.write(EEPROM_VERSION_ADDRESS, 0);
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

#if ENABLE_FM_FAV
    loadFMFav();
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

#if ENABLE_FM_FAV
    if (g_favoritesActive) {
        handleFavoritesMenu();
        processButtonEvents();
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
