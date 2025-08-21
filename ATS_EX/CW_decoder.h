#pragma once

// ======================================================================
// CW_decoder.h - CW decoder using fixed-point Goertzel and text output
// Works from speaker output via 2.2 uF to A6 without external DC bias
// Uses INTERNAL 1.1V, narrowband detector at CWPitch (500/600/700/800 Hz)
// 
// How to use:
// - Switch radio to CW, then hold MODE to open the CW DECODER screen
// - Set CWPitch in Settings to match audible tone (600..700 Hz recommended)
// 
// Wiring:
// - From speaker: series 2.2 uF -> A6; common GND with MCU
// ======================================================================

#include "Defines.h"

#if ENABLE_CW_DECODER

#include <Arduino.h>
#include <avr/pgmspace.h>
#include "UI.h"        // uses: clearBox, drawInverted, showStatus, UI_* constants, oled

// ======================================================================
// ===== CONFIGURATION (LOCAL TO THIS FILE) =============================
// ======================================================================

#ifndef CW_ADC_PIN
#define CW_ADC_PIN A6                 // Analog input for audio (Nano: ADC6 is analog-only)
#endif

#ifndef CW_USE_INTERNAL_REF
#define CW_USE_INTERNAL_REF 1         // INTERNAL 1.1V; do not connect external AREF
#endif

// Goertzel block size (latency vs selectivity):
// - 48 = default (low latency, enough selectivity)
// - 32 faster but noisier, 64 slower but cleaner
static constexpr uint8_t GOERTZ_N = 48;

// Hysteresis thresholds on envelope (0..255):
// - Increase for cleaner (fewer false), decrease for more sensitive
#ifndef CW_THR_HI_ADD
#define CW_THR_HI_ADD 6
#endif
#ifndef CW_THR_LO_ADD
#define CW_THR_LO_ADD 3
#endif

// Power compression for |Q1|+|Q2| -> 0..255:
// - Lower = more sensitive; higher = quieter
#ifndef CW_PWR_SHIFT
#define CW_PWR_SHIFT 10
#endif

// Output area rows (consistent with UI grid)
static constexpr uint8_t CW_VIEW_ROW_FIRST = 2;
static constexpr uint8_t CW_VIEW_ROW_LAST = 7;

// Cursor for CW viewport
static uint8_t g_cwRow = CW_VIEW_ROW_FIRST;
static uint8_t g_cwCol = 0;


// ======================================================================
// ===== GOERTZEL BACKEND (Q14 FIXED-POINT) =============================
// ======================================================================
//
// Precomputed 2*cos(2*pi*k/N) in Q14 for N=48, Fs≈9 kHz,
// bins nearest to 500/600/700/800 Hz:
// - 500 Hz -> k=3 -> 30274 (Q14)
// - 600 Hz -> k=3 -> 30274 (Q14)
// - 700 Hz -> k=4 -> 28378 (Q14)
// - 800 Hz -> k=4 -> 28378 (Q14)
// CWPitch index {0..3} maps to these bins.
//

static const int16_t g_goertzelCoeffQ14[4] PROGMEM = { 30274, 30274, 28378, 28378 };

// Detector state
static uint8_t  cw_inited, cw_env8, cw_avg8, cw_dot = 8;
static uint16_t cw_run = 0;
static uint8_t  cw_mark = 0;

static int16_t  g_coeff_q14 = 30274; // default k=3
static int32_t  g_Q1, g_Q2;          // Goertzel state

// Loads Q14 coeff from LUT by CWPitch setting
static inline void goertzelLoadCoeff() {
    uint8_t idx = g_Settings[CWPitch].param;               // 0..3 -> 500/600/700/800
    g_coeff_q14 = (int16_t)pgm_read_word(&g_goertzelCoeffQ14[idx]);
}

// Returns 0..255 power proxy using |Q1|+|Q2| (no sqrt)
static inline uint8_t goertzelPower8() {
    g_Q1 = 0; g_Q2 = 0;
    for (uint8_t i = 0; i < GOERTZ_N; i++) {
        int16_t s = (int16_t)analogRead(CW_ADC_PIN);       // 0..1023
        int32_t Q0 = ((int32_t)g_coeff_q14 * g_Q1) >> 14;  // Q14 multiply
        Q0 -= g_Q2;
        Q0 += s;
        g_Q2 = g_Q1;
        g_Q1 = Q0;
    }
    uint32_t p = (uint32_t)((g_Q1 >= 0 ? g_Q1 : -g_Q1) + (g_Q2 >= 0 ? g_Q2 : -g_Q2));
    p >>= CW_PWR_SHIFT;                                    // compress to 8-bit
    if (p > 255) p = 255;
    return (uint8_t)p;
}

// ADC/backend init (called on view enter)
static inline void CWDecoder_begin() {
#if CW_USE_INTERNAL_REF
    analogReference(INTERNAL);
#endif
    analogRead(CW_ADC_PIN);                                // warm up
    analogRead(CW_ADC_PIN);                                // settle
    goertzelLoadCoeff();
    cw_env8 = 0;
    cw_avg8 = 0;
    cw_inited = 1;
}

// One block sample and envelope update
static inline void cwSampleAndUpdate() {
    uint8_t mag8 = goertzelPower8();
    cw_env8 += (int8_t)((int16_t)mag8 - cw_env8) >> 1;     // fast envelope
    cw_avg8 += (int8_t)((int16_t)mag8 - cw_avg8) >> 5;     // noise floor
}

// Hysteresis comparator (returns new MARK state 0/1)
static inline uint8_t cwHysteresis() {
    uint8_t thr_hi = (uint8_t)(cw_avg8 + CW_THR_HI_ADD);
    uint8_t thr_lo = (uint8_t)(cw_avg8 + CW_THR_LO_ADD);
    return cw_mark ? (cw_env8 > thr_lo) : (cw_env8 > thr_hi);
}

// Classify MARK->SPACE (dot or dash) and adapt dot length
static inline char cwClassifyMarkToSpace() {
    char     out = (cw_run < (uint16_t)(cw_dot << 1)) ? '.' : '-';
    uint16_t d = cw_run;
    cw_dot += (int8_t)((int16_t)d - cw_dot) >> 3;          // EWMA (auto-speed)
    return out;
}

// Classify SPACE->MARK (inter-letter or inter-word) or none
static inline char cwClassifySpaceToMark() {
    if (cw_run > (uint16_t)(cw_dot * 6))       return ' '; // word gap
    else if (cw_run > (uint16_t)(cw_dot << 1)) return '|'; // letter gap
    return 0;
}

// Poll detector and return '.', '-', '|', ' ' or 0 (no event)
static inline char CWDecoder_poll() {
    if (!cw_inited) return 0;

    cwSampleAndUpdate();

    uint8_t m2 = cwHysteresis();
    cw_run++;

    if (m2 != cw_mark) {
        char pend = cw_mark ? cwClassifyMarkToSpace()
            : cwClassifySpaceToMark();
        cw_run = 0;
        cw_mark = m2;
        if (pend) return pend;
    }
    return 0;
}


// ======================================================================
// ===== MORSE DECODER (A..Z, 0..9) =====================================
// ======================================================================
//
// Encoding: dot=0, dash=1; pack left->right via shift-left.
//

static const uint8_t cw_lut[][3] PROGMEM = {
    {2,  1, 'A'}, {4,  8, 'B'}, {4, 10, 'C'}, {3,  4, 'D'}, {1,  0, 'E'},
    {4,  2, 'F'}, {3,  6, 'G'}, {4,  0, 'H'}, {2,  0, 'I'}, {4,  7, 'J'},
    {3,  5, 'K'}, {4,  4, 'L'}, {2,  3, 'M'}, {2,  2, 'N'}, {3,  7, 'O'},
    {4,  6, 'P'}, {4, 13, 'Q'}, {3,  2, 'R'}, {3,  0, 'S'}, {1,  1, 'T'},
    {3,  1, 'U'}, {4,  1, 'V'}, {3,  3, 'W'}, {4,  9, 'X'}, {4, 11, 'Y'},
    {4, 12, 'Z'},
    {5, 15, '1'}, {5,  7, '2'}, {5,  3, '3'}, {5,  1, '4'}, {5,  0, '5'},
    {5, 16, '6'}, {5, 24, '7'}, {5, 28, '8'}, {5, 30, '9'}, {5, 31, '0'},
};
static constexpr uint8_t CW_LUT_COUNT = sizeof(cw_lut) / sizeof(cw_lut[0]);

static uint8_t s_acc_len = 0;
static uint8_t s_acc_bits = 0;

// Reset current symbol accumulator
static inline void cwAccReset() {
    s_acc_len = 0;
    s_acc_bits = 0;
}

// Add dot/dash to accumulator
static inline void cwAccAdd(bool dash) {
    if (s_acc_len < 5) {
        s_acc_bits = (uint8_t)((s_acc_bits << 1) | (dash ? 1 : 0));
        s_acc_len++;
    }
}

// Decode accumulator to ASCII via LUT
static inline char cwDecodeFromAcc() {
    if (!s_acc_len) return 0;
    for (uint8_t i = 0; i < CW_LUT_COUNT; i++) {
        if (pgm_read_byte(&cw_lut[i][0]) != s_acc_len) continue;
        if (pgm_read_byte(&cw_lut[i][1]) == s_acc_bits)
            return (char)pgm_read_byte(&cw_lut[i][2]);
    }
    return 0;
}


// ======================================================================
// ===== VIEW HELPERS ===================================================
// ======================================================================

// Clear entire CW viewport (rows 2..7)
static inline void cwViewClearViewport() {
    for (uint8_t r = CW_VIEW_ROW_FIRST; r <= CW_VIEW_ROW_LAST; ++r)
        clearBox(0, r * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
}

// Draw header line using UI draw helper
static inline void cwViewDrawHeader() {
    drawInverted(0, 0, F("    CW DECODER    "), true);
}

// Wrap text cursor and clear next line using UI clearBox
static inline void cwViewWrapIfNeeded() {
    if (g_cwCol >= UI_COLS) {
        g_cwCol = 0;
        g_cwRow = (g_cwRow < CW_VIEW_ROW_LAST) ? (uint8_t)(g_cwRow + 1) : CW_VIEW_ROW_FIRST;
        clearBox(0, g_cwRow * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
    }
}

// Print one char with wrapping
static inline void cwViewPut(char c) {
    if (c == '\r') return;
    if (c == '\n') c = ' ';
    cwViewWrapIfNeeded();
    oled.setCursor(g_cwCol * UI_CHAR_W, g_cwRow);
    oled.print(c);
    g_cwCol++;
}


// ======================================================================
// ===== EVENT PROCESSING ===============================================
// ======================================================================

// Flush current symbol and print decoded char
static inline void cwFlushSymbol() {
    char out = cwDecodeFromAcc();
    cwAccReset();
    if (out) cwViewPut(out);
}

// Handle detector events
static inline void cwProcessEvent(char ev) {
    switch (ev) {
    case '.': cwAccAdd(false); break;
    case '-': cwAccAdd(true);  break;
    case '|': cwFlushSymbol();  break;                  // letter gap
    case ' ': cwFlushSymbol(); cwViewPut(' '); break;   // word gap
    default:  break;
    }
}


// ======================================================================
// ===== PUBLIC ENTRY POINTS ============================================
// ======================================================================

// Called when entering CW decoder view
static void cwViewEnter() {
    oled.clear();
    cwViewDrawHeader();
    cwViewClearViewport();
    g_cwRow = CW_VIEW_ROW_FIRST;
    g_cwCol = 0;
    cwAccReset();
    CWDecoder_begin();
}

// Called when leaving CW decoder view
static void cwViewExit() {
    oled.clear();
    showStatus(true); // reuse UI redraw
}

// Called from loop while CW view is active
static inline void cwViewTask() {
    char ev;
    while ((ev = CWDecoder_poll())) {
        cwProcessEvent(ev);
    }
}

#endif  // ENABLE_CW_DECODER
