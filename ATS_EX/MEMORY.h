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
// These checks are meant to fail compilation early if EEPROM layout, struct packing,
// or dependent constants change in a way that would corrupt persistent data.
//
// Rules:
// - END values below are end-exclusive start plus size
// - Monotonic block ordering must hold
// - Each block must fit before the next one
// - The final used address must not exceed device EEPROM size
//

#define FIELD_SIZE(T, f) (sizeof(((T*)0)->f))

// ---------- Struct packing / binary format contracts ----------
static_assert(sizeof(ReceiverHeader) == 7, "ReceiverHeader size must stay 7 bytes");
static_assert(
    sizeof(ReceiverHeader) ==
    FIELD_SIZE(ReceiverHeader, volume) +
    FIELD_SIZE(ReceiverHeader, bandIndex) +
    FIELD_SIZE(ReceiverHeader, currentMode) +
    FIELD_SIZE(ReceiverHeader, currentBFO) +
    FIELD_SIZE(ReceiverHeader, lastCWMode) +
    FIELD_SIZE(ReceiverHeader, lastSsbMode),
    "ReceiverHeader packing changed unexpected padding"
    );

static_assert(sizeof(BandStatePacked) == 6, "BandStatePacked size must stay 6 bytes");
static_assert(
    sizeof(BandStatePacked) ==
    FIELD_SIZE(BandStatePacked, currentFreq) +
    FIELD_SIZE(BandStatePacked, packed_am) +
    FIELD_SIZE(BandStatePacked, packed_ssb) +
    FIELD_SIZE(BandStatePacked, packed_fm) +
    FIELD_SIZE(BandStatePacked, bfoCal),
    "BandStatePacked packing changed unexpected padding"
    );

// g_modeSettings is a raw int8 table MODE_SETTINGS_COUNT times MODE_CONTEXT_COUNT bytes
static_assert(
    sizeof(g_modeSettings) ==
    (uint16_t)MODE_SETTINGS_COUNT * (uint16_t)MODE_CONTEXT_COUNT * sizeof(g_modeSettings[0][0]),
    "g_modeSettings size mismatch"
    );

#if ENABLE_FAVORITES
static_assert(
    sizeof(FavoriteStation) ==
    FIELD_SIZE(FavoriteStation, frequency) +
    FIELD_SIZE(FavoriteStation, modulation) +
    FIELD_SIZE(FavoriteStation, bfo),
    "FavoriteStation packing changed expected packed struct"
    );
#endif

#undef FIELD_SIZE

// ---------- Global sanity ----------
static_assert(g_bandCount > 0, "g_bandCount must be > 0");
static_assert(g_bandCount <= 255, "g_bandCount must fit in uint8_t");
static_assert(g_lastBand == (g_bandCount - 1), "g_lastBand must equal g_bandCount - 1");
static_assert(SETTINGS_MAX > 0, "SETTINGS_MAX must be > 0");

// ---------- Derived block ends end-exclusive ----------
constexpr uint16_t EEPROM_HEADER_END =
(uint16_t)EEPROM_HEADER_START + (uint16_t)sizeof(ReceiverHeader);

constexpr uint16_t EEPROM_BANDS_END =
(uint16_t)EEPROM_BANDS_START + (uint16_t)g_bandCount * (uint16_t)sizeof(BandStatePacked);

constexpr uint16_t EEPROM_SETTINGS_END =
(uint16_t)EEPROM_SETTINGS_START + (uint16_t)SETTINGS_MAX;

constexpr uint16_t EEPROM_MODE_SETTINGS_END =
(uint16_t)EEPROM_MODE_SETTINGS_START + (uint16_t)sizeof(g_modeSettings);

#if ENABLE_FAVORITES
constexpr uint16_t EEPROM_FAVORITES_END =
(uint16_t)EEPROM_FAVORITES_START + (uint16_t)MAX_FAVORITES * (uint16_t)sizeof(FavoriteStation);
#endif

// ---------- EEPROM address monotonicity prevents overlaps by ordering ----------
static_assert(EEPROM_APP_ID_ADDRESS != EEPROM_VERSION_ADDRESS, "EEPROM core addresses must be distinct");
static_assert(EEPROM_APP_ID_ADDRESS < EEPROM_VERSION_ADDRESS, "EEPROM core addresses order invalid");
static_assert(EEPROM_VERSION_ADDRESS < EEPROM_HEADER_START, "EEPROM header must start after version byte");

static_assert(EEPROM_HEADER_START < EEPROM_BANDS_START, "EEPROM_HEADER_START must be before bands block");
static_assert(EEPROM_BANDS_START < EEPROM_SETTINGS_START, "EEPROM_BANDS_START must be before settings block");
static_assert(EEPROM_SETTINGS_START < EEPROM_MODE_SETTINGS_START, "EEPROM_SETTINGS_START must be before mode-settings block");
static_assert(EEPROM_MODE_SETTINGS_START < EEPROM_FAVORITES_START, "EEPROM_MODE_SETTINGS_START must be before favorites block");
static_assert(EEPROM_FAVORITES_START < EEPROM_FAVORITES_COUNT, "EEPROM_FAVORITES_START must be before favorites count");

// ---------- Block boundary checks prevents overlaps by size math ----------
static_assert(EEPROM_HEADER_END <= (uint16_t)EEPROM_BANDS_START,
    "ReceiverHeader overlaps bands block");

static_assert(EEPROM_BANDS_END <= (uint16_t)EEPROM_SETTINGS_START,
    "Bands block overlaps settings block update EEPROM addresses in Defines.h");

static_assert(EEPROM_SETTINGS_END <= (uint16_t)EEPROM_MODE_SETTINGS_START,
    "Settings block overlaps mode-settings block update EEPROM addresses in Defines.h");

static_assert(EEPROM_MODE_SETTINGS_END <= (uint16_t)EEPROM_FAVORITES_START,
    "Mode-settings block overlaps favorites block update EEPROM addresses in Defines.h");

#if ENABLE_FAVORITES
static_assert(MAX_FAVORITES > 0, "MAX_FAVORITES must be > 0");

static_assert(EEPROM_FAVORITES_END == (uint16_t)EEPROM_FAVORITES_COUNT,
    "EEPROM_FAVORITES_COUNT must be placed immediately after favorites list");
#endif

// ---------- Device EEPROM size hard limit ----------
// AVR EEPROM last address ATmega328P 0..1023
static_assert((uint16_t)EEPROM_FAVORITES_COUNT <= (uint16_t)E2END,
    "EEPROM layout exceeds device EEPROM size");

// ==========================================
// ===== COMPONENT-LEVEL STATE HANDLERS =====
// ==========================================

#define PACK4(lo, hi)   (uint8_t)(((uint8_t)(lo) & 0x0F) | (((uint8_t)(hi) & 0x0F) << 4))
#define UNPACK_LO(v)    ((v) & 0x0F)
#define UNPACK_HI(v)    (((v) >> 4) & 0x0F)

// --- Band State ---
// Pack runtime band data into a compact struct and save to EEPROM
static void saveBandState(uint8_t bandIndex) {

    BandStatePacked state;
    const Band& band = g_bandList[bandIndex];

    state.currentFreq = band.currentFreq;

    // Pack 2x 4-bit values (lo/hi) into one byte
    //   packed = (lo & 0x0F) | ((hi & 0x0F) << 4)
    //
    // use swap to implement (hi << 4) cheaply and avoid larger
    // mul-by-16 codegen under -Os
    uint8_t lo, hi;

    // AM [stepIdxAM | bwIdxAM]
    lo = (uint8_t)band.stepIdxAM & 0x0F;
    hi = (uint8_t)band.bwIdxAM & 0x0F;
    __asm__ __volatile__("swap %0" : "+r"(hi));   // hi = hi << 4
    state.packed_am = (uint8_t)(lo | hi);

    // SSB [stepIdxSSB | bwIdxSSB]
    lo = (uint8_t)band.stepIdxSSB & 0x0F;
    hi = (uint8_t)band.bwIdxSSB & 0x0F;
    __asm__ __volatile__("swap %0" : "+r"(hi));
    state.packed_ssb = (uint8_t)(lo | hi);

    // FM [stepIdxFM | bwIdxFM]
    lo = (uint8_t)band.stepIdxFM & 0x0F;
    hi = (uint8_t)band.bwIdxFM & 0x0F;
    __asm__ __volatile__("swap %0" : "+r"(hi));
    state.packed_fm = (uint8_t)(lo | hi);

    state.bfoCal = band.bfoCal;

    const uint16_t addr = (uint16_t)EEPROM_BANDS_START
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

    // step unpack and bandwidth into locals first to keep clamping in registers
    // GCC avoid reloading the Band pointer (Z) for each clamp
    int8_t stepIdxAM  = (int8_t)UNPACK_LO(state.packed_am);
    int8_t bwIdxAM    = (int8_t)UNPACK_HI(state.packed_am);
    int8_t stepIdxSSB = (int8_t)UNPACK_LO(state.packed_ssb);
    int8_t bwIdxSSB   = (int8_t)UNPACK_HI(state.packed_ssb);
    int8_t stepIdxFM  = (int8_t)UNPACK_LO(state.packed_fm);
    int8_t bwIdxFM    = (int8_t)UNPACK_HI(state.packed_fm);

    // bounds cheks (in registers - no need to reload Band pointer)
    if (bwIdxSSB   > g_bwSSBMaxIdx)                 bwIdxSSB   = 0;
    if (bwIdxAM    > g_maxFilterAM)                 bwIdxAM    = 0;
    if (bwIdxFM    > (int8_t)MAX_INDEX(bw_fm_map))  bwIdxFM    = 0;
    if (stepIdxAM  > (int8_t)(AM_STEPS_COUNT - 1))  stepIdxAM  = 0;
    if (stepIdxSSB > (int8_t)(SSB_STEPS_COUNT - 1)) stepIdxSSB = 0;
    if (stepIdxFM  > g_lastStepFM)                  stepIdxFM  = 0;

    // to RAM
    band.stepIdxAM  = stepIdxAM;
    band.bwIdxAM    = bwIdxAM;
    band.stepIdxSSB = stepIdxSSB;
    band.bwIdxSSB   = bwIdxSSB;
    band.stepIdxFM  = stepIdxFM;
    band.bwIdxFM    = bwIdxFM;

    // Per-band BFO calibration + clamp
    band.bfoCal = state.bfoCal;
    if (band.bfoCal < BFO_CALIBRATION_MIN || band.bfoCal > BFO_CALIBRATION_MAX)
        band.bfoCal = 0;

}

// On partial saves only write current band state to reduce EEPROM wear
static inline void saveBands(bool full_save) {

    if (full_save) {
        for (uint8_t i = 0; i < g_bandCount; ++i)
            saveBandState(i);
    } else {
        saveBandState(g_bandIndex);

        // If SWLink is enabled and we are on SW band also save SW master band
        if ((uint8_t)getSettingParam(SWLink) != 0 &&
            currentBandPtr()->bandType == SW_BAND_TYPE &&
            g_bandIndex != SW_MASTER_BAND_INDEX) {
            saveBandState(SW_MASTER_BAND_INDEX);
        }
    }
}

// Helper to load all band configurations at startup
static inline void loadBands() {
    for (uint8_t i = 0; i < g_bandCount; ++i)
        loadBandState(i);
}

// Save or load mode-specific settings as a single block
static inline void handleModeSettingsEEPROM(bool save) {
    if (save) {

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

// Validate mode-dependent settings (AGC/SoftMute/AVC) after EEPROM load
// Catches EEPROM corruption (0xFF → -1 → 0xFFFF)
// that would send invalid values to Si4735 prop and violate AN332 spec
static void validateLoadedSettings() {

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

    // Write APP_ID + VERSION as one 16-bit word
    // EEPROM[0] = APP_ID (low byte), EEPROM[1] = APP_VERSION (high byte)
    const uint16_t idver = (uint16_t)EEPROM_APP_ID | ((uint16_t)APP_VERSION << 8);
    eeprom_update_word((uint16_t*)EEPROM_APP_ID_ADDRESS, idver);

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
        // Settings params are stored as a contiguous int8_t array in RAM,
        // and EEPROM settings block is contiguous as well
        eeprom_update_block(
            g_SettingsParams,
            (void*)EEPROM_SETTINGS_START,
            (uint16_t)SETTINGS_MAX
        );

        handleModeSettingsEEPROM(true);

#if ENABLE_FAVORITES
        saveFavorites();
#endif
    }

    saveLastFreq();
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
    eeprom_read_block(
        g_SettingsParams,
        (const void*)EEPROM_SETTINGS_START,
        SETTINGS_MAX
    );

    handleModeSettingsEEPROM(false);

    // Validate all settings by table
    for (uint8_t i = 0; i < ARRAY_SIZE(g_clampTable); i++) {
        uint8_t idx = pgm_read_byte(&g_clampTable[i].idx);
        uint8_t max = pgm_read_byte(&g_clampTable[i].max);
        uint8_t def = pgm_read_byte(&g_clampTable[i].def);

        if ((uint8_t)getSettingParam(idx) > max)
            setSettingParam(idx, def);
    }

    validateLoadedSettings();

#if ENABLE_FAVORITES
    loadFavorites();
#endif

    loadActiveStateFromBand();

    if (isSSB()) loadSSBPatch();

    saveLastFreq();
}
