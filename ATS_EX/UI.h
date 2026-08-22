#pragma once

// ====================================================================================
//
// UI.h - UI Drawing Subsystem for ATS_EX
//
// This file is the "display driver" for the receiver
// Handles all OLED screen rendering and knows nothing about radio logic or user input
// Its single responsibility is to translate data (frequency, volume, etc.)
// into pixels on the screen
//
// The layout is defined by `UI_*` constants
// Many values are pre-calculated to save CPU cycles during runtime rendering
// Right-alignment is used for frequency and lists
// to prevent visual jitter as numbers change width
//
// ====================================================================================

#include "Defines.h"
#include "Globals.h"
#include "Utils.h"
#include "SSD1306_OLED.h"

// =-=-=-=-=-=-=-=-= Screen Dimensions =-=-=-=-=-=-=-=-=
static constexpr uint8_t  UI_SCREEN_W = 128;
static constexpr uint8_t  UI_CHAR_W = 6;
static constexpr uint8_t  UI_CHAR_H = 8;
static constexpr uint8_t  UI_COLS = 21;                     // 128 / 6 ~= 21

// =-=-=-=-=-=-=-=-= Seven-Segment Spacing =-=-=-=-=-=-=-=-=
// Spacing between seven-segment characters
// keep legibility on small OLED without redesigning glyphs
static constexpr uint8_t  DIGIT_SPACING = 3;

// =-=-=-=-=-=-=-=-= Frequency Area =-=-=-=-=-=-=-=-=
static constexpr uint8_t  UI_FREQ_TOP_PX = 16;              // Y for big digits (px)
static constexpr uint8_t  UI_FREQ_X_OFFSET_SSB_PX = 3;      // left offset SSB
static constexpr uint8_t  UI_FREQ_X_OFFSET_AMFM_PX = 12;    // left offset AM/FM
static constexpr uint8_t  UI_FREQ_MAIN_WIDTH_AMFM = 5;      // convertToChar width in AM/FM
static constexpr uint8_t  UI_FREQ_BUF_LEN = 7;              // frequency display buffer
static constexpr uint16_t UI_SSB_TAIL_WIDTH = 1 +
SEVEN_SEG_DOT_WIDTH + DIGIT_SPACING +
SEVEN_SEG_DIGIT_WIDTH + DIGIT_SPACING +
SEVEN_SEG_DIGIT_WIDTH;                                  // ".dd" tail width

// =-=-=-=-=-=-=-=-= Units Label =-=-=-=-=-=-=-=-=
static constexpr uint8_t  UI_UNIT_X_PX = 109;

// =-=-=-=-=-=-=-=-= Splash Screen =-=-=-=-=-=-=-=-=
static constexpr uint8_t  UI_SPLASH_LINE1_X = 26;
static constexpr uint8_t  UI_SPLASH_LINE1_ROW = 1;
static constexpr uint8_t  UI_SPLASH_LINE2_X = 46;
static constexpr uint8_t  UI_SPLASH_LINE2_ROW = 3;
static constexpr uint8_t  UI_SPLASH_ANIM_ROW = 6;
static constexpr uint8_t  UI_SPLASH_ANIM_STEPS = 21;
static constexpr uint16_t UI_SPLASH_ANIM_DELAY_MS = 70;
static constexpr uint16_t UI_SPLASH_HOLD_MS = 2000;
static constexpr uint8_t  UI_SPLASH_LINE3_ROW = 5;

// =-=-=-=-=-=-=-=-= Seek Delay =-=-=-=-=-=-=-=-=
static constexpr uint16_t UI_SEEK_DELAY_MS = 100;

// =-=-=-=-=-=-=-=-= Labels and Indicators =-=-=-=-=-=-=-=-=
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

static constexpr uint8_t  UI_STEP_LABEL_X = 34;
static constexpr uint8_t  UI_STEP_LABEL_ROW = 0;

static constexpr uint8_t  UI_BW_LABEL_X = 40;
static constexpr uint8_t  UI_BW_LABEL_ROW = 7;

// =-=-=-=-=-=-=-=-= Settings Layout =-=-=-=-=-=-=-=-=
constexpr uint8_t UI_SETTINGS_PER_PAGE = 6;
constexpr uint8_t UI_SETTINGS_LEFT_COL_X = 0;
constexpr uint8_t UI_SETTINGS_RIGHT_COL_X = 68;
constexpr uint8_t UI_SETTINGS_ROW_START = 2;
constexpr uint8_t UI_SETTINGS_ROW_STEP = 2;

static constexpr uint8_t  UI_SETTING_NAME_PREFIX_X = 5;
static constexpr uint8_t  UI_SETTING_VALUE_X = 35;
static constexpr uint8_t  UI_SETTING_NUM_WIDTH = 3;

// =-=-=-=-=-=-=-=-= Dot Positions and Names =-=-=-=-=-=-=-=-=
static constexpr uint8_t  UI_DOTPOS_FM = 3;
static constexpr uint8_t  UI_DOTPOS_SW_MHZ = 2;
static constexpr uint8_t  UI_BAND_NAME_LEN = 4;
static constexpr uint8_t  UI_MODE_ABBR_LEN = 4;                  // "AM ", "LSB", ...

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

const char g_bandModeDesc[][UI_MODE_ABBR_LEN] PROGMEM = { "AM ", "LSB", "USB", "CW ", "FM " };

// ====================================================================================
// ===== HELPER MACROS ================================================================
// ====================================================================================

#define UI_FORMAT_HUNDREDS3_IF(targetIdx)                      \
    do {                                                       \
        if (idx == (targetIdx)) {                              \
            /* CWPitch is stored as 5..8 (meaning 500..800 Hz) */ \
            buf[0] = (char)('0' + (uint8_t)param);             \
            buf[1] = '0';                                      \
            buf[2] = '0';                                      \
            buf[3] = '\0';                                     \
            return;                                            \
        }                                                      \
    } while (0)


// ====================================================================================
// ===== LIGHTWEIGHT NUMBER PRINTING ==================================================
// ====================================================================================
// All functions use local buffers + single oled.print() call
// This batches multiple characters into ONE I2C transaction
// No 32-bit division, no Arduino Print overhead

// Single uint8_t division yielding both quotient and remainder.
// Avoids separate / and % — compiler emits ONE hardware division.
// Struct return maps to two registers on AVR — zero RAM overhead.
struct DivMod10 { uint8_t q; uint8_t r; };

DivMod10 NOINLINE divmod10_u8(uint8_t v) {
    uint8_t q = (uint8_t)(v / 10);
    return { q, (uint8_t)(v - (uint8_t)(q * 10)) };
}

// Print uint8_t with fixed width 2 + separator (range 0-99)
// Used for: RSSI with trailing separator "NN|"
static inline void oledPrintU8_2sep(uint8_t v, char sep) {
    if (v >= 100) v = 99;

    // one division only
    const DivMod10 dm = divmod10_u8(v);

    char b[4];
    b[0] = dm.q ? (char)('0' + dm.q) : ' ';
    b[1] = (char)('0' + dm.r);
    b[2] = sep;
    b[3] = 0;
    oled_puts(b);
}

// Print uint8_t with fixed width 3 + suffix char (range 0-255)
// Used for: Battery "100 " or " 85%"
static inline void oledPrintU8_3suf(uint8_t v, char suf) {
    if (v > 100) v = 100;

    char b[4];

    if (v == 100) {
        b[0] = '1';
        b[1] = '0';
        b[2] = '0';
    } else if (v >= 10) {
        // one division only
        const DivMod10 dm = divmod10_u8(v);
        b[0] = (char)('0' + dm.q);
        b[1] = (char)('0' + dm.r);
        b[2] = suf;
    } else {
        b[0] = ' ';
        b[1] = (char)('0' + v);
        b[2] = suf;
    }

    b[3] = 0;
    oled_puts(b);
}

// Print ".XX" decimal suffix for SSB frequencies
// Used for: Favorites SSB/CW frequency tail
static inline void oledPrintDotDec2(uint8_t v) {
    // one division only
    const DivMod10 dm = divmod10_u8(v);

    char b[4];
    b[0] = '.';
    b[1] = (char)('0' + dm.q);
    b[2] = (char)('0' + dm.r);
    b[3] = 0;
    oled_puts(b);
}

// Print FM frequency "NNN.D" or "NN.D"
// Used for: Favorites FM frequency display
static inline void oledPrintFreqFM(uint16_t freq) {
    char b[6];
    uint16_t ip = freq / 100;   // 88-108
    uint8_t dp = (uint8_t)((freq % 100) / 10);

    if (ip >= 100) {
        // "100.0".."108.0"
        b[0] = '1';
        b[1] = '0';
        b[2] = (char)('0' + (uint8_t)(ip - 100));
        b[3] = '.';
        b[4] = (char)('0' + dp);
        b[5] = 0;
    } else {
        // "88.0".."99.0"
        const DivMod10 dm = divmod10_u8((uint8_t)ip);
        b[0] = (char)('0' + dm.q);
        b[1] = (char)('0' + dm.r);
        b[2] = '.';
        b[3] = (char)('0' + dp);
        b[4] = 0;
    }

    oled_puts(b);
}

// Print settings page counter "N|M"
// Used for: Settings header
static inline void oledPrintPageCounter(uint8_t page, uint8_t maxPages) {
    char b[4];
    b[0] = (char)('0' + page);
    b[1] = '|';
    b[2] = (char)('0' + maxPages);
    b[3] = 0;
    oled_puts(b);
}


// ====================================================================================
// ===== BASIC DRAWING UTILITIES ======================================================
// ====================================================================================

// clear a rectangular region
// partialUpdate with null buffer erases box fast
static inline void clearBox(uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h) {
    oled_box(x, y, w, h);
}

// ====================================================================================
// ===== TEXT WITH INVERSION ==========================================================
// ====================================================================================

// drawInverted for C-string (char*)
static void drawInverted(uint8_t x, uint8_t y, const char* text, bool inv) {
    oled_xy(x, y);
    oled_inv(inv);
    oled_puts(text);
    oled_inv(false);
}

// drawInverted for F() strings (PROGMEM)
static void drawInverted(uint8_t x, uint8_t y, const __FlashStringHelper* text, bool inv) {
    oled_xy(x, y);
    oled_inv(inv);
    oled_puts(text);
    oled_inv(false);
}


// ====================================================================================
// ===== SETTINGS POSITION CALCULATOR =================================================
// ====================================================================================

// Calculate screen position for settings item
// Layout is ALWAYS Row-first (NAV only affects cursor movement order)
// Row 0: items 0,1 | Row 1: items 2,3 | Row 2: items 4,5
static inline void calcSettingPos(uint8_t localIdx,
    uint8_t& xOffset,
    uint8_t& yOffset) {

    // localIdx is 0..5 on page
    uint8_t row = (localIdx >> 1); // 0..2
    bool right = (localIdx & 1);   // 0/1

    xOffset = right ? UI_SETTINGS_RIGHT_COL_X : UI_SETTINGS_LEFT_COL_X;
    yOffset = (uint8_t)(UI_SETTINGS_ROW_START + row * UI_SETTINGS_ROW_STEP);
}

// =-=-=-=-=-=-=-=-= Settings Pos Helpers =-=-=-=-=-=-=-=-=

// Get first setting index for a page (1-based page number)
INLINE_AI
uint8_t getPageStartIndex(uint8_t page) {
    return pgm_read_byte(&g_pageStartIdx[page]);
}

// ====================================================================================
// ===== UI: BRIGHTNESS ===============================================================
// ====================================================================================

// map 0..9 to non linear contrast
// give finer control at low end where eyes are sensitive
static inline uint8_t brightnessToContrast(uint8_t s) {
    static const uint8_t lut[10] PROGMEM = { 0, 24, 49, 75, 102, 131, 160, 190, 221, 254 };
    return pgm_read_byte(&lut[s]);
}

// apply contrast curve to OLED
// add +1 to avoid full black at lowest setting
static void applyBrightness() {
    uint8_t s = (uint8_t)getSettingParam(Brightness);
    oled_contrast((uint8_t)(brightnessToContrast(s) + 1));
}


// ====================================================================================
// ===== UI: SIMPLE STATUS WIDGETS ====================================================
// ====================================================================================

// compute single char indicator for shared slot
// compact mapping keeps UI readable at a glance
INLINE_AI
char stereoIndicatorChar() {
    switch (g_currentMode) {
    case FM:
        return (g_stereoStatus && getSettingParam(ForceMono) == 0) ? '*' : ' ';
    case CW:
        return (g_lastCWMode == LSB) ? 'L' : 'U';
    case LSB:
    case USB:
        return (getSettingParam(Sync) == 1) ? 'S' : ' ';
    case AM:
    default:
        return ' ';
    }
}

// single slot shows CW sideband, SSB sync, or FM stereo
// saves space by overloading position user already watches
void updateStereoIndicator() {
    oled_xy(UI_STEREO_INDICATOR_X, UI_STEREO_INDICATOR_ROW);
    oled_putc(stereoIndicatorChar());
}

// show band tag like "40m"
// invert only when band command is active and not FM
static void showBandTag() {
    RETURN_IF_SETTINGS_ACTIVE();

    const Band* band = currentBandPtr();

    const bool inv = (g_activeCommand == CMD_BAND) && (g_currentMode != FM);

    static char name_buffer[5];
    memcpy(name_buffer, band->name, UI_BAND_NAME_LEN);
    name_buffer[UI_BAND_NAME_LEN] = '\0';

    drawInverted(0, 0, name_buffer, inv);
}

// modulation label and stereo indicator together
// keeps related status in one glance area
static void showModulation() {
    // Invert modulation label only when BAND command is active on FM
    const bool inv = (g_activeCommand == CMD_BAND) && (g_currentMode == FM);

    drawInverted(UI_MODE_LABEL_X,
        UI_MODE_LABEL_ROW,
        (__FlashStringHelper*)g_bandModeDesc[g_currentMode],
        inv);

    updateStereoIndicator();
    showBandTag();
}

// build volume text or ' M' when muted
// user sees explicit mute instead of 0 value
static inline void buildVolumeBuf(char* buf) {
    if (g_muteVolume == 0) {
        // reads from master g_volume - not the chip g_si4735! To compensate
        convertToChar(buf, g_volume, 2, 0, '.', ' ');
    } else {
        buf[0] = ' ';
        buf[1] = 'M';
        buf[2] = '\0';
    }
}

// volume or mute flag
// mute shows ' M' so user understands silent audio state
static void showVolume() {
    RETURN_IF_SETTINGS_ACTIVE();

    char buf[3];
    buildVolumeBuf(buf);

    bool inv = (g_activeCommand == CMD_VOLUME);
    drawInverted(UI_VOLUME_X, UI_VOLUME_ROW, buf, inv);
}

// show Soft Mute hint when DSP attenuates weak signals
// explains faint audio without user guessing
static void showRfHints() {
    if (!g_displayOn || isSSB()) return;

    constexpr uint8_t r0 = UI_VOLUME_ROW + 2;

    clearBox(UI_VOLUME_X, r0 * UI_CHAR_H, 2 * UI_CHAR_W, 2 * UI_CHAR_H);

    if (g_si4735.getCurrentSoftMuteIndicator()) {
        oled_xy(UI_VOLUME_X, r0);
        oled_puts_P("SM");
    }

#if ENABLE_RDS_MINI
    oled_xy(UI_VOLUME_X, r0 + 2);
    if (g_currentMode == FM && rdsMiniUiEnabled()) {
        oled_puts_P("RS");
    } else {
        oled_puts_P("  ");
    }
#endif
}


// ====================================================================================
// ===== UI: SIGNAL QUALITY DISPLAY ===================================================
// ====================================================================================

// print RSSI field at current cursor
// blank on missing value
// FM always numeric, AM/SSB/CW numeric or S-point depending on SMeter setting
static inline void uiPrintSignalValueAtCursor(uint8_t rssi, uint8_t sm, uint8_t mode) {
    enum : uint8_t { SM_UI_SPOINT = 1 };

    if (rssi == UI_SIGNAL_NO_VALUE) {
        oled_puts_P("   ");
        return;
    }

    if (mode != FM && (sm & SM_UI_SPOINT)) {
        char s_buffer[4];
        rssiToSLevel(s_buffer, rssi);
        oled_puts(s_buffer);
        return;
    }

    oledPrintU8_2sep(rssi, UI_RSSI_SEPARATOR);
}

// skip while menus open and draw hints after value
// keep UI free from mapping rules, use rssiToSLevel for S text
static void showSignalQuality() {
    enum : uint8_t { SM_UI_BAR = 2 };

    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    const uint8_t sm = (uint8_t)getSettingParam(SMeter);
    const uint8_t rssi = g_signalQualityValue;
    const uint8_t mode = (uint8_t)g_currentMode;

    // one cursor set for all paths
    oled_xy(UI_RSSI_X, UI_RSSI_ROW);

    uiPrintSignalValueAtCursor(rssi, sm, mode);

#if defined(ENABLE_SIGNAL_BAR) && ENABLE_SIGNAL_BAR
    if (sm & SM_UI_BAR) smDrawSignalBar(rssi);
#endif

    showRfHints();
}

// ====================================================================================
// ===== UI: BATTERY DISPLAY ==========================================================
// ====================================================================================

// show stable battery percent
static void showChargeOnDisplay() {
    RETURN_IF_SETTINGS_ACTIVE();
    uint8_t charge = g_stableBatteryPercent;

    oled_xy(UI_BATT_X, UI_BATT_ROW);
    oledPrintU8_3suf(charge, '%');
}


// ====================================================================================
// ===== UI: STEP DISPLAY =============================================================
// ====================================================================================

// show step value based on mode
// index maps user selection to readable label
static void showStep() {
    bool inv = (g_activeCommand == CMD_STEP);

    drawInverted(UI_STEP_LABEL_X, UI_STEP_LABEL_ROW, F("STEP: "), inv);

    const Band* b = currentBandPtr();

    // preload step indices so switch does not touch band struct fields
    const uint8_t stepAM = (uint8_t)b->stepIdxAM;
    const uint8_t stepSSB = (uint8_t)b->stepIdxSSB;
    const uint8_t stepFM = (uint8_t)b->stepIdxFM;

    uint8_t index;

    switch (g_currentMode) {
    case FM:
        index = (uint8_t)(4 + stepFM);
        break;

    case LSB:
    case USB:
    case CW: // CW shares the same step settings as SSB
        index = (uint8_t)(SSB_STEP_OFFSET + stepSSB);
        break;

    case AM:
    default:
        index = stepAM;
        break;
    }

    oled_inv(inv);
    oled_puts((__FlashStringHelper*)step_lookup_table[index]);
    oled_inv(false);
}


// ====================================================================================
// ===== UI: BANDWIDTH DISPLAY ========================================================
// ====================================================================================

// resolve BW table and index for current mode
// CW returns false since value not selectable
static inline bool bwResolve(const Band* b,
    const uint8_t*& table_ptr,
    uint8_t& index) {

    switch (g_currentMode) {
    case LSB:
    case USB:
        table_ptr = bw_ssb_map;
        index = (uint8_t)b->bwIdxSSB;
        return true;

    case AM:
        table_ptr = bw_am_map;
        index = (uint8_t)b->bwIdxAM;
        return true;

    case FM:
        table_ptr = bw_fm_map;
        index = (uint8_t)b->bwIdxFM;
        return true;

    case CW:
    default:
        return false;
    }
}

// bandwidth label per mode
// CW returns early since value not selectable
static void showBandwidth() {
    const uint8_t* table_ptr = nullptr;
    uint8_t index = 0;

    const Band* b = currentBandPtr();
    if (!bwResolve(b, table_ptr, index)) return;

    uint8_t offset = pgm_read_byte(&table_ptr[index]);
    const char* bw_str_ptr = &bw_all_data[offset];

    bool inv = (g_activeCommand == CMD_BW);
    drawInverted(UI_BW_LABEL_X,
        UI_BW_LABEL_ROW,
        (__FlashStringHelper*)bw_str_ptr,
        inv);
}


// ====================================================================================
// ===== UI: MAIN FREQUENCY DISPLAY ===================================================
// ====================================================================================

// =-=-=-=-=-=-=-=-= Frequency Helpers =-=-=-=-=-=-=-=-=

// forward declare to allow helper to call it before definition
static int renderFrequencyString(const char* freqDisplay,
    int startX,
    int pixelY);

// compute main seven segment width
// precompute to align right without building string twice
static inline uint16_t freqMainWidth(uint16_t khzBFO,
    uint8_t dotPos,
    bool ssbMode) {
    uint8_t mainChars = ssbMode ? ilen(khzBFO) : UI_FREQ_MAIN_WIDTH_AMFM;
    bool hasDot = (!ssbMode && dotPos);
    uint8_t totalChars = mainChars + (hasDot ? 1 : 0);

    uint16_t w = (uint16_t)mainChars * SEVEN_SEG_DIGIT_WIDTH;
    if (hasDot) w += SEVEN_SEG_DOT_WIDTH;
    if (totalChars > 1) {
        w += (uint16_t)(totalChars - 1) * DIGIT_SPACING;
    }
    return w;
}

// draw SSB tail ".dd" next to main block
// keep spacing identical to main digits
static int drawSSBTailDigits(int startX, int pixelY, uint16_t tailBFO) {
    int curX = startX + 1;

    oled_digit('.', curX, pixelY);
    curX += SEVEN_SEG_DOT_WIDTH + DIGIT_SPACING;

    oled_digit('0' + (tailBFO / 10), curX, pixelY);
    curX += SEVEN_SEG_DIGIT_WIDTH + DIGIT_SPACING;

    oled_digit('0' + (tailBFO % 10), curX, pixelY);
    curX += SEVEN_SEG_DIGIT_WIDTH; // no spacing after last digit

    return curX;
}

// attach SSB tail only for SSB modes
// AM FM skip to save cycles
static inline int drawSSBTailIfNeeded(bool ssb,
    int x,
    int y,
    uint16_t tail) {
    return ssb ? drawSSBTailDigits(x, y, tail) : x;
}

// compute left offset per mode
// SSB needs tighter left margin to fit tail
INLINE_AI
uint8_t freqLeftOffset(bool ssbMode) {
    return ssbMode ? UI_FREQ_X_OFFSET_SSB_PX
        : UI_FREQ_X_OFFSET_AMFM_PX;
}

// choose start X so main block stays right aligned to unit label
// use left offset as lower bound to avoid clipping left edge
INLINE_AI
int freqStartX(uint16_t totalWidth,
    uint8_t leftOff) {
    int aligned = (int)UI_UNIT_X_PX - DIGIT_SPACING - (int)totalWidth;
    return aligned > (int)leftOff ? aligned : (int)leftOff;
}

// visible length of numeric part
// SSB uses BFO part so length differs from AM/FM
INLINE_AI
uint8_t visibleLen(bool ssbMode,
    uint16_t khzBFO) {
    return ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);
}

// compute widths and start position
// one place to keep alignment math together
INLINE_AI
void freqComputeLayout(bool ssbMode,
    uint16_t khzBFO,
    uint8_t dotPos,
    uint8_t leftOff,
    uint16_t& mainWidth,
    uint16_t& tailWidth,
    uint16_t& totalWidth,
    int& startX) {
    mainWidth = freqMainWidth(khzBFO, dotPos, ssbMode);
    tailWidth = ssbMode ? UI_SSB_TAIL_WIDTH : 0;
    totalWidth = mainWidth + tailWidth;
    startX = freqStartX(totalWidth, leftOff);
}

// draw main numeric and optional SSB tail
// keep tail attach logic local
INLINE_AI
void freqDrawMainAndTail(const char* freqDisplay,
    int startX,
    int pixelY,
    bool ssbMode,
    uint16_t tailBFO) {
    int endX = renderFrequencyString(freqDisplay, startX, pixelY);
    drawSSBTailIfNeeded(ssbMode, endX, pixelY, tailBFO);
}

// =-=-=-=-=-=-=-=-= Frequency Configuration =-=-=-=-=-=-=-=-=

// set unit and dot policy per band and mode
// user expects MHz on FM and optional SW MHz per preference
static void prepareDisplayConfig(bool ssbMode,
    BandType band,
    uint8_t& outMode,
    uint8_t& outDotPos,
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
    } else if (band == SW_BAND_TYPE &&
        getSettingParam(SWUnits) == 1) {
        // SW can optionally show MHz for user preference
        outDotPos = UI_DOTPOS_SW_MHZ;
        outUnit = "MHz";
    }
}

// build main numeric string and extract BFO tail for SSB
// SSB splits display so user sees precise tuned part and decimals
static void prepareMainFreq(uint8_t displayMode,
    char* freqDisplay,
    uint16_t& khzBFO,
    uint16_t& tailBFO,
    uint8_t dotPos) {
    if (displayMode == 2) { // SSB
        bfoSplitFreq(g_currentFrequency, g_currentBFO, khzBFO, tailBFO);
        convertToChar(freqDisplay, khzBFO, ilen(khzBFO), 0, '.', ' ');
    } else { // AM / FM
        convertToChar(freqDisplay,
            g_currentFrequency,
            UI_FREQ_MAIN_WIDTH_AMFM,
            dotPos,
            '.',
            '/');
    }
}

// =-=-=-=-=-=-=-=-= Frequency Rendering =-=-=-=-=-=-=-=-=

// clear area only when needed
// full clear on force or when width grows on digit count change
static void renderClearOrBlink(bool cleanDisplay,
    uint8_t len,
    uint8_t prevLen,
    uint8_t leftOff,
    int pixelY) {
    if (cleanDisplay) {
        clearBox(0, pixelY, UI_SCREEN_W, SEVEN_SEG_DIGIT_HEIGHT);
    } else if (len != prevLen) {
        // length change may leave stale pixels on the right
        clearBox(leftOff,
            pixelY,
            UI_SCREEN_W - leftOff,
            SEVEN_SEG_DIGIT_HEIGHT);
    }
}

// print unit in a row aligned to seven segment block
// hide on long SSB to avoid collision with decimals
static void renderUnit(bool ssbMode,
    uint8_t len,
    const char* unit) {
    if (ssbMode && len >= 5) return;  // avoid collision with SSB tail

    constexpr uint8_t row = (UI_FREQ_TOP_PX + SEVEN_SEG_DIGIT_HEIGHT - UI_CHAR_H) / UI_CHAR_H;

    clearBox(UI_UNIT_X_PX, row * UI_CHAR_H, 3 * UI_CHAR_W, UI_CHAR_H);
    oled_xy(UI_UNIT_X_PX, row);
    oled_puts(unit);
}

// draw main numeric block char by char
// spacing matches seven segment glyph metrics
static int renderFrequencyString(const char* freqDisplay,
    int startX,
    int pixelY) {
    int curX = startX;

    for (const char* p = freqDisplay; *p; ++p) {
        char ch = *p;
        oled_digit(ch, curX, pixelY);
        curX += (ch == '.' ? SEVEN_SEG_DOT_WIDTH
            : SEVEN_SEG_DIGIT_WIDTH) + DIGIT_SPACING;
    }
    // remove trailing spacing to align SSB tail attach point
    return curX - DIGIT_SPACING;
}

// =-=-=-=-=-=-=-=-= Main Frequency Render Function =-=-=-=-=-=-=-=-=

// right align main frequency and draw optional SSB tail
// avoids jumpiness during tuning and keeps unit column stable
static void showFrequency(bool cleanDisplay = false) {
    RETURN_IF_SETTINGS_ACTIVE();

    static uint8_t prevLen = 0;

    char     freqDisplay[UI_FREQ_BUF_LEN];
    uint16_t khzBFO, tailBFO;
    bool     ssbMode = isSSB();
    BandType band = currentBandPtr()->bandType;

    uint8_t leftOff = freqLeftOffset(ssbMode);

    uint8_t displayMode, dotPos;
    const char* unit;
    prepareDisplayConfig(ssbMode, band, displayMode, dotPos, unit);
    prepareMainFreq(displayMode, freqDisplay, khzBFO, tailBFO, dotPos);

    uint8_t len = visibleLen(ssbMode, khzBFO);

    uint16_t mainWidth, tailWidth, totalWidth;
    int startX;
    freqComputeLayout(ssbMode, khzBFO, dotPos, leftOff,
        mainWidth, tailWidth, totalWidth, startX);

    const uint8_t pixelY = UI_FREQ_TOP_PX;

    renderClearOrBlink(cleanDisplay, len, prevLen, leftOff, pixelY);

    freqDrawMainAndTail(freqDisplay, startX, pixelY, ssbMode, tailBFO);
    renderUnit(ssbMode, len, unit);

    prevLen = len;
}

// called by seek to show in-progress frequency quickly
// delay gives user time to perceive step during scan
static void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;
    showFrequency();
    delay(UI_SEEK_DELAY_MS);
}


// ====================================================================================
// ===== UI: SETTINGS MENU ============================================================
// ====================================================================================

// =-=-=-=-=-=-=-=-= Settings Parameter Formatting =-=-=-=-=-=-=-=-=

// map switch param to user text
// early return skips pgm reads for DisplayOff and SwitchAuto
// invert keeps meaning stable when hardware polarity reversed
static inline void handleSwitchParam(char* buf,
    uint8_t idx,
    int8_t param,
    uint8_t type) {

    if (idx == SettingsIndex::DisplayOff) {
        uint8_t textIdx = param ? 10 + param : 2;
        strcpy_P(buf, paramTexts[textIdx]);
        return;
    }

    if (type == SettingType::SwitchAuto) {
        strcpy_P(buf, paramTexts[(uint8_t)param]);
        return;
    }

    uint8_t base = getSettingTextBase(idx);
    uint8_t inverted = getSettingInverted(idx);
    uint8_t textIdx =
        base + (inverted ? -param : param);
    strcpy_P(buf, paramTexts[textIdx]);
}

// =-=-=-=-=-=-=-=-= Settings Special Param Formatters =-=-=-=-=-=-=-=-=

// handle indices with aliases or lookup tables first
// user expects names for pins and Hz instead of raw numbers
INLINE_AI
bool formatAliasByIndex(char* buf,
    uint8_t idx,
    int8_t param) {
    switch (idx) {

    case SettingsIndex::BATT_PIN: {
        strcpy_P(buf,
            (param == 1) ? BATT_PIN_NAME_ALT
            : BATT_PIN_NAME_DEFAULT);
        return true;
    }

    default:
        return false;
    }
}

// map to switch labels and zero-based mode names
// zero means AUT or OFF by rule to avoid mode confusion
INLINE_AI
bool formatSwitchAndZeroLabels(char* buf,
    uint8_t idx,
    int8_t param,
    uint8_t type) {

    if (type >= SettingType::Switch) {
        handleSwitchParam(buf, idx, param, type);
        return true;
    }

    if (param == 0) {

        if (type == SettingType::ZeroAuto) {
            strcpy_P(buf, paramTexts[0]);  // AUT
            return true;
        }

        if (idx == SQL) {
            strcpy_P(buf, paramTexts[2]);  // OFF
            return true;
        }
    }
    return false;
}

// try to format via aliases and modes first
// split keeps business rules local and easy to change
INLINE_AI
bool SettingParamToUI_Special(char* buf,
    uint8_t idx,
    int8_t param,
    uint8_t type) {
    if (formatAliasByIndex(buf, idx, param)) return true;
    if (formatSwitchAndZeroLabels(buf, idx, param, type)) return true;
    return false;
}

// final numeric formatting path
// fixed width aligns columns, brightness shows 1..10
INLINE_AI
void SettingParamToUI_Number(char* buf,
    uint8_t idx,
    int8_t param) {

    // CWPitch is stored as 5..8 (meaning 500..800 Hz)
    // flash-saving zeros
    UI_FORMAT_HUNDREDS3_IF(SettingsIndex::CWPitch);

    // user expects 1..10 not 0..9
    if (idx == SettingsIndex::Brightness) param++;

    uint8_t v = (param < 0) ? (uint8_t)(-param)
        : (uint8_t)param;
    convertToChar(buf, v, UI_SETTING_NUM_WIDTH);

    if (param < 0) buf[0] = '-'; // keep sign without extra path
    buf[UI_SETTING_NUM_WIDTH] = '\0';
}

#undef UI_FORMAT_HUNDREDS3_IF

// Convert setting param to UI text
// Shows '---' for settings not active in current mode
// Splits formatting path for special cases vs standard numbers
static void SettingParamToUI(char* buf, uint8_t idx) {

    // user expects to see inactive settings are disabled
    if (!isSettingActive(idx)) {
        buf[0] = '-';
        buf[1] = '-';
        buf[2] = '-';
        buf[3] = '\0';
        return;
    }

    int8_t  param = getSettingParam(idx);
    uint8_t type = getSettingType(idx);

    if (SettingParamToUI_Special(buf, idx, param, type)) return;

    SettingParamToUI_Number(buf, idx, param);
}

// =-=-=-=-=-=-=-=-= Settings Draw Helpers =-=-=-=-=-=-=-=-=

// compute on-screen position for global index
// local index avoids reflow when page changes
INLINE_AI
void settingPosFromIndex(uint8_t idx,
    uint8_t page,
    uint8_t& x,
    uint8_t& y) {
    const uint8_t base = (uint8_t)((page - 1) * UI_SETTINGS_PER_PAGE); // page is 1-based
    const uint8_t local = (uint8_t)(idx - base);                       // 0..5
    calcSettingPos(local, x, y);
}

// draw name and value of a setting
// cheap '>' markers show focus and edit state
static void drawSettingItem(uint8_t x,
    uint8_t y,
    const char* name,
    const char* val,
    bool selName,
    bool selVal) {
    oled_xy(UI_SETTING_NAME_PREFIX_X + x, y);
    oled_putc(selName ? '>' : ' ');
    oled_puts(name);

    oled_xy(UI_SETTING_VALUE_X + x, y);
    oled_putc(selVal ? '>' : ' ');
    oled_puts(val);
}

// draw value cell only
// cheap marker indicates edit state without redrawing name
INLINE_AI
void drawValueCell(uint8_t x,
    uint8_t y,
    const char* buf,
    bool editing) {
    oled_xy(UI_SETTING_VALUE_X + x, y);
    oled_putc(editing ? '>' : ' ');
    oled_puts(buf);
}

// highlight when selected and not editing to match UX
INLINE_AI
void drawFullRow(uint8_t x,
    uint8_t y,
    const char* nameBuf,
    const char* valBuf,
    bool isSelected,
    bool isEditing) {

    drawSettingItem(x,
        y,
        nameBuf,
        valBuf,
        (isSelected && !isEditing),
        isEditing);
}

// =-=-=-=-=-=-=-=-= Settings Render Functions =-=-=-=-=-=-=-=-=

// draw single setting row
// format once then choose full redraw or value only to cut OLED traffic
static void DrawSetting(uint8_t idx, bool full) {
    if (!g_settingsActive) return;

    char valBuf[UI_SETTING_NUM_WIDTH + 1];

    uint8_t x, y;
    settingPosFromIndex(idx, g_SettingsPage, x, y);

    SettingParamToUI(valBuf, idx);

    const bool isSelected = (idx == (uint8_t)g_SettingSelected);
    const bool isEditing = (isSelected && g_SettingEditing);

    if (!full) {
        drawValueCell(x, y, valBuf, isEditing);
        return;
    }

    char nameBuf[4];
    getSettingName(idx, nameBuf);

    drawFullRow(x, y, nameBuf, valBuf, isSelected, isEditing);
}

// render header with page counters
// invert draws bar without extra bitmap fixed label masks stale pixels
static void showSettingsTitle() {
    oled_xy(0, 0);
    oled_inv(true);
    oled_puts_P("      SETTINGS    ");
    oledPrintPageCounter(g_SettingsPage, g_SettingsMaxPages);
    oled_inv(false);
}

// draw current page only
// precompute base to cut math per item and guard end of list
static void showSettings() {
    const uint8_t base =
        (uint8_t)((g_SettingsPage - 1) * UI_SETTINGS_PER_PAGE);
    for (uint8_t i = 0; i < UI_SETTINGS_PER_PAGE; ++i) {
        const uint8_t cur = (uint8_t)(base + i);
        if (cur >= SETTINGS_MAX) break;
        DrawSetting(cur, true);
    }
}


// ====================================================================================
// ===== UI: SPLASH SCREEN ============================================================
// ====================================================================================

#if ANIMATE_SPLASH
// draw one animation dash
// simple sweep gives user feedback while init runs
INLINE_AI
void splashAnimStep(uint8_t i) {
    oled_xy(i * UI_CHAR_W, UI_SPLASH_ANIM_ROW);
    oled_putc('-');
}
#endif

#if defined(ENABLE_SPLASH_CREDITS_SCROLL) && ENABLE_SPLASH_CREDITS_SCROLL
static const char s_splashCredits[] PROGMEM = APP_SPLASH_CREDITS_TEXT;

// scrolling caption used as "loading bar" replacement
static void splashCreditsScroll(uint16_t total_ms) {
    char msg[sizeof(s_splashCredits)];
    strcpy_P(msg, (PGM_P)s_splashCredits);

    const uint8_t msgLen = (uint8_t)(sizeof(s_splashCredits) - 1);
    const uint8_t steps = (uint8_t)(msgLen + 3);   // small gap at the end

    uint16_t elapsed = 0;

    for (uint8_t pos = 0; pos < steps; ++pos) {
        uiScrollPrint21AtRow(oled, UI_SPLASH_ANIM_ROW, msg, msgLen, pos);

        delay(SPLASH_CREDITS_STEP_MS);
        elapsed = (uint16_t)(elapsed + SPLASH_CREDITS_STEP_MS);
        if (elapsed >= total_ms) return;
    }

    // scroll ended early, keep splash visible for the rest
    if (elapsed < total_ms) delay((uint16_t)(total_ms - elapsed));
}
#endif

// show startup screen then hold to let user read title before first draw
void showSplashScreen() {
    oled_cls();

    drawInverted(UI_SPLASH_LINE1_X, UI_SPLASH_LINE1_ROW, APP_NAME_LINE1, false);
    drawInverted(UI_SPLASH_LINE2_X, UI_SPLASH_LINE2_ROW, APP_NAME_LINE2, false);

#if defined(ENABLE_SPLASH_CREDITS_SCROLL) && ENABLE_SPLASH_CREDITS_SCROLL
    splashCreditsScroll((uint16_t)SPLASH_CREDITS_TOTAL_MS);
#else
#if ANIMATE_SPLASH
    for (uint8_t i = 0; i < UI_SPLASH_ANIM_STEPS; ++i) {
        splashAnimStep(i);
        delay(UI_SPLASH_ANIM_DELAY_MS);
    }
#endif
    delay(UI_SPLASH_HOLD_MS);
#endif
}


// ====================================================================================
// ===== UI: MESSAGES =================================================================
// ====================================================================================

// show save confirmation then return to main
// brief delay makes message readable
void showSavedConfirmation() {
    oled_xy(UI_SAVED_MSG_X, UI_SAVED_MSG_ROW);
    oled_puts_P("SAVED");
    delay(UI_SAVED_MSG_MS);
    showStatus(true); // force freq redraw to clear message
}


// ====================================================================================
// ===== UI: STATUS ORCHESTRATOR ======================================================
// ====================================================================================

// draw main status in stable order
// frequency first so rest aligns to that layout
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
