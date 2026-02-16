#include "microWire.h"

enum : uint8_t {
    TWI_ST_SLA_W_NACK = 0x20,
    TWI_ST_DATA_NACK = 0x30,
    TWI_ST_SLA_R_NACK = 0x48
};

#define TWI_CLEAR_FLAGS() do { _address_nack = false; _data_nack = false; } while (0)
#define TWI_FAIL(flag)    do { (flag) = true; _requested_bytes = 0; } while (0)

// Wait for TWINT with about 30ms timeout at 16MHz
// On timeout: reset TWI hardware and return 0
static uint8_t __attribute__((noinline)) twiWaitTwintOrReset() {
    uint16_t n = 0;
    do {
        if (TWCR & _BV(TWINT)) return 1;
    } while (--n);

    TWCR = 0;
    TWCR = _BV(TWEN);
    return 0;
}

// Write command to TWCR and wait for completion
static uint8_t __attribute__((noinline)) twiCommand(uint8_t cr) {
    TWCR = cr;
    return twiWaitTwintOrReset();
}

void TwoWire::begin() {
#if defined(MICROWIRE_USE_PINMODE)
    pinMode(SDA, INPUT_PULLUP);
    pinMode(SCL, INPUT_PULLUP);

#elif defined(MICROWIRE_TARGET_328P)
    // ATmega168/328P: SDA=PC4, SCL=PC5
    DDRC &= (uint8_t)~(_BV(4) | _BV(5));
    PORTC |= (uint8_t)(_BV(4) | _BV(5));

#elif defined(MICROWIRE_TARGET_32U4) || defined(MICROWIRE_TARGET_2560)
    // ATmega32U4: SDA=PD1, SCL=PD0
    // ATmega2560: SDA=PD1 (D20), SCL=PD0 (D21)
    DDRD &= (uint8_t)~(_BV(1) | _BV(0));
    PORTD |= (uint8_t)(_BV(1) | _BV(0));

#else
    pinMode(SDA, INPUT_PULLUP);
    pinMode(SCL, INPUT_PULLUP);
#endif

    // TWBR = 72; - not for this project
    TWSR = 0;
}

void TwoWire::setClock(uint32_t clock) {
    TWBR = (((long)F_CPU / clock) - 16) / 2;
}

void TwoWire::beginTransmission(uint8_t address) {
    TWI_CLEAR_FLAGS();
    start();
    if (!_address_nack) write((uint8_t)(address << 1));
}

uint8_t TwoWire::endTransmission(void) {
    return endTransmission(true);
}

// Returns: 0=OK, 2=address NACK, 3=data NACK
uint8_t TwoWire::endTransmission(bool stop) {
    if (stop) this->stop();
    else start();

    uint8_t r = _address_nack ? 2 : (_data_nack ? 3 : 0);
    TWI_CLEAR_FLAGS();
    return r;
}

// Status 0x20 = SLA+W NACK, 0x48 = SLA+R NACK, 0x30 = data NACK
size_t TwoWire::write(uint8_t data) {
    TWDR = data;

    if (!twiCommand((uint8_t)(_BV(TWEN) | _BV(TWINT)))) {
        failDataNack();
        return 1;
    }

    switch ((uint8_t)(TWSR & 0xF8)) {
    case TWI_ST_SLA_W_NACK:
    case TWI_ST_SLA_R_NACK:
        failAddressNack();
        break;

    case TWI_ST_DATA_NACK:
        failDataNack();
        break;

    default:
        break;
    }

    return 1;
}

uint8_t TwoWire::available() {
    return _requested_bytes;
}

// Underflow guard: returns 0 if called beyond requestFrom length
uint8_t TwoWire::read() {
    if (_requested_bytes == 0) return 0;

    if (--_requested_bytes) {
        if (!twiCommand((uint8_t)(_BV(TWEN) | _BV(TWINT) | _BV(TWEA)))) {
            failDataNack();
            return 0;
        }
        return TWDR;
    }

    if (!twiCommand((uint8_t)(_BV(TWEN) | _BV(TWINT)))) {
        failDataNack();
        return 0;
    }

    if (_stop_after_request) this->stop();
    else start();
    return TWDR;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length) {
    return requestFrom(address, length, true);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length, bool sendStop) {
    TWI_CLEAR_FLAGS();

    _stop_after_request = sendStop;
    _requested_bytes = length;

    if (!length) return 0;

    start();
    if (!_address_nack)
        write((uint8_t)((address << 1) | 1));

    const uint8_t rb = _requested_bytes;

    if (!rb) {
        stop();          // abort on NACK or timeout
        return 0;        // no need to preserve rb across stop()
    }

    return rb;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
    return requestFrom(address, quantity, sendStop != 0);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop) {
    if (isize > 0) {
        beginTransmission(address);
        if (isize > 4) isize = 4;
        while (isize-- > 0) write((uint8_t)(iaddress >> (isize * 8)));
        endTransmission(false);
    }
    return requestFrom(address, quantity, sendStop);
}

uint8_t TwoWire::requestFrom(int address, int quantity) {
    return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop) {
    return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)sendStop);
}

size_t TwoWire::write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) write(buffer[i]);
    return size;
}

void TwoWire::start() {
    if (!twiCommand((uint8_t)(_BV(TWSTA) | _BV(TWEN) | _BV(TWINT))))
        failAddressNack();
}

void TwoWire::stop() {
    TWCR = (uint8_t)(_BV(TWSTO) | _BV(TWEN) | _BV(TWINT));
}

void __attribute__((noinline)) TwoWire::failAddressNack() {
    _address_nack = true;
    _requested_bytes = 0;
}

void __attribute__((noinline)) TwoWire::failDataNack() {
    _data_nack = true;
    _requested_bytes = 0;
}

TwoWire Wire = TwoWire();

#undef TWI_CLEAR_FLAGS
#undef TWI_FAIL
