#pragma once

// ====================================================================================
//
// Memory.h - EEPROM Management Subsystem
//
// This module handles all persistent receiver state by managing the EEPROM
// It isolates the rest of the application from the details of memory layout
// and raw read/write operations
//
// Includes logic to detect and prevent writes to worn-out EEPROM
// This is critical for device longevity as EEPROM has a limited write cycle life
// When wear is detected it flags the memory as bad and operates in a
// read-only mode to keep the receiver functional
//
// Also manages data versioning. If the firmware is updated and the EEPROM
// layout changes it will automatically reset settings to factory defaults
// to prevent data corruption
//
// ====================================================================================

// Global flag for EEPROM wear detection
static bool g_eepromBad = false;

// Macro to skip EEPROM writes if wear detected
#define CHECK_EEPROM_WEAR() if (g_eepromBad) return

// ==========================================
// ===== UI & MESSAGING HELPERS =============
// ==========================================

#if ENABLE_EEPROM_RESET_MSG
// Notify user that settings have been reset to defaults
static void drawEepromResetMsg() {
    oled.clear();
    oled.setCursor(37, 3);
    oled.print(g_eepromBad ? F("MEM WEAR") : F("MEM RESET"));
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
    uint8_t lastSsbMode;  // save USB/LSB
};

// Band settings are bit-packed to save EEPROM space
struct __attribute__((packed)) BandStatePacked {
    uint16_t currentFreq;
    uint8_t packed_am;
    uint8_t packed_ssb;
    uint8_t packed_fm;
    int8_t  bfoCal;       // per-band BFO calibration
};

// ==========================================
// ===== COMPILE-TIME EEPROM SAFETY CHECKS ==
// ==========================================
//
// prevent silent EEPROM layout corruption when band count / structs / addresses change
//

#define FIELD_SIZE(T, f) (sizeof(((T*)0)->f))

// ---------- Struct packing / binary format contracts ----------
static_assert(
    sizeof(ReceiverHeader) ==
    FIELD_SIZE(ReceiverHeader, volume) +
    FIELD_SIZE(ReceiverHeader, bandIndex) +
    FIELD_SIZE(ReceiverHeader, currentMode) +
    FIELD_SIZE(ReceiverHeader, currentBFO) +
    FIELD_SIZE(ReceiverHeader, lastCWMode) +
    FIELD_SIZE(ReceiverHeader, lastSsbMode),
    "ReceiverHeader packing changed"
    );

static_assert(
    sizeof(BandStatePacked) ==
    FIELD_SIZE(BandStatePacked, currentFreq) +
    FIELD_SIZE(BandStatePacked, packed_am) +
    FIELD_SIZE(BandStatePacked, packed_ssb) +
    FIELD_SIZE(BandStatePacked, packed_fm) +
    FIELD_SIZE(BandStatePacked, bfoCal),
    "BandStatePacked packing changed"
    );

// g_modeSettings is a raw int8 table: MODE_SETTINGS_COUNT * MODE_CONTEXT_COUNT bytes
static_assert(
    sizeof(g_modeSettings) ==
    (uint16_t)MODE_SETTINGS_COUNT * (uint16_t)MODE_CONTEXT_COUNT * sizeof(g_modeSettings[0][0]),
    "g_modeSettings size mismatch (MODE_SETTINGS_COUNT/MODE_CONTEXT_COUNT changed?)"
    );

#if ENABLE_FAVORITES
static_assert(
    sizeof(FavoriteStation) ==
    FIELD_SIZE(FavoriteStation, frequency) +
    FIELD_SIZE(FavoriteStation, modulation) +
    FIELD_SIZE(FavoriteStation, bfo),
    "FavoriteStation packing changed (expected packed struct)"
    );
#endif

#undef FIELD_SIZE

// ---------- Global sanity ----------
static_assert(g_bandCount > 0, "g_bandCount must be > 0");
static_assert(g_bandCount <= 255, "g_bandCount must fit in uint8_t");
static_assert(g_lastBand == (g_bandCount - 1), "g_lastBand must be g_bandCount - 1");
static_assert(SETTINGS_MAX > 0, "SETTINGS_MAX must be > 0");

// ---------- EEPROM address monotonicity (prevents overlaps by ordering) ----------
static_assert(EEPROM_APP_ID_ADDRESS != EEPROM_VERSION_ADDRESS, "EEPROM core addresses must be distinct");
static_assert(EEPROM_APP_ID_ADDRESS < EEPROM_VERSION_ADDRESS, "EEPROM core addresses order invalid");
static_assert(EEPROM_VERSION_ADDRESS < EEPROM_HEADER_START, "EEPROM header must start after version byte");

static_assert(EEPROM_HEADER_START < EEPROM_BANDS_START, "EEPROM_HEADER_START must be before bands block");
static_assert(EEPROM_BANDS_START < EEPROM_SETTINGS_START, "EEPROM_BANDS_START must be before settings block");
static_assert(EEPROM_SETTINGS_START < EEPROM_MODE_SETTINGS_START, "EEPROM_SETTINGS_START must be before mode-settings block");
static_assert(EEPROM_MODE_SETTINGS_START < EEPROM_FAVORITES_START, "EEPROM_MODE_SETTINGS_START must be before favorites block");
static_assert(EEPROM_FAVORITES_START < EEPROM_FAVORITES_COUNT, "EEPROM_FAVORITES_START must be before favorites count");

// ---------- Block boundary checks (prevents overlaps by size math) ----------

// Header must fit before bands
static_assert(
    (uint16_t)(EEPROM_HEADER_START + (uint16_t)sizeof(ReceiverHeader)) <= (uint16_t)EEPROM_BANDS_START,
    "ReceiverHeader overlaps bands block"
    );

// Bands region must fit before settings
static_assert(
    (uint16_t)(EEPROM_BANDS_START + (uint16_t)g_bandCount * (uint16_t)sizeof(BandStatePacked)) <= (uint16_t)EEPROM_SETTINGS_START,
    "Bands block overlaps settings block - update EEPROM map"
    );

// Settings region must fit before mode settings
static_assert(
    (uint16_t)(EEPROM_SETTINGS_START + (uint16_t)SETTINGS_MAX) <= (uint16_t)EEPROM_MODE_SETTINGS_START,
    "Settings block overlaps mode-settings block - update EEPROM map"
    );

// Mode settings must fit before favorites
static_assert(
    (uint16_t)(EEPROM_MODE_SETTINGS_START + (uint16_t)sizeof(g_modeSettings)) <= (uint16_t)EEPROM_FAVORITES_START,
    "Mode-settings block overlaps favorites block - update EEPROM map"
    );

#if ENABLE_FAVORITES
static_assert(MAX_FAVORITES > 0, "MAX_FAVORITES must be > 0");

// Favorites list must fit before favorites count byte
static_assert(
    (uint16_t)(EEPROM_FAVORITES_START + (uint16_t)MAX_FAVORITES * (uint16_t)sizeof(FavoriteStation)) <= (uint16_t)EEPROM_FAVORITES_COUNT,
    "Favorites block overlaps favorites count - update EEPROM map"
    );
#endif

// ---------- Device EEPROM size (hard limit) ----------
// AVR EEPROM upper bound (ATmega328P: 0..1023)
static_assert(EEPROM_FAVORITES_COUNT <= E2END, "EEPROM layout exceeds device EEPROM size");

// ==========================================
// ===== COMPONENT-LEVEL STATE HANDLERS =====
// ==========================================

#define PACK4(lo, hi)   (uint8_t)(((uint8_t)(lo) & 0x0F) | (((uint8_t)(hi) & 0x0F) << 4))
#define UNPACK_LO(v)    ((v) & 0x0F)
#define UNPACK_HI(v)    (((v) >> 4) & 0x0F)

// --- Band State ---
// Pack runtime band data into a compact struct and save to EEPROM
static void saveBandState(uint8_t bandIndex) {
    CHECK_EEPROM_WEAR();

    BandStatePacked state;
    const Band& band = g_bandList[bandIndex];
    state.currentFreq = band.currentFreq;

    // Pack step and bandwidth indices into single bytes
    state.packed_am = PACK4(band.stepIdxAM, band.bwIdxAM);
    state.packed_ssb = PACK4(band.stepIdxSSB, band.bwIdxSSB);
    state.packed_fm = PACK4(band.stepIdxFM, band.bwIdxFM);
    state.bfoCal = band.bfoCal;

    uint16_t addr = (uint16_t)EEPROM_BANDS_START
        + (uint16_t)bandIndex * (uint16_t)sizeof(BandStatePacked);
    eeprom_update_block(&state, (void*)addr, sizeof(BandStatePacked));
}

// Read packed band data from EEPROM and expand into runtime struct
static void loadBandState(uint8_t bandIndex) {
    BandStatePacked state;
    Band& band = g_bandList[bandIndex];

    uint16_t addr = (uint16_t)EEPROM_BANDS_START
        + (uint16_t)bandIndex * (uint16_t)sizeof(BandStatePacked);
    eeprom_read_block(&state, (const void*)addr, sizeof(BandStatePacked));

    band.currentFreq = state.currentFreq;

    // Unpack step and bandwidth from their respective bytes
    band.stepIdxAM = UNPACK_LO(state.packed_am);
    band.bwIdxAM = UNPACK_HI(state.packed_am);

    band.stepIdxSSB = UNPACK_LO(state.packed_ssb);
    band.bwIdxSSB = UNPACK_HI(state.packed_ssb);

    band.stepIdxFM = UNPACK_LO(state.packed_fm);
    band.bwIdxFM = UNPACK_HI(state.packed_fm);

    // Per-band BFO calibration + clamp
    band.bfoCal = state.bfoCal;
    if (band.bfoCal < BFO_CALIBRATION_MIN || band.bfoCal > BFO_CALIBRATION_MAX)
        band.bfoCal = 0;

    // Boundary checks
    clamp_index(band.bwIdxSSB, g_bwSSBMaxIdx, true);
    clamp_index(band.bwIdxAM, g_maxFilterAM, true);
    clamp_index(band.bwIdxFM, (int8_t)MAX_INDEX(bw_fm_map), true);
    clamp_index(band.stepIdxAM, (int8_t)(AM_STEPS_COUNT - 1), true);
    clamp_index(band.stepIdxSSB, (int8_t)(SSB_STEPS_COUNT - 1), true);
    clamp_index(band.stepIdxFM, g_lastStepFM, true);
}

// On partial saves only write current band state to reduce EEPROM wear
static inline void saveBands(bool full_save) {
    CHECK_EEPROM_WEAR();

    if (full_save) {
        for (uint8_t i = 0; i < g_bandCount; ++i)
            saveBandState(i);
    } else {
        saveBandState(g_bandIndex);
    }
}

// Helper to load all band configurations at startup
static inline void loadBands() {
    for (uint8_t i = 0; i < g_bandCount; ++i)
        loadBandState(i);
}

// --- Favorites ---
#if ENABLE_FAVORITES
// Write the entire list of favorite stations to EEPROM
static void saveFavorites() {
    CHECK_EEPROM_WEAR();

    // Local clamped count to avoid EEPROM OOB writes if RAM gets corrupted
    uint8_t count = (g_totalFavorites > MAX_FAVORITES) ? MAX_FAVORITES : g_totalFavorites;

    eeprom_update_byte((uint8_t*)EEPROM_FAVORITES_COUNT, count);

    for (uint8_t i = 0; i < count; ++i) {
        uint16_t addr = (uint16_t)EEPROM_FAVORITES_START
            + (uint16_t)i * (uint16_t)sizeof(FavoriteStation);
        eeprom_update_block(&g_favorites[i], (void*)addr, sizeof(FavoriteStation));
    }
}

// Read favorite stations from EEPROM, handling uninitialized data
static void loadFavorites() {
    g_totalFavorites = eeprom_read_byte((const uint8_t*)EEPROM_FAVORITES_COUNT);

    // Sanity check favorite count to handle uninitialized EEPROM
    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FAVORITES) {
        g_totalFavorites = 0;
        g_favoriteSelected = 0;
        if (!g_eepromBad) saveFavorites();
        return;
    }

    for (uint8_t i = 0; i < g_totalFavorites; ++i) {
        uint16_t addr = (uint16_t)EEPROM_FAVORITES_START
            + (uint16_t)i * (uint16_t)sizeof(FavoriteStation);
        eeprom_read_block(&g_favorites[i], (const void*)addr, sizeof(FavoriteStation));

        // Clamp invalid modulation
        if (g_favorites[i].modulation > FM)
            g_favorites[i].modulation = AM;
    }

    g_favoriteSelected = 0;
}
#endif

// Save or load mode-specific settings as a single block
static inline void handleModeSettingsEEPROM(bool save) {
    if (save) {
        CHECK_EEPROM_WEAR();
        eeprom_update_block(
            g_modeSettings,
            (void*)EEPROM_MODE_SETTINGS_START,
            sizeof(g_modeSettings)
        );
    } else {
        eeprom_read_block(
            g_modeSettings,
            (const void*)EEPROM_MODE_SETTINGS_START,
            sizeof(g_modeSettings)
        );
    }
}

#undef PACK4
#undef UNPACK_LO
#undef UNPACK_HI

// ==========================================
// ===== SETTINGS VALIDATION ================
// ==========================================

// Validate all loaded settings against hardware limits
// Called once after EEPROM load to ensure RAM state is clean
// Prevents sending 0xFFFF (from EEPROM 0xFF -> int8_t -1 -> uint16_t 0xFFFF)
// to Si4735 reserved bit fields which would violate AN332 specifications
static void validateLoadedSettings() {

    // FM Soft Mute
    if ((uint8_t)getSettingParam(FmSmAtt) > FM_SOFT_MUTE_MAX_ATTN_LEVEL)
        setSettingParam(FmSmAtt, FM_SOFT_MUTE_DEFAULT_ATT);

    if ((uint8_t)getSettingParam(FmSmThr) > FM_SOFT_MUTE_MAX_SNR_LEVEL)
        setSettingParam(FmSmThr, FM_SOFT_MUTE_DEFAULT_THR);

    // AM/SSB Soft Mute Threshold
    if ((uint8_t)getSettingParam(SoftMuteThr) > SOFT_MUTE_MAX_SNR_THRESHOLD)
        setSettingParam(SoftMuteThr, 0);

    // Squelch
    if ((uint8_t)getSettingParam(SQL) > SQUELCH_MAX_LEVEL)
        setSettingParam(SQL, 0);

    // Mode-specific settings validation
    // These are stored separately from g_SettingsParams and need individual checks
    for (uint8_t ctx = 0; ctx < MODE_CONTEXT_COUNT; ++ctx) {

        // Soft Mute Attenuation
        if ((uint8_t)g_modeSettings[MODE_SETTING_SOFT_MUTE][ctx] > SOFT_MUTE_MAX_ATTENUATION)
            g_modeSettings[MODE_SETTING_SOFT_MUTE][ctx] = DEFAULT_MODE_SETTINGS.soft_mute;

        // AVC index
        if ((uint8_t)g_modeSettings[MODE_SETTING_AVC][ctx] > AVC_MAX_INDEX)
            g_modeSettings[MODE_SETTING_AVC][ctx] = DEFAULT_MODE_SETTINGS.avc;

        // AGC/ATT
        if ((uint8_t)g_modeSettings[MODE_SETTING_AGC][ctx] > MAX_ATTENUATION_AM_DB)
            g_modeSettings[MODE_SETTING_AGC][ctx] = DEFAULT_MODE_SETTINGS.agc;
    }
}

// ==========================================
// ===== MAIN ORCHESTRATORS =================
// ==========================================

// Main entry point for writing all receiver state to EEPROM
static void saveAllReceiverInformation(bool full_save = true) {
    if (g_eepromBad) {
        g_stateIsDirty = false;
        return;
    }

    syncActiveStateToBand();

    // Skip write if frequency is unchanged on partial saves to reduce wear
    if (!full_save && g_currentFrequency == g_lastSavedFrequency) {
        g_stateIsDirty = false;
        return;
    }

    eeprom_update_byte((uint8_t*)EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);
    eeprom_update_byte((uint8_t*)EEPROM_VERSION_ADDRESS, APP_VERSION);

    ReceiverHeader header;
    header.volume = g_muteVolume > 0 ? g_muteVolume : g_volume;
    header.bandIndex = g_bandIndex;
    header.currentMode = g_currentMode;
    header.currentBFO = g_currentBFO;
    header.lastCWMode = g_lastCWMode;
    header.lastSsbMode = g_lastSsbMode;
    eeprom_update_block(&header, (void*)EEPROM_HEADER_START, sizeof(header));

    saveBands(full_save);

    if (full_save) {
        for (uint8_t i = 0; i < SETTINGS_MAX; ++i) {
            eeprom_update_byte(
                (uint8_t*)(EEPROM_SETTINGS_START + i),
                getSettingParam(i)
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
// using compile-time defaults in g_SettingsMeta as source of truth
static void readAllReceiverInformation() {
    // Validate EEPROM data with magic bytes and version, reset to defaults if invalid
    if (eeprom_read_byte((const uint8_t*)EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID ||
        eeprom_read_byte((const uint8_t*)EEPROM_VERSION_ADDRESS) != APP_VERSION) {

        // init ALL mode contexts (AM/LSB/USB)
        initModeSettingsDefaults();

        // Initialize settings params from PROGMEM defaults
        initSettingsDefaults();

#if ENABLE_FAVORITES
        g_totalFavorites = 0;
#endif
        // Now both RAM arrays are pristine and consistent, save them
        saveAllReceiverInformation(true);

        // Check if write was successful
        if (eeprom_read_byte((const uint8_t*)EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID)
            g_eepromBad = true;

#if ENABLE_EEPROM_RESET_MSG
        drawEepromResetMsg();
#endif

        loadActiveStateFromBand();
        return;
    }

    // Normal boot path - load all data from valid EEPROM
    ReceiverHeader header;
    eeprom_read_block(&header, (const void*)EEPROM_HEADER_START, sizeof(header));

    g_volume = header.volume;
    g_bandIndex = header.bandIndex;

    if (g_bandIndex > g_lastBand) g_bandIndex = 1; // clamp to valid range
    g_currentMode = header.currentMode > FM ? AM : header.currentMode;

    g_currentBFO = header.currentBFO;
    g_lastCWMode = header.lastCWMode;
    g_lastSsbMode = header.lastSsbMode;

    // clamp invalid last SSB mode (prevents g_currentMode becoming invalid on AM->SSB cycle)
    if (g_lastSsbMode != USB) g_lastSsbMode = LSB;

    loadBands();

    // Load settings bytes from EEPROM into the params buffer
    // EEPROM stores bytes, g_SettingsParams[] is int8_t, so 0xFF becomes -1, etc
    for (uint8_t i = 0; i < SETTINGS_MAX; ++i) {
        setSettingParam(i, (int8_t)eeprom_read_byte(
            (const uint8_t*)(EEPROM_SETTINGS_START + i)
        ));
    }

    // Ensure CPU speed setting is valid after loading from EEPROM
    if ((uint8_t)getSettingParam(CPUSpeed) > 1)
        setSettingParam(CPUSpeed, 0);

    // Brightness is used as LUT index (0..9)
    if ((uint8_t)getSettingParam(Brightness) > BRIGHTNESS_MAX_LEVEL)
        setSettingParam(Brightness, 4);

    // DisplayOff indexes T[0..4]
    if ((uint8_t)getSettingParam(DisplayOff) > DISPLAY_OFF_TIMER_MAX_LEVEL)
        setSettingParam(DisplayOff, 0);

    handleModeSettingsEEPROM(false);

    validateLoadedSettings();

    applyBrightness();

#if ENABLE_FAVORITES
    loadFavorites();
#endif

    loadActiveStateFromBand();
    g_previousFrequency = g_currentFrequency;
    if (isSSB()) loadSSBPatch();

    g_lastSavedFrequency = g_currentFrequency;
}
