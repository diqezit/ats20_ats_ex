#pragma once

// ======================================================================
// UI.h - UI Drawing Subsystem for ATS_EX
// contains all functions responsible for rendering information on the OLED display
// ======================================================================

#include "Defines.h"
#include "Globals.h"
#include "Utils.h"
#include "SSD1306_OLED.h"

// ===== UI layout constants (main) =====
static constexpr uint8_t  UI_SCREEN_W = 128;
static constexpr uint8_t  UI_CHAR_W = 6;
static constexpr uint8_t  UI_CHAR_H = 8;
static constexpr uint8_t  UI_COLS = 21;                     // 128 / 6 ~= 21

// Spacing between seven-segment characters
// keep legibility on small OLED without redesigning glyphs
const int DIGIT_SPACING = 3;

// Frequency area
static constexpr uint8_t  UI_FREQ_TOP_PX = 16;              // Y for big digits (px)
static constexpr uint8_t  UI_FREQ_X_OFFSET_SSB_PX = 3;      // left offset SSB
static constexpr uint8_t  UI_FREQ_X_OFFSET_AMFM_PX = 12;    // left offset AM/FM
static constexpr uint8_t  UI_FREQ_MAIN_WIDTH_AMFM = 5;      // convertToChar width in AM/FM
static constexpr uint8_t  UI_FREQ_BUF_LEN = 7;              // frequency display buffer
static constexpr uint16_t UI_SSB_TAIL_WIDTH =
(uint16_t)DIGIT_SPACING + SEVEN_SEG_DOT_WIDTH +
(uint16_t)DIGIT_SPACING + SEVEN_SEG_DIGIT_WIDTH +
(uint16_t)DIGIT_SPACING + SEVEN_SEG_DIGIT_WIDTH;            // ".dd" tail width

// Units label
static constexpr uint8_t  UI_UNIT_X_PX = 109;
static constexpr uint8_t  UI_UNIT_ROW = 4;

// Splash
static constexpr uint8_t  UI_SPLASH_LINE1_X = 26;
static constexpr uint8_t  UI_SPLASH_LINE1_ROW = 1;
static constexpr uint8_t  UI_SPLASH_LINE2_X = 32;
static constexpr uint8_t  UI_SPLASH_LINE2_ROW = 3;
static constexpr uint8_t  UI_SPLASH_ANIM_ROW = 6;
static constexpr uint8_t  UI_SPLASH_ANIM_STEPS = 21;
static constexpr uint16_t UI_SPLASH_ANIM_DELAY_MS = 70;
static constexpr uint16_t UI_SPLASH_HOLD_MS = 2000;

// Seek delay
static constexpr uint16_t UI_SEEK_DELAY_MS = 100;

// Labels and indicators
static constexpr uint8_t  UI_MODE_LABEL_X = 0;
static constexpr uint8_t  UI_MODE_LABEL_ROW = 7;

static constexpr uint8_t  UI_STEREO_INDICATOR_X = 24;
static constexpr uint8_t  UI_STEREO_INDICATOR_ROW = 7;

static constexpr uint8_t  UI_VOLUME_X = 114;
static constexpr uint8_t  UI_VOLUME_ROW = 0;

static constexpr uint8_t  UI_SAVED_MSG_X = 45;
static constexpr uint8_t  UI_SAVED_MSG_ROW = 3;
static constexpr uint16_t UI_SAVED_MSG_MS = 500;

static constexpr uint8_t  UI_RSSI_X = 90;
static constexpr uint8_t  UI_RSSI_ROW = 7;
static constexpr uint8_t  UI_SIGNAL_NO_VALUE = 255;
static constexpr char     UI_RSSI_SEPARATOR = '|';

static constexpr uint8_t  UI_BATT_X = 108;
static constexpr uint8_t  UI_BATT_ROW = 7;
static constexpr uint8_t  UI_BATT_MAX_PERCENT = 100;

static constexpr uint8_t  UI_STEP_LABEL_X = 34;
static constexpr uint8_t  UI_STEP_LABEL_ROW = 0;

static constexpr uint8_t  UI_BW_LABEL_X = 40;
static constexpr uint8_t  UI_BW_LABEL_ROW = 7;

// Favorites layout
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
static constexpr uint8_t  UI_FAV_FM_PADDING_SPACES = 0;                            // not used for right-align strategy

// Frequency right-align helpers for favorites
static constexpr uint8_t  UI_FAV_PREFIX_COLS = 4;                                  // ">NN " prefix
static constexpr uint8_t  UI_FAV_FREQ_GAP_COLS = 1;                                 // one space gap before mode label
static constexpr uint8_t  UI_FAV_FREQ_RIGHT_COL = UI_FAV_MODE_LABEL_COL - UI_FAV_FREQ_GAP_COLS;
static constexpr uint8_t  UI_FAV_FREQ_MIN_COL = UI_FAV_PREFIX_COLS + 1;            // do not overlap prefix

// Settings layout
static constexpr uint8_t  UI_SETTINGS_PER_PAGE = 6;
static constexpr uint8_t  UI_SETTINGS_PER_COL = 3;
static constexpr uint8_t  UI_SETTINGS_RIGHT_COL_X = 68;
static constexpr uint8_t  UI_SETTINGS_LEFT_COL_X = 0;
static constexpr uint8_t  UI_SETTINGS_ROW_START = 2;
static constexpr uint8_t  UI_SETTINGS_ROW_STEP = 2;

static constexpr uint8_t  UI_SETTING_NAME_PREFIX_X = 5;
static constexpr uint8_t  UI_SETTING_VALUE_X = 35;
static constexpr uint8_t  UI_SETTING_NUM_WIDTH = 3;

// Dot positions and names
static constexpr uint8_t  UI_DOTPOS_FM = 3;
static constexpr uint8_t  UI_DOTPOS_SW_MHZ = 2;
static constexpr uint8_t  UI_BAND_NAME_LEN = 4;
static constexpr uint8_t  UI_MODE_ABBR_LEN = 4;                  // "AM ", "LSB", ...

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

const char g_bandModeDesc[][UI_MODE_ABBR_LEN] = { "AM ", "LSB", "USB", "CW ", "FM " };

static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, UI_BAND_NAME_LEN);
    buffer[UI_BAND_NAME_LEN] = '\0'; // ensure null
}

// ==========================================
// ===== UI DRAWING UTILITIES ===============
// ==========================================

// Utility to clear a rectangular region of the display
static inline void clearBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    // null pointer in partialUpdate clears the area
    oled.partialUpdate(x, y, w, h, NULL);
}

// Sets cursor, prints text with optional inversion, and resets inversion state
template <typename T>
static void drawInverted(uint8_t x, uint8_t y, T text, bool invert) {
    oled.setCursor(x, y);
    oled.invertText(invert);
    oled.print(text);
    oled.invertText(false); // Always reset state to non-inverted
}

// Pre-calculates width of main frequency digits for alignment purposes
// This avoids building the string just to measure it, which is faster
static inline uint16_t freqMainWidth(uint8_t displayMode, uint16_t khzBFO, uint8_t dotPos, bool ssbMode) {
    uint8_t mainChars = (displayMode == 2) ? ilen(khzBFO) : UI_FREQ_MAIN_WIDTH_AMFM;
    bool hasDot = (!ssbMode && dotPos);
    uint8_t totalChars = mainChars + (hasDot ? 1 : 0);

    uint16_t w = (uint16_t)mainChars * SEVEN_SEG_DIGIT_WIDTH;
    if (hasDot) w += SEVEN_SEG_DOT_WIDTH;
    if (totalChars > 1) w += (uint16_t)(totalChars - 1) * DIGIT_SPACING;
    return w;
}

// Renders just the SSB tail digits (e.g., ".00") with spacing
// This is separate to handle its unique layout needs next to the main frequency
static int drawSSBTailDigits(int startX, int pixelY, uint16_t tailBFO) {
    int curX = startX;

    // center the dot between main and tail blocks
    curX += DIGIT_SPACING;

    oled.drawDigit('.', curX, pixelY);                  curX += SEVEN_SEG_DOT_WIDTH + DIGIT_SPACING;
    oled.drawDigit('0' + (tailBFO / 10), curX, pixelY); curX += SEVEN_SEG_DIGIT_WIDTH + DIGIT_SPACING;
    oled.drawDigit('0' + (tailBFO % 10), curX, pixelY); curX += SEVEN_SEG_DIGIT_WIDTH; // No spacing after last digit
    return curX;
}

// Draws SSB tail digits only if in SSB mode, otherwise does nothing
static int drawSSBTailIfNeeded(bool ssb, int x, int y, uint16_t tail) {
    if (!ssb) return x;
    return drawSSBTailDigits(x, y, tail);
}

// Calculates X/Y position for a setting based on its index
// This arranges settings into two columns for a compact menu
static void calcSettingPos(uint8_t idx, uint8_t& xOffset, uint8_t& yOffset) {
    uint8_t place = idx % UI_SETTINGS_PER_PAGE; // Position within the current page
    bool isRight = place >= UI_SETTINGS_PER_COL;
    xOffset = isRight ? UI_SETTINGS_RIGHT_COL_X : UI_SETTINGS_LEFT_COL_X;
    uint8_t withinCol = place - (isRight ? UI_SETTINGS_PER_COL : 0);
    yOffset = UI_SETTINGS_ROW_START + withinCol * UI_SETTINGS_ROW_STEP;
}

// Draws a single item (name and value) in the settings menu
static void drawSettingItem(
    uint8_t x, uint8_t y,
    const char* name, const char* val,
    bool selName, bool selVal) {
    oled.setCursor(UI_SETTING_NAME_PREFIX_X + x, y);
    oled.print(selName ? '>' : ' ');
    oled.print(name);

    oled.setCursor(UI_SETTING_VALUE_X + x, y);
    oled.print(selVal ? '>' : ' ');
    oled.print(val);
}

// ==========================================
// ===== UI: MAIN SCREEN COMPONENTS =========
// ==========================================

// Determines display properties like units (kHz/MHz) based on current band and mode
static void prepareDisplayConfig(
    bool ssbMode, BandType band,
    uint8_t& outMode, uint8_t& outDotPos,
    const char*& outUnit) {

    outMode = 0;
    outDotPos = 0;
    outUnit = "kHz";

    if (ssbMode) {
        outMode = 2; // SSB needs special handling for BFO
    } else if (band == FM_BAND_TYPE) {
        outMode = 1;
        outDotPos = UI_DOTPOS_FM;
        outUnit = "MHz";
    } else if (band == SW_BAND_TYPE && g_Settings[SettingsIndex::SWUnits].param == 1) {
        // SW can optionally show MHz for user preference
        outDotPos = UI_DOTPOS_SW_MHZ;
        outUnit = "MHz";
    }
}

// Formats the main frequency digits and extracts the BFO part for SSB
static void prepareMainFreq(
    uint8_t displayMode, char* freqDisplay,
    uint16_t& khzBFO, uint16_t& tailBFO, uint8_t dotPos) {

    if (displayMode == 2) { // SSB
        splitFreq(khzBFO, tailBFO);
        convertToChar(freqDisplay, khzBFO, ilen(khzBFO), 0, '.', ' ');
    } else { // AM / FM
        convertToChar(freqDisplay, g_currentFrequency, UI_FREQ_MAIN_WIDTH_AMFM, dotPos, '.', '/');
    }
}

// Clears the frequency area to prevent graphical artifacts ("ghosting")
// This is needed when the frequency changes length (e.g., 999 -> 1000)
static void renderClearOrBlink(
    bool cleanDisplay,
    uint8_t len, uint8_t prevLen,
    uint8_t off, int pixelY) {

    if (cleanDisplay) {
        clearBox(0, pixelY, UI_SCREEN_W, SEVEN_SEG_DIGIT_HEIGHT);
    } else if (len != prevLen) {
        // When length changes clear from start to the right edge
        clearBox(off, pixelY, UI_SCREEN_W - off, SEVEN_SEG_DIGIT_HEIGHT);
    }
}

// Positions the unit label (kHz/MHz) to visually align with the seven-segment digits
// It is hidden on long SSB frequencies to avoid overlapping the BFO decimal part
static void renderUnit(bool ssbMode, uint8_t len, const char* unit) {
    if (ssbMode && len >= 5) return;  // avoid collision with SSB tail

    uint8_t row = (UI_FREQ_TOP_PX + SEVEN_SEG_DIGIT_HEIGHT - UI_CHAR_H) / UI_CHAR_H;
    if (row > 7) row = 7;

    if (row != UI_UNIT_ROW)
        clearBox(UI_UNIT_X_PX, UI_UNIT_ROW * UI_CHAR_H, 3 * UI_CHAR_W, UI_CHAR_H);

    clearBox(UI_UNIT_X_PX, row * UI_CHAR_H, 3 * UI_CHAR_W, UI_CHAR_H);
    oled.setCursor(UI_UNIT_X_PX, row);
    oled.print(unit);
}

// Helper function to render each character in the frequency string using drawDigit
static int renderFrequencyString(const char* freqDisplay, int startX, int pixelY) {
    int curX = startX;

    for (const char* p = freqDisplay; *p; ++p) {
        char ch = *p;
        oled.drawDigit(ch, curX, pixelY);
        curX += (ch == '.' ? SEVEN_SEG_DOT_WIDTH : SEVEN_SEG_DIGIT_WIDTH) + DIGIT_SPACING;
    }
    // remove last spacing to allow renderSSBTail to position itself correctly
    return curX - DIGIT_SPACING;
}

// Renders main frequency, aligning it to prevent "jumpiness" during tuning
// The frequency is right-aligned to the units label, so it expands to the left
static void showFrequency(bool cleanDisplay = false) {
    RETURN_IF_SETTINGS_ACTIVE();

    static uint8_t prevLen = 0;
    static int prevStartX = -1;

    char     freqDisplay[UI_FREQ_BUF_LEN];
    uint16_t khzBFO = 0, tailBFO = 0;
    bool     ssbMode = isSSB();
    BandType band = g_bandList[g_bandIndex].bandType;

    uint8_t  off = (ssbMode ? UI_FREQ_X_OFFSET_SSB_PX : UI_FREQ_X_OFFSET_AMFM_PX);

    uint8_t displayMode, dotPos;
    const char* unit;
    prepareDisplayConfig(ssbMode, band, displayMode, dotPos, unit);
    prepareMainFreq(displayMode, freqDisplay, khzBFO, tailBFO, dotPos);

    uint8_t len = ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);

    uint16_t mainWidth = freqMainWidth(displayMode, khzBFO, dotPos, ssbMode);
    uint16_t tailWidth = ssbMode ? UI_SSB_TAIL_WIDTH : 0;
    uint16_t totalWidth = mainWidth + tailWidth;

    // Calculate start position for right-alignment
    int startX = off;
    int alignedStart = (int)UI_UNIT_X_PX - DIGIT_SPACING - (int)totalWidth;
    if (alignedStart > startX) startX = alignedStart;

    int pixelY = UI_FREQ_TOP_PX;

    uint8_t offClear = (prevStartX < 0) ? (uint8_t)startX : (uint8_t)min((uint8_t)startX, (uint8_t)prevStartX);
    renderClearOrBlink(cleanDisplay, len, prevLen, offClear, pixelY);

    int mainEndX = renderFrequencyString(freqDisplay, startX, pixelY);

    drawSSBTailIfNeeded(ssbMode, mainEndX, pixelY, tailBFO);
    renderUnit(ssbMode, len, unit);

    prevLen = len;
    prevStartX = startX;
}

//This function is called by station seek logic
static void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;
    showFrequency();
    delay(UI_SEEK_DELAY_MS);
}

//Draw current band tag (e.g., "40m")
static void showBandTag() {
    RETURN_IF_SETTINGS_ACTIVE();

    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    drawInverted(0, 0, name_buffer, invert);
}

// Overloads a single screen position for multiple status indicators
// Shows 'L'/'U' for CW sideband, 'S' for SSB sync, or '*' for FM stereo
void updateStereoIndicator() {
    char c;

    if (g_currentMode == FM) {
        c = (g_stereoStatus && g_Settings[ForceMono].param == 0) ? '*' : ' ';
    } else if (g_currentMode == CW) {
        c = (g_lastCWMode == LSB) ? 'L' : 'U';
    } else if (isSSB() && g_Settings[Sync].param == 1) {
        c = 'S'; // Sync-lock indicator
    } else {
        c = ' ';
    }

    oled.setCursor(UI_STEREO_INDICATOR_X, UI_STEREO_INDICATOR_ROW);
    oled.print(c);
}

//Draw current modulation (AM/LSB/USB/CW/FM) and stereo indicator
static void showModulation() {
    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    drawInverted(UI_MODE_LABEL_X, UI_MODE_LABEL_ROW, g_bandModeDesc[g_currentMode], invert);

    oled.print(' ');
    updateStereoIndicator();
    showBandTag();
}

//Draw volume level or mute status
static void showVolume() {
    RETURN_IF_SETTINGS_ACTIVE();

    char buf[3];

    if (g_muteVolume == 0) {
        convertToChar(buf, g_si4735.getCurrentVolume(), 2, 0, 0);
    } else {
        buf[0] = ' ';
        buf[1] = 'M';
        buf[2] = '\0';
    }
    bool invert = (g_activeCommand == CMD_VOLUME);
    drawInverted(UI_VOLUME_X, UI_VOLUME_ROW, buf, invert);
}

// Displays the current signal quality value (RSSI) and RF hints
// draw RSSI at fixed slot and refresh RF hints below volume
static void showSignalQuality() {
    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    oled.setCursor(UI_RSSI_X, UI_RSSI_ROW);

    if (g_signalQualityValue == UI_SIGNAL_NO_VALUE) {
        oled.print(F("   "));
        showRfHints();
        return;
    }

    if (g_signalQualityValue < 10) oled.print(' ');
    oled.print(g_signalQualityValue);
    oled.print(UI_RSSI_SEPARATOR);

    showRfHints();
}

// Shows 'SM' (Soft Mute) hint when the DSP is quieting audio on weak signals.
// This appears under the volume level to explain why audio might be faint.
static void showRfHints() {
    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    const uint8_t x = UI_VOLUME_X;
    const uint8_t row = UI_VOLUME_ROW + 2;
    if (row > 7) return;

    const uint8_t y = row * UI_CHAR_H;
    bool show = (!isSSB()) && g_si4735.getCurrentSoftMuteIndicator();

    clearBox(x, y, (uint8_t)(2 * UI_CHAR_W), UI_CHAR_H);
    if (show) {
        oled.setCursor(x, row);
        oled.print(F("SM"));
    }
}

// Renders the stable battery percentage value on the display.
static void showChargeOnDisplay() {
    RETURN_IF_SETTINGS_ACTIVE();
    int charge = min(g_stableBatteryPercent, (int)UI_BATT_MAX_PERCENT);
    drawInverted(UI_BATT_X, UI_BATT_ROW, charge, false);
    if (charge < UI_BATT_MAX_PERCENT) oled.print('%');
}

// display the step on the screen
static void showStep() {
    bool invert = (g_activeCommand == CMD_STEP);

    drawInverted(UI_STEP_LABEL_X, UI_STEP_LABEL_ROW, F("STEP: "), invert);

    const Band& current_band = g_bandList[g_bandIndex];
    uint8_t index = (g_currentMode == FM)
        ? (4 + current_band.stepIdxFM)
        : (isSSB() ? (SSB_STEP_OFFSET + current_band.stepIdxSSB)
            : current_band.stepIdxAM);

    oled.invertText(invert);
    oled.print((__FlashStringHelper*)step_lookup_table[index]);
    oled.invertText(false);
}

// Renders the bandwidth label
static void showBandwidth() {
    const uint8_t* table_ptr = nullptr;
    uint8_t index = 0;

    switch (g_currentMode) {
    case LSB:
    case USB:
        table_ptr = bw_ssb_map;
        index = g_bandList[g_bandIndex].bwIdxSSB;
        break;
    case AM:
        table_ptr = bw_am_map;
        index = g_bandList[g_bandIndex].bwIdxAM;
        break;
    case FM:
        table_ptr = bw_fm_map;
        index = g_bandList[g_bandIndex].bwIdxFM;
        break;
    case CW:
        // for CW mode non-selectable value
        return;
    }

    uint8_t offset = pgm_read_byte(&table_ptr[index]);
    const char* bw_str_ptr = &bw_all_data[offset];

    bool invert = (g_activeCommand == CMD_BW);
    drawInverted(UI_BW_LABEL_X, UI_BW_LABEL_ROW, (__FlashStringHelper*)bw_str_ptr, invert);
}

// ==========================================
// ===== UI: FAVORITES MENU DRAWING =========
// ==========================================

#if ENABLE_FAVORITES

static constexpr uint8_t FAV_HEADER_ROW = UI_FAV_HEADER_ROW;
static constexpr uint8_t FAV_LIST_START_ROW = UI_FAV_LIST_START_ROW; // Start list at row 2 for spacing
static constexpr uint8_t FAV_ROW_GAP = UI_FAV_ROW_GAP;               // Use 2 character rows
static constexpr uint8_t FAV_ITEMS_PER_PG = UI_FAV_ITEMS_PER_PG;     // 3 items per page

// returns page for a given favorite index
static inline uint8_t fav_pageOf(uint8_t index) {
    return (g_totalFavorites > 0) ? (index / FAV_ITEMS_PER_PG) : 0;
}

// returns page bounds [start,end)
static inline void fav_getPageBounds(uint8_t page, uint8_t& start, uint8_t& end) {
    start = page * FAV_ITEMS_PER_PG;
    end = (start + FAV_ITEMS_PER_PG < g_totalFavorites) ? (start + FAV_ITEMS_PER_PG) : g_totalFavorites;
}

// row index for item on current page
static inline uint8_t fav_rowForIndex(uint8_t index, uint8_t page_start_index) {
    return FAV_LIST_START_ROW + (index - page_start_index) * FAV_ROW_GAP;
}

// Header padding to right-align the XX|YY counter
static inline uint8_t fav_calcHeaderPadding(uint8_t selected, uint8_t total) {
    uint8_t selDigits = (selected > 9) ? 2 : 1;
    uint8_t totDigits = (total > 9) ? 2 : 1;
    uint8_t counterWidth = selDigits + 1 + totDigits;   // "XX|XX"
    return (UI_COLS - UI_FAV_HEADER_TITLE_WIDTH) - counterWidth; // 21 chars total
}

// prefix '>' + number with padding
static inline void fav_drawPrefix(uint8_t idx, bool sel) {
    oled.print(sel ? '>' : ' ');
    if (idx + 1 < 10) oled.print('0');
    oled.print(idx + 1);
    oled.print(' ');
}

// calculate start column for right-aligned frequency block
// this prevents frequency from overlapping prefix or mode labels
static inline uint8_t fav_calcFreqStartCol(uint8_t freqWidth) {
    int16_t col = (int16_t)UI_FAV_FREQ_RIGHT_COL - (int16_t)freqWidth;
    if (col < (int16_t)UI_FAV_FREQ_MIN_COL) col = UI_FAV_FREQ_MIN_COL;
    return (uint8_t)col;
}

// FM line, right-aligned before the mode label
static inline void fav_drawFreqFM(const FavoriteStation& fav, uint8_t row) {
    uint16_t ip = fav.frequency / 100;
    uint8_t  dp = (fav.frequency % 100) / 10;

    uint8_t ipDigits = (ip < 100) ? 2 : 3;              // 88..99 -> 2, 100..108 -> 3
    uint8_t width = ipDigits + 1 /*'.'*/ + 1 /*dp*/;    // 4 or 5 chars total

    uint8_t startCol = fav_calcFreqStartCol(width);
    oled.setCursor(startCol * UI_CHAR_W, row);
    oled.print(ip);
    oled.print('.');
    oled.print(dp);
}

// Displays AM/SSB/CW frequency, applying the BFO offset for an accurate reading
static inline void fav_drawFreqAMSSB(const FavoriteStation& fav, uint8_t row) {
    char buf[8];
    uint16_t khz = fav.frequency, tl = 0;

    bool isSSB = (fav.modulation == LSB || fav.modulation == USB || fav.modulation == CW);
    if (isSSB) {
        // Apply BFO to show the actual tuned frequency, not just the base
        int16_t d = fav.bfo / 1000, r = fav.bfo % 1000;
        if (r < 0) { r += 1000; d--; }
        khz += d; tl = r / 10;
    }

    convertToChar(buf, khz, 5, 0, '.', ' ');

    uint8_t width = isSSB ? 8 : 5;      // 5 digits for kHz; add ".dd" only for SSB/CW
    uint8_t startCol = fav_calcFreqStartCol(width);

    oled.setCursor(startCol * UI_CHAR_W, row);
    oled.print(buf);

    if (isSSB) {
        oled.print('.');
        if (tl < 10) oled.print('0');   // keep two decimals
        oled.print(tl);
    }
}

// Mode label at fixed column (rightmost)
static inline void fav_drawModeLabel(const FavoriteStation& fav, uint8_t row) {
    oled.setCursor(UI_FAV_MODE_LABEL_X, row);
    oled.print(g_bandModeDesc[fav.modulation]);
}

// Draws one favorite line
static inline void fav_drawLine(uint8_t idx, uint8_t row, bool sel) {
    const auto& fav = g_favorites[idx];

    clearBox(0, row * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
    oled.setCursor(0, row);

    fav_drawPrefix(idx, sel);

    if (fav.modulation == FM) {
        fav_drawFreqFM(fav, row);
    } else {
        fav_drawFreqAMSSB(fav, row);
    }

    fav_drawModeLabel(fav, row);
}

// Header with status info
static void fav_drawHeader() {
    oled.setCursor(0, FAV_HEADER_ROW);
    oled.invertText(true);
    oled.print(F("DEL:BW FAVORITES"));

    if (g_totalFavorites > 0) {
        uint8_t total = g_totalFavorites;
        uint8_t selected = g_favoriteSelected + 1;

        uint8_t padding = fav_calcHeaderPadding(selected, total);
        while (padding--) oled.print(' ');

        oled.print(selected);
        oled.print('|');
        oled.print(total);
    } else {
        oled.print(F("     "));
    }

    oled.invertText(false);
}

// Draws current page of favorites
static void fav_drawPage(uint8_t page) {
    clearBox(0, (FAV_LIST_START_ROW - 1) * UI_CHAR_H, UI_SCREEN_W, UI_FAV_PAGE_CLEAR_ROWS * UI_CHAR_H);

    uint8_t start, end;
    fav_getPageBounds(page, start, end);

    for (uint8_t i = start; i < end; ++i) {
        uint8_t row = fav_rowForIndex(i, start);
        fav_drawLine(i, row, i == g_favoriteSelected);
    }
}

// Updates only selection cursors to avoid full-screen flicker on simple navigation
static void fav_updateCursors(uint8_t page) {
    if (g_totalFavorites > 0) {
        uint8_t start, end;
        fav_getPageBounds(page, start, end);

        for (uint8_t i = start; i < end; ++i) {
            uint8_t row = fav_rowForIndex(i, start);
            oled.setCursor(0, row);
            oled.print(i == g_favoriteSelected ? '>' : ' ');
        }
    }
}

// Header redraw decision
static void fav_handleHeader(bool force_redraw) {
    static uint8_t prev_selected = 0xFF;

    if (force_redraw || prev_selected != g_favoriteSelected) {
        fav_drawHeader();
    }
    prev_selected = g_favoriteSelected;
}

// Content redraw or cursor update
static void fav_handleContent(bool force_redraw) {
    static uint8_t prev_page = 0xFF;
    uint8_t page = fav_pageOf(g_favoriteSelected);

    // Full redraw on page change or if forced
    if (force_redraw || page != prev_page) {
        prev_page = page;
        if (!g_totalFavorites) {
            clearBox(0, (FAV_LIST_START_ROW - 1) * UI_CHAR_H, UI_SCREEN_W, UI_FAV_PAGE_CLEAR_ROWS * UI_CHAR_H);
            drawInverted(UI_FAV_EMPTY_LIST_X, UI_FAV_EMPTY_LIST_ROW, F("EMPTY LIST"), false);
        } else {
            fav_drawPage(page);
        }
    } else {
        // Just move the cursor on same-page selection change
        fav_updateCursors(page);
    }
}

// Favorites menu orchestrator
// Decides whether to do a full redraw or a faster cursor-only update
static void showFavorites(bool force_redraw) {
    fav_handleHeader(force_redraw);
    fav_handleContent(force_redraw);
}

#endif  // ENABLE_FAVORITES

// ==========================================
// ===== UI: SETTINGS MENU DRAWING ==========
// ==========================================

// builds user-facing text for switch-like params
static inline void handleSwitchParam(
    char* buf, uint8_t idx,
    int8_t param, uint8_t type) {
    uint8_t base = pgm_read_byte(&switch_setting_map[idx].baseIndex);
    uint8_t inverted = pgm_read_byte(&switch_setting_map[idx].inverted);

    uint8_t textIdx = (idx == SettingsIndex::DisplayOff)
        ? (param ? 10 + param : 2) // "OFF" or specific timeout
        : (type == SettingType::SwitchAuto ? param
            : (base + (inverted ? -param : param))); // "ON"/"OFF" or "MAN"/"AUT"

    strcpy_P(buf, paramTexts[textIdx]);
}

// Translates an internal setting value to a human-readable string for display
// For example, brightness 0 becomes "1", and enum values become "ON" or "OFF"
static void SettingParamToUI(char* buf, uint8_t idx) {
    const auto& s = g_Settings[idx];
    int8_t param = s.param;

    if (idx == CWPitch) {
        uint16_t pitch = pgm_read_word(&cw_pitch_options_hz[param]);
        convertToChar(buf, pitch, UI_SETTING_NUM_WIDTH);
        return;
    }

    if (idx == SettingsIndex::BATT_PIN) {
        strcpy_P(buf, (param == 1) ? BATT_PIN_NAME_ALT : BATT_PIN_NAME_DEFAULT);
        return;
    }

    if (s.type >= SettingType::Switch) {
        handleSwitchParam(buf, idx, param, s.type);
        return;
    }

    if (param == 0) {
        if (s.type == SettingType::ZeroAuto) {
            strcpy_P(buf, paramTexts[0]); // AUT
            return;
        }
        if (idx == SQL) {
            strcpy_P(buf, paramTexts[2]); // OFF
            return;
        }
    }

    int8_t valueToDisplay = param;
    if (idx == SettingsIndex::Brightness) {
        valueToDisplay++; // Show 1-10 to user instead of 0-9
    }

    uint8_t val_to_convert = (valueToDisplay < 0) ? -valueToDisplay : valueToDisplay;
    convertToChar(buf, val_to_convert, UI_SETTING_NUM_WIDTH);

    if (valueToDisplay < 0) {
        buf[0] = '-';
    }
    buf[3] = '\0';
}

// draws one settings item
static void DrawSetting(uint8_t idx, bool full) {
    if (!g_settingsActive) return;

    char buf[5];

    uint8_t xOffset, yOffset;
    calcSettingPos(idx - ((g_SettingsPage - 1) * UI_SETTINGS_PER_PAGE), xOffset, yOffset);

    if (full) {
        // Full redraw of name and value
        SettingParamToUI(buf, idx);
        drawSettingItem(
            xOffset, yOffset, g_Settings[idx].name, buf,
            (idx == g_SettingSelected && !g_SettingEditing),
            (idx == g_SettingSelected && g_SettingEditing));
    } else {
        // Partial redraw of just the value
        SettingParamToUI(buf, idx);
        oled.setCursor(UI_SETTING_VALUE_X + xOffset, yOffset);
        oled.print((idx == g_SettingSelected && g_SettingEditing) ? '>' : ' ');
        oled.print(buf);
    }
}

// settings title bar
static void showSettingsTitle() {
    oled.setCursor(0, 0);
    oled.invertText(true);
    oled.print(F("      SETTINGS    "));
    oled.print((uint8_t)g_SettingsPage);
    oled.print('|');
    oled.print((uint8_t)g_SettingsMaxPages);
    oled.invertText(false);
}

// draws visible settings page
static void showSettings() {
    for (uint8_t i = 0; i < UI_SETTINGS_PER_PAGE && i + ((g_SettingsPage - 1) * UI_SETTINGS_PER_PAGE) < SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * UI_SETTINGS_PER_PAGE), true);
}

// ==========================================
// ===== UI: ORCHESTRATORS & GENERAL ========
// ==========================================

// Maps user brightness 0-9 to a non-linear contrast curve
// This provides finer control at lower brightness levels where it matters most
static void applyBrightness() {
    uint8_t s = g_Settings[Brightness].param;

    // A quadratic-like curve to make low-end adjustments less drastic
    uint8_t contrast_value = (((uint32_t)s * ((uint16_t)s * 130 + 6060)) >> 8);

    oled.setContrast(contrast_value + 1);
}

// Startup screen
void showSplashScreen() {
    oled.clear();

    drawInverted(UI_SPLASH_LINE1_X, UI_SPLASH_LINE1_ROW, APP_NAME_LINE1, false);
    drawInverted(UI_SPLASH_LINE2_X, UI_SPLASH_LINE2_ROW, F("MOD NO RDS"), false);

#if ANIMATE_SPLASH
    for (int i = 0; i < UI_SPLASH_ANIM_STEPS; i++) {
        oled.setCursor(i * UI_CHAR_W, UI_SPLASH_ANIM_ROW);
        oled.print('-');
        delay(UI_SPLASH_ANIM_DELAY_MS);
    }
#endif

    delay(UI_SPLASH_HOLD_MS);
    oled.clear();
}

// Displays a confirmation message when a station is saved to favorites
void showSavedConfirmation() {
    oled.setCursor(UI_SAVED_MSG_X, UI_SAVED_MSG_ROW);
    oled.print(F("SAVED"));
    delay(UI_SAVED_MSG_MS);
    showStatus(true); // Redraw the main screen to clear the message
}

// Orchestrator for drawing the main status screen
void showStatus(bool cleanFreq) {
    showSignalQuality();
    showFrequency(cleanFreq);
    showModulation();
    showStep();
    showBandwidth();
#if ENABLE_BATTERY_MONITOR
    updateAndShowBattery(true);
#endif
    showVolume();
}
