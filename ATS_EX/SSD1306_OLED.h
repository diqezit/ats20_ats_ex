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
    By diqezit v1.7
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
    - Unused variables: _mode, _lastChar, _println, _getn, etc.
    - Simplified write: only scale=1, ASCII, no shifts or modes

*/

#ifndef GyverOLED_h
#define GyverOLED_h

#include <microWire.h>
#include "CustomFonts.h"

// ===== Constants =====
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
#define OLED_DISPLAY_ON 0xAF

#define OLED_COMMAND_MODE 0x00
#define OLED_ONE_COMMAND_MODE 0x80
#define OLED_DATA_MODE 0x40
#define OLED_ONE_DATA_MODE 0xC0

#define OLED_ADDRESSING_MODE 0x20
#define OLED_VERTICAL 0x01

#define OLED_NORMAL_V 0xC8
#define OLED_NORMAL_H 0xA1

#define OLED_CONTRAST 0x81
#define OLED_SETCOMPINS 0xDA
#define OLED_SETVCOMDETECT 0xDB
#define OLED_CLOCKDIV 0xD5
#define OLED_SETMULTIPLEX 0xA8
#define OLED_COLUMNADDR 0x21
#define OLED_PAGEADDR 0x22
#define OLED_CHARGEPUMP 0x8D

#define OLED_NORMALDISPLAY 0xA6

#define BUFSIZE_128x64 (128 * 64 / 8)

// Initialization list
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
};

// ===== Class Definition =====
template <int _TYPE, int _BUFF = OLED_NO_BUFFER>
class GyverOLED : public Print {
public:
    // Constructor
    GyverOLED(uint8_t address = 0x3C) : _address(address) {}

    // ===== Service Functions =====
    // Sets up I2C and sends initialization commands to configure display parameters
    void init(int __attribute__((unused)) sda = 0, int __attribute__((unused)) scl = 0) {
        Wire.begin();

        // Transfer Time (sec) = (Bytes * 9 bits/byte) / Clock Speed (Hz)
        // 1024 bytes at 35kHz = 0.26 sec per full screen refresh
        // reduce I2C speed for better stability on noisy power lines
        // if instability persists try less aggressive 50000L (50 kHz)
        Wire.setClock(35000L);

        beginCommand();
        for (uint8_t i = 0; i < sizeof(_oled_init); i++) {
            sendByte(pgm_read_byte(&_oled_init[i]));
        }
        endTransm();
        beginCommand();
        sendByte(OLED_SETCOMPINS);
        sendByte(OLED_HEIGHT_64);
        sendByte(OLED_SETMULTIPLEX);
        sendByte(OLED_64);
        endTransm();
        setCursorXY(0, 0);
    }

    // Fills entire screen with zero bytes to erase all content
    void clear() { fill(0); }

    // Erases specified rectangular area by writing zero bytes to defined window
    void clear(int x0, int y0, int x1, int y1) {
        x1++;
        y1++;
        y0 >>= 3;
        y1 = (y1 - 1) >> 3;
        y0 = constrain(y0, 0, _maxRow);
        y1 = constrain(y1, 0, _maxRow);
        x0 = constrain(x0, 0, _maxX);
        x1 = constrain(x1, 0, _maxX);
        setWindow(x0, y0, x1, y1);
        beginData();
        for (int x = x0; x < x1; x++)
            for (int y = y0; y < y1 + 1; y++)
                sendByte(0);
        endTransm();
        setCursorXY(_x, _y);
    }

    // Adjusts display brightness level from 0 (dim) to 255 (max)
    void setContrast(uint8_t value) { sendCommand(OLED_CONTRAST, value); }

    // Powers display on or off without changing content
    void setPower(bool mode) { sendCommand(mode ? OLED_DISPLAY_ON : OLED_DISPLAY_OFF); }

    // ===== Printing Functions =====
    // Outputs single ASCII character with inversion support
    virtual size_t write(uint8_t data) {
        int newX = _x + 6;                      // Assume font width 6
        if (newX > _maxX) return 1;             // Skip if beyond screen

        beginData();
        for (uint8_t col = 0; col < 6; col++) {
            uint8_t bits = getFont(data, col);
            if (_invState) bits = ~bits;
            sendByte(bits);                     // Direct output (no shift)
            _x += 1;
        }
        endTransm();
        return 1;
    }

    // Positions cursor in character grid coordinates (multiplies y by 8 for pixel alignment)
    void setCursor(int x, int y) { setCursorXY(x, y << 3); }

    // Sets cursor to exact pixel coordinates for aligned drawing (assumes y multiple of 8)
    void setCursorXY(int x, int y) {
        _x = x;
        _y = y;
        setWindow(x, (y >> 3), _maxX, (y >> 3));  // Simplified window for single row (scale=1)
    }

    // Limits font scaling to 1 (higher values constrained) and updates cursor
    void setScale(uint8_t scale) {
        scale = constrain(scale, 1, 1);
        _scaleX = scale;
        _scaleY = scale * 8;
        setCursorXY(_x, _y);
    }

    // Toggles text inversion mode for white-on-black or black-on-white rendering
    void invertText(bool inv) { _invState = inv; }

    // ===== Seven Segment Drawing =====
    // Bitmasks for digits 0-9 and dot
    static const uint8_t symbolMasks[11] PROGMEM;

    // Segment definitions: {x, y, len, isHoriz}
    struct SegDef {
        uint8_t x;
        uint8_t y;
        uint8_t len;
        uint8_t isHoriz;
    };

    static const SegDef segs[8] PROGMEM;

    // =-=-=-=-=-=-=-=-= Low-level primitive drawing (buffer manipulation) =-=-=-=-=-=-=-=-=

    // Sets a single pixel in the local buffer (bitwise)
    void local_setPixel(unsigned char* buf, uint8_t curr_x, uint8_t curr_y, uint8_t pages) {
        uint8_t page = curr_y >> 3;
        uint8_t bit = curr_y & 7;
        int idx = curr_x * pages + page;
        buf[idx] |= (1 << bit);
    }

    // Renders horizontal segment in buffer (2px thick)
    void draw_horizontal_line(unsigned char* buf, uint8_t s_x, uint8_t s_y, uint8_t s_len, uint8_t pages) {
        for (uint8_t i = 0; i < s_len; i++) {
            uint8_t curr_x = s_x + i;
            local_setPixel(buf, curr_x, s_y, pages);
            local_setPixel(buf, curr_x, s_y + 1, pages);
        }
    }

    // Renders vertical segment in buffer (2px thick)
    void draw_vertical_line(unsigned char* buf, uint8_t s_x, uint8_t s_y, uint8_t s_len, uint8_t pages) {
        for (uint8_t i = 0; i < s_len; i++) {
            local_setPixel(buf, s_x, s_y + i, pages);
            local_setPixel(buf, s_x + 1, s_y + i, pages);
        }
    }

    // =-=-=-=-=-=-=-=-= Mid-level data transfer =-=-=-=-=-=-=-=-=

    // send rectangular window of data to display
    // NULL data pointer clears the window to save flash
    void partialUpdate(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const unsigned char* data) {
        beginCommand();
        sendByte(OLED_COLUMNADDR);
        sendByte(x);
        sendByte(x + w - 1);
        sendByte(OLED_PAGEADDR);
        sendByte(y / 8);
        sendByte((y + h - 1) / 8);
        endTransm();

        // page count must be exact for transfer
        uint8_t pages = ((y + h - 1) / 8) - (y / 8) + 1;
        uint16_t len = (uint16_t)w * pages;

        beginData();
        for (uint16_t i = 0; i < len; i++) {
            sendByte(data != NULL ? data[i] : 0x00);
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
        for (uint16_t i = 0; i < used_bytes; i++) {
            buf[i] = 0;
        }
    }

    // read segment definition from PROGMEM
    // pass params by pointer to avoid stack overhead of returning a struct
    inline void loadSegmentDef(uint8_t segIndex,
        uint8_t* x, uint8_t* y,
        uint8_t* len, uint8_t* isHoriz) {
        const SegDef* seg_ptr = &segs[segIndex];
        *x = pgm_read_byte((const uint8_t*)seg_ptr + offsetof(SegDef, x));
        *y = pgm_read_byte((const uint8_t*)seg_ptr + offsetof(SegDef, y));
        *len = pgm_read_byte((const uint8_t*)seg_ptr + offsetof(SegDef, len));
        *isHoriz = pgm_read_byte((const uint8_t*)seg_ptr + offsetof(SegDef, isHoriz));
    }

    // draw a single segment into the buffer
    // uses 2px thick lines for better visibility
    inline void drawSegment(uint8_t* buf,
        uint8_t x, uint8_t y,
        uint8_t len, uint8_t isHoriz) {
        if (isHoriz) {
            draw_horizontal_line(buf, x, y, len, SEG_PAGES);
        } else {
            draw_vertical_line(buf, x, y, len, SEG_PAGES);
        }
    }

    // =-=-=-=-=-=-=-=-= Mid-level helper =-=-=-=-=-=-=-=-=

    // iterate over mask bits and draw all active segments
    // mask mapping keeps glyph definitions compact in flash
    inline void renderSegmentsToBuffer(uint8_t* buf, uint8_t mask) {
        for (uint8_t b = 0; b < 8; b++) {
            if (mask & (1 << b)) {
                uint8_t s_x, s_y, s_len, isHoriz;
                loadSegmentDef(b, &s_x, &s_y, &s_len, &isHoriz);
                drawSegment(buf, s_x, s_y, s_len, isHoriz);
            }
        }
    }

    // =-=-=-=-=-=-=-=-= High-level orchestrator  =-=-=-=-=-=-=-=-=

    // render one seven-segment glyph into local buffer and push to display
    // split into helpers for clarity without flash size penalty
    void drawDigit(char c, int px, int py) {
        if ((c < '0' || c > '9') && (c != '.')) return;

        // dot glyph is narrower than a full digit
        const uint8_t digitW = (c == '.') ? SEVEN_SEG_DOT_WIDTH : SEVEN_SEG_DIGIT_WIDTH;
        static uint8_t localBuf[SEG_BUF_SZ];

        // Clear buffer
        prepareLocalBuffer(localBuf, digitW);

        // Fetch mask for character
        uint8_t index = (c == '.') ? 10 : (c - '0');
        uint8_t mask = pgm_read_byte(&symbolMasks[index]);

        // Render segments
        renderSegmentsToBuffer(localBuf, mask);

        // Push to panel
        partialUpdate(px, py, digitW, SEVEN_SEG_DIGIT_HEIGHT, localBuf);
    }

    // ===== System Functions =====

    // Fills entire display with specified byte value and resets cursor
    void fill(uint8_t data) {
        setWindow(0, 0, _maxX, _maxRow);
        beginData();
        for (uint16_t i = 0; i < SCREEN_BYTES; i++) sendByte(data);
        endTransm();
        setCursorXY(_x, _y);
    }

    // Transmits single byte over I2C with batch handling to optimize transfers
    void sendByte(uint8_t data) {
        Wire.write(data);
        _writes++;
        if (_writes >= I2C_BATCH) {
            endTransm();
            beginData();
        }
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
    void setWindow(int x0, int y0, int x1, int y1) {
        beginCommand();
        sendByte(OLED_COLUMNADDR);
        sendByte(constrain(x0, 0, _maxX));
        sendByte(constrain(x1, 0, _maxX));
        sendByte(OLED_PAGEADDR);
        sendByte(constrain(y0, 0, _maxRow));
        sendByte(constrain(y1, 0, _maxRow));
        endTransm();
    }

    // Starts I2C transmission in data mode
    void beginData() {
        Wire.beginTransmission(_address);
        Wire.write(OLED_DATA_MODE);
    }

    // Starts I2C transmission in command mode
    void beginCommand() {
        Wire.beginTransmission(_address);
        Wire.write(OLED_COMMAND_MODE);
    }

    // Starts I2C transmission for single command
    void beginOneCommand() {
        Wire.beginTransmission(_address);
        Wire.write(OLED_ONE_COMMAND_MODE);
    }

    // Ends I2C transmission and resets write counter with short delay
    void endTransm() {
        Wire.endTransmission();
        _writes = 0;
        delayMicroseconds(2);
    }

    // retrieves font column byte using a lookup table for a compact font map
    uint8_t getFont(uint8_t font, uint8_t row) {
        if (font < 32 || font > 126) return 0;

        // find real index in table
        uint8_t index = pgm_read_byte(&(_charLookup[font - 32]));

        // if the index is 0xFF - char is not in our font map
        if (index == 0xFF) return 0;

        // font data from the compact map using real index
        return pgm_read_byte(&(_charMap_min[index][row]));
    }

    // ===== Variables and Constants =====
    const uint8_t _address = 0x3C;
    const uint8_t _maxRow = 8 - 1;
    const uint8_t _maxY = 64 - 1;
    const uint8_t _maxX = OLED_WIDTH - 1;

    bool _invState = 0;
    uint8_t _scaleX = 1, _scaleY = 8;
    int _x = 0, _y = 0;
    uint8_t _writes = 0;

private:
    // derived constants for seven-seg drawing and transfers
    static constexpr uint8_t SEG_PAGES = (SEVEN_SEG_DIGIT_HEIGHT + 7) / 8;
    static constexpr uint8_t SEG_MAX_W = SEVEN_SEG_DIGIT_WIDTH;
    static constexpr uint16_t SEG_BUF_SZ = SEG_MAX_W * SEG_PAGES;

    static constexpr uint8_t I2C_BATCH = 16;
    static constexpr uint16_t SCREEN_BYTES = BUFSIZE_128x64; // full screen size
};

// ===== Static Member Definitions (outside class for linkage) =====
template <int _TYPE, int _BUFF>
const uint8_t GyverOLED<_TYPE, _BUFF>::symbolMasks[11] PROGMEM = {
    0b00111111, // 0: a,b,c,d,e,f
    0b00000110, // 1: b,c
    0b01011011, // 2: a,b,d,e,g
    0b01001111, // 3: a,b,c,d,g
    0b01100110, // 4: b,c,f,g
    0b01101101, // 5: a,c,d,f,g
    0b01111101, // 6: a,c,d,e,f,g
    0b00000111, // 7: a,b,c
    0b01111111, // 8: a,b,c,d,e,f,g
    0b01101111, // 9: a,b,c,d,f,g
    0b10000000  // . (uses bit 7)
};

// "Blueprints" for the seven-segment digits.
// Each entry defines a segment's position and dimensions on a canvas whose size is
// defined by SEVEN_SEG_DIGIT_WIDTH and SEVEN_SEG_DIGIT_HEIGHT.
// Format: {X-coordinate, Y-coordinate, Length, IsHorizontal (1 or 0)}
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

template <int _TYPE, int _BUFF>
const typename GyverOLED<_TYPE, _BUFF>::SegDef GyverOLED<_TYPE, _BUFF>::segs[8] PROGMEM = {
    {  1,  0, 12, 1 },  // A  top
    { 12,  1, 15, 0 },  // B  upper right
    { 12, 16, 15, 0 },  // C  lower right
    {  1, 30, 12, 1 },  // D  bottom
    {  0, 16, 15, 0 },  // E  lower left
    {  0,  1, 15, 0 },  // F  upper left
    {  1, 15, 12, 1 },  // G  middle
    {  1, 30,  2, 1 }   // Dot
};

#endif
