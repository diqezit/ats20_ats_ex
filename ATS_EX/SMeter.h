// ----------------------------------------------------------------------
// SMeter.h - S-Meter RSSI to S-level string and signal bar
// ----------------------------------------------------------------------

#pragma once

#include <avr/pgmspace.h>
#include "Defines.h"
#include "Globals.h"
#include "SSD1306_OLED.h"

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// =-=-=-=-=-=-=-=-= Constants =-=-=-=-=-=-=-=-=

static constexpr uint8_t SM_S9 = 9;             // S9 threshold for S-meter
static constexpr uint8_t SM_NO_RSSI = 255;      // means no signal received
static constexpr uint8_t SM_BAR_Y = 8;          // page 1 start (y=8..15)
static constexpr uint8_t SM_BAR_MASK = 0x0C;    // 2px bar at y=10..11
static constexpr uint8_t SM_BAR_BASE = 0x08;    // 1px baseline at y=11
static constexpr uint8_t SM_BAR_W = 128;        // bar max width = screen width
static constexpr uint8_t SM_BAR_SCALE = 49;     // strength range 0..49

// =-=-=-=-=-=-=-=-= Interpolation Tables =-=-=-=-=-=-=-=-=

// AM thresholds: first 15 entries double as HF S-meter thresholds (S0..S9+50)
static const uint8_t AM_THR[] PROGMEM = {
    1, 2, 3, 4, 10, 16, 22, 28, 34, 44, 54, 64, 74, 84, 94, 95, 96
};
static const uint8_t AM_VAL[] PROGMEM = {
    1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49
};
static const uint8_t FM_THR[] PROGMEM = {
    1, 2, 8, 14, 24, 34, 44, 54, 64, 74, 76, 77
};
static const uint8_t FM_VAL[] PROGMEM = {
    1, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49
};

// S-meter HF reuses AM_THR[0..14]
static constexpr uint8_t LEN_HF = 15;

struct SmTable {
    const uint8_t* thr;
    const uint8_t* val;
    uint8_t        len;
};

// Pick AM or FM strength table based on current mode
static inline SmTable smGetStrengthTable() {
    SmTable t;
    if (g_currentMode == FM) {
        t.thr = FM_THR;
        t.val = FM_VAL;
        t.len = (uint8_t)ARRAY_SIZE(FM_THR);
    } else {
        t.thr = AM_THR;
        t.val = AM_VAL;
        t.len = (uint8_t)ARRAY_SIZE(AM_THR);
    }
    return t;
}

// =-=-=-=-=-=-=-=-= Helpers =-=-=-=-=-=-=-=-=

static inline bool smNoSignal(uint8_t rssi) {
    return rssi == SM_NO_RSSI;
}

static inline uint8_t smPgm(const uint8_t* p, uint8_t i) {
    return pgm_read_byte(&p[i]);
}

// Scan PROGMEM threshold array for first entry >= rssi
// Returns len when rssi is above all thresholds
static inline uint8_t smScanIndex(
    uint8_t rssi,
    const uint8_t* thr,
    uint8_t len) {
    uint8_t i = 0;
    while (i < len && rssi > smPgm(thr, i)) ++i;
    return i;
}

// Rounded integer division  round(a * b / c) without float
// Used for table interpolation and bar width scaling
static inline uint8_t smLerp(uint8_t a, uint8_t b, uint8_t c) {
    return (uint8_t)(((uint16_t)a * b + (c >> 1)) / c);
}

// Send n identical bytes to OLED in active data block
static inline void smSendRun(uint8_t n, uint8_t val) {
    while (n--) oled.sendByte(val);
}

static inline void smBlank(char* buf) {
    buf[0] = ' ';
    buf[1] = ' ';
    buf[2] = ' ';
    buf[3] = '\0';
}

static inline void smFormat(char* buf, uint8_t s, bool plus) {
    buf[0] = 'S';
    buf[1] = (char)('0' + s);
    buf[2] = plus ? '+' : ' ';
    buf[3] = '\0';
}

// =-=-=-=-=-=-=-=-= S-Level =-=-=-=-=-=-=-=-=

void rssiToSLevel(char* buf, uint8_t rssi) {
    if (smNoSignal(rssi)) { smBlank(buf); return; }

    // Reuse AM_THR[0..14] as HF S-meter thresholds
    uint8_t idx = smScanIndex(rssi, AM_THR, LEN_HF);

    // Avoid computing the same comparison twice
    const bool plus = (idx >= SM_S9);
    if (plus) idx = SM_S9;

    smFormat(buf, idx, plus);
}

// =-=-=-=-=-=-=-=-= Table Interpolation =-=-=-=-=-=-=-=-=

static uint8_t smLookup(uint8_t rssi, const SmTable& t) {
    for (uint8_t i = 0; i < t.len; ++i) {
        uint8_t t1 = smPgm(t.thr, i);
        if (rssi > t1) continue;

        uint8_t v1 = smPgm(t.val, i);
        if (i == 0) return v1;

        uint8_t t0 = smPgm(t.thr, i - 1);
        uint8_t v0 = smPgm(t.val, i - 1);
        uint8_t gap = (uint8_t)(t1 - t0);
        if (!gap) return v1;

        return v0 + smLerp((uint8_t)(rssi - t0), (uint8_t)(v1 - v0), gap);
    }
    return smPgm(t.val, (uint8_t)(t.len - 1));
}

static inline uint8_t rssiToStrength49(uint8_t rssi) {
    return smNoSignal(rssi) ? 0 : smLookup(rssi, smGetStrengthTable());
}

// =-=-=-=-=-=-=-=-= Signal Bar Draw =-=-=-=-=-=-=-=-=

void __attribute__((noinline))
smDrawSignalBar(uint8_t rssi) {

    oled.setCursorXY(0, SM_BAR_Y);
    oled.beginData();

    if (smNoSignal(rssi)) {
        smSendRun(SM_BAR_W, 0x00);
        oled.endTransm();
        return;
    }

    uint8_t s = rssiToStrength49(rssi);
    uint8_t w = s ? smLerp(s, SM_BAR_W, SM_BAR_SCALE) : 0;

    smSendRun(w, SM_BAR_MASK);
    smSendRun((uint8_t)(SM_BAR_W - w), SM_BAR_BASE);

    oled.endTransm();
}
