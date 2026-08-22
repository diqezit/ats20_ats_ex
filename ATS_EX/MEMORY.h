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

#define FOR_EACH_BAND(i) for (uint8_t i = 0; i < g_bandCount; ++i)
#define CLIP_MODE(slot, maxv, def) \
    clip(g_modeSettings[slot][ctx], (maxv), (def))
#define EE_XFER(do_save, ptr, addr, nbytes)                    \
    do {                                                       \
        if (do_save) EE_UPDATE_BLOCK((ptr), (addr), (nbytes)); \
        else         EE_READ_BLOCK((ptr), (addr), (nbytes));   \
    } while (0)

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
    oled_cls();
    oled_xy(37, 3);
    oled_puts(g_eepromBad ? F("MEM WEAR") : F("MEM RESET"));
    delay(2000);
}
#endif

// ==========================================
// ===== DATA STRUCTURES FOR EEPROM =========
// ==========================================

// Main configuration data, grouped for a single EEPROM write
struct PACKED ReceiverHeader {
    uint8_t volume;
    uint8_t bandIndex;
    uint8_t currentMode;
    int16_t currentBFO;
    uint8_t lastCWMode;
    uint8_t lastSsbMode;  // save USB/LSB
};

// Band settings are bit-packed to save EEPROM space
struct PACKED BandStatePacked {
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
#define ASSERT_PACKED(T, n, sum) \
    static_assert(sizeof(T) == (n), #T " size must stay " #n " bytes"); \
    static_assert(sizeof(T) == (sum), #T " packing changed unexpected padding")
#define ASSERT_LT(a, b, msg) static_assert((uint16_t)(a) <  (uint16_t)(b), msg)
#define ASSERT_LE(a, b, msg) static_assert((uint16_t)(a) <= (uint16_t)(b), msg)

// ---------- Struct packing / binary format contracts ----------
ASSERT_PACKED(ReceiverHeader, 7,
    FIELD_SIZE(ReceiverHeader, volume) +
    FIELD_SIZE(ReceiverHeader, bandIndex) +
    FIELD_SIZE(ReceiverHeader, currentMode) +
    FIELD_SIZE(ReceiverHeader, currentBFO) +
    FIELD_SIZE(ReceiverHeader, lastCWMode) +
    FIELD_SIZE(ReceiverHeader, lastSsbMode));

ASSERT_PACKED(BandStatePacked, 6,
    FIELD_SIZE(BandStatePacked, currentFreq) +
    FIELD_SIZE(BandStatePacked, packed_am) +
    FIELD_SIZE(BandStatePacked, packed_ssb) +
    FIELD_SIZE(BandStatePacked, packed_fm) +
    FIELD_SIZE(BandStatePacked, bfoCal));

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
#undef ASSERT_PACKED

// ---------- Global sanity ----------
static_assert(g_bandCount > 0, "g_bandCount must be > 0");
static_assert(g_bandCount <= 255, "g_bandCount must fit in uint8_t");
static_assert(g_lastBand == (g_bandCount - 1), "g_lastBand must equal g_bandCount - 1");
static_assert(SETTINGS_MAX > 0, "SETTINGS_MAX must be > 0");

// ---------- Derived block ends end-exclusive ----------
constexpr uint16_t EEPROM_HEADER_END =
    EE_END(EEPROM_HEADER_START, sizeof(ReceiverHeader));

constexpr uint16_t EEPROM_BANDS_END =
    EE_END(EEPROM_BANDS_START, (uint16_t)g_bandCount * (uint16_t)sizeof(BandStatePacked));

constexpr uint16_t EEPROM_SETTINGS_END =
    EE_END(EEPROM_SETTINGS_START, SETTINGS_MAX);

constexpr uint16_t EEPROM_MODE_SETTINGS_END =
    EE_END(EEPROM_MODE_SETTINGS_START, sizeof(g_modeSettings));

#if ENABLE_FAVORITES
constexpr uint16_t EEPROM_FAVORITES_END =
    EE_END(EEPROM_FAVORITES_START, (uint16_t)MAX_FAVORITES * (uint16_t)sizeof(FavoriteStation));
#endif

// ---------- EEPROM address monotonicity prevents overlaps by ordering ----------
static_assert(EEPROM_APP_ID_ADDRESS != EEPROM_VERSION_ADDRESS, "EEPROM core addresses must be distinct");
ASSERT_LT(EEPROM_APP_ID_ADDRESS,      EEPROM_VERSION_ADDRESS,     "EEPROM core addresses order invalid");
ASSERT_LT(EEPROM_VERSION_ADDRESS,     EEPROM_HEADER_START,        "EEPROM header must start after version byte");
ASSERT_LT(EEPROM_HEADER_START,        EEPROM_BANDS_START,         "EEPROM_HEADER_START must be before bands block");
ASSERT_LT(EEPROM_BANDS_START,         EEPROM_SETTINGS_START,      "EEPROM_BANDS_START must be before settings block");
ASSERT_LT(EEPROM_SETTINGS_START,      EEPROM_MODE_SETTINGS_START, "EEPROM_SETTINGS_START must be before mode-settings block");
ASSERT_LT(EEPROM_MODE_SETTINGS_START, EEPROM_FAVORITES_START,     "EEPROM_MODE_SETTINGS_START must be before favorites block");
ASSERT_LT(EEPROM_FAVORITES_START,     EEPROM_FAVORITES_COUNT,     "EEPROM_FAVORITES_START must be before favorites count");

// ---------- Block boundary checks prevents overlaps by size math ----------
ASSERT_LE(EEPROM_HEADER_END,        EEPROM_BANDS_START,         "ReceiverHeader overlaps bands block");
ASSERT_LE(EEPROM_BANDS_END,         EEPROM_SETTINGS_START,      "Bands block overlaps settings block update EEPROM addresses in Defines.h");
ASSERT_LE(EEPROM_SETTINGS_END,      EEPROM_MODE_SETTINGS_START, "Settings block overlaps mode-settings block update EEPROM addresses in Defines.h");
ASSERT_LE(EEPROM_MODE_SETTINGS_END, EEPROM_FAVORITES_START,     "Mode-settings block overlaps favorites block update EEPROM addresses in Defines.h");

#if ENABLE_FAVORITES
static_assert(MAX_FAVORITES > 0, "MAX_FAVORITES must be > 0");
static_assert(EEPROM_FAVORITES_END == (uint16_t)EEPROM_FAVORITES_COUNT,
    "EEPROM_FAVORITES_COUNT must be placed immediately after favorites list");
#endif

// ---------- Device EEPROM size hard limit ----------
// AVR EEPROM last address ATmega328P 0..1023
ASSERT_LE(EEPROM_FAVORITES_COUNT, E2END, "EEPROM layout exceeds device EEPROM size");

#undef ASSERT_LT
#undef ASSERT_LE

// ==========================================
// ===== PACK / UNPACK HELPERS ==============
// ==========================================

INLINE_AI
uint16_t bandAddr(uint8_t i) {
    return EE_END(EEPROM_BANDS_START, (uint16_t)i * (uint16_t)sizeof(BandStatePacked));
}

// Pack 2x 4-bit values (lo/hi) into one byte
//   packed = (lo & 0x0F) | ((hi & 0x0F) << 4)
//
// use swap to implement (hi << 4) cheaply and avoid larger
// mul-by-16 codegen under -Os
INLINE_AI
uint8_t pack4(uint8_t lo, uint8_t hi) {
    lo &= 0x0F;
    hi &= 0x0F;
    __asm__ __volatile__("swap %0" : "+r"(hi));   // hi = hi << 4
    return (uint8_t)(lo | hi);
}

// Unpack one byte and clip each nibble to its max (else 0)
INLINE_AI
void unpack4(uint8_t packed, int8_t& lo, int8_t& hi, int8_t loMax, int8_t hiMax) {
    lo = (int8_t)(packed & 0x0F);
    hi = (int8_t)((packed >> 4) & 0x0F);
    if (lo > loMax) lo = 0;
    if (hi > hiMax) hi = 0;
}

// EEPROM byte as uint8: overflow / 0xFF → factory default
INLINE_AI
void clip(int8_t& v, uint8_t max, int8_t def) {
    if ((uint8_t)v > max) v = def;
}

INLINE_AI
void packBand(BandStatePacked& s, const Band& b) {
    s.currentFreq = b.currentFreq;
    s.packed_am  = pack4(b.stepIdxAM,  b.bwIdxAM);   // AM  [step | bw]
    s.packed_ssb = pack4(b.stepIdxSSB, b.bwIdxSSB);  // SSB [step | bw]
    s.packed_fm  = pack4(b.stepIdxFM,  b.bwIdxFM);   // FM  [step | bw]
    s.bfoCal     = b.bfoCal;
}

INLINE_AI
void unpackBand(const BandStatePacked& s, Band& b) {
    // step unpack and bandwidth into locals first to keep clamping in registers
    // GCC avoid reloading the Band pointer (Z) for each clamp
    int8_t stepAM, bwAM, stepSSB, bwSSB, stepFM, bwFM;
    unpack4(s.packed_am,  stepAM,  bwAM,  (int8_t)(AM_STEPS_COUNT - 1), g_maxFilterAM);
    unpack4(s.packed_ssb, stepSSB, bwSSB, (int8_t)(SSB_STEPS_COUNT - 1), g_bwSSBMaxIdx);
    unpack4(s.packed_fm,  stepFM,  bwFM,  g_lastStepFM, (int8_t)MAX_INDEX(bw_fm_map));

    b.currentFreq = s.currentFreq;
    b.stepIdxAM  = stepAM;
    b.bwIdxAM    = bwAM;
    b.stepIdxSSB = stepSSB;
    b.bwIdxSSB   = bwSSB;
    b.stepIdxFM  = stepFM;
    b.bwIdxFM    = bwFM;

    // Per-band BFO calibration + clamp
    b.bfoCal = s.bfoCal;
    if (b.bfoCal < BFO_CALIBRATION_MIN || b.bfoCal > BFO_CALIBRATION_MAX)
        b.bfoCal = 0;
}

// ==========================================
// ===== BAND STATE =========================
// ==========================================

// Pack runtime band data into a compact struct and save to EEPROM
static void saveBandState(uint8_t bandIndex) {
    BandStatePacked state;
    packBand(state, g_bandList[bandIndex]);
    EE_UPDATE_BLOCK(&state, bandAddr(bandIndex), sizeof(state));
}

// Read packed band data from EEPROM and expand into runtime struct
static void loadBandState(uint8_t bandIndex) {
    BandStatePacked state;
    EE_READ_BLOCK(&state, bandAddr(bandIndex), sizeof(state));
    unpackBand(state, g_bandList[bandIndex]);
}

// On partial saves only write current band state to reduce EEPROM wear
static inline void saveBands(bool full_save) {

    if (full_save) {
        FOR_EACH_BAND(i)
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
    FOR_EACH_BAND(i)
        loadBandState(i);
}

// ==========================================
// ===== MODE SETTINGS BLOCK ================
// ==========================================

// Save or load mode-specific settings as a single block
INLINE_AI
void handleModeSettingsEEPROM(bool save) {
    EE_XFER(save, g_modeSettings, EEPROM_MODE_SETTINGS_START, sizeof(g_modeSettings));
}

// ==========================================
// ===== SETTINGS VALIDATION ================
// ==========================================

// Validate mode-dependent settings (AGC/SoftMute/AVC) after EEPROM load
// Catches EEPROM corruption (0xFF → -1 → 0xFFFF)
// that would send invalid values to Si4735 prop and violate AN332 spec
static void validateLoadedSettings() {

    // SETTING_NO_CLAMP (255): (uint8_t)val > 255 is never true (keeps signed BFO)
    for (uint8_t i = 0; i < SETTINGS_MAX; i++)
        clip(g_SettingsParams[i], getSettingMax(i), getSettingDefault(i));

    // Mode-specific settings live outside g_SettingsParams
    // Limits stay immediates (cpi), not PROGMEM — cheaper than getSettingMax()
    for (uint8_t ctx = 0; ctx < MODE_CONTEXT_COUNT; ++ctx) {
        CLIP_MODE(MODE_SETTING_SOFT_MUTE, SOFT_MUTE_MAX_ATTENUATION, DEFAULT_MODE_SETTINGS.soft_mute);
        CLIP_MODE(MODE_SETTING_AVC,       AVC_MAX_INDEX,             DEFAULT_MODE_SETTINGS.avc);
        CLIP_MODE(MODE_SETTING_AGC,       MAX_ATTENUATION_AM_DB,     DEFAULT_MODE_SETTINGS.agc);
    }
}

// ==========================================
// ===== HEADER I/O =========================
// ==========================================

INLINE_AI
void fillHdr(ReceiverHeader& h) {
    h.volume      = g_muteVolume > 0 ? g_muteVolume : g_volume;
    h.bandIndex   = g_bandIndex;
    h.currentMode = g_currentMode;
    h.currentBFO  = g_currentBFO;
    h.lastCWMode  = g_lastCWMode;
    h.lastSsbMode = g_lastSsbMode;
}

INLINE_AI
void applyHdr(const ReceiverHeader& h) {
    g_volume    = h.volume;
    g_bandIndex = h.bandIndex;
    if (g_bandIndex > g_lastBand) g_bandIndex = 1; // clamp to valid range
    g_currentMode = h.currentMode > FM ? AM : h.currentMode;
    g_currentBFO  = h.currentBFO;
    g_lastCWMode  = h.lastCWMode;
    g_lastSsbMode = h.lastSsbMode;
    // clamp invalid last SSB mode (prevents g_currentMode becoming invalid on AM->SSB cycle)
    if (g_lastSsbMode != USB) g_lastSsbMode = LSB;
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
    EE_UPDATE16(EEPROM_APP_ID_ADDRESS, idver);

    ReceiverHeader header;
    fillHdr(header);
    EE_UPDATE_BLOCK(&header, EEPROM_HEADER_START, sizeof(header));

    saveBands(full_save);

    if (full_save) {
        // Settings params are stored as a contiguous int8_t array in RAM,
        // and EEPROM settings block is contiguous as well
        EE_UPDATE_BLOCK(
            g_SettingsParams,
            EEPROM_SETTINGS_START,
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
    if (EE_READ8(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID ||
        EE_READ8(EEPROM_VERSION_ADDRESS) != APP_VERSION) {

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
        if (EE_READ8(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID)
            g_eepromBad = true;

#if ENABLE_EEPROM_RESET_MSG
        drawEepromResetMsg();
#endif

        loadActiveStateFromBand();
        return;
    }

    // Normal boot path - load all data from valid EEPROM
    ReceiverHeader header;
    EE_READ_BLOCK(&header, EEPROM_HEADER_START, sizeof(header));
    applyHdr(header);

    loadBands();

    // Load settings bytes from EEPROM into the params buffer
    // EEPROM stores bytes, g_SettingsParams[] is int8_t, so 0xFF becomes -1, etc
    EE_READ_BLOCK(
        g_SettingsParams,
        EEPROM_SETTINGS_START,
        SETTINGS_MAX
    );

    handleModeSettingsEEPROM(false);

    validateLoadedSettings();

#if ENABLE_FAVORITES
    loadFavorites();
#endif

    loadActiveStateFromBand();

    if (isSSB()) loadSSBPatch();

    saveLastFreq();
}

#undef CLIP_MODE
#undef FOR_EACH_BAND
#undef EE_XFER
