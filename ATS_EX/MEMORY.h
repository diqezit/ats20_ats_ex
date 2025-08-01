#pragma once

// ======================================================================
// Memory.h - EEPROM Management Subsystem
// Handles saving and loading all persistent receiver state
// ======================================================================

// ==========================================
// ===== UI & MESSAGING HELPERS =============
// ==========================================

#if ENABLE_EEPROM_RESET_MSG
// Notify user that settings have been reset to defaults
static void drawEepromResetMsg() {
    oled.clear();
    oled.setCursor(40, 2);
    oled.print(F("EEPROM RESET"));
    delay(2000);
}
#endif

// ==========================================
// ===== DATA STRUCTURES FOR EEPROM =========
// ==========================================

// Main configuration data, grouped for a single EEPROM write
struct __attribute__((packed)) ReceiverHeader {
    uint8_t volume;
    uint8_t bandIndex;
    uint8_t currentMode;
    int16_t currentBFO;
    uint8_t lastCWMode;
};

// Band settings are bit-packed to save EEPROM space
struct __attribute__((packed)) BandStatePacked {
    uint16_t currentFreq;
    uint8_t packed_am;
    uint8_t packed_ssb;
    uint8_t packed_fm;
};

// ==========================================
// ===== COMPONENT-LEVEL STATE HANDLERS =====
// ==========================================

// --- Band State ---
// Pack runtime band data into a compact struct and save to EEPROM
static void saveBandState(uint8_t bandIndex) {
    BandStatePacked state;
    const Band& band = g_bandList[bandIndex];
    state.currentFreq = band.currentFreq;
    // Pack step and bandwidth indices into single bytes
    state.packed_am = (band.stepIdxAM & 0x0F) |
        ((band.bwIdxAM & 0x0F) << 4);
    state.packed_ssb = (band.stepIdxSSB & 0x0F) |
        ((band.bwIdxSSB & 0x0F) << 4);
    state.packed_fm = (band.stepIdxFM & 0x0F) |
        ((band.bwIdxFM & 0x0F) << 4);
    eeprom_update_block(
        &state,
        (void*)(EEPROM_BANDS_START + (bandIndex * sizeof(BandStatePacked))),
        sizeof(BandStatePacked)
    );
}

// Read packed band data from EEPROM and expand into runtime struct
static void loadBandState(uint8_t bandIndex) {
    BandStatePacked state;
    Band& band = g_bandList[bandIndex];
    eeprom_read_block(
        &state,
        (const void*)(EEPROM_BANDS_START + (bandIndex * sizeof(BandStatePacked))),
        sizeof(BandStatePacked)
    );
    band.currentFreq = state.currentFreq;
    // Unpack step and bandwidth from their respective bytes
    band.stepIdxAM = state.packed_am & 0x0F;
    band.bwIdxAM = (state.packed_am >> 4) & 0x0F;
    band.stepIdxSSB = state.packed_ssb & 0x0F;
    band.bwIdxSSB = (state.packed_ssb >> 4) & 0x0F;
    band.stepIdxFM = state.packed_fm & 0x0F;
    band.bwIdxFM = (state.packed_fm >> 4) & 0x0F;
}

// On partial saves only write current band state to reduce EEPROM wear
static inline void saveBands(bool full_save) {
    if (full_save) {
        for (uint8_t i = 0; i <= g_lastBand; ++i)
            saveBandState(i);
    } else {
        saveBandState(g_bandIndex);
    }
}

// Helper to load all band configurations at startup
static inline void loadBands() {
    for (uint8_t i = 0; i <= g_lastBand; ++i)
        loadBandState(i);
}

// --- Favorites ---
#if ENABLE_FAVORITES
// Write the entire list of favorite stations to EEPROM
static void saveFavorites() {
    eeprom_update_byte((uint8_t*)EEPROM_FAVORITES_COUNT, g_totalFavorites);
    uint16_t addr = EEPROM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        eeprom_update_block(&g_favorites[i], (void*)addr, sizeof(FavoriteStation));
        addr += sizeof(FavoriteStation);
    }
}

// Read favorite stations from EEPROM, handling uninitialized data
static void loadFavorites() {
    g_totalFavorites = eeprom_read_byte((const uint8_t*)EEPROM_FAVORITES_COUNT);

    // Sanity check favorite count to handle uninitialized EEPROM
    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FAVORITES) {
        g_totalFavorites = 0;
        saveFavorites();
        return;
    }

    uint16_t addr = EEPROM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        eeprom_read_block(&g_favorites[i], (const void*)addr, sizeof(FavoriteStation));
        addr += sizeof(FavoriteStation);
    }
}
#endif

// Save or load mode-specific settings as a single block
static inline void handleModeSettingsEEPROM(bool save) {
    if (save)
        eeprom_update_block(
            g_modeSettings,
            (void*)EEPROM_MODE_SETTINGS_START,
            sizeof(g_modeSettings)
        );
    else
        eeprom_read_block(
            g_modeSettings,
            (const void*)EEPROM_MODE_SETTINGS_START,
            sizeof(g_modeSettings)
        );
}

// ==========================================
// ===== MAIN ORCHESTRATORS =================
// ==========================================

// Main entry point for writing all receiver state to EEPROM
static void saveAllReceiverInformation(bool full_save = true) {
    syncActiveStateToBand();

    // Skip write if frequency is unchanged on partial saves to reduce wear
    if (!full_save && g_currentFrequency == g_lastSavedFrequency) {
        g_stateIsDirty = false;
        return;
    }

    eeprom_update_byte((uint8_t*)EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);
    eeprom_update_byte((uint8_t*)EEPROM_VERSION_ADDRESS, APP_VERSION);

    ReceiverHeader header;
    header.volume = g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume();
    header.bandIndex = g_bandIndex;
    header.currentMode = g_currentMode;
    header.currentBFO = g_currentBFO;
    header.lastCWMode = g_lastCWMode;
    eeprom_update_block(&header, (void*)EEPROM_HEADER_START, sizeof(header));

    saveBands(full_save);

    if (full_save) {
        for (uint8_t i = 0; i < SETTINGS_MAX; ++i) {
            eeprom_update_byte(
                (uint8_t*)(EEPROM_SETTINGS_START + i),
                g_Settings[i].param
            );
        }
        handleModeSettingsEEPROM(true);

#if ENABLE_FAVORITES
        saveFavorites();
#endif
    }

    g_lastSavedFrequency = g_currentFrequency;
}

// Main entry point for loading all state from EEPROM on boot
// It validates EEPROM data using magic bytes and version
// If data is invalid it orchestrates a factory reset
// using compile-time defaults in g_Settings as source of truth
static void readAllReceiverInformation() {
    // Validate EEPROM data with magic bytes and version, reset to defaults if invalid
    if (eeprom_read_byte((const uint8_t*)EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID ||
        eeprom_read_byte((const uint8_t*)EEPROM_VERSION_ADDRESS) != APP_VERSION) {
#if ENABLE_EEPROM_RESET_MSG
        drawEepromResetMsg();
#endif
        // Populate g_modeSettings from the compile-time defaults already in g_Settings
        syncModeDependentSettings(false);

#if ENABLE_FAVORITES
        g_totalFavorites = 0;
#endif
        // Now both RAM arrays are pristine and consistent, save them
        saveAllReceiverInformation(true);
        loadActiveStateFromBand();
        applyBandConfiguration();
        return;
    }

    // Normal boot path - load all data from valid EEPROM
    ReceiverHeader header;
    eeprom_read_block(&header, (const void*)EEPROM_HEADER_START, sizeof(header));

    g_volume = header.volume;
    g_bandIndex = header.bandIndex;
    // Prevent loading an out-of-bounds band index
    if (g_bandIndex > g_lastBand) g_bandIndex = 1;
    g_currentMode = header.currentMode;
    g_currentBFO = header.currentBFO;
    g_lastCWMode = header.lastCWMode;

    loadBands();

    for (uint8_t i = 0; i < SETTINGS_MAX; ++i) {
        g_Settings[i].param = eeprom_read_byte(
            (const uint8_t*)(EEPROM_SETTINGS_START + i)
        );
    }

    // Ensure CPU speed setting is valid after loading from EEPROM
    if (g_Settings[SettingsIndex::CPUSpeed].param > 1)
        g_Settings[SettingsIndex::CPUSpeed].param = 0;

    handleModeSettingsEEPROM(false);

    applyBrightness();

#if ENABLE_FAVORITES
    loadFavorites();
#endif

    loadActiveStateFromBand();
    g_previousFrequency = g_currentFrequency;
    if (isSSB()) loadSSBPatch();

    applyBandConfiguration();
    g_lastSavedFrequency = g_currentFrequency;
}
