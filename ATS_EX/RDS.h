#pragma once

// RDS.h — Minimal RDS RadioText + Clock Time Decoder
//
// Decodes Group 2A/2B (RadioText) and Group 4A (CT) from FM broadcasts
// Auto-scrolls text in a 16-char window (right 5 chars reserved for clock)
// Clears instantly on frequency change, A/B flag flip, or signal loss
// Toggled via long-press MODE in FM mode
// Uses only rdsQueryMini() from SI4735_fixed.h (no heavy PU2CLR RDS)

#include "Arduino.h"
#include "Defines.h"
#include "Globals.h"
#include "SSD1306_OLED.h"

#if ENABLE_RDS_MINI

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// =-=-=-=-=-=-=-=-= RDS configuration =-=-=-=-=-=-=-=-=

#define RDS_ROW             6
#define RDS_FULL_WIN        21
#define RDS_RT_MAX          32
#define RDS_SCROLL_GAP      3

#define RDS_POLL_MS         400
#define RDS_SETTLE_MS       900
#define RDS_TIMEOUT_MS      6000
#define RDS_SCROLL_MS       350

#define RDS_CLK_COLS        5
#define RDS_RT_WIN          ((uint8_t)(RDS_FULL_WIN - RDS_CLK_COLS))

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
static uint16_t s_freq;
static char     s_rt[RDS_RT_MAX];
static uint8_t  s_hh, s_mm;

// =-=-=-=-=-=-=-=-= Time arithmetic helpers =-=-=-=-=-=-=-=-=

// 16-bit millisecond delta with proper wrap handling (~65s period)
static inline __attribute__((always_inline))
uint16_t rdsElapsed(uint16_t now, uint16_t since) {
    return (uint16_t)(now - since);
}

// =-=-=-=-=-=-=-=-= State query helpers =-=-=-=-=-=-=-=-=

// Returns true when any valid RDS data has been received
static inline __attribute__((always_inline)) bool rdsHasData() {
    return s_ok != 0;
}

// Returns true when valid clock time has been received
static inline __attribute__((always_inline)) bool rdsHasValidClock() {
    return s_hh != RDS_NO_VALUE;
}

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

// Format "HH:MM" into 5-char buffer (no null terminator)
static inline __attribute__((always_inline))
void fmtTime5(char* dst, uint8_t hh, uint8_t mm) {
    fmtDigit2(dst, hh);
    dst[2] = ':';
    fmtDigit2(dst + 3, mm);
}

// =-=-=-=-=-=-=-=-= Block B field extractors =-=-=-=-=-=-=-=-=

static inline __attribute__((always_inline)) uint8_t rdsGroupType(uint16_t bB) { return (uint8_t)(bB >> 12); }
static inline __attribute__((always_inline)) uint8_t rdsVer(uint16_t bB) { return (uint8_t)((bB >> 11) & 1); }
static inline __attribute__((always_inline)) uint8_t rdsAB(uint16_t bB) { return (uint8_t)bB & 0x10; }
static inline __attribute__((always_inline)) uint8_t rdsAdr(uint16_t bB) { return (uint8_t)(bB & 0x0F); }
static inline __attribute__((always_inline)) bool rdsIsGroup2(uint16_t bB) { return rdsGroupType(bB) == RDS_GROUP2_TYPE; }
// group type 4 + version A combined into bits[15:11]
static inline __attribute__((always_inline)) bool rdsIsGroup4A(uint16_t bB) { return (uint8_t)(bB >> 11) == RDS_GROUP4A_TAG; }

// =-=-=-=-=-=-=-=-= Display output =-=-=-=-=-=-=-=-=

// Fill 16-char RT zone into buffer at offset 0
static inline __attribute__((always_inline))
void rdsFillRT(char* buf) {
    uint8_t remain = (s_len > s_scrl) ? (s_len - s_scrl) : 0;
    const char* r = &s_rt[s_scrl];

    for (uint8_t i = 0; i < RDS_RT_WIN; i++)
        *buf++ = (i < remain) ? *r++ : ' ';
}

// Fill 5-char clock zone into buffer
static inline __attribute__((always_inline))
void rdsFillClock(char* clk) {
    if (!rdsHasData()) {
        memset(clk, ' ', RDS_CLK_COLS);
    } else if (rdsHasValidClock()) {
        fmtTime5(clk, s_hh, s_mm);
    } else {
        clk[0] = clk[1] = clk[3] = clk[4] = '-';
        clk[2] = ':';
    }
}

// Draw entire RDS row (RT + Clock) in a single pass
// One buffer + one oled.print() = one I2C burst for the whole row
static void __attribute__((noinline)) rdsRedraw() {
    char buf[RDS_FULL_WIN + 1];
    rdsFillRT(buf);
    rdsFillClock(&buf[RDS_RT_WIN]);
    buf[RDS_FULL_WIN] = '\0';
    oled.setCursor(0, RDS_ROW);
    oled.print(buf);
}

// =-=-=-=-=-=-=-=-= Buffer management =-=-=-=-=-=-=-=-=

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

// Clears all RDS content (RT + clock) but keeps HW state
static void __attribute__((noinline)) rdsClearContent() {
    rdsClearRT();
    s_hh = RDS_NO_VALUE;
    s_mm = RDS_NO_VALUE;
    s_ok = 0;
}

// Full reset: content + HW state + timers
static void __attribute__((noinline)) rdsResetAll() {
    rdsClearContent();
    s_hw = 0;
    s_poll = 0;
    s_st = 0;
}

// Clears content and refreshes display — common pattern for freq change and sync loss
static void __attribute__((noinline)) rdsClearAndRedraw() {
    rdsClearContent();
    rdsRedraw();
}

// Full reset with display refresh — used when deactivating or toggling
static void __attribute__((noinline)) rdsResetAndRedraw() {
    rdsResetAll();
    rdsRedraw();
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

// RT scrolls only within its 16-char window
// RDS_SCROLL_GAP adds visual pause between cycles so repeated text doesnt blur
static inline void rdsAdvanceScroll(uint16_t n) {
    if (s_len <= RDS_RT_WIN) {
        s_scrl = 0;
        return;
    }
    if (rdsElapsed(n, s_st) >= RDS_SCROLL_MS) {
        s_st = n;
        if (++s_scrl >= (uint8_t)(s_len + RDS_SCROLL_GAP)) s_scrl = 0;
    }
}

// =-=-=-=-=-=-=-=-= RDS char decode =-=-=-=-=-=-=-=-=

static bool __attribute__((noinline)) rdsStoreChars(uint8_t pos, const uint8_t* data, uint8_t n) {
    bool changed = false;
    char* dst = &s_rt[pos];

    while (n--) {
        char c = *data++;
        if (c < RDS_CTRL_CHAR_MAX) c = ' '; // control chars cant be displayed

        if (*dst != c) {
            *dst = c;
            changed = true;
        }
        dst++;
    }
    return changed;
}

// 2A carries 4 chars per group (blocks C+D), 2B only 2 (block D only)
static inline bool rdsDecodeGroup2Chars(uint16_t bB) {
    const uint8_t* cd = g_si4735.rdsBlockCDPtr();
    uint8_t v = rdsVer(bB);
    uint8_t a = rdsAdr(bB);
    if (!v && a < RDS_2A_MAX_ADR)
        return rdsStoreChars((uint8_t)(a << 2), cd, RDS_2A_CHARS);
    if (v && a < RDS_2B_MAX_ADR)
        return rdsStoreChars((uint8_t)(a << 1), cd + 2, RDS_2B_CHARS);
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
    *hh = h;
    *mm = (uint8_t)((dh & 0x0F) << 2) | (uint8_t)(dl >> 6);
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
    if (!rdsHasData()) return false;
    uint16_t ft = (uint16_t)g_lastFreqChange;
    return (int16_t)(s_ok - ft) < 0 || rdsElapsed(n, s_ok) > RDS_TIMEOUT_MS;
}

// 16-bit ms wraps at ~65s — enough for poll/settle windows on AVR
static inline bool rdsIsTooEarly(uint16_t n) {
    if (rdsElapsed(n, s_poll) < RDS_POLL_MS) return true;
    s_poll = n;
    if (g_processFreqChange) return true;
    // PLL needs time to lock after frequency change before RDS data becomes valid
    return rdsElapsed(n, (uint16_t)g_lastFreqChange) < RDS_SETTLE_MS;
}

static inline bool rdsHasSignal() {
    return g_si4735.getRdsReceived() && g_si4735.getRdsSync();
}

// =-=-=-=-=-=-=-=-= Frequency change detection =-=-=-=-=-=-=-=-=

// Instant wipe when tuning or seek changes frequency
// Prevents stale text from previous station lingering on screen
static inline bool rdsCheckFreqChanged() {
    if (s_freq == g_currentFrequency) return false;
    s_freq = g_currentFrequency;
    rdsClearAndRedraw();
    return true;
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
    if (!s_hw) return;
    rdsDisableHw();
    rdsResetAndRedraw();
}

// Only power up RDS when user enables it — saves I2C traffic while browsing bands
static inline bool rdsActivateHw() {
    if (s_hw) return false;
    rdsEnableHw();
    rdsClearAndRedraw();
    return true;
}

// Wipe display when station stops sending RDS or user tunes away
static inline bool rdsHandleSyncLoss(uint16_t n) {
    if (rdsHasSignal()) return false;
    if (rdsIsStale(n)) rdsClearAndRedraw();
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
    rdsResetAndRedraw();
}

// Takes uint16_t — upper 16 bits of millis() never needed at these intervals
void __attribute__((noinline)) rdsMiniTask(uint16_t now16) {
    if (rdsIsGated())               return;
    if (rdsIsWrongMode()) { rdsDeactivateHw(); return; }
    if (rdsCheckFreqChanged())      return;

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
