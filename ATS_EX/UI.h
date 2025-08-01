#pragma once

// ======================================================================
// UI.h - UI Drawing Subsystem for ATS_EX
// contains all functions responsible for rendering information on the OLED display
// ======================================================================

#include "Defines.h"
#include "Globals.h"
#include "Utils.h"
#include "SSD1306_OLED.h"

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// Spacing between seven-segment characters
const int DIGIT_SPACING = 2;

static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, 4);
    buffer[4] = '\0'; // ensure null
}

// ==========================================
// ===== UI DRAWING UTILITIES ===============
// ==========================================

// Utility to clear a rectangular region of the display
static inline void clearBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    // null pointer in partialUpdate clears the area
    oled.partialUpdate(x, y, w, h, NULL);
}

// Utility to calculate the X/Y offsets for a setting in the settings menu
static void calcSettingPos(uint8_t idx, uint8_t& xOffset, uint8_t& yOffset) {
    uint8_t place = idx % 6; // Position within the current page (0-5)
    bool isRight = place > 2;
    xOffset = isRight ? 68 : 0;
    yOffset = ((place - (isRight * 3)) << 1) + 2;
}

// Renders just the SSB tail digits (e.g., ".00") with spacing
static int drawSSBTailDigits(int startX, int pixelY, uint16_t tailBFO) {
    int curX = startX;
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

// Sets cursor, prints text with optional inversion, and resets inversion state
template <typename T>
static void drawInverted(uint8_t x, uint8_t y, T text, bool invert) {
    oled.setCursor(x, y);
    oled.invertText(invert);
    oled.print(text);
    oled.invertText(false); // Always reset state to non-inverted
}

// Draws a single item (name and value) in the settings menu
static void drawSettingItem(
    uint8_t x, uint8_t y,
    const char* name, const char* val,
    bool selName, bool selVal) {
    oled.setCursor(5 + x, y);
    oled.print(selName ? '>' : ' ');
    oled.print(name);

    oled.setCursor(35 + x, y);
    oled.print(selVal ? '>' : ' ');
    oled.print(val);
}

// ==========================================
// ===== UI DRAWING SUBSYSTEM ===============
// ==========================================

// --- UI: General & Splash Screen ---

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

    drawInverted(26, 1, F("ATS-20* V5.9"), false);
    drawInverted(32, 3, F("MOD NO RDS"), false);

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

// --- UI: Main Screen Drawing ---

// display mode, dot position, units based on current band
static void prepareDisplayConfig(
    bool ssbMode, BandType band,
    uint8_t& outMode, uint8_t& outDotPos,
    const char*& outUnit) {

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
static void prepareMainFreq(
    uint8_t displayMode, char* freqDisplay,
    uint16_t& khzBFO, uint16_t& tailBFO, uint8_t dotPos) {

    if (displayMode == 2) { // SSB
        splitFreq(khzBFO, tailBFO);
        convertToChar(freqDisplay, khzBFO, ilen(khzBFO), 0, '.', ' ');
    } else { // AM / FM
        convertToChar(freqDisplay, g_currentFrequency, 5, dotPos, '.', '/');
    }
}

// clean background when frequency length changes
static void renderClearOrBlink(
    bool cleanDisplay,
    uint8_t len, uint8_t prevLen,
    uint8_t off, int pixelY) {

    if (cleanDisplay) {
        clearBox(0, pixelY, 128, SEVEN_SEG_DIGIT_HEIGHT);
    } else if (len != prevLen) {
        // if frequency length changes - clear from its starting position to the end of the screen
        clearBox(off, pixelY, 128 - off, SEVEN_SEG_DIGIT_HEIGHT);
    }
}

// renders measurement units (kHz/MHz)
static void renderUnit(bool ssbMode, uint8_t len, const char* unit) {
    if (!ssbMode || len < 5) {
        drawInverted(108, 4, unit, false);
    }
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

//Draw frequency on display.
static void showFrequency(bool cleanDisplay = false) {
    RETURN_IF_SETTINGS_ACTIVE();

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

    // Set cursor position for frequency display
    int pixelY = 16 * 1;

    renderClearOrBlink(cleanDisplay, len, prevLen, off, pixelY);

    // Render main frequency and get its end X position
    int mainEndX = renderFrequencyString(freqDisplay, off, pixelY);

    drawSSBTailIfNeeded(ssbMode, mainEndX, pixelY, tailBFO);
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
    RETURN_IF_SETTINGS_ACTIVE();

    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    drawInverted(0, 0, name_buffer, invert);
}

// determine the indicator character based on mode
void updateStereoIndicator() {
    char c = (g_currentMode == CW)
        ? (g_lastCWMode == LSB ? 'L' : 'U')
        : (isSSB() && g_Settings[Sync].param == 1) ? 'S'
        : (g_currentMode == FM && g_stereoStatus) ? '*' : ' ';

    oled.setCursor(24, 7);
    oled.print(c);
}

//Draw current modulation (AM/LSB/USB/CW/FM) and stereo indicator
static void showModulation() {
    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    drawInverted(0, 7, g_bandModeDesc[g_currentMode], invert);

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
    drawInverted(114, 0, buf, invert);
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
    RETURN_IF_SETTINGS_ACTIVE();
    int charge = min(g_stableBatteryPercent, 100);
    drawInverted(108, 7, charge, false);
    if (charge < 100) oled.print('%');
}

// display the step on the screen
static void showStep() {
    bool invert = (g_activeCommand == CMD_STEP);

    drawInverted(34, 0, F("STEP: "), invert);

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
    drawInverted(40, 7, (__FlashStringHelper*)bw_str_ptr, invert);
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

// --- UI: Favorites Menu Drawing ---

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
    oled.print(F(" MHZ  "));
}

// Display favorites menu
static void showFav() {
    drawInverted(0, 0, F("     FM FAVORITES    "), true);

    if (!g_totalFavorites) {
        drawInverted(30, 3, F("NO SAVED"), false);
        return;
    }

    uint8_t start = (g_favoriteSelected >> 1) << 1;
    uint8_t end = start + 2;
    if (end > g_totalFavorites) end = g_totalFavorites;

    for (uint8_t i = start; i < end; i++) {
        drawFavItem(i, 2 + ((i - start) << 1), i == g_favoriteSelected);
    }

    if (end == start + 1) {
        // Since drawFavItem sets the cursor - clear by coordinates
        clearBox(0, 4 * 8, 128, 8); // y is in pixels
    }

    oled.setCursor(0, 7);
    oled.print(' ');
    oled.print(g_favoriteSelected + 1);
    oled.print('/');
    oled.print(g_totalFavorites);
    oled.print(F(" DEL:BW"));
}
#endif

// --- UI: Settings Menu Drawing ---

// Maps a setting parameter to its UI display string
// case handles DisplayOff due to non-sequential text indices,
// while other types use direct or data-driven mapping from PROGMEM
static inline void handleSwitchParam(
    char* buf, uint8_t idx,
    int8_t param, uint8_t type) {
    uint8_t base = pgm_read_byte(&switch_setting_map[idx].baseIndex);
    uint8_t inverted = pgm_read_byte(&switch_setting_map[idx].inverted);

    uint8_t textIdx = (idx == SettingsIndex::DisplayOff)
        ? (param ? 10 + param : 2)
        : (type == SettingType::SwitchAuto ? param
            : (base + (inverted ? -param : param)));

    strcpy_P(buf, paramTexts[textIdx]);
}

// Converts a setting parameter to its UI display string
static void SettingParamToUI(char* buf, uint8_t idx) {
    const auto& s = g_Settings[idx];
    int8_t param = s.param;

    if (idx == SettingsIndex::BATT_PIN) {
        // LF - named constants in Battery.h for the UI strings
        strcpy_P(buf, (param == 1) ? BATT_PIN_NAME_ALT : BATT_PIN_NAME_DEFAULT);
        return;
    }

    if (s.type >= SettingType::Switch) {
        handleSwitchParam(buf, idx, param, s.type);
        return;
    }

    // settings like Attenuation (ATT) value of 0 represents automatiс mode
    if (s.type == SettingType::ZeroAuto && param == 0) {
        strcpy_P(buf, paramTexts[0]); // "AUT"
        return;
    }

    int8_t val = param;

    // brightness is stored 0-indexed internally but displayed to the user as 1-based
    if (idx == SettingsIndex::Brightness) val++;

    uint8_t val_to_convert = (val < 0) ? -val : val;

    convertToChar(buf, val_to_convert, 3);

    // restore for neg value
    if (param < 0) buf[0] = '-';

    buf[3] = '\0';
}

// Draw a single setting item in the settings menu
static void DrawSetting(uint8_t idx, bool full) {
    if (!g_settingsActive) return;

    char buf[5];

    uint8_t xOffset, yOffset;
    // Calculate position based on the index relative to start of page
    calcSettingPos(idx - ((g_SettingsPage - 1) * 6), xOffset, yOffset);

    if (full) {
        SettingParamToUI(buf, idx);
        drawSettingItem(
            xOffset, yOffset, g_Settings[idx].name, buf,
            (idx == g_SettingSelected && !g_SettingEditing),
            (idx == g_SettingSelected && g_SettingEditing));
    } else {
        SettingParamToUI(buf, idx);
        oled.setCursor(35 + xOffset, yOffset);
        oled.print((idx == g_SettingSelected && g_SettingEditing) ? '>' : ' ');
        oled.print(buf);
    }
}

// Draw the title of the settings menu
static void showSettingsTitle() {
    oled.setCursor(0, 0);
    oled.invertText(true);
    oled.print(F("      SETTINGS    "));
    oled.print((uint8_t)g_SettingsPage);
    oled.print('|');
    oled.print((uint8_t)g_SettingsMaxPages);
    oled.invertText(false);
}

// Draw the complete settings screen (all visible items)
static void showSettings() {
    for (uint8_t i = 0; i < 6 && i + ((g_SettingsPage - 1) * 6) < SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * 6), true);
}
