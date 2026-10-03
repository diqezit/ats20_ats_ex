// ----------------------------------------------------------------------
// Boot.h - Hardware initialization, ISRs and low-level boot logic
// ----------------------------------------------------------------------

#pragma once

#include <microWire.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "Defines.h"
#include "Globals.h"
#include "Utils.h"
#include "Memory.h"
#include "Battery.h"
#include "Rotary.h"
#include "UI.h"

// ====================================================================================
// ===== LOCAL MACROS =================================================================
// ====================================================================================

#define TWI_TWPS_1()            (TWSR = 0)
#define CLKPR_UNLOCK_KEY        0x80

#define ENC_EICRA_ANY           (_BV(ISC00) | _BV(ISC10))
#define ENC_EIFR_CLR            (_BV(INTF0) | _BV(INTF1))
#define ENC_EIMSK_BOTH          (_BV(INT0) | _BV(INT1))

#define AMP_PIN_OUT()           (AMP_DDR |= (1 << AMP_BIT))
#define LED_PB5_OUT()           (DDRB |= (1 << DDB5))
#define UART0_OFF()             (UCSR0B = 0)

#define ENC_BTN_PCMSK           _BV(ENCODER_BUTTON - 14)
#define AGC_BTN_PBMSK           _BV(AGC_BUTTON - 8)

// ==========================================
// ===== I2C CLOCK COMPENSATION ============
// ==========================================

// Wire.setClock() computes TWBR from compile-time F_CPU which is wrong
// when CLKPR divides actual CPU clock. We write TWBR directly using
// real CPU frequency so SCL stays correct at any prescaler
//
// CLKPR 0-1 - target SCL = I2C_BASE_HZ (normal operation)
// CLKPR 2+  - target SCL = min(I2C_BASE_HZ, 50kHz) (keep bus stable)

namespace {

    constexpr uint32_t I2C_SLOW_HZ = 50000UL;
    constexpr uint8_t  CLKPR_SLOW_THRESHOLD = 2;
    constexpr uint8_t  TWBR_TABLE_SIZE = 5;  // CLKPR 0..4 covers div1..div16
    constexpr uint8_t  CLKPR_MASK = 0x0F;

    // from AVR datasheet - SCL = F_cpu / (16 + 2*TWBR) when TWPS=0
    constexpr uint8_t  TWI_FIXED = 16;
    constexpr uint8_t  TWBR_MAX = 255;
    constexpr uint16_t TWI_DIV_MAX = TWI_FIXED + 2U * TWBR_MAX; // 526

    static_assert(I2C_BASE_HZ >= 35000UL && I2C_BASE_HZ <= 600000UL,
        "I2C_BASE_HZ outside practical range 35kHz..600kHz");

    constexpr uint32_t minVal(uint32_t a, uint32_t b) {
        return (a < b) ? a : b;
    }

    // pick target SCL - full speed for low prescaler, capped for high
    constexpr uint32_t targetSCL(uint8_t clkpr) {
        return (clkpr < CLKPR_SLOW_THRESHOLD)
            ? static_cast<uint32_t>(I2C_BASE_HZ)
            : minVal(static_cast<uint32_t>(I2C_BASE_HZ), I2C_SLOW_HZ);
    }

    // TWBR from actual frequency and desired SCL, clamped 0..255
    constexpr uint8_t calcTWBR(uint32_t f_actual, uint32_t scl_hz) {
        return (f_actual / scl_hz <= TWI_FIXED) ? static_cast<uint8_t>(0)
            : (f_actual / scl_hz > TWI_DIV_MAX) ? TWBR_MAX
            : static_cast<uint8_t>((f_actual / scl_hz - TWI_FIXED) / 2);
    }

    // all values computed at compile time, no runtime math needed
    // changing I2C_BASE_HZ or F_CPU rebuilds the whole table
    constexpr uint8_t TWBR_TABLE[TWBR_TABLE_SIZE] = {
        calcTWBR(F_CPU >> 0, targetSCL(0)),   // CLKPR=0 full CPU clock
        calcTWBR(F_CPU >> 1, targetSCL(1)),   // CLKPR=1 half clock
        calcTWBR(F_CPU >> 2, targetSCL(2)),   // CLKPR=2 quarter
        calcTWBR(F_CPU >> 3, targetSCL(3)),   // CLKPR=3 eighth
        calcTWBR(F_CPU >> 4, targetSCL(4))    // CLKPR=4 sixteenth
    };

} // anonymous namespace

// read current CLKPR and set matching TWBR so I2C runs at correct speed
void applyI2CSpeed() {
    uint8_t p = CLKPR & CLKPR_MASK;
    if (p >= TWBR_TABLE_SIZE) p = TWBR_TABLE_SIZE - 1;
    TWI_TWPS_1();    // TWPS=0 means TWI hardware prescaler is 1
    TWBR = TWBR_TABLE[p];
}

// change CPU divider with mandatory CLKPCE unlock sequence
// then fix I2C timing to match new clock
static void setCpuPrescaler(uint8_t prescaler) {
    uint8_t oldSREG = SREG;
    cli();
    CLKPR = CLKPR_UNLOCK_KEY; // unlock - write CLKPCE bit
    CLKPR = prescaler;        // must follow within 4 cycles
    SREG = oldSREG;
    applyI2CSpeed();          // SCL would drift without this
}

// ==========================================
// ===== ISR ================================
// ==========================================

// rotary encoder signals on D2 and D3
ISR(INT0_vect) { rotaryEncoder(); }

// INT1 does the same as INT0, alias to avoid duplicated ISR prologue/epilogue code
ISR(INT1_vect, ISR_ALIASOF(INT0_vect));

// ==========================================
// ===== BOOT HELPERS ======================
// ==========================================

// Timer0 CTC with 64 prescaler - compare match every 250 ticks = exact 1 ms
// gives millis() with zero drift and a minimal compare-match ISR (see Utils.h FastTime section)
// OC0A/OC0B pins stay disconnected (COM bits 0): D5/D6 keep normal GPIO use
static inline void initTimer0() {
    TCCR0A = (uint8_t)_BV(WGM01);                  // CTC
    TCCR0B = (uint8_t)(_BV(CS01) | _BV(CS00));     // prescaler 64
    OCR0A  = (uint8_t)((F_CPU / 64 / 1000) - 1);   // 249 @ 16 MHz -> 1000.000 us period
    TIMSK0 = (uint8_t)_BV(OCIE0A);                 // compare-A interrupt
}

// bootloader enables UART for firmware upload - turn it off
// saves power and frees PD0/PD1 if needed later
static inline void disableBootloaderUART() {
    UART0_OFF();
}

// lightweight replacement for Arduino init()
// only Timer0 is needed, Timer1/Timer2/ADC skipped to save flash
static inline void initFast() {
    initTimer0();
    disableBootloaderUART();
    sei();
}

// Rebuild RAM band table (g_bandList) from compact PROGMEM definitions (g_bandDefs)
// Must run before any logic or EEPROM load accesses g_bandList
static inline void initBandList() {
    const BandDef* d = g_bandDefs;
    for (uint8_t i = 0; i < g_bandCount; i++, d++) {
        Band* b = &g_bandList[i];
        memcpy_P(b, d, sizeof(BandDef));
        const uint8_t t = b->bandType;
        b->stepIdxAM  = (t == LW_BAND_TYPE || t == MW_BAND_TYPE)
                      ? BandDefaults::stepLW_MW   // 9 kHz grid on LW/MW
                      : BandDefaults::stepSW;     // 5 kHz elsewhere
        b->stepIdxSSB = BandDefaults::stepSSB;
        b->stepIdxFM  = BandDefaults::stepFM;
        b->bwIdxAM    = BandDefaults::bwAM;
        b->bwIdxSSB   = BandDefaults::bwSSB;
        b->bwIdxFM    = BandDefaults::bwFM;
        b->bfoCal     = BandDefaults::bfoCal;
    }
}

// Button / encoder pin hardware setup
// 
// DDR input + pull-up for every button and encoder pin
// 
// Called from initHardwarePins() - first pin-touching step of setup()
// still BEFORE first button-pin read (eepromResetKeysHeld())
// and BEFORE initEncoderInterrupts() enables INT0/INT1
static constexpr uint8_t BTN_PORT_MASK(uint8_t pin, uint8_t base) {
    // bit mask of pin inside port starting at Arduino pin base
    // 0 if out of range
    return (pin >= base && pin < base + 8) ? (uint8_t)(1u << (pin - base)) : 0;
}

static inline void initButtonPullups() {

    constexpr uint8_t mD =
        BTN_PORT_MASK(ENCODER_PIN_A, 0) | BTN_PORT_MASK(ENCODER_PIN_B, 0) |
        BTN_PORT_MASK(MODE_SWITCH, 0)   | BTN_PORT_MASK(BANDWIDTH_BUTTON, 0) |
        BTN_PORT_MASK(VOLUME_BUTTON, 0) | BTN_PORT_MASK(AVC_BUTTON, 0);

    constexpr uint8_t mB =
        BTN_PORT_MASK(BAND_BUTTON, 8) | BTN_PORT_MASK(SOFTMUTE_BUTTON, 8) |
        BTN_PORT_MASK(STEP_BUTTON, 8) | BTN_PORT_MASK(AGC_BUTTON, 8);

    constexpr uint8_t mC = BTN_PORT_MASK(ENCODER_BUTTON, 14);

    if (mD) { DDRD &= (uint8_t)~mD; PORTD |= mD; }   // input + pull-up
    if (mB) { DDRB &= (uint8_t)~mB; PORTB |= mB; }
    if (mC) { DDRC &= (uint8_t)~mC; PORTC |= mC; }
}

// amplifier control pin as output, start muted
// PB5 LED as output for status indication
static inline void initHardwarePins() {
    AMP_PIN_OUT();
    setAmpState(false);
    LED_PB5_OUT();
    initButtonPullups();
}

// D2=INT0 D3=INT1 both on CHANGE for quadrature decoding
// clear pending flags first to avoid false trigger on enable
static inline void initEncoderInterrupts() {
    EICRA = ENC_EICRA_ANY;    // both on any edge
    EIFR = ENC_EIFR_CLR;     // clear stale flags
    EIMSK = ENC_EIMSK_BOTH;   // enable both
}

// mark stored version byte as invalid so loadReceiverConfig()
// will treat EEPROM as blank and write fresh defaults
static inline void clearEEPROMVersion() {
    EE_UPDATE8(EEPROM_VERSION_ADDRESS, 0);
}

// user holds encoder or AGC button at power-on to request reset
// active-low inputs debounced with 25ms re-check
static inline bool eepromResetKeysHeld() {
    const uint8_t enc = ENC_BTN_PCMSK;
    const uint8_t agc = AGC_BTN_PBMSK;
    if ((PINC & enc) && (PINB & agc)) return false;  // both released
    delay(25);
    return !((PINC & enc) && (PINB & agc));  // still held after debounce
}

// if reset combo detected wipe settings for fresh start
// otherwise show splash screen while hardware warms up
static inline void handleEEPROMReset() {
#if DEBUG_MODE
    initDebugUART();
    debugPrint_P(PSTR("Debug started\n"));
#endif
    if (eepromResetKeysHeld()) {
        clearEEPROMVersion();
    } else {
#if ENABLE_SPLASH_SCREEN
        showSplashScreen();
#endif
    }
}

// reset Si4735 chip detect its I2C address and configure for MW
// encoder interrupts must be ready before tuning starts
static inline void initSi4735() {
    initEncoderInterrupts();
    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);
    g_si4735.setMaxSeekTime(SEEK_TIME);
    delay(SYSTEM_INIT_DELAY_MS);
}

// pull all saved band/frequency/mode data from EEPROM into RAM
static inline void loadReceiverConfig() {
    readAllReceiverInformation();
}

// ask chip what frequency it actually tuned to and put it on screen
static inline void syncFrequencyDisplay() {
    g_currentFrequency = g_si4735.getFrequency();
    showFrequency(true);
}

// final boot step - set CPU speed from user setting
// tune to last saved band/frequency and update display
static inline void applyInitialConfiguration() {
    setCpuPrescaler(getSettingParam(CPUSpeed));
    applyBandConfiguration(false);
    syncFrequencyDisplay();
}

#undef AGC_BTN_PBMSK
#undef ENC_BTN_PCMSK
#undef UART0_OFF
#undef LED_PB5_OUT
#undef AMP_PIN_OUT
#undef ENC_EIMSK_BOTH
#undef ENC_EIFR_CLR
#undef ENC_EICRA_ANY
#undef CLKPR_UNLOCK_KEY
#undef TWI_TWPS_1
