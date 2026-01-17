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

// ===== UI layout constants (main) =====
static constexpr uint8_t  UI_SCREEN_W = 128;
static constexpr uint8_t  UI_CHAR_W = 6;
static constexpr uint8_t  UI_CHAR_H = 8;
static constexpr uint8_t  UI_COLS = 21;                     // 128 / 6 ~= 21

// Spacing between seven-segment characters
// keep legibility on small OLED without redesigning glyphs
static constexpr uint8_t   DIGIT_SPACING = 3;

// Frequency area
static constexpr uint8_t  UI_FREQ_TOP_PX = 16;              // Y for big digits (px)
static constexpr uint8_t  UI_FREQ_X_OFFSET_SSB_PX = 3;      // left offset SSB
static constexpr uint8_t  UI_FREQ_X_OFFSET_AMFM_PX = 12;    // left offset AM/FM
static constexpr uint8_t  UI_FREQ_MAIN_WIDTH_AMFM = 5;      // convertToChar width in AM/FM
static constexpr uint8_t  UI_FREQ_BUF_LEN = 7;              // frequency display buffer
static constexpr uint16_t UI_SSB_TAIL_WIDTH = 1 +
SEVEN_SEG_DOT_WIDTH + DIGIT_SPACING +
SEVEN_SEG_DIGIT_WIDTH + DIGIT_SPACING +
SEVEN_SEG_DIGIT_WIDTH;                                  // ".dd" tail width

// Units label
static constexpr uint8_t  UI_UNIT_X_PX = 109;

// Splash
static constexpr uint8_t  UI_SPLASH_LINE1_X = 26;
static constexpr uint8_t  UI_SPLASH_LINE1_ROW = 1;
static constexpr uint8_t  UI_SPLASH_LINE2_X = 46;
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

const char g_bandModeDesc[][UI_MODE_ABBR_LEN] = { "AM ", "LSB", "USB", "CW ", "FM " };

static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, UI_BAND_NAME_LEN);
    buffer[UI_BAND_NAME_LEN] = '\0'; // ensure null
}

// ==========================================
// ===== UI DRAWING UTILITIES ===============
// ==========================================

// clear a rectangular region
// partialUpdate with null buffer erases box fast
static inline void clearBox(uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h) {
    oled.partialUpdate(x, y, w, h, NULL);
}

// print text with temporary inversion
// reset invert state to avoid leaking styles into next draw
template <typename T>
static void drawInverted(uint8_t x,
    uint8_t y,
    T text,
    bool invert) {
    oled.setCursor(x, y);
    oled.invertText(invert);
    oled.print(text);
    oled.invertText(false);
}

// Calculate screen position for settings item
// Layout is ALWAYS Row-first (NAV only affects cursor movement order)
// Row 0: items 0,1 | Row 1: items 2,3 | Row 2: items 4,5
static inline void calcSettingPos(
    uint8_t idx,
    uint8_t& xOffset,
    uint8_t& yOffset) {
    uint8_t place = idx % UI_SETTINGS_PER_PAGE; // 0..5 on page
    uint8_t row = place >> 1;                   // 0..2 every two items
    bool right = (place & 1);                   // odd places are right column

    xOffset = right ? UI_SETTINGS_RIGHT_COL_X : UI_SETTINGS_LEFT_COL_X;
    yOffset = UI_SETTINGS_ROW_START + row * UI_SETTINGS_ROW_STEP;
}

// ==========================================
// ===== UI: BRIGHTNESS =====================
// ==========================================

// map 0..9 to non linear contrast
// give finer control at low end where eyes are sensitive
static inline uint8_t brightnessToContrast(uint8_t s) {

    // orig curve is
    // contrast(s) = ((uint32_t)s * ((uint16_t)s * 130 + 6060)) >> 8
    // its for s=0..9
    static uint8_t lut[10] = { 0, 24, 49, 75, 102, 131, 160, 190, 221, 254 };
    return lut[s];
}

// apply contrast curve to OLED
// add +1 to avoid full black at lowest setting
static void applyBrightness() {
    uint8_t s = (uint8_t)getSettingParam(Brightness);
    oled.setContrast((uint8_t)(brightnessToContrast(s) + 1));
}

// ==========================================
// ===== UI: SIMPLE STATUS WIDGETS ==========
// ==========================================

// compute single char indicator for shared slot
// compact mapping keeps UI readable at a glance
static inline __attribute__((always_inline))
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
    oled.setCursor(UI_STEREO_INDICATOR_X, UI_STEREO_INDICATOR_ROW);
    oled.write(stereoIndicatorChar());
}

// show band tag like "40m"
// invert only when band command is active and not FM
static void showBandTag() {
    RETURN_IF_SETTINGS_ACTIVE();

    bool invert = (g_activeCommand == CMD_BAND &&
        g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    drawInverted(0, 0, name_buffer, invert);
}

// modulation label and stereo indicator together
// keeps related status in one glance area
static void showModulation() {
    bool invert = (g_activeCommand == CMD_BAND &&
        g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);

    drawInverted(UI_MODE_LABEL_X,
        UI_MODE_LABEL_ROW,
        g_bandModeDesc[g_currentMode],
        invert);

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

    bool invert = (g_activeCommand == CMD_VOLUME);
    drawInverted(UI_VOLUME_X, UI_VOLUME_ROW, buf, invert);
}

// show Soft Mute hint when DSP attenuates weak signals
// explains faint audio without user guessing
static void showRfHints() {
    if (isSSB()) return;

    constexpr uint8_t x = UI_VOLUME_X;
    constexpr uint8_t row = UI_VOLUME_ROW + 2;  // always valid

    clearBox(x, row * UI_CHAR_H, 2 * UI_CHAR_W, UI_CHAR_H);

    if (g_si4735.getCurrentSoftMuteIndicator()) {
        oled.setCursor(x, row);
        oled.print(F("SM"));
    }
}

// =-=-=-=-=-=-=-=-= UI: Signal Quality Display Mini-Block =-=-=-=-=-=-=-=-=

// blank field on missing value so user does not see stale text
static inline void uiSignalClear() {
    oled.setCursor(UI_RSSI_X, UI_RSSI_ROW);
    oled.print(F("   "));
}

// switch between raw RSSI and S scale since users read S on HF and digits on FM
static inline void uiRenderSignalValue(uint8_t rssi, uint8_t useSMeter) {
    oled.setCursor(UI_RSSI_X, UI_RSSI_ROW);

    switch (useSMeter) {
    case 0: {
        // pad single digit for width 2
        if (rssi < 10) oled.write(' ');
        oled.print(rssi);
        oled.write(UI_RSSI_SEPARATOR);
        break;
    }
    default: {
        char s_buffer[4];
        rssiToSLevel(s_buffer, rssi);
        oled.print(s_buffer);
        break;
    }
    }
}

// skip while menus open and draw hints after value
// keep UI free from mapping rules, use rssiToSLevel for S text
static void showSignalQuality() {
    if (g_settingsActive
#if ENABLE_FAVORITES
        || g_favoritesActive
#endif
        ) return;

    // Clear if no RSSI data OR RSSI polling disabled in AM
    if (g_signalQualityValue == UI_SIGNAL_NO_VALUE ||
        (g_currentMode == AM && getSettingParam(RSSI_AM_Off) == 1)) {
        uiSignalClear();
    } else {
        uiRenderSignalValue(g_signalQualityValue, (uint8_t)getSettingParam(SMeter));
    }

    showRfHints();
}

// show stable battery percent
static void showChargeOnDisplay() {
    RETURN_IF_SETTINGS_ACTIVE();
    uint8_t charge = g_stableBatteryPercent;
    drawInverted(UI_BATT_X, UI_BATT_ROW, charge, false);
    if (charge < 100) oled.print('%');
}

// show step value based on mode
// index maps user selection to readable label
static void showStep() {
    bool invert = (g_activeCommand == CMD_STEP);

    drawInverted(UI_STEP_LABEL_X, UI_STEP_LABEL_ROW, F("STEP: "), invert);

    const Band& current_band = g_bandList[g_bandIndex];
    uint8_t index;

    switch (g_currentMode) {
    case FM:
        index = (uint8_t)(4 + current_band.stepIdxFM);
        break;

    case LSB:
    case USB:
    case CW: // CW shares the same step settings as SSB
        index = (uint8_t)(SSB_STEP_OFFSET + current_band.stepIdxSSB);
        break;

    case AM:
    default:
        index = (uint8_t)current_band.stepIdxAM;
        break;
    }

    oled.invertText(invert);
    oled.print((__FlashStringHelper*)step_lookup_table[index]);
    oled.invertText(false);
}

// resolve BW table and index for current mode
// CW returns false since value not selectable
static inline bool bwResolve(const uint8_t*& table_ptr,
    uint8_t& index) {
    switch (g_currentMode) {
    case LSB:
    case USB:
        table_ptr = bw_ssb_map;
        index = g_bandList[g_bandIndex].bwIdxSSB;
        return true;
    case AM:
        table_ptr = bw_am_map;
        index = g_bandList[g_bandIndex].bwIdxAM;
        return true;
    case FM:
        table_ptr = bw_fm_map;
        index = g_bandList[g_bandIndex].bwIdxFM;
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
    if (!bwResolve(table_ptr, index)) return;

    uint8_t offset = pgm_read_byte(&table_ptr[index]);
    const char* bw_str_ptr = &bw_all_data[offset];

    bool invert = (g_activeCommand == CMD_BW);
    drawInverted(UI_BW_LABEL_X,
        UI_BW_LABEL_ROW,
        (__FlashStringHelper*)bw_str_ptr,
        invert);
}

// ==========================================
// ===== UI: MAIN FREQUENCY DISPLAY =========
// ==========================================

// =-=-=-=-=-=-=-=-= UI: MAIN FREQUENCY HELPERS =-=-=-=-=-=-=-=-=

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

    oled.drawDigit('.', curX, pixelY);
    curX += SEVEN_SEG_DOT_WIDTH + DIGIT_SPACING;

    oled.drawDigit('0' + (tailBFO / 10), curX, pixelY);
    curX += SEVEN_SEG_DIGIT_WIDTH + DIGIT_SPACING;

    oled.drawDigit('0' + (tailBFO % 10), curX, pixelY);
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
static inline __attribute__((always_inline))
uint8_t freqLeftOffset(bool ssbMode) {
    return ssbMode ? UI_FREQ_X_OFFSET_SSB_PX
        : UI_FREQ_X_OFFSET_AMFM_PX;
}

// choose start X so main block stays right aligned to unit label
// use left offset as lower bound to avoid clipping left edge
static inline __attribute__((always_inline))
int freqStartX(uint16_t totalWidth,
    uint8_t leftOff) {
    int aligned = (int)UI_UNIT_X_PX - DIGIT_SPACING - (int)totalWidth;
    return aligned > (int)leftOff ? aligned : (int)leftOff;
}

// visible length of numeric part
// SSB uses BFO part so length differs from AM/FM
static inline __attribute__((always_inline))
uint8_t visibleLen(bool ssbMode,
    uint16_t khzBFO) {
    return ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);
}

// compute widths and start position
// one place to keep alignment math together
static inline __attribute__((always_inline))
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
static inline __attribute__((always_inline))
void freqDrawMainAndTail(const char* freqDisplay,
    int startX,
    int pixelY,
    bool ssbMode,
    uint16_t tailBFO) {
    int endX = renderFrequencyString(freqDisplay, startX, pixelY);
    drawSSBTailIfNeeded(ssbMode, endX, pixelY, tailBFO);
}

// =-=-=-=-=-=-=-=-= UI: MAIN FREQUENCY CONFIG =-=-=-=-=-=-=-=-=

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
        splitFreq(khzBFO, tailBFO);
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
    oled.setCursor(UI_UNIT_X_PX, row);
    oled.print(unit);
}

// draw main numeric block char by char
// spacing matches seven segment glyph metrics
static int renderFrequencyString(const char* freqDisplay,
    int startX,
    int pixelY) {
    int curX = startX;

    for (const char* p = freqDisplay; *p; ++p) {
        char ch = *p;
        oled.drawDigit(ch, curX, pixelY);
        curX += (ch == '.' ? SEVEN_SEG_DOT_WIDTH
            : SEVEN_SEG_DIGIT_WIDTH) + DIGIT_SPACING;
    }
    // remove trailing spacing to align SSB tail attach point
    return curX - DIGIT_SPACING;
}

// =-=-=-=-=-=-=-=-= UI: MAIN FREQUENCY RENDER =-=-=-=-=-=-=-=-=

// right align main frequency and draw optional SSB tail
// avoids jumpiness during tuning and keeps unit column stable
static void showFrequency(bool cleanDisplay = false) {
    RETURN_IF_SETTINGS_ACTIVE();

    static uint8_t prevLen = 0;

    char     freqDisplay[UI_FREQ_BUF_LEN];
    uint16_t khzBFO, tailBFO;
    bool     ssbMode = isSSB();
    BandType band = g_bandList[g_bandIndex].bandType;

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

// ==========================================
/* ===== UI: FAVORITES MENU DRAWING ========= */
// ==========================================

#if ENABLE_FAVORITES

// =-=-=-=-=-=-=-=-= UI: FAVORITES HELPERS =-=-=-=-=-=-=-=-=

// page for a given index
// simple math keeps navigation predictable
#define fav_pageOf(i) ((i) / UI_FAV_ITEMS_PER_PG)

// page bounds [start, end)
// clamp to total to avoid out of range reads
static inline void fav_getPageBounds(uint8_t page,
    uint8_t& start,
    uint8_t& end) {
    start = page * UI_FAV_ITEMS_PER_PG;
    end = (start + UI_FAV_ITEMS_PER_PG < g_totalFavorites)
        ? (start + UI_FAV_ITEMS_PER_PG)
        : g_totalFavorites;
}

// row index on current page
// fixed spacing keeps list readable on small OLED
#define fav_rowForIndex(i, ps) (UI_FAV_LIST_START_ROW + ((i) - (ps)) * UI_FAV_ROW_GAP)

// padding to right-align XX|YY in header
// stable header width aids orientation
static inline uint8_t fav_calcHeaderPadding(uint8_t selected,
    uint8_t total) {
    uint8_t selDigits = (selected > 9) ? 2 : 1;
    uint8_t totDigits = (total > 9) ? 2 : 1;
    uint8_t counterWidth = selDigits + 1 + totDigits; // "XX|YY"
    return (UI_COLS - UI_FAV_HEADER_TITLE_WIDTH) - counterWidth;
}

// start column for right-aligned freq block
// prevents overlap with prefix and mode label
static inline uint8_t fav_calcFreqStartCol(uint8_t freqWidth) {
    uint8_t col = UI_FAV_FREQ_RIGHT_COL - freqWidth;
    if (col < UI_FAV_FREQ_MIN_COL) col = UI_FAV_FREQ_MIN_COL;
    return col;
}

// =-=-=-=-=-=-=-=-= UI: FAVORITES AM/SSB HELPERS =-=-=-=-=-=-=-=-=

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

// print ".dd" with leading zero
// aligns with main list formatting
static inline __attribute__((always_inline))
void fav_printDotDec2(uint16_t tl) {
    oled.print('.');
    if (tl < 10) oled.print('0');
    oled.print(tl);
}

// =-=-=-=-=-=-=-=-= UI: FAVORITES RENDER =-=-=-=-=-=-=-=-=

// prefix '>' + 1-based 2-digit number
// fixed width keeps grid aligned
static inline void fav_drawPrefix(uint8_t idx, bool sel) {
    oled.print(sel ? '>' : ' ');
    uint8_t num = idx + 1;
    oled.print((char)('0' + num / 10));
    oled.print((char)('0' + num % 10));
    oled.print(' ');
}

// FM freq right-aligned before mode label
// right align preserves visual grid across list
static inline void fav_drawFreqFM(const FavoriteStation& fav,
    uint8_t row) {
    uint16_t ip = fav.frequency / 100;
    uint8_t  dp = (fav.frequency % 100) / 10;

    uint8_t width = (ip < 100) ? 4 : 5;  // 88..99 -> 4, 100..108 -> 5

    uint8_t startCol = fav_calcFreqStartCol(width);
    oled.setCursor(startCol * UI_CHAR_W, row);
    oled.print(ip);
    oled.print('.');
    oled.print(dp);
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

    if (isSSB) fav_printDotDec2(tl);
}

// mode label at fixed column (rightmost)
// fixed column makes scan easy on small OLED
static inline void fav_drawModeLabel(const FavoriteStation& fav,
    uint8_t row) {
    oled.setCursor(UI_FAV_MODE_LABEL_X, row);
    oled.print(g_bandModeDesc[fav.modulation]);
}

// draw one favorite line
// clear row to avoid leftovers from previous content
static inline void fav_drawLine(uint8_t idx,
    uint8_t row,
    bool sel) {
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

// =-=-=-=-=-=-=-=-= UI: FAVORITES HEADER =-=-=-=-=-=-=-=-=

// header with title and right-aligned counter
// stable header helps orientation across pages
static void fav_drawHeader() {
    oled.setCursor(0, UI_FAV_HEADER_ROW);
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

// =-=-=-=-=-=-=-=-= UI: FAVORITES CONTENT FLOW =-=-=-=-=-=-=-=-=

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
        fav_getPageBounds(page, start, end);

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
    fav_getPageBounds(page, start, end);

    for (uint8_t i = start; i < end; ++i) {
        uint8_t row = fav_rowForIndex(i, start);
        oled.setCursor(0, row);
        oled.print(i == g_favoriteSelected ? '>' : ' ');
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

// =-=-=-=-=-=-=-=-= UI: FAVORITES ORCHESTRATION =-=-=-=-=-=-=-=-=

// Favorites menu orchestrator
// choose cheapest update path to keep UI snappy
static void showFavorites(bool force_redraw) {
    if (g_favoriteSelected >= g_totalFavorites) {
        g_favoriteSelected = g_totalFavorites
            ? (g_totalFavorites - 1) : 0;
    }

    fav_drawHeader();
    fav_handleContent(force_redraw);
}

#endif  // ENABLE_FAVORITES

// ==========================================
// ===== UI: SETTINGS MENU DRAWING ==========
// ==========================================

// =-=-=-=-=-=-=-=-= UI: SETTINGS PAGE =-=-=-=-=-=-=-=-=

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

    uint8_t base =
        pgm_read_byte(&switch_setting_map[idx].baseIndex);
    uint8_t inverted =
        pgm_read_byte(&switch_setting_map[idx].inverted);
    uint8_t textIdx =
        base + (inverted ? -param : param);
    strcpy_P(buf, paramTexts[textIdx]);
}

// =-=-=-=-=-=-=-=-= UI: SPECIAL PARAM FORMATTERS =-=-=-=-=-=-=-=-=

// handle indices with aliases or lookup tables first
// user expects names for pins and Hz instead of raw numbers
static inline __attribute__((always_inline))
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
static inline __attribute__((always_inline))
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
static inline __attribute__((always_inline))
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
static inline __attribute__((always_inline))
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
        strcpy_P(buf, PSTR("---"));
        return;
    }

    int8_t  param = getSettingParam(idx);
    uint8_t type = getSettingType(idx);

    if (SettingParamToUI_Special(buf, idx, param, type)) return;

    SettingParamToUI_Number(buf, idx, param);
}

// =-=-=-=-=-=-=-=-= UI: DRAW HELPERS =-=-=-=-=-=-=-=-=

// compute on-screen position for global index
// local index avoids reflow when page changes
static inline __attribute__((always_inline))
void settingPosFromIndex(uint8_t idx,
    uint8_t page,
    uint8_t& x,
    uint8_t& y) {
    uint8_t local = idx - ((page - 1) * UI_SETTINGS_PER_PAGE);
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
    oled.setCursor(UI_SETTING_NAME_PREFIX_X + x, y);
    oled.write(selName ? '>' : ' ');
    oled.print(name);

    oled.setCursor(UI_SETTING_VALUE_X + x, y);
    oled.write(selVal ? '>' : ' ');
    oled.print(val);
}

// draw value cell only
// cheap marker indicates edit state without redrawing name
static inline __attribute__((always_inline))
void drawValueCell(uint8_t x,
    uint8_t y,
    const char* buf,
    bool editing) {
    oled.setCursor(UI_SETTING_VALUE_X + x, y);
    oled.write(editing ? '>' : ' ');
    oled.print(buf);
}

// draw full row with highlight flags
// highlight when selected and not editing to match UX
static inline __attribute__((always_inline))
void drawFullRow(uint8_t x,
    uint8_t y,
    uint8_t idx,
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

// =-=-=-=-=-=-=-=-= UI: SETTINGS RENDER =-=-=-=-=-=-=-=-=

// draw single setting row
// format once then choose full redraw or value only to cut OLED traffic
static void DrawSetting(uint8_t idx,
    bool full) {
    if (!g_settingsActive) return;

    char valBuf[UI_SETTING_NUM_WIDTH + 1];
    char nameBuf[4];

    uint8_t x, y;
    settingPosFromIndex(idx, g_SettingsPage, x, y);

    SettingParamToUI(valBuf, idx);

    const bool isSelected = (idx == g_SettingSelected);
    const bool isEditing = (isSelected && g_SettingEditing);

    if (full) {
        getSettingName(idx, nameBuf);
        drawFullRow(x, y, idx, nameBuf, valBuf, isSelected, isEditing);
    } else {
        drawValueCell(x, y, valBuf, isEditing);
    }
}

// render header with page counters
// invert draws bar without extra bitmap fixed label masks stale pixels
static void showSettingsTitle() {
    oled.setCursor(0, 0);
    oled.invertText(true);
    oled.print(F("      SETTINGS    "));
    oled.print((uint8_t)g_SettingsPage);
    oled.write('|');
    oled.print((uint8_t)g_SettingsMaxPages);
    oled.invertText(false);
}

// draw current page only
// precompute base to cut math per item and guard end of list
static void showSettings() {
    const uint8_t base =
        (g_SettingsPage - 1) * UI_SETTINGS_PER_PAGE;
    for (uint8_t i = 0; i < UI_SETTINGS_PER_PAGE; ++i) {
        const uint8_t cur = base + i;
        if (cur >= SETTINGS_MAX) break;
        DrawSetting(cur, true);
    }
}

// ==========================================
// ===== UI: ORCHESTRATORS & MESSAGES =======
// ==========================================

// =-=-=-=-=-=-=-=-= UI: SPLASH =-=-=-=-=-=-=-=-=

#if ANIMATE_SPLASH
// draw one animation dash
// simple sweep gives user feedback while init runs
static inline __attribute__((always_inline))
void splashAnimStep(uint8_t i) {
    oled.setCursor(i * UI_CHAR_W, UI_SPLASH_ANIM_ROW);
    oled.write('-');
}
#endif

// show startup screen then clear
// hold to let user read title before first draw
void showSplashScreen() {
    oled.clear();

    drawInverted(UI_SPLASH_LINE1_X, UI_SPLASH_LINE1_ROW,
        APP_NAME_LINE1, false);
    drawInverted(UI_SPLASH_LINE2_X, UI_SPLASH_LINE2_ROW,
        F("ATS EX"), false);

#if ANIMATE_SPLASH
    for (uint8_t i = 0; i < UI_SPLASH_ANIM_STEPS; i++) {
        splashAnimStep(i);
        delay(UI_SPLASH_ANIM_DELAY_MS);
    }
#endif

    delay(UI_SPLASH_HOLD_MS);
    oled.clear();
}

// =-=-=-=-=-=-=-=-= UI: MESSAGES =-=-=-=-=-=-=-=-=

// show save confirmation then return to main
// brief delay makes message readable
void showSavedConfirmation() {
    oled.setCursor(UI_SAVED_MSG_X, UI_SAVED_MSG_ROW);
    oled.print(F("SAVED"));
    delay(UI_SAVED_MSG_MS);
    showStatus(true); // force freq redraw to clear message
}

// =-=-=-=-=-=-=-=-= UI: STATUS ORCHESTRATOR =-=-=-=-=-=-=-=-=

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
