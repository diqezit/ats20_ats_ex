#pragma once

const DCfont* LastFont = DEFAULT_FONT;

void oledSetFont(const DCfont* font)
{
    if (font && LastFont != font)
    {
        LastFont = font;
        oled.setFont(font);
    }
}

template <typename T>
void oledPrint(T value, int offX = -1, int offY = -1, const DCfont* font = LastFont, bool invert = false)
{
    oledSetFont(font);
    if (invert)
        oled.invertOutput(invert);
    if (offX >= 0 && offY >= 0)
        oled.setCursor(offX, offY);

    oled.print(value);

    if (invert)
        oled.invertOutput(false);
}

//Faster alternative for convertToChar
// not used in the code
/* void utoa(char* out, uint16_t num)
{
    char* p = out;
    if (num == 0)
        *p++ = '0';
    else
    {
        for (uint16_t base = 10000; base > 0; base /= 10)
        {
            if (num >= base)
            {
                *p++ = '0' + num / base;
                num %= base;
            }
            else if (p != out)
                *p++ = '0';
        }
    }
    *p = '\0';
}
*/

//Better than sprintf which has overwhelmingly large overhead, it helps to reduce binary size
void convertToChar(char* str, uint16_t value, uint8_t len, uint8_t dot = 0, char separator = '.', char space = ' ')
{
    uint8_t current_pos = len + (dot > 0);
    str[current_pos] = '\0';

    for (uint8_t i = 0; i < len; ++i)
    {
        if (dot > 0 && i == (len - dot))
            str[--current_pos] = separator;
        
        str[--current_pos] = (value % 10) + '0';
        value /= 10;
    }

    uint8_t integer_part_len = (dot > 0) ? dot : len;
    for (uint8_t i = 0; i < integer_part_len - 1 && str[i] == '0'; ++i)
    {
        str[i] = space;
    }
}

//Measure integer digit length
uint8_t ilen(uint16_t n) {
    if (n < 100) return 1 + (n >= 10);
    if (n < 10000) return 3 + (n >= 1000);
    return 5;
}

//Split KHz frequency + BFO to KHz and .00 tail
void splitFreq(uint16_t& khz, uint16_t& tail)
{
    // replaced the original 32-bit math to save a ton of flash space
    // old way ( (freq * 1000) + bfo ) was linking huge lib

    khz = g_currentFrequency;
    int16_t bfo_temp = g_currentBFO;

    // manually handle the "borrow" if BFO is negative
    // way cheaper than converting everything to 32-bit
    while (bfo_temp < 0) {
        bfo_temp += 1000;
        khz--;
    }

    // bfo_temp is just the positive part, 0-999
    // final division is a cheap 16-bit one
    tail = bfo_temp / 10;
}

uint8_t strlen8(const char* s)
{
    const char* start = s;
    while (*s)
        s++;
    return s - start;
}

// division via subtraction loop to save flash space
static inline uint8_t sw_div(uint16_t& dividend, const uint16_t divisor) {
    uint8_t quotient = 0;
    while (dividend >= divisor) {
        quotient++;
        dividend -= divisor;
    }
    return quotient;
}

// save more flash image size
static void doSwitchLogic(int8_t& param, int8_t low, int8_t high, int8_t step) {
    param += step;
    if (param < low)
        param = high;
    else if (param > high)
        param = low;
}

static void toggleSetting(uint8_t settingIndex) {
    g_Settings[settingIndex].param = 1 - g_Settings[settingIndex].param;
}

