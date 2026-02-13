#pragma once

// ====================================================================================
//
// RDS.h — Minimal RDS RadioText Decoder
//
// RDS that decodes Group 2A/2B (RadioText) from FM broadcasts
// and displays it by scrolling text
//
//  - Decodes RT Group 2A (4 chars/packet) and 2B (2 chars/packet)
//  - Auto-scrolls text longer than 21 chars (screen width)
//  - Clears on A/B flag change, signal loss, or frequency change
//  - Toggled via long-press MODE in FM mode
//  - Uses only rdsQueryMini() from SI4735_fixed.h (no heavy PU2CLR RDS)
//
// Flash cost +/- 500 bytes. Does NOT pull setRdsConfig/getRdsStatus/getRdsText
//
// ====================================================================================

#include "Arduino.h"
#include "Defines.h"
#include "Globals.h"
#include "SSD1306_OLED.h"

#if ENABLE_RDS_MINI

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// ==========================================
// ===== CONFIGURATION ======================
// ==========================================

#define RDS_ROW           6       // OLED text row for RT display
#define RDS_WIN           21      // visible character window (128px / 6px)
#define RDS_RT_MAX        32      // max RadioText buffer length
#define RDS_GAP           3       // blank chars between scroll cycles
#define RDS_AB_INIT       0xFF    // sentinel for uninitialized A/B flag

#define RDS_POLL_MS       400     // min interval between I2C polls
#define RDS_SETTLE_MS     900     // quiet time after frequency change
#define RDS_TIMEOUT_MS    6000    // clear text after signal loss
#define RDS_SCROLL_MS     350     // scroll step interval

// ==========================================
// ===== STATE ==============================
// ==========================================

static bool     s_on;                   // UI enabled flag
static uint8_t  s_hw, s_ab;             // HW init flag, current A/B flag
static uint8_t  s_len, s_scrl;          // trimmed text length, scroll position
static uint16_t s_poll, s_ok, s_st;     // timestamps: last poll, last valid, last scroll
static char     s_rt[RDS_RT_MAX];       // RadioText buffer

// ==========================================
// ===== BLOCK B FIELD EXTRACTORS ===========
// ==========================================

static inline __attribute__((always_inline)) bool    rdsIsGroup2(uint16_t bB) { return (bB & 0xF000) == 0x2000; }
static inline __attribute__((always_inline)) uint8_t rdsAB(uint16_t bB) { return (uint8_t)((bB >> 4) & 1); }
static inline __attribute__((always_inline)) uint8_t rdsVer(uint16_t bB) { return (uint8_t)((bB >> 11) & 1); }
static inline __attribute__((always_inline)) uint8_t rdsAdr(uint16_t bB) { return (uint8_t)(bB & 0x0F); }

// ==========================================
// ===== BUFFER MANAGEMENT ==================
// ==========================================

// Reset RT buffer and scroll state; called on A/B flip, timeout, freq change
static void __attribute__((noinline)) rdsClr() {
    memset(s_rt, ' ', RDS_RT_MAX);
    s_len = 0;
    s_scrl = 0;
    s_ab = RDS_AB_INIT;
}

// Full state reset including HW flag; called on toggle and mode switch
static void __attribute__((noinline)) rdsRst() {
    rdsClr();
    s_hw = 0;
    s_poll = 0;
    s_ok = 0;
    s_st = 0;
}

// ==========================================
// ===== CHARACTER STORE ====================
// ==========================================

// Store one decoded character; returns true if buffer changed
// Non-printable chars replaced with space
static bool __attribute__((noinline)) rdsS(uint8_t p, uint8_t c) {
    if (c < 32) c = ' ';
    char cc = (char)c;
    if (s_rt[p] == cc) return false;
    s_rt[p] = cc;
    return true;
}

// ==========================================
// ===== DISPLAY ============================
// ==========================================

// Render visible window of RT buffer to OLED row
// Single oled.print() call minimizes I2C traffic
static void __attribute__((noinline)) rdsDrw() {
    char b[RDS_WIN + 1];
    memset(b, ' ', RDS_WIN);
    b[RDS_WIN] = 0;

    uint8_t start = s_scrl;
    uint8_t len = s_len;

    if (start < len) {
        uint8_t n = (uint8_t)(len - start);
        if (n > RDS_WIN) n = RDS_WIN;
        memcpy(b, &s_rt[start], n);
    }

    oled.setCursor(0, RDS_ROW);
    oled.print(b);
}

// ==========================================
// ===== PUBLIC API =========================
// ==========================================

inline bool rdsMiniUiEnabled() { return s_on; }

// Toggle RDS display on/off; disables HW when turning off
void __attribute__((noinline)) rdsMiniToggleUi() {
    s_on = !s_on;
    if (!s_on && s_hw) g_si4735.setProperty(FM_RDS_CONFIG, 0);
    rdsRst();
    if (!s_on) rdsDrw();
}

// ==========================================
// ===== TASK HELPERS =======================
// ==========================================

// Check if RDS task should be skipped (UI blocked or not FM)
static inline bool rdsGated() {
    if (g_settingsActive || !g_displayOn || !s_on) return true;
#if ENABLE_FAVORITES
    if (g_favoritesActive) return true;
#endif
    return false;
}

// Disable RDS HW when leaving FM mode
static inline bool rdsCheckMode() {
    if (g_currentMode != FM) {
        if (s_hw) {
            g_si4735.setProperty(FM_RDS_CONFIG, 0);
            rdsRst();
            rdsDrw();
        }
        return true;
    }
    return false;
}

// Check if buffer is stale (freq changed or timeout)
static inline bool rdsIsStale(uint16_t n) {
    uint16_t ft = (uint16_t)g_lastFreqChange;
    return s_ok && ((int16_t)(s_ok - ft) < 0 || (uint16_t)(n - s_ok) > (uint16_t)RDS_TIMEOUT_MS);
}

// Decode Group 2A/2B and store chars
static inline bool rdsDecode(uint16_t bB) {
    uint8_t v = rdsVer(bB);
    uint8_t a = rdsAdr(bB);
    bool ch = false;

    if (!v && a < 8) {
        uint8_t p = (uint8_t)(a << 2);
        ch = rdsS(p, g_si4735.rdsGetBlockCH());
        ch |= rdsS(p + 1, g_si4735.rdsGetBlockCL());
        ch |= rdsS(p + 2, g_si4735.rdsGetBlockDH());
        ch |= rdsS(p + 3, g_si4735.rdsGetBlockDL());
    } else if (v && a < 16) {
        uint8_t p = (uint8_t)(a << 1);
        ch = rdsS(p, g_si4735.rdsGetBlockDH());
        ch |= rdsS(p + 1, g_si4735.rdsGetBlockDL());
    }

    return ch;
}

// Trim trailing spaces and update length
static inline void rdsTrim() {
    uint8_t l = RDS_RT_MAX;
    while (l && s_rt[l - 1] == ' ') --l;
    s_len = l;
}

// Advance scroll position if text exceeds window
static inline void rdsScroll(uint16_t n) {
    if (s_len > RDS_WIN) {
        if ((uint16_t)(n - s_st) >= (uint16_t)RDS_SCROLL_MS) {
            s_st = n;
            if (++s_scrl >= (uint8_t)(s_len + RDS_GAP)) s_scrl = 0;
        }
    } else {
        s_scrl = 0;
    }
}

// ==========================================
// ===== MAIN TASK ==========================
// ==========================================

// Main RDS task — call from loop()
// Handles gating, HW init, I2C polling, Group 2 decoding, and scroll
void rdsMiniTask() {
    if (rdsGated()) return;
    if (rdsCheckMode()) return;

    const uint32_t now32 = millis();
    const uint16_t n = (uint16_t)now32;

    // respect poll interval
    if ((uint16_t)(n - s_poll) < (uint16_t)RDS_POLL_MS) return;
    s_poll = n;

    // wait for frequency to stabilize
    if (g_processFreqChange) return;
    if (now32 - g_lastFreqChange < (uint32_t)RDS_SETTLE_MS) return;

    // HW init on first poll after enable
    if (!s_hw) {
        g_si4735.rdsEnableMini();
        s_hw = 1;
        s_ok = 0;
        rdsClr();
        rdsDrw();
        return;
    }

    // I2C query
    if (!g_si4735.rdsQueryMini()) return;

    // sync check with timeout or frequency change
    if (!g_si4735.getRdsReceived() || !g_si4735.getRdsSync()) {
        if (rdsIsStale(n)) {
            s_ok = 0;
            rdsClr();
            rdsDrw();
        }
        return;
    }

    s_ok = (uint16_t)(n | 1);

    // decode Group 2 (RadioText)
    uint16_t bB = g_si4735.rdsGetBlockB();
    if (rdsIsGroup2(bB)) {
        uint8_t ab = rdsAB(bB);
        if (s_ab != ab) { rdsClr(); s_ab = ab; }

        if (rdsDecode(bB)) {
            rdsTrim();
            s_scrl = 0;
            s_st = n;
        }
    }

    rdsScroll(n);
    rdsDrw();
}

#else

// ==========================================
// ===== STUBS (RDS disabled) ===============
// ==========================================

inline bool rdsMiniUiEnabled() { return false; }
inline void rdsMiniToggleUi() {}
inline void rdsMiniTask() {}

#endif
