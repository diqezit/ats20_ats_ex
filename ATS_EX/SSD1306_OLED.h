/*
    (!) It is based on the original GyverOLED library by AlexGyver
        https://github.com/GyverLibs/GyverOLED
        AlexGyver, alex@alexgyver.ru
        https://alexgyver.ru/
        MIT License

    (!) But heavily simplified for flash size and delete reundant functions
    (!) Simplified only for SSD1306 128x64, I2C, minimal buffer (only for segments drawings)
    (!) Minimal features for only main functionality with ATS_EX receiver

    Manual generation of segments 14x32 resolution (7-segment display) use SSD1306 minimal library
    By diqezit v2.1
    Charset: '.', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'

    My repos: https://github.com/diqezit/TestDisplay
    Datasheet for OLED SSD1306DEVICE: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
    Use workflow: https://github.com/uNetworking/SSD1306

    What was removed in this modification:
    - Support for SPI and SSH1106
    - Buffer support (OLED_BUFFER, update, createBuffer, sendBuffer, etc.)
    - Unused graphics: dot, line, rect, roundRect, circle, bezier
    - Bitmaps: drawBitmap, drawByte, drawBytes
    - Advanced text: autoPrintln, home, textMode, isEnd, Russian support, scale >1, line breaks (\r\n)
    - Display controls: flipH, flipV, invertDisplay
    - Partial clear with arguments (kept only full clear and custom partial)
    - Dynamic buffer and related flags
    - Unused variables: _mode, _lastChar, _println, _getn, _scaleX, _scaleY, _maxY, etc.
    - Simplified write: only scale=1, ASCII, no shifts or modes
    - Removed setScale() function (unused)
    - Removed delayMicroseconds(2) from endTransm() (I2C is slow enough)

*/

#ifndef SSD1306_OLED_H
#define SSD1306_OLED_H

#include <microWire.h>
#include "CustomFonts.h"

// =====================================================================================
// ===== Convenience macros =============================================================
// =====================================================================================

// print with temporary inversion: setCursor + invertText + print + invertText(false)
#define OLED_PRINT_INV_AT(OLED, X, Y, INV, TEXT)          \
    do {                                                  \
        (OLED).setCursor((X), (Y));                       \
        (OLED).invertText((INV));                         \
        (OLED).print((TEXT));                             \
        (OLED).invertText(false);                         \
    } while (0)

// write one char with temporary inversion: setCursor + invertText + write + invertText(false)
#define OLED_WRITE_INV_AT(OLED, X, Y, INV, CH)            \
    do {                                                  \
        (OLED).setCursor((X), (Y));                       \
        (OLED).invertText((INV));                         \
        (OLED).write((uint8_t)(CH));                      \
        (OLED).invertText(false);                         \
    } while (0)

// clear rectangle via partialUpdate(NULL)
#define OLED_CLEAR_BOX(OLED, X, Y, W, H)                  \
    do {                                                  \
        (OLED).partialUpdate((X), (Y), (W), (H), NULL);   \
    } while (0)

// clear rectangle + print text: partialUpdate + setCursor + print
#define OLED_CLEAR_AND_PRINT_AT(OLED, PX, PY, W, H, CX, CY, TEXT) \
    do {                                                         \
        (OLED).partialUpdate((PX), (PY), (W), (H), NULL);         \
        (OLED).setCursor((CX), (CY));                             \
        (OLED).print((TEXT));                                     \
    } while (0)

// clear rectangle + print text inverted:
// partialUpdate + setCursor + invertText + print + invertText(false)
#define OLED_CLEAR_AND_PRINT_INV_AT(OLED, PX, PY, W, H, CX, CY, INV, TEXT) \
    do {                                                                  \
        (OLED).partialUpdate((PX), (PY), (W), (H), NULL);                  \
        (OLED).setCursor((CX), (CY));                                      \
        (OLED).invertText((INV));                                          \
        (OLED).print((TEXT));                                              \
        (OLED).invertText(false);                                          \
    } while (0)


// =====================================================================================
// ===== Constants =====================================================================
// =====================================================================================

#define SSD1306_128x64 1
#define OLED_NO_BUFFER 0

// ===== Seven Segment Digit Dimensions =====
// These constants define the canvas size for the large frequency digits
// Changing these requires redesigning the segment blueprints in the 'segs' array
constexpr uint8_t SEVEN_SEG_DIGIT_WIDTH = 14;
constexpr uint8_t SEVEN_SEG_DIGIT_HEIGHT = 32;
constexpr uint8_t SEVEN_SEG_DOT_WIDTH = 4;

// ===== Backend Constants =====
#define OLED_WIDTH 128
#define OLED_HEIGHT_64 0x12
#define OLED_64 0x3F

#define OLED_DISPLAY_OFF 0xAE
#define OLED_DISPLAY_ON  0xAF

#define OLED_COMMAND_MODE     0x00
#define OLED_ONE_COMMAND_MODE 0x80
#define OLED_DATA_MODE        0x40

#define OLED_ADDRESSING_MODE 0x20
#define OLED_VERTICAL        0x01

#define OLED_NORMAL_V 0xC8
#define OLED_NORMAL_H 0xA1

#define OLED_CONTRAST      0x81
#define OLED_SETCOMPINS    0xDA
#define OLED_SETVCOMDETECT 0xDB
#define OLED_CLOCKDIV      0xD5
#define OLED_SETMULTIPLEX  0xA8
#define OLED_COLUMNADDR    0x21
#define OLED_PAGEADDR      0x22
#define OLED_CHARGEPUMP    0x8D

#define OLED_NORMALDISPLAY 0xA6

#define BUFSIZE_128x64 (128 * 64 / 8)

#define OLED_CLAMP(val, minv, maxv) ((val) < (minv) ? (minv) : ((val) > (maxv) ? (maxv) : (val)))


// =====================================================================================
// ===== Initialization list ============================================================
// =====================================================================================

static const uint8_t _oled_init[] PROGMEM = {
    OLED_DISPLAY_OFF,
    OLED_CLOCKDIV,
    0x80,
    OLED_CHARGEPUMP,
    0x14,
    OLED_ADDRESSING_MODE,
    OLED_VERTICAL,
    OLED_NORMAL_H,
    OLED_NORMAL_V,
    OLED_CONTRAST,
    0x7F,
    OLED_SETVCOMDETECT,
    0x40,
    OLED_NORMALDISPLAY,
    OLED_DISPLAY_ON,
    OLED_SETCOMPINS,
    OLED_HEIGHT_64,
    OLED_SETMULTIPLEX,
    OLED_64,
};


// =====================================================================================
// ===== Static Data Tables (Global PROGMEM) ===========================================
// =====================================================================================

// Segment bit positions for masks (7-seg + dot)
#define OLED_SEG_A   (1u << 0)
#define OLED_SEG_B   (1u << 1)
#define OLED_SEG_C   (1u << 2)
#define OLED_SEG_D   (1u << 3)
#define OLED_SEG_E   (1u << 4)
#define OLED_SEG_F   (1u << 5)
#define OLED_SEG_G   (1u << 6)
#define OLED_SEG_DOT (1u << 7)

// Bitmasks for digits 0-9 and dot / just for easier edit - thats all
static const uint8_t _oled_symbolMasks[11] PROGMEM = {
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_C | OLED_SEG_D | OLED_SEG_E | OLED_SEG_F),              // 0
    (OLED_SEG_B | OLED_SEG_C),                                                                  // 1
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_D | OLED_SEG_E | OLED_SEG_G),                           // 2
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_C | OLED_SEG_D | OLED_SEG_G),                           // 3
    (OLED_SEG_B | OLED_SEG_C | OLED_SEG_F | OLED_SEG_G),                                        // 4
    (OLED_SEG_A | OLED_SEG_C | OLED_SEG_D | OLED_SEG_F | OLED_SEG_G),                           // 5
    (OLED_SEG_A | OLED_SEG_C | OLED_SEG_D | OLED_SEG_E | OLED_SEG_F | OLED_SEG_G),              // 6
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_C),                                                     // 7
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_C | OLED_SEG_D | OLED_SEG_E | OLED_SEG_F | OLED_SEG_G), // 8
    (OLED_SEG_A | OLED_SEG_B | OLED_SEG_C | OLED_SEG_D | OLED_SEG_F | OLED_SEG_G),              // 9
    (OLED_SEG_DOT)                                                                              // .
};

#define OLED_SEG4(X, Y, LEN, H) (uint8_t)(X), (uint8_t)(Y), (uint8_t)(LEN), (uint8_t)(H)

// "Blueprints" for the seven-segment digits
// Each entry defines a segment position and dimensions on a canvas whose size is
// defined by SEVEN_SEG_DIGIT_WIDTH and SEVEN_SEG_DIGIT_HEIGHT
// Format (flat): x, y, len, isHoriz
//
//      ---a---
//     |       |
//     f       b
//     |       |
//      ---g---
//     |       |
//     e       c
//     |       |
//      ---d---
//
static const uint8_t _oled_segs[32] PROGMEM = {
    OLED_SEG4(1,    0,  12, 1),     // A  top
    OLED_SEG4(12,   1,  15, 0),     // B  upper right
    OLED_SEG4(12,   16, 15, 0),     // C  lower right
    OLED_SEG4(1,    30, 12, 1),     // D  bottom
    OLED_SEG4(0,    16, 15, 0),     // E  lower left
    OLED_SEG4(0,    1,  15, 0),     // F  upper left
    OLED_SEG4(1,    15, 12, 1),     // G  middle
    OLED_SEG4(1,    30,  2, 1)      // Dot
};

#undef OLED_SEG4


// =====================================================================================
// ===== Class Definition ===============================================================
// =====================================================================================

template <int _TYPE, int _BUFF = OLED_NO_BUFFER>
class GyverOLED {
public:
    // Constructor
    GyverOLED(uint8_t address = 0x3C) : _address(address) {}

    // =================================================================================
    // ===== Service Functions =========================================================
    // =================================================================================

    // Sets up I2C and sends initialization commands to configure display parameters
    void init(int __attribute__((unused)) sda = 0, int __attribute__((unused)) scl = 0) {
        Wire.begin();
        Wire.setClock(I2C_BASE_HZ);

        beginCommand();
        for (uint8_t i = 0; i < sizeof(_oled_init); i++) {
            sendByte(pgm_read_byte(&_oled_init[i]));
        }
        endTransm();

        setCursorXY(0, 0);
    }

    // Fills entire screen with zero bytes to erase all content
    void clear() { fill(0); }

    // Erases specified rectangular area by writing zero bytes to defined window
    void clear(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
        x1++;
        y1++;
        y0 >>= 3;
        y1 = (uint8_t)(y1 - 1) >> 3;
        y0 = OLED_CLAMP(y0, 0, _maxRow);
        y1 = OLED_CLAMP(y1, 0, _maxRow);
        x0 = OLED_CLAMP(x0, 0, _maxX);
        x1 = OLED_CLAMP(x1, 0, OLED_WIDTH);

        setWindowRaw(x0, y0, x1, y1);

        beginData();
        uint16_t bytes = (uint16_t)(x1 - x0) * (y1 - y0 + 1);
        while (bytes--) sendByte(0);
        endTransm();

        setCursorXY(_x, _y);
    }

    // Adjusts display brightness level from 0 (dim) to 255 (max)
    void setContrast(uint8_t value) { sendCommand(OLED_CONTRAST, value); }

    // Powers the OLED on/off
    //
    // SSD1306 has an internal DC-DC converter that generates the OLED panel voltage
    // Even when the display is turned OFF (0xAE), the charge pump may still run and can produce RF interference
    // Disabling it reduces noise (especially noticeable on MW/SW) and also lowers power consumption
    //
    // 0x8D + 0x14 = charge pump ON
    // 0x8D + 0x10 = charge pump OFF
    void setPower(bool mode) {
        beginCommand();
        sendByte(OLED_CHARGEPUMP);
        sendByte(mode ? 0x14 : 0x10);                 // enable/disable charge pump
        sendByte(mode ? OLED_DISPLAY_ON : OLED_DISPLAY_OFF);
        endTransm();
    }

    // =================================================================================
    // ===== Text Output ===============================================================
    // =================================================================================

    // Outputs single ASCII character with inversion support
    // glyph stored as 5 columns in _charMap_min[][5]
    // 6th column (spacing) is generated at runtime (0x00 or 0xFF if inverted)
    static constexpr uint8_t FONT_COLS = 5;
    static constexpr uint8_t FONT_SPACER = 1;
    static constexpr uint8_t FONT_TOTAL = FONT_COLS + FONT_SPACER;

    // Core write without beginData()/endTransm() for batching
    inline void writeCore(uint8_t data) {
        if (_x + FONT_TOTAL > _maxX) return;  // Skip if beyond screen

        const uint8_t inv = (uint8_t)_invState;

        // 5 columns from font table
        for (uint8_t col = 0; col < FONT_COLS; col++) {
            uint8_t bits = getFont(data, col);
            if (inv) bits = (uint8_t)~bits;
            sendByte(bits);
        }

        // 1 column spacing
        sendByte(inv ? 0xFF : 0x00);

        _x += FONT_TOTAL; // Update cursor once
    }

    // Single-char write
    size_t write(uint8_t data) {
        beginData();
        writeCore(data);
        endTransm();
        return 1;
    }

    // =-=-=-=-=-=-=-=-= Local print() (no Arduino Print.cpp) =-=-=-=-=-=-=-=-=

    // print single char
    size_t print(char c) {
        write((uint8_t)c);
        return 1;
    }

    // print RAM string
    size_t print(const char* s) {
        if (!s) return 0;
        beginData();
        size_t n = 0;
        while (*s) {
            writeCore((uint8_t)*s++);
            n++;
        }
        endTransm();
        return n;
    }

    // print PROGMEM string (F("..."))
    size_t print(const __FlashStringHelper* s) {
        if (!s) return 0;
        PGM_P p = (PGM_P)s;
        beginData();
        size_t n = 0;
        while (1) {
            char c = (char)pgm_read_byte(p++);
            if (!c) break;
            writeCore((uint8_t)c);
            n++;
        }
        endTransm();
        return n;
    }

    // =================================================================================
    // ===== Cursor / Text Mode ========================================================
    // =================================================================================

    // Positions cursor in character grid coordinates (multiplies y by 8 for pixel alignment)
    void setCursor(uint8_t x, uint8_t y) { setCursorXY(x, (uint8_t)(y << 3)); }

    // Sets cursor to exact pixel coordinates for aligned drawing (assumes y multiple of 8)
    void setCursorXY(uint8_t x, uint8_t y) {
        _x = x;
        _y = y;
        setWindowRaw(x, (uint8_t)(y >> 3), _maxX, (uint8_t)(y >> 3));
    }

    // Toggles text inversion mode for white-on-black or black-on-white rendering
    void invertText(bool inv) { _invState = inv; }

    // =================================================================================
    // ===== Seven Segment Drawing ======================================================
    // =================================================================================

    // =-=-=-=-=-=-=-=-= Low-level primitive drawing (buffer manipulation) =-=-=-=-=-=-=-=-=

    // Sets a single pixel in the local buffer (bitwise)
    void local_setPixel(unsigned char* buf, uint8_t curr_x, uint8_t curr_y) {
        // Keep shifts strictly 8-bit to avoid int-promotions turning this into 16-bit shift loops under -Os
        uint8_t page = curr_y;
        page >>= 3;
        uint8_t bit = (uint8_t)(curr_y & 7);

        // idx = curr_x * SEG_PAGES + page
        // Here SEG_PAGES is constexpr and equals 4 for 32px high seven-seg glyph buffer
        static_assert(SEG_PAGES == 4, "local_setPixel assumes SEG_PAGES == 4");
        uint8_t idx = (uint8_t)((curr_x << 2) + page);

        buf[idx] |= (uint8_t)(1u << bit);
    }

    // Renders horizontal segment in buffer (2px thick)
    void draw_horizontal_line(unsigned char* buf, uint8_t s_x, uint8_t s_y, uint8_t s_len) {
        for (uint8_t i = 0; i < s_len; i++) {
            uint8_t curr_x = (uint8_t)(s_x + i);
            local_setPixel(buf, curr_x, s_y);
            local_setPixel(buf, curr_x, (uint8_t)(s_y + 1));
        }
    }

    // Renders vertical segment in buffer (2px thick)
    void draw_vertical_line(unsigned char* buf, uint8_t s_x, uint8_t s_y, uint8_t s_len) {
        for (uint8_t i = 0; i < s_len; i++) {
            uint8_t curr_y = (uint8_t)(s_y + i);
            local_setPixel(buf, s_x, curr_y);
            local_setPixel(buf, (uint8_t)(s_x + 1), curr_y);
        }
    }

    // =-=-=-=-=-=-=-=-= Mid-level data transfer =-=-=-=-=-=-=-=-=

    // send rectangular window of data to display
    void partialUpdate(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const unsigned char* data) {
        uint8_t startPage = y >> 3;
        uint8_t endPage = (y + h - 1) >> 3;
        setWindowRaw(x, startPage, (uint8_t)(x + w - 1), endPage);

        uint8_t pages = endPage - startPage + 1;
        uint16_t len = (uint16_t)w * pages;

        beginData();
        if (!data) {
            while (len--) sendByte(0x00);
        } else {
            while (len--) sendByte(*data++);
        }
        endTransm();
    }

    // =-=-=-=-=-=-=-=-= Low-level helpers =-=-=-=-=-=-=-=-=

    // clear only the part of the buffer that will be used
    // prevents corrupting other parts of the static buffer
    inline void prepareLocalBuffer(uint8_t* buf, uint8_t width) {
        const uint16_t used_bytes = (uint16_t)width * SEG_PAGES;
        // compile-time check prevents buffer overflow from bad constants
        static_assert(SEVEN_SEG_DIGIT_WIDTH * SEG_PAGES <= SEG_BUF_SZ, "SEG_BUF_SZ is too small");
        memset(buf, 0, used_bytes);
    }

    // =-=-=-=-=-=-=-=-= Mid-level helper =-=-=-=-=-=-=-=-=

    // iterate over mask bits and draw all active segments
    // mask mapping keeps glyph definitions compact in flash
    inline void renderSegmentsToBuffer(uint8_t* buf, uint8_t mask) {
        const uint8_t* ptr = _oled_segs;
        for (uint8_t b = 0; b < 8; b++, ptr += 4) {
            if (mask & (1 << b)) {
                uint8_t s_x = pgm_read_byte(ptr);
                uint8_t s_y = pgm_read_byte(ptr + 1);
                uint8_t s_len = pgm_read_byte(ptr + 2);
                uint8_t isHoriz = pgm_read_byte(ptr + 3);

                if (isHoriz) draw_horizontal_line(buf, s_x, s_y, s_len);
                else        draw_vertical_line(buf, s_x, s_y, s_len);
            }
        }
    }

    // =-=-=-=-=-=-=-=-= High-level orchestrator  =-=-=-=-=-=-=-=-=

    // render one seven-segment glyph into local buffer and push to display
    // split into helpers for clarity without flash size penalty
    void drawDigit(char c, uint8_t px, uint8_t py) {
        uint8_t index;
        if (c == '.') index = 10;
        else if (c >= '0' && c <= '9') index = (uint8_t)(c - '0');
        else return;

        // dot glyph is narrower than a full digit
        const uint8_t digitW = (c == '.') ? SEVEN_SEG_DOT_WIDTH : SEVEN_SEG_DIGIT_WIDTH;
        static uint8_t localBuf[SEG_BUF_SZ];

        // Clear buffer
        prepareLocalBuffer(localBuf, digitW);

        // Fetch mask for character from global table
        uint8_t mask = pgm_read_byte(&_oled_symbolMasks[index]);

        // Render segments
        renderSegmentsToBuffer(localBuf, mask);

        // Push to panel
        partialUpdate(px, py, digitW, SEVEN_SEG_DIGIT_HEIGHT, localBuf);
    }

    // =================================================================================
    // ===== System Functions ===========================================================
    // =================================================================================

    // Fills entire display with specified byte value and resets cursor
    void fill(uint8_t data) {
        setWindowRaw(0, 0, _maxX, _maxRow);
        beginData();
        for (uint16_t i = 0; i < SCREEN_BYTES; i++) sendByte(data);
        endTransm();
        setCursorXY(_x, _y);
    }

    // Transmits single byte over I2C with batch handling to optimize transfers
    // NOTE: microWire has no internal buffer: Wire.write() sends immediately.
    // So we do not force periodic endTransm()/beginData() here.
    inline void sendByte(uint8_t data) {
        Wire.write(data);
    }

    // Sends single command byte to display controller
    void sendCommand(uint8_t cmd1) {
        beginOneCommand();
        sendByte(cmd1);
        endTransm();
    }

    // Sends command byte followed by parameter to display controller
    void sendCommand(uint8_t cmd1, uint8_t cmd2) {
        beginCommand();
        sendByte(cmd1);
        sendByte(cmd2);
        endTransm();
    }

    // Defines active window area for data writing on display
    void setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
        // keep safety clamps here (used by clear() etc.)
        uint8_t cx0 = OLED_CLAMP(x0, 0, _maxX);
        uint8_t cx1 = OLED_CLAMP(x1, 0, _maxX);
        uint8_t cy0 = OLED_CLAMP(y0, 0, _maxRow);
        uint8_t cy1 = OLED_CLAMP(y1, 0, _maxRow);

        // use raw to avoid repeating command code
        setWindowRaw(cx0, cy0, cx1, cy1);
    }

    // Starts I2C transmission in data mode
    void beginData() { beginTransmMode(OLED_DATA_MODE); }

    // Starts I2C transmission in command mode
    void beginCommand() { beginTransmMode(OLED_COMMAND_MODE); }

    // Starts I2C transmission for single command
    void beginOneCommand() { beginTransmMode(OLED_ONE_COMMAND_MODE); }

    // Ends I2C transmission
    // No delay needed - I2C is slow enough for display timing
    void endTransm() {
        Wire.endTransmission();
    }

    // retrieves font column byte using a lookup table for a compact font map
    // col range is guaranteed by write() loop, no bounds check needed
    uint8_t getFont(uint8_t font, uint8_t col) {
        if (font < 32 || font > 126) return 0;
        uint8_t index = pgm_read_byte(&(_charLookup[font - 32]));
        return (index == 0xFF) ? 0 : pgm_read_byte(&(_charMap_min[index][col]));
    }

    // =================================================================================
    // ===== Variables and Constants ====================================================
    // =================================================================================

    const uint8_t _address = 0x3C;
    static constexpr uint8_t _maxRow = 8 - 1;
    static constexpr uint8_t _maxX = OLED_WIDTH - 1;

    bool _invState = 0;
    uint8_t _x = 0, _y = 0;

private:
    // =================================================================================
    // ===== Low-level I2C ==============================================================
    // =================================================================================

    void beginTransmMode(uint8_t mode) {
        Wire.beginTransmission(_address);
        Wire.write(mode);
    }

    // Raw window setter to avoid clamp-heavy logic in hot paths
    // Safe only if caller guarantees: x0..x1 in 0..127, y0..y1 in 0..7 (pages)
    inline void setWindowRaw(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
        beginCommand();
        sendByte(OLED_COLUMNADDR);
        sendByte(x0);
        sendByte(x1);
        sendByte(OLED_PAGEADDR);
        sendByte(y0);
        sendByte(y1);
        endTransm();
    }

    // derived constants for seven-seg drawing and transfers
    static constexpr uint8_t  SEG_PAGES = (SEVEN_SEG_DIGIT_HEIGHT + 7) / 8;
    static constexpr uint8_t  SEG_MAX_W = SEVEN_SEG_DIGIT_WIDTH;
    static constexpr uint16_t SEG_BUF_SZ = (uint16_t)SEG_MAX_W * SEG_PAGES;

    static constexpr uint16_t SCREEN_BYTES = BUFSIZE_128x64; // full screen size
};


// =====================================================================================
// ===== Cleanup =======================================================================
// =====================================================================================

#undef OLED_SEG_A
#undef OLED_SEG_B
#undef OLED_SEG_C
#undef OLED_SEG_D
#undef OLED_SEG_E
#undef OLED_SEG_F
#undef OLED_SEG_G
#undef OLED_SEG_DOT

#endif // SSD1306_OLED_H
