#include "Arduino.h"
#include "SimpleButton.h"


#define BUTTONSTATE_IDLE          0           // Button not pressed (initial state)
// DO NOT CHANGE!!! BUTTONSTATE_IDLE must always be defined as 0 (Zero)!
#define BUTTONSTATE_DEBOUNCE      1           // Button press detected, waiting for debounce
#define BUTTONSTATE_RELEASE       2           // Button was released again

// BUTTONSTATE_RELEASE is the last state (in numerical order) for which the button is assumed not pressed
// All states which are (numerical) higher then BUTTONSTATE_RELEASE are representing states with pressed button
// In total 16 different states can be coded (0..15), apart from the rule with BUTTONSTATE_RELEASE the order is irrelevant

#define BUTTONSTATE_PRESSED       3           // Button pressed. Measure time to see if it is a Longpress
#define BUTTONSTATE_LONGPRESS     4           // Press is a longpress
#define BUTTONSTATE_LONGRELEASE   5           // Button released after longpress but wait for debounce the release
#define BUTTONSTATE_SHORTRELEASE  6           // Button released after shortpress but wait for debounce the release
//#define BUTTONSTATE_2DEBOUNCE     7

uint8_t SimpleButton::checkEvent(uint8_t(*eventHandler)(uint8_t event, uint8_t pin)) {
    uint8_t ret = 0;

    // time in 16ms ticks (0x3F0 mask keeps it aligned and cheap)
    uint16_t now = (uint16_t)(millis() & 0x3f0);

    // Packed layout:
    //   b15..b10: pin number
    //   b9..b4  : timestamp (16ms ticks)
    //   b3..b0  : state (0..15)
    uint8_t  state = (uint8_t)(_PinDebounceState & 0x0F);     // 4-bit FSM state
    uint16_t stamp = (uint16_t)(_PinDebounceState & 0x3f0);   // last change time

    const uint8_t pinState = ((*(volatile uint8_t*)(uintptr_t)_pinPort) & _pinMask) ? HIGH : LOW;

    // handle wrap (0x400 == 1024ms window in 16ms ticks)
    if (now < stamp) now = (uint16_t)(now + 0x400);

    const uint16_t elapsed = (uint16_t)(now - stamp);

    switch (state) {
    case BUTTONSTATE_IDLE:
        if (!pinState) {
            state = BUTTONSTATE_DEBOUNCE;
            stamp = now;
        }
        break;

    case BUTTONSTATE_DEBOUNCE:
        if (pinState) {
            state = BUTTONSTATE_IDLE;
        } else if (elapsed >= BUTTONTIME_PRESSDEBOUNCE) {
            state = BUTTONSTATE_PRESSED;
        }
        break;

    case BUTTONSTATE_PRESSED:
        if (pinState) {
            stamp = now;
            state = BUTTONSTATE_SHORTRELEASE;
        } else if (elapsed >= BUTTONTIME_LONGPRESS1) {
            ret = BUTTONEVENT_FIRSTLONGPRESS;
            state = BUTTONSTATE_LONGPRESS;
            stamp = now;
        }
        break;

    case BUTTONSTATE_LONGPRESS:
        if (pinState) {
            state = BUTTONSTATE_LONGRELEASE;
        } else if (elapsed >= BUTTONTIME_LONGPRESSREPEAT) {
            stamp = now;
            ret = BUTTONEVENT_LONGPRESS;
        }
        break;

    case BUTTONSTATE_LONGRELEASE:
        if (pinState) {
            ret = BUTTONEVENT_LONGPRESSDONE;
            state = BUTTONSTATE_RELEASE;
            stamp = now;
        } else {
            state = BUTTONSTATE_LONGPRESS;
        }
        break;

    case BUTTONSTATE_SHORTRELEASE:
        if (pinState) {
            ret = BUTTONEVENT_SHORTPRESS;
            state = BUTTONSTATE_RELEASE;
        } else {
            state = BUTTONSTATE_PRESSED;
        }
        break;

    case BUTTONSTATE_RELEASE:
        if (pinState) {
            if (elapsed >= BUTTONTIME_RELEASEDEBOUNCE)
                state = BUTTONSTATE_IDLE;
        } else {
            stamp = now;
        }
        break;

    default:
        break;
    }

    // write back packed state
    _PinDebounceState = (uint16_t)((_PinDebounceState & 0xfc00) | (stamp & 0x3f0) | (uint16_t)state);

    if (ret) {
        if (eventHandler) {
            const uint8_t pin = (uint8_t)(_PinDebounceState >> 10);
            ret = eventHandler(ret, pin);
        }
    } else {
        if (state > BUTTONSTATE_RELEASE)
            ret = BUTTON_PRESSED;
    }

    return ret;
}
