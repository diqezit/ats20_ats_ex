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


SimpleButton::SimpleButton(uint8_t pin) {
    // Fast GPIO init (saves Flash vs pinMode on AVR):
    // - configure as INPUT
    // - enable internal pull-up (button drives pin LOW when pressed)
    //
    // Also cache the PINx register address + bit mask once here
    // This removes per-call port branching and runtime bit shifting in checkEvent()

    // Arduino Uno / ATmega328P pin mapping used here:
    //   0..7   -> PORTD / PIND  (PD0..PD7)
    //   8..13  -> PORTB / PINB  (PB0..PB5)
    //   14..19 -> PORTC / PINC  (PC0..PC5)  [A0..A5]

    if (pin < 8) {
        // Digital pins D0..D7 (PORTD)
        DDRD &= ~(1 << pin);    // input
        PORTD |= (1 << pin);    // pull-up on

        _pinReg = &PIND;                // cached input register
        _pinMask = (uint8_t)(1 << pin); // cached bit mask
    } else if (pin < 14) {
        // Digital pins D8..D13 (PORTB)
        uint8_t bit = (uint8_t)(pin - 8);

        DDRB &= ~(1 << bit);    // input
        PORTB |= (1 << bit);    // pull-up on

        _pinReg = &PINB;
        _pinMask = (uint8_t)(1 << bit);
    } else {
        // Analog pins A0..A5 as digital 14..19 (PORTC)
        uint8_t bit = (uint8_t)(pin - 14);

        DDRC &= ~(1 << bit);    // input
        PORTC |= (1 << bit);    // pull-up on

        _pinReg = &PINC;
        _pinMask = (uint8_t)(1 << bit);
    }

    // Store the Arduino pin number inside the packed state word.
    // _PinDebounceState layout (16-bit):
    //   b15..b10: pin number (0..63)
    //   b9..b4  : debounce timestamp (units of 16ms)
    //   b3..b0  : FSM state (0..15)
    //
    // debounce+state start at 0 => IDLE state, timestamp=0
    _PinDebounceState = ((uint16_t)pin << 10);
}


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

    const uint8_t pinState = ((*_pinReg & _pinMask) ? HIGH : LOW);

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
