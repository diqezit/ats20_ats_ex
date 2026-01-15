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
// Rtop = 390 kΩ from +3.48 V to A6 node (470 kΩ also OK, gives ~0.5 V bias)
// Rbot = 75 kΩ from A6 node to GND
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
// ===== TYPE DEFINITIONS & ENUMS ======================================
// ======================================================================

typedef enum : uint8_t {
    CW_EVENT_NONE = 0,
    CW_EVENT_DOT = '.',
    CW_EVENT_DASH = '-',
    CW_EVENT_LETTER_GAP = '|',
    CW_EVENT_WORD_GAP = ' '
} cw_event_t;

typedef enum : uint8_t {
    CW_STATE_SPACE = 0,
    CW_STATE_MARK = 1
} cw_state_t;

typedef struct {
    uint8_t row;
    uint8_t col;
} cw_cursor_t;

// ======================================================================
// ===== CONFIGURATION & CONSTANTS =====================================
// ======================================================================

#ifndef CW_ADC_PIN
#define CW_ADC_PIN A6
#endif

// Define GOERTZ_N first, before validation
#ifndef GOERTZ_N
#define GOERTZ_N 128
#endif

static constexpr uint8_t GOERTZ_Q14_SHIFT = 14;

#ifndef CW_PWR_SHIFT
#define CW_PWR_SHIFT 3
#endif

#ifndef DEBUG_CW
#define DEBUG_CW 0
#endif

// Enable adaptive timing for automatic WPM tracking
#ifdef ADAPTIVE_K
#undef ADAPTIVE_K
#endif
#define ADAPTIVE_K 1

#ifndef SHOW_CW_HEADER
#define SHOW_CW_HEADER 1
#endif

// Now validate GOERTZ_N
#if GOERTZ_N < 64 || GOERTZ_N > 255
#error "GOERTZ_N must be between 64 and 255"
#endif

#if CW_PWR_SHIFT < 1 || CW_PWR_SHIFT > 8
#error "CW_PWR_SHIFT must be between 1 and 8"
#endif

#define UI_ROW_Y(r) (r)
#define ABS32(v)    ((v) < 0 ? -(v) : (v))
#define CW_PACK(len,bits) ((uint8_t)((((len)-1)&0x07)<<5) | ((bits)&0x1F))
#define CW_LEN(b)   (((b)>>5)+1)
#define CW_BITS(b)  ((b)&0x1F)
#define IS_VALID_ADC_RANGE(mn, mx) ((mx) < 1022 && (mn) > 1 && ((mx) - (mn)) > 3)

static constexpr uint8_t CW_VIEW_ROW_FIRST = 2;
static constexpr uint8_t CW_VIEW_ROW_LAST = 7;
static constexpr uint8_t CW_VIEW_ROWS = CW_VIEW_ROW_LAST - CW_VIEW_ROW_FIRST + 1;

// --- Fast ADC for CW (no analogRead / analogReference) ---
static inline void cwAdcInit_Internal1V1_A6() {
    // Reference = INTERNAL 1.1V (REFS1:REFS0 = 11)
    // Channel = ADC6 (A6)
    uint8_t ch = (uint8_t)(CW_ADC_PIN - A0);   // A6-A0 = 6

    ADMUX = _BV(REFS1) | _BV(REFS0) | (ch & 0x07);

    // Enable ADC, prescaler /128 (16MHz -> 125kHz ADC clock)
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

    delay(AdcConfig::SETTLE_MS);

    // dummy conversion after ref change
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {}
    (void)ADC;
}

static inline uint16_t cwAdcRead() {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {}
    return ADC;
}

// ======================================================================
// ===== ALGORITHM CONFIGURATION STRUCTURES ============================
// ======================================================================

struct AdcConfig {
    static constexpr uint8_t SETTLE_MS = 5;
    static constexpr uint8_t DC_SLOW_SHIFT = 8;
    static constexpr uint8_t DC_SEED_ITER = 16;
    static constexpr uint8_t DC_SEED_SHIFT = 3;
};

struct GoertzelConfig {
    static constexpr uint8_t N = GOERTZ_N;
    static constexpr uint8_t PWR_SHIFT = CW_PWR_SHIFT;
    static constexpr uint8_t Q14_SHIFT = GOERTZ_Q14_SHIFT;
};

struct MorseTimingConfig {
    static constexpr uint8_t DOT_MIN = 2;
    static constexpr uint8_t DOT_MAX = 6;
    static constexpr uint8_t DOT_DASH_THRESHOLD = 2;

    static constexpr uint8_t DOT_TIMING_OLD_WEIGHT = 15;
    static constexpr uint8_t DOT_TIMING_NEW_WEIGHT = 1;

    static constexpr uint8_t LETTER_GAP_NUM = 3;
    static constexpr uint8_t LETTER_GAP_DEN = 2;
    static constexpr uint8_t WORD_GAP_NUM = 9;
    static constexpr uint8_t WORD_GAP_DEN = 2;

    static constexpr uint8_t MIN_MARK_BLOCKS = 2;
    static constexpr uint8_t MIN_SPACE_BLOCKS = 1;
    static constexpr uint8_t MAX_DASH_FACTOR = 5;
    static constexpr uint8_t DASH_ADAPT_FACTOR = 2;
    static constexpr uint8_t ITU_DASH_TO_DOT_RATIO = 3;
};

struct FreqTrackingConfig {
    static constexpr uint8_t K_600HZ = 10;
    static constexpr uint8_t K_DEFAULT = K_600HZ;
};

struct SymbolConfig {
    static constexpr uint8_t GATE_BLOCKS = 1;
    static constexpr uint16_t TIMEOUT_MAX = 10;
    static constexpr uint8_t STARTUP_SKIP = 2;
    static constexpr uint8_t MAX_MORSE_LEN = 5;
};

struct SignalConfig {
    static constexpr uint8_t ENV_FAST_SHIFT = 0;
    static constexpr uint8_t NOISE_SLOW_SHIFT = 6;
    static constexpr uint8_t HYSTERESIS_MARGIN = 3;
    static constexpr uint8_t HYSTERESIS_CONST = 3;
};

struct AdaptationConfig {
    static constexpr uint8_t  BLOCK_TIME_MS = 14;
    static constexpr uint16_t WPM_FACTOR = 1200;
    static constexpr uint8_t  WPM_MIN = 5;
    static constexpr uint8_t  WPM_MAX = 60;
    static constexpr uint8_t  WPM_SMOOTH_SHIFT = 1;
    static constexpr uint8_t  WPM_UPDATE_PERIOD = 5;

    static constexpr uint8_t  MARK_MIN_BLOCKS = 2;
    static constexpr uint8_t  MARK_MAX_BLOCKS = 30;

    static constexpr uint8_t  DOT_CLASSIFY_NUM = 3;
    static constexpr uint8_t  DOT_CLASSIFY_DEN = 2;

    static constexpr uint8_t  AVG_SCALE = 10;
    static constexpr uint8_t  AVG_SMOOTH_OLD = 7;
    static constexpr uint8_t  AVG_SMOOTH_NEW = 8;
    static constexpr uint8_t  AVG_SMOOTH_SHIFT = 4;

    static constexpr uint8_t  DOTLEN_UP_DELTA = 2;
    static constexpr uint8_t  DOTLEN_DOWN_OFFSET = 1;

    static constexpr uint8_t  LONG_DASH_FACTOR = 4;
    static constexpr uint8_t  SPEED_DEC_DOT_EST_NUM = 1;
    static constexpr uint8_t  SPEED_DEC_DOT_EST_DEN = 3;
};

// ======================================================================
// ===== GLOBAL STATE VARIABLES ========================================
// ======================================================================

static cw_cursor_t g_cwCursor = { CW_VIEW_ROW_FIRST, 0 };
static int16_t g_dcOffset = 512;

static struct {
    uint8_t initialized : 1;
    uint8_t currentState : 1;
    uint8_t flushHold : 4;
    uint8_t gateCounter : 2;
    uint8_t envelopeLevel;
    uint8_t noiseFloor;
    uint8_t dotLength;
    uint16_t runLength;
    uint16_t symbolTimeout;
} g_cwState = { 0, CW_STATE_SPACE, 0, 0, 0, 0, 3, 0, 0 };

static struct {
    uint8_t binK;
} g_freqTracker = { FreqTrackingConfig::K_DEFAULT };

static struct {
    uint8_t length;
    uint8_t pattern;
} g_morseAccumulator = { 0, 0 };

// Adaptation state variables
static struct {
    uint16_t avgDotTime;       // Average dot time in blocks (x10 for precision)
    uint8_t currentWPM;        // Current WPM estimate
    uint8_t symbolsSinceUpdate;// Counter for display update
} g_adaptation = { 30, 0, 0 }; // Start with 3.0 blocks average

#if DEBUG_CW
// ADC debug info
static struct {
    uint16_t minAdc;
    uint16_t maxAdc;
    uint8_t blocks;
} g_adcDebug = { 1023, 0, 0 };
#endif

static int16_t g_acBuffer[GoertzelConfig::N];

// ======================================================================
// ===== LOOKUP TABLES =================================================
// ======================================================================

// Goertzel coefficient table (Q14 fixed-point)
static const int16_t g_goertzelCoeffQ14[33] PROGMEM = {
    32767, 32728, 32610, 32413, 32138, 31785, 31357, 30852,
    30274, 29621, 28898, 28106, 27246, 26320, 25330, 24279,
    23170, 22005, 20788, 19519, 18205, 16846, 15447, 14010,
    12540, 11039,  9512,  7962,  6393,  4808,  3212,  1608,
    0
};

// Morse code lookup table
static const uint8_t g_morseLookup[] PROGMEM = {
    // A..Z
    CW_PACK(2,1), CW_PACK(4,8), CW_PACK(4,10), CW_PACK(3,4),
    CW_PACK(1,0), CW_PACK(4,2), CW_PACK(3,6), CW_PACK(4,0),
    CW_PACK(2,0), CW_PACK(4,7), CW_PACK(3,5), CW_PACK(4,4),
    CW_PACK(2,3), CW_PACK(2,2), CW_PACK(3,7), CW_PACK(4,6),
    CW_PACK(4,13), CW_PACK(3,2), CW_PACK(3,0), CW_PACK(1,1),
    CW_PACK(3,1), CW_PACK(4,1), CW_PACK(3,3), CW_PACK(4,9),
    CW_PACK(4,11), CW_PACK(4,12),

    // 0..9
    CW_PACK(5,31), CW_PACK(5,15), CW_PACK(5,7),  CW_PACK(5,3),
    CW_PACK(5,1),  CW_PACK(5,0),  CW_PACK(5,16), CW_PACK(5,24),
    CW_PACK(5,28), CW_PACK(5,30)
};

static constexpr uint8_t MORSE_LUT_SIZE = sizeof(g_morseLookup);
static constexpr uint8_t MORSE_LETTERS = 26;

// ======================================================================
// ===== FORWARD DECLARATIONS ==========================================
// ======================================================================

// Main entry points
static inline void CWDecoder_begin();
static inline cw_event_t CWDecoder_poll();
static void cwViewEnter();
static void cwViewExit();
static inline void cwViewTask();

// Display functions
static inline void UI_ClearRow(uint8_t row);
static inline void UI_SetCursorCR(uint8_t col, uint8_t row);
static inline void cwViewPut(char c);
static inline void cwViewClearViewport();
static inline void cwViewDrawHeader();
static inline void cwViewHomeAndClear();
static inline void cwViewWrapIfNeeded();
static inline void cwViewShowWPM();

// ADC functions
static inline void adcSetInternalVref();
static inline int16_t acCoupleSignal(uint16_t adcValue);
static inline void adcSeedDcOffset();

// Goertzel functions
static inline int16_t getGoertzelCoeff(uint8_t bin);
static uint32_t calculateGoertzelPower(uint8_t bin) __attribute__((noinline));
static inline uint32_t findBestFrequencyPower(uint8_t currentBin);
static uint8_t sampleAndAnalyzeSignal() __attribute__((noinline));

// Signal analysis functions
static inline bool validateSignalBlock(uint16_t minVal, uint16_t maxVal);
static uint8_t fillBufferAndValidate() __attribute__((noinline));
static inline void updateEnvelopeAndNoise(uint8_t magnitude);
static inline bool detectToneWithHysteresis();

// Morse timing functions
static inline void adaptDotTiming(uint16_t markLength);
static inline void updateWPM(uint16_t dotTimeBlocks);
static inline cw_event_t classifyMarkEvent();
static inline cw_event_t classifySpaceEvent();
static inline cw_event_t classifyCurrentEvent(bool wasMark);

// Morse decoding functions
static inline void resetMorseAccumulator();
static inline void addToMorsePattern(bool isDash);
static inline char lookupMorseCharacter();
static inline char decodeMorseFromAccumulator();
static void flushCurrentSymbol() __attribute__((noinline));
static inline void processDecodedEvent(cw_event_t event);

// State management functions
static inline void resetStateOnTransition(cw_state_t newState);
static inline cw_event_t makeStateTransition(cw_state_t newState);
static inline void initializeDecoderState();

// Decoder core helper functions
static inline cw_event_t handleSymbolTimeout();
static inline cw_event_t handleStateTransition(bool toneDetected);

#if DEBUG_CW
// Debug functions
static inline void debugOutputMorsePattern();
static inline void debugOutputDecodedChar(char chr);
static inline void debugOutputTiming(cw_event_t event, uint16_t length);
static inline void debugOutputAdcInfo();
static inline void debugOutputWPM();
#endif

// ======================================================================
// ================ DISPLAY IMPLEMENTATION =============================
// ======================================================================

static inline void UI_ClearRow(uint8_t row) {
    clearBox(0, row * UI_CHAR_H, UI_SCREEN_W, UI_CHAR_H);
}

static inline void UI_SetCursorCR(uint8_t col, uint8_t row) {
    oled.setCursor((uint8_t)(col * UI_CHAR_W), row);
}

static inline void cwViewClearViewport() {
    for (uint8_t r = CW_VIEW_ROW_FIRST; r <= CW_VIEW_ROW_LAST; ++r) {
        UI_ClearRow(r);
    }
}

static inline void cwViewDrawHeader() {
#if SHOW_CW_HEADER
    drawInverted(0, 0, F("CW DECODER"), true);
    cwViewShowWPM();
#endif
}

// Display WPM in header
static inline void cwViewShowWPM() {
#if SHOW_CW_HEADER
    if (g_adaptation.currentWPM > 0) {
        UI_SetCursorCR(UI_COLS - 6, 0);
        oled.print(g_adaptation.currentWPM);
        oled.print(F("WPM"));
    }
#endif
}

static inline void cwViewHomeAndClear() {
    cwViewClearViewport();
    g_cwCursor.row = CW_VIEW_ROW_FIRST;
    g_cwCursor.col = 0;
}

// Screen space is limited so we wrap to next line and scroll when needed
static inline void cwViewWrapIfNeeded() {
    if (g_cwCursor.col >= UI_COLS) {
        g_cwCursor.col = 0;
        if (g_cwCursor.row < CW_VIEW_ROW_LAST) {
            g_cwCursor.row++;
            UI_ClearRow(g_cwCursor.row);
        } else {
            cwViewHomeAndClear(); // scroll by clearing everything
        }
    }
}

static inline void cwViewPut(char c) {
    if (c == '\r') return;
    if (c == '\n') c = ' '; // newlines show as spaces on small display

    cwViewWrapIfNeeded();
    UI_SetCursorCR(g_cwCursor.col, g_cwCursor.row);
    oled.print(c);
    g_cwCursor.col++;
}

// ======================================================================
// ================ ADC IMPLEMENTATION ==================================
// ======================================================================

// Internal 1.1V reference gives better precision than 5V for weak CW signals
static inline void adcSetInternalVref() {
    cwAdcInit_Internal1V1_A6();
}

// Speaker output has DC component that varies with radio settings
static inline int16_t acCoupleSignal(uint16_t adcValue) {
    g_dcOffset += ((int32_t)adcValue - g_dcOffset)
        >> AdcConfig::DC_SLOW_SHIFT;
    return (int16_t)adcValue - g_dcOffset;
}

// Initial DC offset estimation prevents startup transients
static inline void adcSeedDcOffset() {
    g_dcOffset = cwAdcRead();
    for (uint8_t i = 0; i < AdcConfig::DC_SEED_ITER; i++) {
        uint16_t sample = cwAdcRead();
        g_dcOffset += ((int32_t)sample - g_dcOffset) >> AdcConfig::DC_SEED_SHIFT;
    }
}

// ======================================================================
// ================ GOERTZEL IMPLEMENTATION ============================
// ======================================================================

typedef struct {
    int32_t Q1;
    int32_t Q2;
} GoertzelState;

// Table only stores half the coefficients due to symmetry
static inline int16_t getGoertzelCoeff(uint8_t bin) {
    if (bin <= 32)
        return (int16_t)pgm_read_word(&g_goertzelCoeffQ14[bin]);

    uint8_t mirrorBin = 64 - bin;
    int16_t coeff = (int16_t)pgm_read_word(&g_goertzelCoeffQ14[mirrorBin]);
    return -coeff;
}

// Resets the algorithm state before processing a new block of samples
static inline void goertzelReset(GoertzelState* state) {
    state->Q1 = 0;
    state->Q2 = 0;
}

// Processes a single sample using fixed-point math
static inline void goertzelProcessSample(
    int16_t sample,
    int16_t coeff,
    GoertzelState* state) {
    int32_t Q0 = ((int32_t)coeff * state->Q1 >> GoertzelConfig::Q14_SHIFT)
        - state->Q2 + sample;
    state->Q2 = state->Q1;
    state->Q1 = Q0;
}

// Calculates signal power using a fast approximation instead of sqrt
static inline uint32_t goertzelGetPower(GoertzelState* state) {
    return (uint32_t)ABS32(state->Q1) + (uint32_t)ABS32(state->Q2);
}

// This function now orchestrates Goertzel process using the helper functions above
static uint32_t calculateGoertzelPower(uint8_t bin) {
    GoertzelState state;
    goertzelReset(&state);

    int16_t coeff = getGoertzelCoeff(bin);

    for (uint8_t i = 0; i < GoertzelConfig::N; i++)
        goertzelProcessSample(g_acBuffer[i], coeff, &state);

    return goertzelGetPower(&state);
}

// frequency tracking
static inline uint32_t findBestFrequencyPower(uint8_t currentBin) {
    return calculateGoertzelPower(currentBin);
}

// ======================================================================
// ================ SIGNAL PROCESSING IMPLEMENTATION ===================
// ======================================================================

// Reject audio blocks that are clipped or have no signal
static inline bool validateSignalBlock(uint16_t minVal, uint16_t maxVal) {
    return IS_VALID_ADC_RANGE(minVal, maxVal);
}

// Fill buffer with ADC samples and validate signal quality
static uint8_t fillBufferAndValidate() {
    uint16_t minVal = 1023, maxVal = 0;

    for (uint8_t i = 0; i < GoertzelConfig::N; i++) {
        uint16_t sample = cwAdcRead();
        if (sample < minVal) minVal = sample;
        if (sample > maxVal) maxVal = sample;
        g_acBuffer[i] = acCoupleSignal(sample);
    }

#if DEBUG_CW
    // Update ADC debug info
    g_adcDebug.minAdc = minVal;
    g_adcDebug.maxAdc = maxVal;
    g_adcDebug.blocks++;

    // Print every 100 blocks
    if (g_adcDebug.blocks >= 100) {
        debugOutputAdcInfo();
        g_adcDebug.blocks = 0;
    }
#endif

    return validateSignalBlock(minVal, maxVal) ? 1 : 0;
}

static uint8_t sampleAndAnalyzeSignal() {
    if (!fillBufferAndValidate()) return 0;

    // Prevent runLength from overflowing if signal gets stuck ON
    if (g_cwState.runLength > 200)
        g_cwState.runLength = 200;

    uint32_t bestPower = findBestFrequencyPower(g_freqTracker.binK);

    uint32_t scaledPower = bestPower >> GoertzelConfig::PWR_SHIFT;
    return (scaledPower > 255) ? 255 : (uint8_t)scaledPower;
}

// two filters distinguish signal from noise floor
static inline void updateEnvelopeAndNoise(uint8_t magnitude) {

    // fast filter tracks signal peaks
    g_cwState.envelopeLevel += (int8_t)((int16_t)magnitude - g_cwState.envelopeLevel)
        >> SignalConfig::ENV_FAST_SHIFT;

    // much slower filter estimates background noise level
    g_cwState.noiseFloor += ((int16_t)magnitude - g_cwState.noiseFloor)
        >> SignalConfig::NOISE_SLOW_SHIFT;

    if (g_cwState.flushHold)
        g_cwState.flushHold--;
}

// Hysteresis prevents chattering on weak or noisy signals
static inline bool detectToneWithHysteresis() {
    uint8_t base = g_cwState.noiseFloor;

    uint8_t margin = (base >> SignalConfig::HYSTERESIS_MARGIN)
        + SignalConfig::HYSTERESIS_CONST;

    // Use a higher threshold to turn ON, and a lower one to stay ON
    uint8_t thresholdOn = base + margin;
    uint8_t thresholdOff = base + 1;

    return g_cwState.currentState == CW_STATE_MARK
        ? (g_cwState.envelopeLevel > thresholdOff)
        : (g_cwState.envelopeLevel > thresholdOn);
}

// ======================================================================
// ================ TIMING CLASSIFICATION IMPLEMENTATION ===============
// ======================================================================

static inline uint8_t clamp(uint8_t value, uint8_t min, uint8_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline uint8_t calculateThreshold(uint8_t dotLen,
    uint8_t numerator,
    uint8_t denominator) {
    return ((uint16_t)dotLen * numerator) / denominator;
}

// Helper function to determine if mark is dot or dash
static inline bool isMarkDot(uint16_t clampedLength) {
    const uint8_t dashThreshold =
        g_cwState.dotLength + MorseTimingConfig::DOT_DASH_THRESHOLD;
    return (clampedLength <= dashThreshold);
}

// Helper function to clamp mark length to valid range
static inline uint16_t clampMarkLength(uint16_t length) {
    const uint16_t maxLength =
        (uint16_t)g_cwState.dotLength *
        MorseTimingConfig::MAX_DASH_FACTOR;
    return (length > maxLength) ? maxLength : length;
}

static inline cw_event_t classifyMarkEvent() {
    const uint16_t length = g_cwState.runLength;

    if (length < MorseTimingConfig::MIN_MARK_BLOCKS) return CW_EVENT_NONE;

    const uint16_t clampedLength = clampMarkLength(length);
    const bool isDot = isMarkDot(clampedLength);

#if DEBUG_CW
    debugOutputTiming(isDot ? CW_EVENT_DOT : CW_EVENT_DASH, length);
#endif

    adaptDotTiming(clampedLength);

    return isDot ? CW_EVENT_DOT : CW_EVENT_DASH;
}

// Helper function to calculate gap thresholds
static inline void calculateGapThresholds(uint8_t& letterThreshold, uint8_t& wordThreshold) {
    letterThreshold = calculateThreshold(
        g_cwState.dotLength,
        MorseTimingConfig::LETTER_GAP_NUM,
        MorseTimingConfig::LETTER_GAP_DEN);

    wordThreshold = calculateThreshold(
        g_cwState.dotLength,
        MorseTimingConfig::WORD_GAP_NUM,
        MorseTimingConfig::WORD_GAP_DEN);
}

static inline cw_event_t classifySpaceEvent() {
    const uint16_t length = g_cwState.runLength;

    if (length < MorseTimingConfig::MIN_SPACE_BLOCKS) return CW_EVENT_NONE;

    uint8_t letterThreshold, wordThreshold;
    calculateGapThresholds(letterThreshold, wordThreshold);

    if (length >= wordThreshold) return CW_EVENT_WORD_GAP;
    if (length >= letterThreshold) return CW_EVENT_LETTER_GAP;

    return CW_EVENT_NONE;
}

static inline cw_event_t classifyCurrentEvent(bool wasMark) {
    return wasMark ? classifyMarkEvent() : classifySpaceEvent();
}

// ======================================================================
// ================ ADAPTATION IMPLEMENTATION ==========================
// ======================================================================

#if ADAPTIVE_K

// compute average dot length in blocks from fixed-point state
static inline __attribute__((always_inline))
uint16_t avgBlocksCurrent() {
    return g_adaptation.avgDotTime / AdaptationConfig::AVG_SCALE;
}

// cast once to keep compares uniform on AVR
static inline __attribute__((always_inline))
uint16_t dotLenU16() {
    return static_cast<uint16_t>(g_cwState.dotLength);
}

// bump dotLength upward with limit guard
static inline __attribute__((always_inline))
void incDotLength() {
    if (g_cwState.dotLength < MorseTimingConfig::DOT_MAX)
        g_cwState.dotLength++;
}

// smooth dot timing and adjust dotLength with a dead band
static inline __attribute__((always_inline))
void adaptDotBranch(uint16_t markLength) {
    const uint16_t newDotTime =
        markLength * AdaptationConfig::AVG_SCALE;

    g_adaptation.avgDotTime =
        ((uint32_t)g_adaptation.avgDotTime *
            AdaptationConfig::AVG_SMOOTH_OLD +
            (uint32_t)newDotTime *
            AdaptationConfig::AVG_SMOOTH_NEW) >>
        AdaptationConfig::AVG_SMOOTH_SHIFT;

    const uint16_t avgBlocks = avgBlocksCurrent();

    // dead band avoids dotLength flicker and header jitter
    if (avgBlocks >= dotLenU16() + AdaptationConfig::DOTLEN_UP_DELTA) {
        incDotLength();
    } else if (avgBlocks + AdaptationConfig::DOTLEN_DOWN_OFFSET <
        dotLenU16()) {
        if (g_cwState.dotLength > MorseTimingConfig::DOT_MIN)
            g_cwState.dotLength--;
    }
}

// long dash implies slowdown, snap average toward dot estimate and nudge dotLength
static inline __attribute__((always_inline))
void adaptDashBranch(uint16_t markLength) {
    if (markLength >
        dotLenU16() * AdaptationConfig::LONG_DASH_FACTOR) {
        const uint16_t estimatedDot =
            (markLength *
                AdaptationConfig::SPEED_DEC_DOT_EST_NUM) /
            AdaptationConfig::SPEED_DEC_DOT_EST_DEN;

        if (estimatedDot > g_cwState.dotLength) {
            g_adaptation.avgDotTime =
                estimatedDot * AdaptationConfig::AVG_SCALE;
            incDotLength();
        }
    }
}

// keep tracking stable on tiny MCU and small screen
// ignore too short or too long marks as noise or stalls
// 1.5x dot threshold locks early without false dash bursts
// WPM refresh in batches to cut header churn
static inline void adaptDotTiming(uint16_t markLength) {
    if (markLength < AdaptationConfig::MARK_MIN_BLOCKS ||
        markLength > AdaptationConfig::MARK_MAX_BLOCKS)
        return;

    const uint8_t threshold =
        (dotLenU16() * AdaptationConfig::DOT_CLASSIFY_NUM) /
        AdaptationConfig::DOT_CLASSIFY_DEN;

    const uint8_t kind = (markLength <= threshold) ? 0 : 1; // 0=dot, 1=dash

    switch (kind) {
    case 0:
        adaptDotBranch(markLength);
        break;
    case 1:
        adaptDashBranch(markLength);
        break;
    default:
        break;
    }

    if (++g_adaptation.symbolsSinceUpdate >=
        AdaptationConfig::WPM_UPDATE_PERIOD) {
        g_adaptation.symbolsSinceUpdate = 0;
        updateWPM(avgBlocksCurrent());
    }
}

// UI wants a WPM number users can trust at a glance
// use 1200/dot_ms with light smoothing and range clamp
static inline void updateWPM(uint16_t dotTimeBlocks) {
    if (dotTimeBlocks == 0) return;

    const uint16_t dotTimeMs =
        dotTimeBlocks * AdaptationConfig::BLOCK_TIME_MS;
    uint8_t newWPM = AdaptationConfig::WPM_FACTOR / dotTimeMs;

    if (g_adaptation.currentWPM == 0) {
        g_adaptation.currentWPM = newWPM;
    } else {
        g_adaptation.currentWPM =
            (g_adaptation.currentWPM + newWPM) >>
            AdaptationConfig::WPM_SMOOTH_SHIFT;
    }

    if (g_adaptation.currentWPM < AdaptationConfig::WPM_MIN)
        g_adaptation.currentWPM = AdaptationConfig::WPM_MIN;
    if (g_adaptation.currentWPM > AdaptationConfig::WPM_MAX)
        g_adaptation.currentWPM = AdaptationConfig::WPM_MAX;

#if SHOW_CW_HEADER
    cwViewShowWPM();
#endif
}

#else

static inline void adaptDotTiming(uint16_t markLength) {
    (void)markLength;
}

#endif

// ======================================================================
// ================ MORSE DECODING IMPLEMENTATION ======================
// ======================================================================

static inline void resetMorseAccumulator() {
    g_morseAccumulator.length = 0;
    g_morseAccumulator.pattern = 0;
}

static inline void addToMorsePattern(bool isDash) {
    if (g_morseAccumulator.length < SymbolConfig::MAX_MORSE_LEN) {
        g_morseAccumulator.pattern = (g_morseAccumulator.pattern << 1) | (isDash ? 1 : 0);
        g_morseAccumulator.length++;
    }
}

// convert found index from lookup table to ASCII char
static inline char indexToChar(uint8_t i) {
    if (i < MORSE_LETTERS) return 'A' + i;  // First 26 entries are A through Z
    return '0' + (i - MORSE_LETTERS);       // The rest are 0 through 9
}

// Searches lookup table for current pattern
static inline char lookupMorseCharacter() {
    for (uint8_t i = 0; i < MORSE_LUT_SIZE; i++) {
        uint8_t packedEntry = pgm_read_byte(&g_morseLookup[i]);

        if (CW_LEN(packedEntry) != g_morseAccumulator.length)
            continue;

        if (CW_BITS(packedEntry) == g_morseAccumulator.pattern)
            return indexToChar(i);
    }
    return 0;
}

// Handles special cases like (/) before calling main lookup
static inline char decodeMorseFromAccumulator() {
    if (g_morseAccumulator.length == 0) return 0;

    // slash character is common in callsigns but not in main lookup table
    if (g_morseAccumulator.length == 5 &&
        g_morseAccumulator.pattern == 18) {
        return '/';
    }

    return lookupMorseCharacter();
}

static void flushCurrentSymbol() {
    char decodedChar = 0;
    uint8_t len = g_morseAccumulator.length;

    if (len && len <= SymbolConfig::MAX_MORSE_LEN)
        decodedChar = decodeMorseFromAccumulator();

#if DEBUG_CW
    if (len) {
        debugOutputMorsePattern();
        if (decodedChar) {
            debugOutputDecodedChar(decodedChar);
        } else {
            debugPrintCRLF();
        }
    }
#endif

    resetMorseAccumulator();

    if (decodedChar) {
        cwViewPut(decodedChar);
        g_cwState.flushHold = 1; // brief pause prevents pattern corruption
    }
}

static inline void processDecodedEvent(cw_event_t event) {
    switch (event) {
    case CW_EVENT_DOT:
        if (!g_cwState.flushHold) addToMorsePattern(false);
        break;

    case CW_EVENT_DASH:
        if (!g_cwState.flushHold) addToMorsePattern(true);
        break;

    case CW_EVENT_LETTER_GAP:
        flushCurrentSymbol();
        break;

    case CW_EVENT_WORD_GAP:
        flushCurrentSymbol();
        cwViewPut(' ');
        break;

    default:
        break;
    }
}

// ======================================================================
// ================ STATE MACHINE IMPLEMENTATION =======================
// ======================================================================

// Reset all counters when transitioning states
static inline void resetStateOnTransition(cw_state_t newState) {
    g_cwState.runLength = 0;
    g_cwState.currentState = newState;
    g_cwState.gateCounter = 0;
    g_cwState.symbolTimeout = 0;
}

// Perform state transition and return classified event
static inline cw_event_t makeStateTransition(cw_state_t newState) {
    cw_event_t event = classifyCurrentEvent(
        g_cwState.currentState == CW_STATE_MARK);
    resetStateOnTransition(newState);
    return event;
}

// Initialize complete decoder state
static inline void initializeDecoderState() {
    g_freqTracker.binK = FreqTrackingConfig::K_DEFAULT;

    // Initialize control flags and counters
    g_cwState.initialized = 1;
    g_cwState.currentState = CW_STATE_SPACE;
    g_cwState.gateCounter = 0;
    g_cwState.flushHold = SymbolConfig::STARTUP_SKIP; // skip initial noise

    // Initialize signal processing parameters
    g_cwState.envelopeLevel = 0;
    g_cwState.noiseFloor = 0;
    g_cwState.dotLength = 3;
    g_cwState.runLength = 0;
    g_cwState.symbolTimeout = 0;

    resetMorseAccumulator();

    // Initialize adaptation
    g_adaptation.avgDotTime = 30;  // Start with 3.0 blocks (x10 for precision)
    g_adaptation.currentWPM = 0;
    g_adaptation.symbolsSinceUpdate = 0;
}

// Flush symbol on user pause for immediate screen feedback
static inline cw_event_t handleSymbolTimeout() {
    if (g_cwState.symbolTimeout > SymbolConfig::TIMEOUT_MAX &&
        g_morseAccumulator.length > 0) {
        flushCurrentSymbol();
        g_cwState.symbolTimeout = 0;
        return CW_EVENT_LETTER_GAP;
    }
    return CW_EVENT_NONE;
}

// Gate state transitions to filter signal flutter
static inline cw_event_t handleStateTransition(bool toneDetected) {
    cw_state_t newState = toneDetected ? CW_STATE_MARK : CW_STATE_SPACE;

    if (newState != g_cwState.currentState) {
        if (++g_cwState.gateCounter >= SymbolConfig::GATE_BLOCKS) {
            return makeStateTransition(newState);
        }
    } else {
        g_cwState.gateCounter = 0;
    }
    return CW_EVENT_NONE;
}

// ======================================================================
// ================ DEBUG IMPLEMENTATION ===============================
// ======================================================================

#if DEBUG_CW

// Helper for repeated pattern separator + number
static inline void debugPutCharWithNum(char sep, uint16_t num) {
    debugPutChar(sep);
    debugPrintNum(num);
}

// Output timing info
static inline void debugOutputTiming(cw_event_t event, uint16_t length) {
    debugPutChar(event);
    debugPutCharWithNum(':', length);
    debugPutCharWithNum('/', g_cwState.dotLength);
    debugPutChar(' ');
}

// Output WPM info
static inline void debugOutputWPM() {
    debugPrint_P(PSTR(" WPM:"));
    debugPrintNum(g_adaptation.currentWPM);
    debugPrint_P(PSTR(" AVG:"));
    debugPrintNum(g_adaptation.avgDotTime / 10);
    debugPutChar(' ');
}

// ADC debug output - minimal
static inline void debugOutputAdcInfo() {
    static uint16_t prevMin = 0xFFFF, prevMax = 0;
    static int16_t  prevDc = 0;
    static uint8_t  prevEnv = 0xFF, prevNoise = 0xFF;

    if (g_adcDebug.minAdc == prevMin &&
        g_adcDebug.maxAdc == prevMax &&
        g_dcOffset == prevDc &&
        g_cwState.envelopeLevel == prevEnv &&
        g_cwState.noiseFloor == prevNoise) {
        return;
    }

    prevMin = g_adcDebug.minAdc;
    prevMax = g_adcDebug.maxAdc;
    prevDc = g_dcOffset;
    prevEnv = g_cwState.envelopeLevel;
    prevNoise = g_cwState.noiseFloor;

    debugPrint_P(PSTR("ADC:"));
    debugPrintNum(g_adcDebug.minAdc);
    debugPutChar('-');
    debugPrintNum(g_adcDebug.maxAdc);
    debugPutCharWithNum('/', g_dcOffset);
    debugPutCharWithNum(' ', g_cwState.envelopeLevel);
    debugPutCharWithNum('/', g_cwState.noiseFloor);
    debugPrintCRLF();
}

// print bit pattern as dots and dashes
static inline void debugOutputMorseBits() {
    for (uint8_t i = 0; i < g_morseAccumulator.length; i++) {
        uint8_t bitPos = g_morseAccumulator.length - 1 - i;
        char symbol = ((g_morseAccumulator.pattern >> bitPos) & 1) ? '-' : '.';
        debugPutChar(symbol);
    }
}

// Output complete Morse pattern
static inline void debugOutputMorsePattern() {
    if (g_morseAccumulator.length > 0) {
        debugPutChar('P');
        debugPutChar(':');
        debugOutputMorseBits();
        debugPutChar('=');
    }
}

static inline void debugOutputDecodedChar(char chr) {
    if (!chr) return;
    debugPutChar(chr);
    debugPrintCRLF();
}

#endif

// ======================================================================
// ================ MAIN DECODER IMPLEMENTATION ========================
// ======================================================================

// Initialize decoder on first use
static inline void CWDecoder_begin() {
    adcSetInternalVref();
    adcSeedDcOffset();
    initializeDecoderState();
}

// Main decoder poll processes one audio block per call
static inline cw_event_t CWDecoder_poll() {
    if (!g_cwState.initialized) return CW_EVENT_NONE;

    uint8_t magnitude = sampleAndAnalyzeSignal();
    updateEnvelopeAndNoise(magnitude);

    if (g_cwState.flushHold) return CW_EVENT_NONE;

    bool toneDetected = detectToneWithHysteresis();

    // Increment run length and symbol timeout timers
    g_cwState.runLength++;
    g_cwState.symbolTimeout++;

    cw_event_t event = handleSymbolTimeout();
    if (event != CW_EVENT_NONE) return event;

    return handleStateTransition(toneDetected);
}

// ======================================================================
// ================ VIEW MANAGEMENT IMPLEMENTATION =====================
// ======================================================================

static inline void cwViewTask() {
    cw_event_t event;
    while ((event = CWDecoder_poll()) != CW_EVENT_NONE) {
        processDecodedEvent(event);
    }
}

static void cwViewEnter() {
    oled.clear();
    cwViewDrawHeader();
    cwViewHomeAndClear();
    resetMorseAccumulator();

#if DEBUG_CW
    initDebugUART();
    debugPrint_P(PSTR("CW Debug Start\r\n"));
#endif

    CWDecoder_begin();
}

static void cwViewExit() {
    oled.clear();
    showStatus(true);
}

#endif  // ENABLE_CW_DECODER
