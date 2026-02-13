// ----------------------------------------------------------------------
// SMeter.h - S-Meter conversion logic (RSSI to S-point display)
// ----------------------------------------------------------------------

#pragma once

#include <avr/pgmspace.h>
#include "Defines.h"
#include "Globals.h"

// ==========================================
// ========== S-METER LOGIC =================
// ==========================================

// linear scan saves flash on avr for small progmem tables
// CREAD helper abstracts required pgm_read_byte call
static uint8_t scanThreshold(
    uint8_t rssi,
    const uint8_t* thr,
    uint8_t len) {
    uint8_t i = 0;
    while (i < len && rssi > CREAD(thr, i)) ++i;
    return i;
}

// map index to S-point based on receiver mode
// for AM-family only (AM/LSB/USB/CW)
static inline void mapIdxToSAndPlus(
    uint8_t idx,
    uint8_t* s,
    uint8_t* plus) {

    // HF mapping:
    // idx 0..8  -> S0..S8
    // idx >= 9  -> S9+
    if (idx < 9) {
        *s = idx;
        *plus = 0;
        return;
    }
    *s = 9;
    *plus = 1;
}

// format fixed-width string to keep UI columns aligned
// ui shows a simple plus indicator not a numeric dB value
static void formatSMeter(
    char* buf,
    uint8_t s,
    uint8_t plus) {
    buf[0] = 'S';
    buf[1] = (s < 9) ? ('0' + s) : '9';
    buf[2] = plus ? '+' : ' ';
    buf[3] = '\0';
}

// orchestrate s-meter display from raw rssi value
// blanks output on no signal to prevent stale readings
// FM rendered as numeric RSSI
void rssiToSLevel(char* buffer, uint8_t rssi) {
    if (rssi == UI_SIGNAL_NO_VALUE) {
        *((uint32_t*)buffer) = 0x00202020; // "   \0"
        return;
    }

    uint8_t idx = scanThreshold(rssi, THR_HF, LEN_HF);

    uint8_t s, plus;
    mapIdxToSAndPlus(idx, &s, &plus);
    formatSMeter(buffer, s, plus);
}
