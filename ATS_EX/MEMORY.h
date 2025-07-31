#pragma once

#include <EEPROM.h>

// ======================================================================
// Memory.h - EEPROM Management Subsystem
// Handles saving and loading all persistent receiver state
// ======================================================================

// ==========================================
// ===== INITIALIZATION & DEFAULTS ==========
// ==========================================

// Populates settings with factory defaults
// usually on first boot or corruption
static void initializeDefaultModeSettings() {
    for (uint8_t i = 0; i < MODE_CONTEXT_COUNT; i++) {
        g_modeSettings[MODE_SETTING_AGC][i] = defaultModeSettings[i].agc;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][i] = defaultModeSettings[i].soft_mute;
        g_modeSettings[MODE_SETTING_AVC][i] = defaultModeSettings[i].avc;
    }
}

// ==========================================
// ===== LOW-LEVEL EEPROM HELPERS ===========
// ==========================================

// Writes one band variable data to a specific EEPROM address
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

// Reads one band variable data from a specific EEPROM address
static void readBandStateFromEEPROM(uint16_t addr, Band& band) {
    band.currentFreq = (EEPROM.read(addr + 0) << 8) | EEPROM.read(addr + 1);
    band.stepIdxAM = EEPROM.read(addr + 2);
    band.stepIdxSSB = EEPROM.read(addr + 3);
    band.stepIdxFM = EEPROM.read(addr + 4);
    band.bwIdxAM = EEPROM.read(addr + 5);
    band.bwIdxSSB = EEPROM.read(addr + 6);
    band.bwIdxFM = EEPROM.read(addr + 7);
}

// ==========================================
// ===== FM FAVORITES MANAGEMENT ============
// ==========================================

#if ENABLE_FM_FAV
// Saves FM favorites list to EEPROM
static void saveFMFav() {
    EEPROM.update(EEPROM_FM_FAVORITES_COUNT, g_totalFavorites);

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        uint16_t freq = g_fmFavorites[i].frequency;
        EEPROM.update(addr++, freq >> 8);
        EEPROM.update(addr++, freq & 0xFF);
    }
}

// Loads FM favorites from EEPROM
// resetting if data is invalid
static void loadFMFav() {
    g_totalFavorites = EEPROM.read(EEPROM_FM_FAVORITES_COUNT);

    // Protect against invalid count from uninitialized EEPROM (0xFF)
    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FM_FAVORITES) {
        g_totalFavorites = 0;
        saveFMFav();
        return;
    }

    uint16_t addr = EEPROM_FM_FAVORITES_START;

    // Read 16-bit frequency byte-by-byte to ensure correct endianness
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        uint8_t highByte = EEPROM.read(addr);
        addr++;
        uint8_t lowByte = EEPROM.read(addr);
        addr++;
        uint16_t freq = (highByte << 8) | lowByte;
        g_fmFavorites[i].frequency = freq;
    }
}
#endif

// ==========================================
// ===== GENERIC BLOCK I/O HELPERS ==========
// ==========================================

// Generic helper to write a block of memory to EEPROM
static inline void writeEepromBlock(uint16_t& addr, const void* src, uint16_t size) {
    const uint8_t* p = (const uint8_t*)src;
    for (uint16_t i = 0; i < size; i++) {
        EEPROM.update(addr + i, p[i]);
    }
    addr += size;
}

// Generic helper to read a block of memory from EEPROM
static inline void readEepromBlock(uint16_t& addr, void* dst, uint16_t size) {
    uint8_t* p = (uint8_t*)dst;
    for (uint16_t i = 0; i < size; i++) {
        p[i] = EEPROM.read(addr + i);
    }
    addr += size;
}

// ==========================================
// ===== COMPONENT-LEVEL STATE HANDLERS =====
// ==========================================

// Wraps block I/O for reading or writing mode-specific settings
static inline void handleModeSettingsEEPROM(bool save) {
    uint16_t addr = EEPROM_MODE_SETTINGS_START;
    if (save)
        writeEepromBlock(addr, g_modeSettings, sizeof(g_modeSettings));
    else
        readEepromBlock(addr, g_modeSettings, sizeof(g_modeSettings));
}

// Writes main configuration block (volume, mode, BFO) to EEPROM
static inline void writeEepromHeader(uint16_t& addr) {
    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0xFF);
    EEPROM.update(addr++, g_lastCWMode);
}

// Reads main configuration block with sanity checks for data integrity
static inline void readEepromHeader(uint16_t& addr) {
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    // Sanity check against invalid data
    if (g_bandIndex > g_lastBand) g_bandIndex = 1;
    g_currentMode = EEPROM.read(addr++);

    // Reconstruct 16-bit BFO from two bytes
    uint8_t highBFO = EEPROM.read(addr);
    addr++;
    uint8_t lowBFO = EEPROM.read(addr);
    addr++;
    g_currentBFO = (highBFO << 8) | lowBFO;

    g_lastCWMode = EEPROM.read(addr++);
}

// Saves band data
// non-full save writes only current band to reduce EEPROM wear
static inline void saveBands(bool full_save) {
    const uint8_t band_state_size = sizeof(Band) - offsetof(Band, currentFreq);

    if (full_save) {
        for (uint8_t i = 0; i <= g_lastBand; ++i)
            writeBandStateToEEPROM(EEPROM_BANDS_START + (i * band_state_size), g_bandList[i]);
    } else {
        writeBandStateToEEPROM(EEPROM_BANDS_START + (g_bandIndex * band_state_size), g_bandList[g_bandIndex]);
    }
}

// Loads state for all bands from EEPROM into the global band list
static inline void loadBands() {
    const uint8_t band_state_size = sizeof(Band) - offsetof(Band, currentFreq);
    for (uint8_t i = 0; i <= g_lastBand; ++i)
        readBandStateFromEEPROM(EEPROM_BANDS_START + (i * band_state_size), g_bandList[i]);
}

// ==========================================
// ===== UI & MESSAGING HELPERS =============
// ==========================================

#if ENABLE_EEPROM_RESET_MSG
// Displays a message on screen when EEPROM is reset to defaults
static void drawEepromResetMsg() {
    oled.clear();
    oled.setCursor(40, 2);
    oled.print(F("EEPROM RESET"));
    delay(2000);
}
#endif

// ==========================================
// ===== MAIN ORCHESTRATORS =================
// ==========================================

// Main entry point for writing all receiver state to EEPROM
static void saveAllReceiverInformation(bool full_save = true) {
    syncActiveStateToBand();

    // Optimization: skip write if frequency has not changed
    if (!full_save && g_currentFrequency == g_lastSavedFrequency) {
        g_stateIsDirty = false;
        return;
    }

    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);
    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);

    uint16_t addr = EEPROM_HEADER_START;
    writeEepromHeader(addr);

    saveBands(full_save);

    if (full_save) {
        for (uint8_t i = 0; i < SETTINGS_MAX; ++i)
            EEPROM.update(EEPROM_SETTINGS_START + i, g_Settings[i].param);

        handleModeSettingsEEPROM(true);

#if ENABLE_FM_FAV
        saveFMFav();
#endif
    }

    g_lastSavedFrequency = g_currentFrequency;
}

// Main entry point for loading all state from EEPROM on boot
static void readAllReceiverInformation() {
    // Check for magic bytes to validate EEPROM data
    // reset if invalid
    if (EEPROM.read(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID ||
        EEPROM.read(EEPROM_VERSION_ADDRESS) != APP_VERSION) {
#if ENABLE_EEPROM_RESET_MSG
        drawEepromResetMsg();
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

    uint16_t addr = EEPROM_HEADER_START;
    readEepromHeader(addr);

    loadBands();

    for (uint8_t i = 0; i < SETTINGS_MAX; ++i)
        g_Settings[i].param = EEPROM.read(EEPROM_SETTINGS_START + i);

    // Safety check to prevent loading invalid CPU speed on settings
    if (g_Settings[SettingsIndex::CPUSpeed].param > 1)
        g_Settings[SettingsIndex::CPUSpeed].param = 0;

    handleModeSettingsEEPROM(false);

    applyBrightness();

#if ENABLE_FM_FAV
    loadFMFav();
#endif

    loadActiveStateFromBand();
    g_previousFrequency = g_currentFrequency;
    if (isSSB()) loadSSBPatch();

    applyBandConfiguration();
    g_lastSavedFrequency = g_currentFrequency;
}
