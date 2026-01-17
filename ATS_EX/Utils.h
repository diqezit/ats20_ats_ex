#pragma once

// =====================================================================================
// String & Number Conversion Utilities
// =====================================================================================

// Converts an integer to a character string with options for padding and decimal points.
// A lightweight alternative to sprintf to save flash space.
static inline void convertToChar(
    char* str,
    uint16_t value,
    uint8_t len,
    uint8_t dot = 0,
    char separator = '.',
    char space = ' '
) {
    // len must be >= 1
    if (len == 0) {
        str[0] = '\0';
        return;
    }

    // dot must be strictly less than len
    if (dot >= len) dot = 0;

    uint8_t current_pos = len + (dot > 0);
    str[current_pos] = '\0';

    for (uint8_t i = 0; i < len; ++i) {
        if (dot > 0 && i == (len - dot)) {
            str[--current_pos] = separator;
        }
        str[--current_pos] = (value % 10) + '0';
        value /= 10;
    }

    uint8_t integer_part_len = (dot > 0) ? dot : len;
    for (uint8_t i = 0; i < integer_part_len - 1 && str[i] == '0'; ++i) {
        str[i] = space;
    }
}

// Measures integer digit length for display formatting
static inline uint8_t ilen(uint16_t n) {
    if (n < 100) return 1 + (n >= 10);
    if (n < 10000) return 3 + (n >= 1000);
    return 5;
}


// =====================================================================================
// Core State, Tuning & EEPROM Utilities
// =====================================================================================

// Checks if the current mode is LSB, USB, or CW
static inline bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// Gets the current mode context for loading mode-specific settings
static inline ModeContext getModeContext() {
    switch (g_currentMode) {
    case AM:  return MODE_CONTEXT_AM;
    case LSB: return MODE_CONTEXT_LSB;
    case USB: return MODE_CONTEXT_USB;
    case CW:  return (g_lastCWMode == USB)
        ? MODE_CONTEXT_USB : MODE_CONTEXT_LSB;  // CW inherits from LSB/USB
    default:  return MODE_CONTEXT_AM;           // FM not used for AVC
    }
}

// Marks receiver state as dirty to trigger an EEPROM save on idle
static inline void markStateAsDirty() {
    g_lastUserActivityTime = millis() / 1000;
    g_stateIsDirty = true;
}

// Splits the main frequency and BFO into a displayable format (e.g., 7050.50)
static inline void splitFreq(uint16_t& khz, uint16_t& tail) {
    int16_t b = g_currentBFO;
    int16_t d = b / 1000;
    int16_t r = b % 1000;

    if (r < 0) {
        r += 1000;
        d -= 1;
    }

    khz = g_currentFrequency + d;
    tail = r / 10;
}

// Aligns frequency to the current step grid. Keeps tuning predictable
static inline void snapToNewStep(uint16_t* freq, bool isUp) {
    uint8_t step_index = SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB;
    uint16_t step_khz = g_tabStep[step_index] / 1000;
    if (step_khz == 0) return;

    uint16_t remainder = *freq % step_khz;
    if (remainder != 0) {
        if (isUp) {
            // Snap to the next grid step
            *freq += step_khz - remainder;
        } else {
            // Snap to the current grid step
            *freq -= remainder;
        }
    }
}

// Handles parameter switching with wrap-around logic. Used in settings menu
static void doSwitchLogic(int8_t& param, int8_t low, int8_t high, int8_t step) {
    param += step;
    if (param < low) {
        param = high;
    } else if (param > high) {
        param = low;
    }
}

// Helper to clamp an index to a valid range, resetting to 0 if out of bounds
// 'strict' uses a > comparison, otherwise >= is used
static inline void clamp_index(int8_t& var, const int8_t max_val, bool strict = false) {
    if (strict ? (var > max_val) : (var >= max_val)) var = 0;
}

// generic helper to update a value and call a function if it has changed
// avoids duplicating the "if (new_value != old_value)" pattern
template<typename T>
static inline __attribute__((always_inline))
void updateIfChanged(T& old_value, T new_value, void (*update_fn)()) {
    if (old_value != new_value) {
        old_value = new_value;
        update_fn();
    }
}

static inline uint16_t adcReadAx(uint8_t analogPin) {
    uint8_t ch = (uint8_t)(analogPin - A0);      // A0..A7 -> 0..7
    ADMUX = (1 << REFS0) | (ch & 0x07);         // опора AVcc
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // /128
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {}
    return ADC;
}

#if DEBUG_MODE

// =====================================================================================
// Lighweight Debugger (replaces SerialPrint to save flash space)
// =====================================================================================

// UART (baud 9600)
void initDebugUART() {
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0A = 0;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Prints PROGMEM strings for debug. Usage: debugPrint_P(PSTR("Hello"));
void debugPrint_P(const char* str) {
    char c;
    while ((c = pgm_read_byte(str++))) {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = c;
    }
}

// Fast single-char print (RAM)
static inline void debugPutChar(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

// CRLF
static inline void debugPrintCRLF() {
    debugPutChar('\r'); debugPutChar('\n');
}

// Prints a character buffer
void debugPrintBuf(const char* buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = buf[i];
    }
}

// Prints a number to UART
void debugPrintNum(int16_t num) {
    if (num == 0) {
        debugPrint_P(PSTR("0"));
        return;
    }
    bool negative = (num < 0);
    if (negative) {
        while (!(UCSR0A & (1 << UDRE0))); UDR0 = '-';
        num = -num;
    }
    char buf[6];
    uint8_t i = 0;
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i--) {
        while (!(UCSR0A & (1 << UDRE0))); UDR0 = buf[i];
    }
}

// Prints a number in hexadecimal format
void debugPrintHex(uint16_t num) {
    debugPrint_P(PSTR("0x"));
    for (int8_t i = 12; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        char ch = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        while (!(UCSR0A & (1 << UDRE0))); UDR0 = ch;
    }
    debugPrint_P(PSTR("\n"));
}

#endif
