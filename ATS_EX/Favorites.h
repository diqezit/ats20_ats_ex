#pragma once

// ====================================================================================
//
// Favorites.h - Favorites Station Management
//
// Self-contained module for all favorites functionality:
//   - CRUD operations (add, delete, find, exists)
//   - EEPROM persistence (save, load)
//   - UI rendering (list, header, frequency display)
//   - Loop integration (timeout, mode handler)
//
// ====================================================================================

#if ENABLE_FAVORITES

// ====================================================================================
// ===== FAVORITES: EEPROM PERSISTENCE ================================================
// ====================================================================================

// Write the entire list of favorite stations to EEPROM
static void saveFavorites() {
    CHECK_EEPROM_WEAR();

    // Clamp count to prevent EEPROM OOB writes if RAM is corrupted
    const uint8_t count = (g_totalFavorites > MAX_FAVORITES) ? MAX_FAVORITES : g_totalFavorites;

    // Persist count first so loader knows how many records are valid
    eeprom_update_byte((uint8_t*)EEPROM_FAVORITES_COUNT, count);

    // Favorites are stored as a packed contiguous array in RAM and EEPROM:
    // - RAM:  g_favorites[0..MAX_FAVORITES-1]
    // - EEPROM: EEPROM_FAVORITES_START .. EEPROM_FAVORITES_START + MAX_FAVORITES*sizeof(FavoriteStation)
    const uint16_t len = (uint16_t)count * (uint16_t)sizeof(FavoriteStation);

    eeprom_update_block(
        (const void*)g_favorites,
        (void*)EEPROM_FAVORITES_START,
        len
    );
}

// Read favorite stations from EEPROM, handling uninitialized data
static void loadFavorites() {
    g_totalFavorites = eeprom_read_byte((const uint8_t*)EEPROM_FAVORITES_COUNT);

    // Sanity check favorite count to handle uninitialized EEPROM / corrupted value
    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FAVORITES) {
        g_totalFavorites = 0;
        g_favoriteSelected = 0;
        if (!g_eepromBad) saveFavorites();
        return;
    }

    // EEPROM favorites area is contiguous and FavoriteStation is packed (5 bytes)
    const uint16_t len = (uint16_t)g_totalFavorites * (uint16_t)sizeof(FavoriteStation);

    eeprom_read_block(
        (void*)g_favorites,
        (const void*)EEPROM_FAVORITES_START,
        len
    );

    // Clamp invalid modulation values (protects UI and band logic from bad EEPROM)
    for (uint8_t i = 0; i < g_totalFavorites; ++i) {
        if (g_favorites[i].modulation > FM)
            g_favorites[i].modulation = AM;
    }

    g_favoriteSelected = 0;
}

// ====================================================================================
// ===== FAVORITES: CRUD HELPERS ======================================================
// ====================================================================================

// check duplicate to keep list useful
inline static __attribute__((always_inline))
bool favoriteExists(uint16_t f, uint8_t m) {
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        if (g_favorites[i].frequency == f && g_favorites[i].modulation == m)
            return true;
    }
    return false;
}

// compact list after removal (shift left by one element)
//
// Removes the hole at index start by moving:
//   favorites[start]   <- favorites[start + 1]
//   ...
//   favorites[total-2] <- favorites[total - 1]
//
// copy is done byte-by-byte to keep AVR code size small (no per-struct index math)
// regions overlap but dst < src, so forward copy is safe (memmove-like)
inline static __attribute__((always_inline))
void compactFavoritesFrom(uint8_t start) {
    const uint8_t total = g_totalFavorites;

    // nothing to shift if start is the last valid element (or list is empty)
    if ((uint8_t)(start + 1) >= total) return;

    uint8_t* dst = (uint8_t*)&g_favorites[start];
    uint8_t* src = (uint8_t*)&g_favorites[start + 1];

    // bytes to move = number of trailing elements * sizeof(FavoriteStation)
    uint8_t n = (uint8_t)(total - start - 1) * (uint8_t)sizeof(FavoriteStation);

    while (n--) *dst++ = *src++;
}

// keep selection valid after delete
inline static __attribute__((always_inline))
void fixFavoriteSelectionAfterDelete() {
    if (g_totalFavorites == 0) {
        g_favoriteSelected = 0;
        return;
    }

    if (g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites - 1;
    }
}

// require full reconfig on FM<->AM or SSB patch need
inline static __attribute__((always_inline))
bool favoriteNeedsFullReset(
    BandType prevType, BandType newType, bool wantSSB, bool hadSSB) {
    return (prevType != newType) || (wantSSB && !hadSSB);
}

// Add current station details to RAM and set dirty flag
static void addFavorite() {
    if (g_totalFavorites >= MAX_FAVORITES) return;

    // Snapshot volatile/non-volatile inputs once
    const uint16_t f = g_currentFrequency;
    const uint8_t  m = (uint8_t)g_currentMode;
    const int16_t  b = (int16_t)g_currentBFO;

    // Walk list with a pointer; after the loop slot already points to the insert position
    FavoriteStation* slot = g_favorites;
    uint8_t n = g_totalFavorites;

    while (n--) {
        if (slot->frequency == f && slot->modulation == m) return;  // duplicate
        ++slot;  // advances by sizeof(FavoriteStation) == 5
    }

    // Insert at `slot` (no recompute total*5)
    slot->frequency = f;
    slot->modulation = m;
    slot->bfo = b;

    ++g_totalFavorites;
    g_favoritesDirty = true;
}

// Delete selected favorite from RAM and set dirty flag
static void deleteFavorite() {
    if (!g_totalFavorites) return;

    // Shift all subsequent items one position to the left to fill the gap
    compactFavoritesFrom(g_favoriteSelected);

    g_totalFavorites--;

    // If the last item was deleted, move the selection to the new last item
    fixFavoriteSelectionAfterDelete();

    g_favoritesDirty = true;
}

// ====================================================================================
// ===== FAVORITES: TUNE TO STATION ===================================================
// ====================================================================================

// Finds the band index whose frequency range and modulation family match the favorite
// Falls back to the current band if no match
static inline uint8_t findBandForFavorite(const FavoriteStation& fav) {
    const bool favIsFm = (fav.modulation == FM);
    for (uint8_t i = 0; i < g_bandCount; ++i) {
        const Band& b = g_bandList[i];
        if (favIsFm == (b.bandType == FM_BAND_TYPE) &&
            fav.frequency >= b.minimumFreq &&
            fav.frequency <= b.maximumFreq) {
            return i;
        }
    }
    return g_bandIndex;
}

/// Attempts a preset style FM retune without full reconfiguration
///
/// When the radio is already in FM and the selected favorite is FM in the same FM band
/// setFrequency is enough and avoids the click caused by full reconfiguration
///
/// Limited to FM to FM with no band change
/// Everything else returns false for the full path
static inline bool fav_tryTuneFastFmToFm(const FavoriteStation& fav, uint8_t targetBand) {
    if (g_currentMode != FM || fav.modulation != FM || targetBand != g_bandIndex) return false;

    Band* band = currentBandPtr();
    if (band->bandType != FM_BAND_TYPE) return false;

    const uint16_t f = fav.frequency;

    // Update band memory and live state so UI and saves stay consistent
    band->currentFreq = f;
    g_currentFrequency = f;
    g_currentBFO = 0;

    // Retune hardware without mode re init or amp toggling
    g_si4735.setFrequency(f);

    // Reset tune tracking so settle based tasks start fresh
    g_lastFreqChange = millis();
    g_processFreqChange = false;
    g_previousFrequency = f;
    g_signalQualityValue = INVALID_RSSI_VALUE;
    g_stereoStatus = false;

    return true;
}

/// Tunes to a favorite using the full reconfiguration path
///
/// Band or modulation changes require a complete hardware setup sequence
/// Seek limits step spacing AFC bandwidth and SSB patch handling all depend on it
/// The external amp is muted during the transition to reduce pops on mode switches
static void fav_tuneFullReconfig(const FavoriteStation& fav, uint8_t targetBand) {
    const BandType prevType = currentBandPtr()->bandType;
    const bool ssbWasReady = g_ssbLoaded;

    const uint8_t mod = fav.modulation;
    const uint16_t freq = fav.frequency;
    const int16_t bfo = fav.bfo;

    setAmpState(false);
    g_currentMode = mod;

    if (g_bandIndex != targetBand) {
        syncActiveStateToBand();
        g_bandIndex = targetBand;
    }

    // currentBandPtr reflects target band after potential switch
    Band* band = currentBandPtr();
    band->currentFreq = freq;
    g_currentBFO = bfo;

    // SSB modes sit between AM and FM in the modulation enum
    const bool wantSSB = (mod > AM && mod < FM);

    applyBandConfiguration(
        favoriteNeedsFullReset(prevType, band->bandType, wantSSB, ssbWasReady)
    );

    setAmpState(true);
}

/// Tunes to the selected favorite
///
/// Prefers the fast FM preset jump when it is safe
/// All other cases use the full reconfiguration path
void tuneToSelectedFavorite() {
    if (!g_totalFavorites) return;

    const FavoriteStation& fav = g_favorites[g_favoriteSelected];
    const uint8_t targetBand = findBandForFavorite(fav);

    if (fav_tryTuneFastFmToFm(fav, targetBand)) return;

    fav_tuneFullReconfig(fav, targetBand);
}

// ====================================================================================
// ===== FAVORITES: UI LAYOUT CONSTANTS ===============================================
// ====================================================================================

static constexpr uint8_t  UI_FAV_HEADER_ROW = 0;
static constexpr uint8_t  UI_FAV_LIST_START_ROW = 2;
static constexpr uint8_t  UI_FAV_ROW_GAP = 2;
static constexpr uint8_t  UI_FAV_ITEMS_PER_PG = 3;

// Mode label layout in favorites (right-aligned)
static constexpr uint8_t  UI_MODE_STR_W = 3;                                      // visible width of "AM ", "FM ", "USB", etc
static constexpr uint8_t  UI_FAV_MODE_LABEL_COL = UI_COLS - UI_MODE_STR_W;        // column for mode label (rightmost)
static constexpr uint8_t  UI_FAV_MODE_LABEL_X = UI_FAV_MODE_LABEL_COL * UI_CHAR_W;
static constexpr uint8_t  UI_FAV_PAGE_CLEAR_ROWS = 7;
static constexpr uint8_t  UI_FAV_HEADER_TITLE_WIDTH = 16;                         // "DEL:BW FAVORITES"
static constexpr uint8_t  UI_FAV_EMPTY_LIST_ROW = 4;
static constexpr uint8_t  UI_FAV_EMPTY_LIST_X = 36;

// Frequency right-align helpers for favorites
static constexpr uint8_t  UI_FAV_PREFIX_COLS = 4;                                 // ">NN " prefix
static constexpr uint8_t  UI_FAV_FREQ_GAP_COLS = 1;                               // one space gap before mode label
static constexpr uint8_t  UI_FAV_FREQ_RIGHT_COL = UI_FAV_MODE_LABEL_COL - UI_FAV_FREQ_GAP_COLS;
static constexpr uint8_t  UI_FAV_FREQ_MIN_COL = UI_FAV_PREFIX_COLS + 1;           // do not overlap prefix

// ====================================================================================
// ===== FAVORITES: UI PRINT HELPERS ==================================================
// ====================================================================================

// Print favorites prefix ">NN " or " NN "
// Used for: Favorites list line prefix
static inline void oledPrintFavPrefix(uint8_t idx, bool selected) {
    // one division only
    const DivMod10 dm = divmod10_u8(idx + 1);

    char b[5];
    b[0] = selected ? '>' : ' ';
    b[1] = (char)('0' + dm.q);
    b[2] = (char)('0' + dm.r);
    b[3] = ' ';
    b[4] = 0;
    oled.print(b);
}

static inline void favCounterBuild(uint8_t sel, uint8_t tot, char b[6]) {
    if (tot > 0) {
        // two divisions total for both numbers
        const DivMod10 dt = divmod10_u8(tot);
        const DivMod10 ds = divmod10_u8(sel);

        if (dt.q) {
            // tot >= 10  -> layout: [s10][s1]|[t10][t1]
            b[4] = (char)('0' + dt.r);
            b[3] = (char)('0' + dt.q);
            b[2] = '|';
            b[1] = (char)('0' + ds.r);
            if (ds.q) b[0] = (char)('0' + ds.q);
        } else {
            // tot < 10   -> layout: [ ][s10][s1]|[t1]
            b[4] = (char)('0' + dt.r);
            b[3] = '|';
            b[2] = (char)('0' + ds.r);
            if (ds.q) b[1] = (char)('0' + ds.q);
        }
    }
}

// Print favorites counter "XX|YY" right-aligned in 5 chars
// Used for: Favorites header counter
static inline void oledPrintFavCounter(uint8_t sel, uint8_t tot) {
    char b[6];
    b[0] = ' ';
    b[1] = ' ';
    b[2] = ' ';
    b[3] = ' ';
    b[4] = ' ';
    b[5] = 0;

    favCounterBuild(sel, tot, b);

    oled.print(b);
}

// ====================================================================================
// ===== FAVORITES: UI HELPER MACROS ==================================================
// ====================================================================================

// page for a given index
// simple math keeps navigation predictable
#define fav_pageOf(i) ((i) / UI_FAV_ITEMS_PER_PG)

// row index on current page
// fixed spacing keeps list readable on small OLED
#define fav_rowForIndex(i, ps) (UI_FAV_LIST_START_ROW + ((i) - (ps)) * UI_FAV_ROW_GAP)

// ====================================================================================
// ===== FAVORITES: UI DATA HELPERS ===================================================
// ====================================================================================

// page bounds [start, end)
// clamp to total to avoid out of range reads
static uint16_t __attribute__((noinline)) fav_getPageBounds(uint8_t page) {
    uint8_t start = (uint8_t)(page * UI_FAV_ITEMS_PER_PG);
    uint8_t end = (uint8_t)(start + UI_FAV_ITEMS_PER_PG);

    if (end > g_totalFavorites) end = g_totalFavorites;

    // packed: low = start, high = end
    return (uint16_t)start | ((uint16_t)end << 8);
}

static inline void fav_unpackPageBounds(uint8_t page, uint8_t& start, uint8_t& end) {
    const uint16_t bounds = fav_getPageBounds(page);
    start = (uint8_t)bounds;
    end = (uint8_t)(bounds >> 8);
}

// start column for right-aligned freq block
// prevents overlap with prefix and mode label
static inline uint8_t fav_calcFreqStartCol(uint8_t freqWidth) {
    uint8_t col = UI_FAV_FREQ_RIGHT_COL - freqWidth;
    if (col < UI_FAV_FREQ_MIN_COL) col = UI_FAV_FREQ_MIN_COL;
    return col;
}

// modes that show ".dd"
// user expects decimals only in SSB or CW
static inline __attribute__((always_inline))
bool fav_isSSB(uint8_t mod) {
    return (mod == LSB || mod == USB || mod == CW);
}

// apply BFO into kHz and decimal tail
// keep tail positive so decimals print stable
static inline __attribute__((always_inline))
void fav_splitBFO(int16_t bfo,
    uint16_t& khz,
    uint16_t& tail) {
    int16_t d = bfo / 1000;
    int16_t r = bfo % 1000;
    if (r < 0) { r += 1000; --d; }  // borrow one kHz
    khz += d;
    tail = (uint16_t)(r / 10);      // two decimals
}

// width for AM vs SSB/CW rows
// SSB uses ".dd" so width grows
static inline __attribute__((always_inline))
uint8_t fav_widthAMSSB(bool isSSB) {
    return isSSB ? 8 : 5;
}

// ====================================================================================
// ===== FAVORITES: UI RENDER FUNCTIONS ===============================================
// ====================================================================================

// FM freq right-aligned before mode label
// right align preserves visual grid across list
static inline void fav_drawFreqFM(const FavoriteStation& fav,
    uint8_t row) {
    uint16_t ip = fav.frequency / 100;   // 88-108
    uint8_t width = (ip < 100) ? 4 : 5;
    uint8_t startCol = fav_calcFreqStartCol(width);

    oled.setCursor(startCol * UI_CHAR_W, row);
    oledPrintFreqFM(fav.frequency);
}

// AM/SSB/CW freq with applied BFO and optional ".dd"
// show tuned freq so list matches what user hears
static inline void fav_drawFreqAMSSB(const FavoriteStation& fav,
    uint8_t row) {
    char buf[8];
    uint16_t khz = fav.frequency, tl = 0;

    bool isSSB = fav_isSSB(fav.modulation);
    if (isSSB) fav_splitBFO(fav.bfo, khz, tl);

    convertToChar(buf, khz, 5, 0, '.', ' ');

    uint8_t width = fav_widthAMSSB(isSSB);
    uint8_t startCol = fav_calcFreqStartCol(width);

    oled.setCursor(startCol * UI_CHAR_W, row);
    oled.print(buf);

    if (isSSB) oledPrintDotDec2((uint8_t)tl);
}

// mode label at fixed column (rightmost)
// fixed column makes scan easy on small OLED
static inline void fav_drawModeLabel(const FavoriteStation& fav,
    uint8_t row) {
    oled.setCursor(UI_FAV_MODE_LABEL_X, row);
    oled.print((__FlashStringHelper*)g_bandModeDesc[fav.modulation]);
}

// draw one favorite line
// clear row to avoid leftovers from previous content
static inline void fav_drawLine(uint8_t idx,
    uint8_t row,
    bool sel) {
    const auto& fav = g_favorites[idx];

    clearBox(0, row * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
    oled.setCursor(0, row);

    oledPrintFavPrefix(idx, sel);

    if (fav.modulation == FM) {
        fav_drawFreqFM(fav, row);
    } else {
        fav_drawFreqAMSSB(fav, row);
    }

    fav_drawModeLabel(fav, row);
}

// ====================================================================================
// ===== FAVORITES: UI HEADER =========================================================
// ====================================================================================

// header with title and right-aligned counter
// stable header helps orientation across pages
static void fav_drawHeader() {
    oled.setCursor(0, UI_FAV_HEADER_ROW);
    oled.invertText(true);
    oled.print(F("DEL:BW FAVORITES"));
    oledPrintFavCounter(g_favoriteSelected + 1, g_totalFavorites);
    oled.invertText(false);
}

// ====================================================================================
// ===== FAVORITES: UI CONTENT FLOW ===================================================
// ====================================================================================

// render explicit empty list message
// avoid blank UI so user sees state immediately
static inline void fav_renderEmptyList() {
    clearBox(0,
        (UI_FAV_LIST_START_ROW - 1) * UI_CHAR_H,
        UI_SCREEN_W,
        UI_FAV_PAGE_CLEAR_ROWS * UI_CHAR_H);
    drawInverted(UI_FAV_EMPTY_LIST_X,
        UI_FAV_EMPTY_LIST_ROW,
        F("EMPTY LIST"),
        false);
}

// decide if content needs full redraw
// update prev on decision to keep state in sync
static inline bool fav_shouldRedrawContent(bool force_redraw,
    uint8_t cur_page,
    uint8_t& prev_page) {
    if (force_redraw || cur_page != prev_page) {
        prev_page = cur_page; // commit page change
        return true;
    }
    return false;
}

// draw current page or empty state
// keep the branch local so caller stays small
static inline void fav_drawPageOrEmpty(uint8_t page) {
    if (!g_totalFavorites) {
        fav_renderEmptyList();
    } else {
        // full clear per page avoids ghosting
        clearBox(0,
            (UI_FAV_LIST_START_ROW - 1) * UI_CHAR_H,
            UI_SCREEN_W,
            UI_FAV_PAGE_CLEAR_ROWS * UI_CHAR_H);

        uint8_t start, end;
        fav_unpackPageBounds(page, start, end);

        for (uint8_t i = start; i < end; ++i) {
            uint8_t row = fav_rowForIndex(i, start);
            fav_drawLine(i, row, i == g_favoriteSelected);
        }
    }
}

// update only selection cursors on same page
// reduces flicker and i2c traffic
static void fav_updateCursors(uint8_t page) {
    if (!g_totalFavorites) return;

    uint8_t start, end;
    fav_unpackPageBounds(page, start, end);

    const uint8_t sel = g_favoriteSelected;

    for (uint8_t i = start; i < end; ++i) {
        uint8_t row = fav_rowForIndex(i, start);
        oled.setCursor(0, row);
        oled.write(i == sel ? '>' : ' ');
    }
}

// content redraw or cursor update
// full redraw on page change else cursor only
static void fav_handleContent(bool force_redraw) {
    static uint8_t prev_page = 0xFF;
    uint8_t page = fav_pageOf(g_favoriteSelected);

    if (fav_shouldRedrawContent(force_redraw, page, prev_page)) {
        fav_drawPageOrEmpty(page);
    } else {
        fav_updateCursors(page);
    }
}

// ====================================================================================
// ===== FAVORITES: UI ORCHESTRATOR ===================================================
// ====================================================================================

// Favorites menu orchestrator
// choose cheapest update path to keep UI snappy
static void __attribute__((noinline)) showFavorites(bool force_redraw) {
    if (g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites
            ? (g_totalFavorites - 1) : 0;
    }

    fav_drawHeader();
    fav_handleContent(force_redraw);
}

// ====================================================================================
// ===== FAVORITES: LOOP INTEGRATION ==================================================
// ====================================================================================

// Provides auto-exit for the favorites menu on inactivity
static inline void handleFavoritesTimeout() {
    if (!g_favoritesActive) return;

    const uint16_t now16 = (uint16_t)millis();

    if ((uint16_t)(now16 - g_lastAdjustmentTime) > (uint16_t)SETTINGS_MENU_TIMEOUT)
        exitFavoritesMenu();
}

// Main favorites mode handler for loop()
// Returns true if favorites mode consumed the iteration
static inline bool handleFavoritesMode(int16_t safe_encoder_delta) {
    if (!g_favoritesActive) return false;

    if (safe_encoder_delta)
        handleFavoritesMenu(safe_encoder_delta);

    processButtonEvents();
    handleFavoritesTimeout();
    return true;
}

#endif  // ENABLE_FAVORITES
