#pragma once

// RDS.h — Minimal RDS RadioText + Clock Time Decoder
//
// Decodes Group 2A/2B (RadioText) and Group 4A (CT) from FM broadcasts
// Auto-scrolls text longer than 21 chars (screen width)
// Clears on A/B flag change, signal loss or frequency change
// Toggled via long-press MODE in FM mode
// Uses only rdsQueryMiniNoCheck() from SI4735_fixed.h (no heavy PU2CLR RDS)

#include "Arduino.h"
#include "Defines.h"
#include "Globals.h"
#include "SSD1306_OLED.h"

#if ENABLE_RDS_MINI

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// =-=-=-=-=-=-=-=-= RDS configuration =-=-=-=-=-=-=-=-=

#define RDS_ROW             6
#define RDS_WIN             21
#define RDS_RT_MAX          32
#define RDS_SCROLL_GAP      3
#define RDS_CHAR_WIDTH      6

#define RDS_POLL_MS         400
#define RDS_SETTLE_MS       900
#define RDS_TIMEOUT_MS      6000
#define RDS_SCROLL_MS       350

#define RDS_CLK_COLS        5
#define RDS_CLK_COL         ((uint8_t)(RDS_WIN - RDS_CLK_COLS))
#define RDS_CLK_X           ((uint8_t)(RDS_CLK_COL * RDS_CHAR_WIDTH))

#define RDS_NO_VALUE        0xFF
#define RDS_CTRL_CHAR_MAX   32

#define RDS_GROUP2_TYPE     2
#define RDS_GROUP4A_TAG     8

#define RDS_2A_MAX_ADR      8
#define RDS_2A_CHARS        4
#define RDS_2B_MAX_ADR      16
#define RDS_2B_CHARS        2

#define RDS_HOUR_MAX        24
#define RDS_MINUTE_MAX      60

// =-=-=-=-=-=-=-=-= RDS state =-=-=-=-=-=-=-=-=

static bool     s_on;
static uint8_t  s_hw, s_ab;
static uint8_t  s_len, s_scrl;
static uint16_t s_poll, s_ok, s_st;
static char     s_rt[RDS_RT_MAX];
static uint8_t  s_hh, s_mm;

// =-=-=-=-=-=-=-=-= AVR and formatting helpers =-=-=-=-=-=-=-=-=

// AVR-GCC promotes uint8_t shifts to 16-bit — inline asm avoids that
static inline __attribute__((always_inline)) uint8_t avrSwapNibbles(uint8_t v) {
    asm("swap %0" : "+r"(v));
    return v;
}

static inline __attribute__((always_inline))
void fmtDigit2(char* dst, uint8_t val) {
    DivMod10 d = divmod10_u8(val);
    dst[0] = (char)('0' + d.q);
    dst[1] = (char)('0' + d.r);
}

static inline __attribute__((always_inline))
void fmtSpaces(char* dst, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) dst[i] = ' ';
}

// =-=-=-=-=-=-=-=-= Block B field extractors =-=-=-=-=-=-=-=-=

static inline __attribute__((always_inline)) uint8_t rdsGroupType(uint16_t bB) { return (uint8_t)(bB >> 12); }
static inline __attribute__((always_inline)) uint8_t rdsVer(uint16_t bB) { return (uint8_t)((bB >> 11) & 1); }
static inline __attribute__((always_inline)) uint8_t rdsAB(uint16_t bB) { return (uint8_t)bB & 0x10; }
static inline __attribute__((always_inline)) uint8_t rdsAdr(uint16_t bB) { return (uint8_t)(bB & 0x0F); }
static inline __attribute__((always_inline)) bool rdsIsGroup2(uint16_t bB) { return rdsGroupType(bB) == RDS_GROUP2_TYPE; }
// group type 4 + version A combined into bits[15:11]
static inline __attribute__((always_inline)) bool rdsIsGroup4A(uint16_t bB) { return (uint8_t)(bB >> 11) == RDS_GROUP4A_TAG; }

// =-=-=-=-=-=-=-=-= HW control and buffer management =-=-=-=-=-=-=-=-=

// noinline — called from two places, saves ~8 bytes vs inlined duplicates
static void __attribute__((noinline)) rdsDisableHw() {
    siSetProperty(FM_RDS_CONFIG, 0);
}

static inline void rdsEnableHw() {
    g_si4735.rdsEnableMini();
    s_hw = 1;
}

// RDS spec mandates full wipe when A/B flag flips — station started new message
static void __attribute__((noinline)) rdsClearRT() {
    memset(s_rt, ' ', RDS_RT_MAX);
    s_len = 0;
    s_scrl = 0;
    s_ab = RDS_NO_VALUE;
}

static inline void rdsClearClock() {
    s_hh = RDS_NO_VALUE;
    s_mm = RDS_NO_VALUE;
}

static void __attribute__((noinline)) rdsResetAll() {
    rdsClearRT();
    rdsClearClock();
    s_hw = 0;
    s_poll = 0;
    s_ok = 0;
    s_st = 0;
}

static inline void rdsClearAll() {
    rdsClearRT();
    rdsClearClock();
    s_ok = 0;
}

// =-=-=-=-=-=-=-=-= RT scroll logic =-=-=-=-=-=-=-=-=

static inline void rdsTrimRT() {
    uint8_t l = RDS_RT_MAX;
    while (l && s_rt[l - 1] == ' ') --l;
    s_len = l;
}

static inline void rdsResetScroll(uint16_t n) {
    s_scrl = 0;
    s_st = n;
}

// RDS_SCROLL_GAP adds visual pause between cycles so repeated text doesnt blur
static inline void rdsAdvanceScroll(uint16_t n) {
    if (s_len <= RDS_WIN) {
        s_scrl = 0;
        return;
    }
    if ((uint16_t)(n - s_st) >= (uint16_t)RDS_SCROLL_MS) {
        s_st = n;
        if (++s_scrl >= (uint8_t)(s_len + RDS_SCROLL_GAP)) s_scrl = 0;
    }
}

// =-=-=-=-=-=-=-=-= RDS char decode =-=-=-=-=-=-=-=-=

static bool __attribute__((noinline)) rdsStoreChars(uint8_t pos, const uint8_t* data, uint8_t n) {
    bool changed = false;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t c = data[i];
        if (c < RDS_CTRL_CHAR_MAX) c = ' '; // control chars cant be displayed
        uint8_t p = pos + i;
        if (s_rt[p] != (char)c) {
            s_rt[p] = (char)c;
            changed = true;
        }
    }
    return changed;
}

// 2A carries 4 chars per group (blocks C+D), 2B only 2 (block D only)
static inline bool rdsDecodeGroup2Chars(uint16_t bB) {
    uint8_t v = rdsVer(bB);
    uint8_t a = rdsAdr(bB);
    if (!v && a < RDS_2A_MAX_ADR)
        return rdsStoreChars((uint8_t)(a << 2), g_si4735.rdsBlockCDPtr(), RDS_2A_CHARS);
    if (v && a < RDS_2B_MAX_ADR)
        return rdsStoreChars((uint8_t)(a << 1), g_si4735.rdsBlockCDPtr() + 2, RDS_2B_CHARS);
    return false;
}

// Hour spans DH/CL boundary, minute spans DL/DH — per RDS standard bit layout
// Local offset math skipped — costs too much flash on AVR for minimal benefit
static inline void rdsExtractCT(uint8_t* hh, uint8_t* mm) {
    const uint8_t dl = g_si4735.rdsGetBlockDL();
    const uint8_t dh = g_si4735.rdsGetBlockDH();
    const uint8_t cl = g_si4735.rdsGetBlockCL();
    uint8_t h = avrSwapNibbles(dh) & 0x0F;
    if (cl & 0x01) h |= 0x10;
    uint8_t m = (uint8_t)(dh & 0x0F);
    m <<= 1;
    m <<= 1;
    m |= (uint8_t)(dl >> 6);
    *hh = h;
    *mm = m;
}

// =-=-=-=-=-=-=-=-= Display output =-=-=-=-=-=-=-=-=

static void __attribute__((noinline)) rdsDrawRT() {
    uiScrollPrint21AtRow(oled, RDS_ROW, s_rt, s_len, s_scrl);
}

static void __attribute__((noinline)) rdsDrawClock() {
    char t[RDS_CLK_COLS + 1];
    if (s_hh == RDS_NO_VALUE || s_mm == RDS_NO_VALUE) {
        fmtSpaces(t, RDS_CLK_COLS);
    } else {
        fmtDigit2(t, s_hh);
        t[2] = ':';
        fmtDigit2(t + 3, s_mm);
    }
    t[RDS_CLK_COLS] = 0;
    oled.setCursor(RDS_CLK_X, RDS_ROW);
    oled.print(t);
}

// RT scrolls on left side, clock pinned to right edge — both on same OLED row
static void __attribute__((noinline)) rdsRedraw() {
    rdsDrawRT();
    rdsDrawClock();
}

// =-=-=-=-=-=-=-=-= Gating and timing =-=-=-=-=-=-=-=-=

static inline bool rdsIsGated() {
    if (g_settingsActive || !g_displayOn || !s_on) return true;
#if ENABLE_FAVORITES
    if (g_favoritesActive) return true;
#endif
    return false;
}

// SI4735 ignores RDS commands outside FM band
static inline bool rdsIsWrongMode() {
    return g_currentMode != FM;
}

// Freq changed after last decode OR no data for RDS_TIMEOUT_MS — station gone
static inline bool rdsIsStale(uint16_t n) {
    uint16_t ft = (uint16_t)g_lastFreqChange;
    return s_ok && ((int16_t)(s_ok - ft) < 0 || (uint16_t)(n - s_ok) > (uint16_t)RDS_TIMEOUT_MS);
}

// 16-bit ms wraps at ~65s — enough for poll/settle windows on AVR
static inline bool rdsIsTooEarly(uint16_t n) {
    if ((uint16_t)(n - s_poll) < (uint16_t)RDS_POLL_MS) return true;
    s_poll = n;
    if (g_processFreqChange) return true;
    // PLL needs time to lock after frequency change before RDS data becomes valid
    if ((uint16_t)(n - (uint16_t)g_lastFreqChange) < (uint16_t)RDS_SETTLE_MS) return true;
    return false;
}

static inline bool rdsHasSignal() {
    return g_si4735.getRdsReceived() && g_si4735.getRdsSync();
}

// =-=-=-=-=-=-=-=-= Group processors =-=-=-=-=-=-=-=-=

static inline void rdsProcessGroup2(uint16_t bB, uint16_t n) {
    if (!rdsIsGroup2(bB)) return;
    uint8_t ab = rdsAB(bB);
    if (s_ab != ab) { rdsClearRT(); s_ab = ab; }
    if (rdsDecodeGroup2Chars(bB)) {
        rdsTrimRT();
        rdsResetScroll(n);
    }
}

static inline void rdsProcessGroup4A(uint16_t bB) {
    if (!rdsIsGroup4A(bB)) return;
    uint8_t hh, mm;
    rdsExtractCT(&hh, &mm);
    if (hh < RDS_HOUR_MAX && mm < RDS_MINUTE_MAX) {
        s_hh = hh;
        s_mm = mm;
    }
}

// =-=-=-=-=-=-=-=-= Task state transitions =-=-=-=-=-=-=-=-=

static inline void rdsDeactivateHw() {
    if (s_hw) {
        rdsDisableHw();
        rdsResetAll();
        rdsRedraw();
    }
}

// Only power up RDS when user enables it — saves I2C traffic while browsing bands
static inline bool rdsActivateHw() {
    if (!s_hw) {
        rdsEnableHw();
        s_ok = 0;
        rdsClearRT();
        rdsRedraw();
        return true;
    }
    return false;
}

// Wipe display when station stops sending RDS or user tunes away
static inline bool rdsHandleSyncLoss(uint16_t n) {
    if (rdsHasSignal()) return false;
    if (rdsIsStale(n)) {
        rdsClearAll();
        rdsRedraw();
    }
    return true;
}

// s_ok never zero when valid — zero reserved as "no data yet" sentinel
static inline void rdsMarkValid(uint16_t n) {
    s_ok = (uint16_t)(n | 1);
}

// =-=-=-=-=-=-=-=-= Public API =-=-=-=-=-=-=-=-=

__attribute__((noinline)) bool rdsMiniUiEnabled() { return s_on; }

// Frees I2C bus when toggling off so other peripherals can communicate
void __attribute__((noinline)) rdsMiniToggleUi() {
    s_on = !s_on;
    if (!s_on && s_hw) rdsDisableHw();
    rdsResetAll();
    if (!s_on) rdsRedraw();
}

// Takes uint16_t — upper 16 bits of millis() never needed at these intervals
void __attribute__((noinline)) rdsMiniTask(uint16_t now16) {
    if (rdsIsGated())     return;
    if (rdsIsWrongMode()) { rdsDeactivateHw(); return; }
    const uint16_t n = now16;
    if (rdsIsTooEarly(n))           return;
    if (rdsActivateHw())            return;
    if (!g_si4735.rdsQueryMini())   return;
    if (rdsHandleSyncLoss(n))       return;
    rdsMarkValid(n);
    uint16_t bB = g_si4735.rdsGetBlockB();
    rdsProcessGroup2(bB, n);
    rdsProcessGroup4A(bB);
    rdsAdvanceScroll(n);
    rdsRedraw();
}

#else

inline bool rdsMiniUiEnabled() { return false; }
inline void rdsMiniToggleUi() {}
inline void rdsMiniTask(uint16_t) {}

#endif
