#pragma once
#include "Defines.h"

#if ENABLE_GAME

#include <Arduino.h>
#include <string.h>
#include "SSD1306_OLED.h"

extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// =-=-=-=-=-=-=-=-= Layout =-=-=-=-=-=-=-=-=
static constexpr uint8_t  G_W = 21;
static constexpr uint8_t  G_TOP = 1;
static constexpr uint8_t  G_BOT = 7;
static constexpr uint8_t  G_PADL = 2;
static constexpr uint8_t  G_PADR = (uint8_t)(G_W - 3);
static constexpr uint8_t  G_PH = 1;
static constexpr uint8_t  G_PYLO = (uint8_t)(G_TOP + G_PH);
static constexpr uint8_t  G_PYHI = (uint8_t)(G_BOT - G_PH);
static constexpr uint8_t  G_MX = (uint8_t)(G_W / 2);
static constexpr uint8_t  G_MY = (uint8_t)((G_TOP + G_BOT) / 2);
static constexpr uint8_t  G_WIN = 9;
static constexpr uint8_t  G_SERVE_TICKS = 5;
static constexpr uint8_t  G_LAST = (uint8_t)(G_W - 1);

static_assert(G_PH == 1, "G_PH must be 1");

// =-=-=-=-=-=-=-=-= State =-=-=-=-=-=-=-=-=
static struct {
    uint8_t  aiY;
    uint8_t  plY;
    uint8_t  bx;
    uint8_t  by;
    int8_t   vx;
    int8_t   vy;
    uint8_t  scAI;
    uint8_t  scPL;
    uint8_t  rally;
    uint8_t  tick;
    uint8_t  serve;
    uint16_t frame;
    bool     on;
} gm;

// =-=-=-=-=-=-=-=-= Helpers =-=-=-=-=-=-=-=-=
static inline uint8_t gmClamp(uint8_t y) {
    if (y < G_PYLO) return G_PYLO;
    if (y > G_PYHI) return G_PYHI;
    return y;
}

static inline bool gmHitPad(uint8_t by, uint8_t padY) {
    uint8_t d = (by >= padY) ? (uint8_t)(by - padY) : (uint8_t)(padY - by);
    return d <= G_PH;
}

static inline uint16_t gmSpeed() {
    return (gm.rally < 9) ? (uint16_t)(70 - (gm.rally << 2)) : 35;
}

static inline bool gmRoundOver() {
    return gm.scAI >= G_WIN || gm.scPL >= G_WIN;
}

static inline bool gmPlayerWon() {
    return gm.scPL >= G_WIN;
}

static inline bool gmServing() {
    return gm.serve > 0;
}

static inline bool gmBallGoingLeft() {
    return gm.vx < 0;
}

static inline int8_t gmScoreDiff() {
    return (int8_t)gm.scAI - (int8_t)gm.scPL;
}

static inline uint8_t gmAiSkipMask() {
    int8_t d = gmScoreDiff();
    if (d > 2)  return 1;
    if (d > 0)  return 3;
    if (d < -2) return 0;
    return (gm.rally > 4) ? 1 : 7;
}

static inline char gmDigit(uint8_t v) {
    return (char)('0' + v);
}

static inline void gmStepTo(uint8_t& y, uint8_t target) {
    if (y < target) ++y;
    else if (y > target) --y;
}

static inline void gmBounce(int8_t& nx, int8_t ny, uint8_t padY) {
    int8_t d = ny - (int8_t)padY;
    if (d < -(int8_t)G_PH || d >(int8_t)G_PH) return;
    gm.vx = -gm.vx;
    nx += gm.vx;
    if (d) gm.vy = d;
    ++gm.rally;
}

static inline bool gmElapsed(uint16_t now) {
    return (uint16_t)(now - gm.frame) >= gmSpeed();
}

static inline void gmSyncFrame() {
    gm.frame = (uint16_t)millis();
}

static inline void gmInitLine(char* ln) {
    memset(ln, ' ', G_W);
}

static inline void gmWalls(char* ln) {
    ln[0] = '|';
    ln[G_LAST] = '|';
}

static inline void gmClampY(int8_t& ny) {
    if (ny <= (int8_t)G_TOP) { ny = (int8_t)G_TOP; gm.vy = 1; }
    else if (ny >= (int8_t)G_BOT) { ny = (int8_t)G_BOT; gm.vy = -1; }
}

static inline void gmPadCheck(int8_t& nx, int8_t ny) {
    bool left = gmBallGoingLeft();
    if (left && nx == (int8_t)G_PADL) gmBounce(nx, ny, gm.aiY);
    else if (!left && nx == (int8_t)G_PADR) gmBounce(nx, ny, gm.plY);
}

// =-=-=-=-=-=-=-=-= Core =-=-=-=-=-=-=-=-=
__attribute__((noinline))
static void gmRow(uint8_t row, const char* s) {
    oled.setCursor(0, row);
    oled.print(s);
}

__attribute__((noinline))
static void gmServe(bool toRight) {
    gm.bx = G_MX;
    gm.by = G_MY;
    gm.vx = toRight ? (int8_t)1 : (int8_t)-1;
    gm.vy = (int8_t)-gm.vy;
    gm.rally = 0;
    gm.serve = G_SERVE_TICKS;
}

__attribute__((noinline))
static void gmGoal(uint8_t& sc, bool serveDir) {
    if (sc < G_WIN) ++sc;
    gmServe(serveDir);
}

__attribute__((noinline))
static void gmReset() {
    gm.aiY = gm.plY = G_MY;
    gm.scAI = gm.scPL = 0;
    gm.vy = 1;
    gm.tick = 0;
    gmServe(true);
}

// =-=-=-=-=-=-=-=-= Draw =-=-=-=-=-=-=-=-=
__attribute__((noinline))
static void gmDraw() {
    char ln[G_W + 1];
    ln[G_W] = '\0';

    gmInitLine(ln);
    ln[6] = 'A'; ln[7] = 'I';
    ln[9] = gmDigit(gm.scAI);
    ln[10] = '-';
    ln[11] = gmDigit(gm.scPL);
    ln[13] = 'P'; ln[14] = 'L';
    gmRow(0, ln);

    for (uint8_t y = G_TOP; y <= G_BOT; ++y) {
        gmInitLine(ln);
        gmWalls(ln);
        if (y & 1) ln[G_MX] = ':';
        if (gmHitPad(y, gm.aiY)) ln[G_PADL] = '|';
        if (gmHitPad(y, gm.plY)) ln[G_PADR] = '|';
        if (y == gm.by && (!gmServing() || (gm.tick & 1)))
            ln[gm.bx] = 'o';
        gmRow(y, ln);
    }
}

static inline void gmDrawAndSync() {
    gmSyncFrame();
    gmDraw();
}

__attribute__((noinline))
static void gmEndRound() {
    oled.setCursor(48, 4);
    oled.print(gmPlayerWon() ? F("WIN ") : F("LOSE"));
    delay(2000);
    gmReset();
}

// =-=-=-=-=-=-=-=-= Tick =-=-=-=-=-=-=-=-=
static inline void gmMovePlayer(int16_t enc) {
    if (enc > 0) --gm.plY;
    else if (enc < 0) ++gm.plY;
    gm.plY = gmClamp(gm.plY);
}

static inline void gmMoveAI() {
    if (!gmBallGoingLeft()) {
        if (!(gm.tick & 3)) gmStepTo(gm.aiY, G_MY);
        return;
    }
    uint8_t m = gmAiSkipMask();
    if (m && (gm.tick & m)) return;
    gmStepTo(gm.aiY, gm.by);
    gm.aiY = gmClamp(gm.aiY);
}

static inline void gmMoveBall() {
    int8_t nx = (int8_t)gm.bx + gm.vx;
    int8_t ny = (int8_t)gm.by + gm.vy;

    gmClampY(ny);
    gmPadCheck(nx, ny);

    if (nx <= 0)              gmGoal(gm.scPL, false);
    else if (nx >= (int8_t)G_LAST) gmGoal(gm.scAI, true);
    else { gm.bx = (uint8_t)nx; gm.by = (uint8_t)ny; }
}

// =-=-=-=-=-=-=-=-= API =-=-=-=-=-=-=-=-=
static inline bool gameIsActive() { return gm.on; }

static inline void gameToggle() {
    gm.on = !gm.on;
    oled.clear();
    if (!gm.on) return;
    gmReset();
    gmDrawAndSync();
}

static inline void gameTask(int16_t enc) {
    if (!gm.on) return;

    const uint16_t now = (uint16_t)millis();
    if (!gmElapsed(now)) return;
    gm.frame = now;

    ++gm.tick;
    gmMovePlayer(enc);

    if (gmServing()) {
        --gm.serve;
        gmDraw();
        return;
    }

    gmMoveAI();
    gmMoveBall();
    gmDraw();

    if (gmRoundOver()) {
        gmEndRound();
        gmDrawAndSync();
    }
}

#else

// =-=-=-=-=-=-=-=-= Stubs =-=-=-=-=-=-=-=-=
static inline bool gameIsActive() { return false; }
static inline void gameToggle() {}
static inline void gameTask(int16_t) {}

#endif
