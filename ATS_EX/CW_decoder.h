#pragma once

// ======================================================================
// CW_decoder.h - CW decoder using fixed-point Goertzel and text output
// Works from speaker output via 2.2 uF to A6 with external mid-bias
// Uses INTERNAL 1.1V, narrowband detector at CWPitch (500/600/700/800 Hz)
//
// How to use:
// - Switch radio to CW, then hold MODE to open the CW DECODER screen
// - Set CWPitch in Settings to match audible tone (600..700 Hz recommended)
//
// Wiring:
// - From speaker: series 2.2 uF -> A6 node; common GND with MCU
// - External bias divider at A6 node (for INTERNAL 1.1V):
//   Rtop = 390 kΩ from +3.48 V to A6 node  (470 kΩ also OK, gives ~0.5 V bias)
//   Rbot = 75 kΩ  from A6 node to GND
// ======================================================================
// For test use 600Hz 30WPM - A A A A A / T T T T T / E E E E E / N A N A N A / PARIS PARIS PARIS 
// ======================================================================
#include "Defines.h"

#if ENABLE_CW_DECODER

#include <Arduino.h>
#include <avr/pgmspace.h>
#include "UI.h"
#include "Utils.h"


// ======================================================================
// ===== CONFIGURATION ==================================================
// ======================================================================

#ifndef CW_ADC_PIN
#define CW_ADC_PIN A6
#endif

static constexpr uint8_t GOERTZ_N = 128;

#ifndef CW_PWR_SHIFT
#define CW_PWR_SHIFT 3
#endif

#ifndef DEBUG_CW
#define DEBUG_CW 1
#endif

// Always enable Goertzel bin adaptation for 500/600/700/800 Hz support
#ifdef ADAPTIVE_K
#undef ADAPTIVE_K
#endif
#define ADAPTIVE_K 1

// Optional header line on OLED (saves flash when 0)
#ifndef SHOW_CW_HEADER
#define SHOW_CW_HEADER 0
#endif

// Forward declarations
static inline char CWDecoder_poll();
static inline void CWDecoder_begin();
static inline void cwViewPut(char c);
static void cwFlushSymbol();

// ======================================================================
// ===== DISPLAY HELPERS ================================================
// ======================================================================

#define UI_ROW_Y(r) (r)
#define ABS32(v)    ((v) < 0 ? -(v) : (v))

static constexpr uint8_t CW_VIEW_ROW_FIRST = 2;
static constexpr uint8_t CW_VIEW_ROW_LAST = 7;

static uint8_t g_cwRow = CW_VIEW_ROW_FIRST;
static uint8_t g_cwCol = 0;

static inline void UI_ClearRow(uint8_t row) {
    clearBox(0, row * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
}

static inline void UI_SetCursorCR(uint8_t col, uint8_t row) {
    oled.setCursor((uint8_t)(col * UI_CHAR_W), row);
}

// ======================================================================
// ===== ADC REF + DC COUPLING ==========================================
// ======================================================================

struct AdcRefConfig {
    static constexpr uint8_t SETTLE_MS = 5;
    static constexpr uint8_t DC_SLOW_SHIFT = 8;
    static constexpr uint8_t DC_SEED_ITER = 16;
    static constexpr uint8_t DC_SEED_SHIFT = 3;
};

static int16_t g_dc = 512;

static inline void adcSetVref() {
    analogReference(INTERNAL);
    delay(AdcRefConfig::SETTLE_MS);
    (void)analogRead(CW_ADC_PIN);
}

static inline int16_t acCouple(uint16_t x) {
    g_dc += ((int16_t)x - g_dc) >> AdcRefConfig::DC_SLOW_SHIFT;
    return (int16_t)x - g_dc;
}

static inline void adcSeedDC() {
    g_dc = analogRead(CW_ADC_PIN);
    for (uint8_t i = 0; i < AdcRefConfig::DC_SEED_ITER; i++) {
        uint16_t v = analogRead(CW_ADC_PIN);
        g_dc += ((int16_t)v - g_dc) >> AdcRefConfig::DC_SEED_SHIFT;
    }
}

// ======================================================================
// ===== GOERTZEL BACKEND (Q14 FIXED-POINT) =============================
// ======================================================================

struct GoertzelConfig {
    static constexpr uint8_t N = GOERTZ_N;
    static constexpr uint8_t PWR_SHIFT = CW_PWR_SHIFT;
};

// Q14 coefficients for N=128
static const int16_t g_kCoeffQ14_0_32[33] PROGMEM =
{
    32767, 32728, 32610, 32413, 32138, 31785, 31357, 30852,
    30274, 29621, 28898, 28106, 27246, 26320, 25330, 24279,
    23170, 22005, 20788, 19519, 18205, 16846, 15447, 14010,
    12540, 11039,  9512,  7962,  6393,  4808,  3212,  1608,
        0
};

static inline int16_t coefFromK(uint8_t k) {
    if (k <= 32) return (int16_t)pgm_read_word(&g_kCoeffQ14_0_32[k]);
    uint8_t  m = (uint8_t)(64 - k);
    int16_t  v = (int16_t)pgm_read_word(&g_kCoeffQ14_0_32[m]);
    return (int16_t)(-v);
}

// ======================================================================
// ===== STATE ===========================================================
// ======================================================================

static uint8_t  cw_inited = 0;
static uint8_t  cw_env8 = 0;
static uint8_t  cw_avg8 = 0;
static uint8_t  cw_dot = 3;
static uint8_t  cw_mark = 0;
static uint16_t cw_run = 0;

static uint8_t  g_binK = 8;    // start near 600 Hz; ADAPTIVE_K will converge
static constexpr uint8_t K_MIN = 4;
static constexpr uint8_t K_MAX = 16;

static constexpr uint8_t NB_BLOCKS = 1;
static uint8_t  g_gateCnt = 0;

static uint8_t  g_flushHold = 0;
static uint16_t g_symbolTimeout = 0;
static constexpr uint16_t SYMBOL_TIMEOUT_MAX = 20;

static int16_t  g_acBuf[GoertzelConfig::N];

// ======================================================================
// ===== GOERTZEL CORE ===================================================
// ======================================================================

// Avoid inlining large loops to reduce flash size
static uint32_t goertzelPowerOnBuf(uint8_t k) __attribute__((noinline));
static uint32_t goertzelPowerOnBuf(uint8_t k) {
    int16_t coef = coefFromK(k);
    int32_t Q1 = 0;
    int32_t Q2 = 0;

    for (uint8_t i = 0; i < GoertzelConfig::N; i++) {
        int32_t Q0 = ((int32_t)coef * Q1 >> 14) - Q2 + g_acBuf[i];
        Q2 = Q1;
        Q1 = Q0;
    }

    return ((uint32_t)ABS32(Q1) + (uint32_t)ABS32(Q2));
}

static inline uint8_t cwIsBlockValid(uint16_t mn, uint16_t mx) {
    return (mx < 1022 && mn > 1 && (uint16_t)(mx - mn) > 3) ? 1 : 0;
}

static uint8_t fillAcBufAndCheck() __attribute__((noinline));
static uint8_t fillAcBufAndCheck() {
    uint16_t mn = 1023;
    uint16_t mx = 0;

    for (uint8_t i = 0; i < GoertzelConfig::N; i++) {
        uint16_t x = analogRead(CW_ADC_PIN);
        if (x < mn) mn = x;
        if (x > mx) mx = x;
        g_acBuf[i] = acCouple(x);
    }
    return cwIsBlockValid(mn, mx);
}

// Select best power and adapt bin if needed
static inline uint32_t goertzelBestPower(uint8_t kcur) {
#if ADAPTIVE_K
    if (cw_mark) {
        return goertzelPowerOnBuf(kcur);
    } else {
        uint8_t  klo = (kcur > K_MIN) ? (uint8_t)(kcur - 1) : kcur;
        uint8_t  khi = (kcur < K_MAX) ? (uint8_t)(kcur + 1) : kcur;

        uint32_t p_cur = goertzelPowerOnBuf(kcur);
        uint32_t p_lo = (klo != kcur) ? goertzelPowerOnBuf(klo) : 0;
        uint32_t p_hi = (khi != kcur) ? goertzelPowerOnBuf(khi) : 0;

        if (p_lo > p_cur + (p_cur >> 3)) g_binK = klo;
        if (p_hi > p_cur + (p_cur >> 3)) g_binK = khi;

        uint32_t p_best = p_cur;
        if (p_lo > p_best) p_best = p_lo;
        if (p_hi > p_best) p_best = p_hi;
        return p_best;
    }
#else
    (void)kcur;
    return goertzelPowerOnBuf(g_binK);
#endif
}

static uint8_t goertzelPower8() __attribute__((noinline));
static uint8_t goertzelPower8() {
    if (!fillAcBufAndCheck()) return 0;
    uint32_t p_best = goertzelBestPower(g_binK);
    uint32_t p8 = p_best >> GoertzelConfig::PWR_SHIFT;
    return (p8 > 255) ? 255 : (uint8_t)p8;
}

// ======================================================================
// ===== ENVELOPE, THRESHOLDS, HYSTERESIS ===============================
// ======================================================================

static inline void CWDecoder_begin() {
    adcSetVref();
    adcSeedDC();

    g_binK = 8; // ADAPTIVE_K will quickly converge to actual tone

    cw_env8 = 0;
    cw_avg8 = 0;
    cw_run = 0;
    cw_mark = 0;
    g_gateCnt = 0;
    g_flushHold = 2;  // Skip first couple of blocks after start
    g_symbolTimeout = 0;
    cw_dot = 3;

    cw_inited = 1;
}

static inline void cwSampleAndUpdate() {
    uint8_t mag8 = goertzelPower8();
    // Fast envelope tracking
    cw_env8 += (int8_t)(((int16_t)mag8 - cw_env8) >> 0);
    // Slow noise floor tracking
    cw_avg8 += (int8_t)(((int16_t)mag8 - cw_avg8) >> 6);
    if (g_flushHold) g_flushHold--;
}

// Basic hysteresis with earlier release
static inline uint8_t cwHysteresis() {
    uint8_t base = cw_avg8;
    uint8_t margin = (base >> 3) + 2;
    uint8_t thr_on = (uint8_t)(base + margin);
    uint8_t thr_off = (uint8_t)(base + 1);
    return cw_mark ? (cw_env8 > thr_off) : (cw_env8 > thr_on);
}

// ======================================================================
// ===== MORSE SYMBOL CLASSIFICATION ====================================
// ======================================================================

struct MorseConfig {
    static constexpr uint8_t DOT_MIN = 1;
    static constexpr uint8_t DOT_MAX = 4;
    static constexpr uint8_t SPACE_MIN = 2;
};

static inline char classifyMark() {
    uint16_t r = cw_run;
    if (r < 2) return 0; // Filter 1-block glitches

    // dot/dash threshold - dot length + 2
    uint8_t threshold = cw_dot + 2;

    if (r <= threshold) {
        // More stable weighted average for dot timing
        if (r >= 2 && r <= 4) {
            cw_dot = (uint8_t)((cw_dot * 3 + r) >> 2); // 75/25 average
            if (cw_dot < MorseConfig::DOT_MIN) cw_dot = MorseConfig::DOT_MIN;
            if (cw_dot > MorseConfig::DOT_MAX) cw_dot = MorseConfig::DOT_MAX;
        }
        return '.';
    } else {
        return '-';
    }
}

static inline char classifySpace() {
    uint16_t r = cw_run;
    if (r < 2) return 0; // Filter 1-block glitches

    // Normalize dot length to a sane range for calculations
    uint8_t dot = cw_dot;
    if (dot < 2) dot = 2;
    if (dot > 4) dot = 4;

    // Adaptive thresholds based on classic Morse timing
    // Inter-letter space: ~3 dots
    // Threshold at ~2.5 dots
    uint8_t letThr = (dot << 1) + (dot >> 1);
    // Inter-word space: ~7 dots
    // Threshold at ~6 dots
    uint8_t wordThr = dot * 6;

    if (r >= wordThr) return ' ';
    if (r >= letThr)  return '|';
    return 0;
}

static inline char cwClassify(uint8_t wasMark) {
    return wasMark ? classifyMark() : classifySpace();
}

// ======================================================================
// ===== MORSE DECODER (A..Z, 0..9) =====================================
// ======================================================================

#define CW_PACK(len,bits) ((uint8_t)((((len)-1)&0x07)<<5) | ((bits)&0x1F))
#define CW_LEN(b)         (((b)>>5)+1)
#define CW_BITS(b)        ((b)&0x1F)

// Morse code lookup table (A..Z, 0..9)
static const uint8_t cw_lut_packed[] PROGMEM =
{
    // A..Z
    CW_PACK(2,1),  CW_PACK(4,8),  CW_PACK(4,10), CW_PACK(3,4),
    CW_PACK(1,0),  CW_PACK(4,2),  CW_PACK(3,6),  CW_PACK(4,0),
    CW_PACK(2,0),  CW_PACK(4,7),  CW_PACK(3,5),  CW_PACK(4,4),
    CW_PACK(2,3),  CW_PACK(2,2),  CW_PACK(3,7),  CW_PACK(4,6),
    CW_PACK(4,13), CW_PACK(3,2),  CW_PACK(3,0),  CW_PACK(1,1),
    CW_PACK(3,1),  CW_PACK(4,1),  CW_PACK(3,3),  CW_PACK(4,9),
    CW_PACK(4,11), CW_PACK(4,12),

    // 0..9
    CW_PACK(5,31), CW_PACK(5,15), CW_PACK(5,7),  CW_PACK(5,3),
    CW_PACK(5,1),  CW_PACK(5,0),  CW_PACK(5,16), CW_PACK(5,24),
    CW_PACK(5,28), CW_PACK(5,30)
};
static constexpr uint8_t CW_LUT_COUNT =
sizeof(cw_lut_packed) / sizeof(cw_lut_packed[0]);

static uint8_t s_acc_len = 0;
static uint8_t s_acc_bits = 0;

static inline void cwAccReset() {
    s_acc_len = 0;
    s_acc_bits = 0;
}

static inline void cwAccAdd(bool dash) {
    if (s_acc_len < 5) {
        s_acc_bits = (uint8_t)((s_acc_bits << 1) | (dash ? 1 : 0));
        s_acc_len++;
    }
}

static inline char cwDecodeFromAcc() {
    if (!s_acc_len) return 0;

    // Special case: '/' = -..-. = 5 bits 10010 (18)
    if (s_acc_len == 5 && s_acc_bits == 18) return '/';

    for (uint8_t i = 0; i < CW_LUT_COUNT; i++) {
        uint8_t pb = pgm_read_byte(&cw_lut_packed[i]);
        if (CW_LEN(pb) != s_acc_len) continue;
        if (CW_BITS(pb) == s_acc_bits)
            return (i < 26)
            ? (char)('A' + i)
            : (char)('0' + (i - 26));
    }
    return 0;
}

// ======================================================================
// ===== VIEW HELPERS ===================================================
// ======================================================================

static inline void cwViewClearViewport() {
    for (uint8_t r = CW_VIEW_ROW_FIRST; r <= CW_VIEW_ROW_LAST; ++r)
        UI_ClearRow(r);
}

static inline void cwViewDrawHeader() {
#if SHOW_CW_HEADER
    drawInverted(0, 0, F("CW DECODER"), true);
#endif
}

static inline void cwViewHomeAndClear() {
    cwViewClearViewport();
    g_cwRow = CW_VIEW_ROW_FIRST;
    g_cwCol = 0;
}

static inline void cwViewWrapIfNeeded() {
    if (g_cwCol >= UI_COLS) {
        g_cwCol = 0;
        if (g_cwRow < CW_VIEW_ROW_LAST) {
            g_cwRow++;
            UI_ClearRow(g_cwRow);
        } else {
            cwViewHomeAndClear();
        }
    }
}

static inline void cwViewPut(char c) {
    if (c == '\r') return;
    if (c == '\n') c = ' ';

    cwViewWrapIfNeeded();
    UI_SetCursorCR(g_cwCol, g_cwRow);
    oled.print(c);
    g_cwCol++;
}

// ======================================================================
// ===== EVENT PIPELINE + POLL ==========================================
// ======================================================================

static void cwFlushSymbol() __attribute__((noinline));
static void cwFlushSymbol() {
    char out = 0;

    if (s_acc_len >= 1 && s_acc_len <= 5) {
        out = cwDecodeFromAcc();
    }

#if DEBUG_CW
    if (s_acc_len > 0) {
        debugPutChar('P');
        debugPutChar(':');
        for (uint8_t i = 0; i < s_acc_len; i++) {
            debugPutChar(((s_acc_bits >> (s_acc_len - 1 - i)) & 1) ? '-' : '.');
        }
        debugPutChar('=');
        debugPutChar(out ? out : '?');
        if (s_acc_len > 5) {
            debugPutChar('!');
        }
        debugPutChar('\r');
        debugPutChar('\n');
    }
#endif

    cwAccReset();

    if (out) {
#if DEBUG_CW
        debugPutChar('C');
        debugPutChar(':');
        debugPutChar(out == ' ' ? '_' : out);
        debugPutChar(' ');
        debugPutChar('[');
        debugPutChar('0' + cw_dot);  // Single digit (1..4)
        debugPutChar(']');
        debugPutChar('\r');
        debugPutChar('\n');
#endif
        cwViewPut(out);
        g_flushHold = 1;
    }
}

static inline void cwResetOnStateChange(uint8_t new_mark) {
    cw_run = 0;
    cw_mark = new_mark;
    g_gateCnt = 0;
    g_symbolTimeout = 0;
}

static inline char cwMakeStateChangeAndEvent(uint8_t new_mark) {
    char ev = cwClassify(cw_mark);
    cwResetOnStateChange(new_mark);
    return ev;
}

static inline char CWDecoder_poll() {
    if (!cw_inited) return 0;

    cwSampleAndUpdate();

    if (g_flushHold) return 0;

    uint8_t m2 = cwHysteresis();
    cw_run++;
    g_symbolTimeout++;

    // Force symbol boundary on timeout only if we have something accumulated
    if (g_symbolTimeout > SYMBOL_TIMEOUT_MAX && s_acc_len > 0) {
        cwFlushSymbol();
        g_symbolTimeout = 0;
        return '|';
    }

    if (m2 != cw_mark) {
        if (++g_gateCnt >= NB_BLOCKS) {
            return cwMakeStateChangeAndEvent(m2);
        }
    } else {
        g_gateCnt = 0;
    }
    return 0;
}

static inline void cwProcessEvent(char ev) {
    switch (ev) {
    case '.': if (!g_flushHold) cwAccAdd(false); break;
    case '-': if (!g_flushHold) cwAccAdd(true);  break;
    case '|': cwFlushSymbol();                   break;
    case ' ': cwFlushSymbol(); cwViewPut(' ');   break;
    default:  break;
    }
}

// ======================================================================
// ===== PUBLIC ENTRY POINTS ============================================
// ======================================================================

static void cwViewEnter() {
    oled.clear();
    cwViewDrawHeader();
    cwViewHomeAndClear();
    cwAccReset();

#if DEBUG_CW
    initDebugUART();
#endif

    CWDecoder_begin();
}

static void cwViewExit() {
    oled.clear();
    showStatus(true);
}

static inline void cwViewTask() {
    char ev;
    while ((ev = CWDecoder_poll()))
        cwProcessEvent(ev);
}

#endif  // ENABLE_CW_DECODER
