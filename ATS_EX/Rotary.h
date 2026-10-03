// Rotary encoder library for Arduino.
#include "Arduino.h"

#ifndef rotary_h
#define rotary_h

// Enable this to emit codes twice per step
// #define HALF_STEP

// Values returned by 'process'
#define DIR_NONE 0x0    // No complete step yet
#define DIR_CW   0x10   // Clockwise step
#define DIR_CCW  0x20   // Anti-clockwise step

class Rotary
{
  public:
    // constructor
    constexpr Rotary(char p1, char p2)
        : state(0), pin1((unsigned char)p1), pin2((unsigned char)p2) {}

    // Process pin(s)
    unsigned char process();
  private:
    unsigned char state;
    unsigned char pin1;
    unsigned char pin2;
};
#endif

