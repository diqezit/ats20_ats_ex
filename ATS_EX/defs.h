#pragma once

// EEPROM Memory Map
// This map describes how data is organized in the receiver's non-volatile memory
//
// Address(es) | Size | Symbol(s)                  | Description
//-------------|------|----------------------------|----------------------------------------------------
// 0           | 1 B  | EEPROM_APP_ID_ADDRESS      | Custom ID to validate firmware data structure
// 1-6         | 6 B  | EEPROM_DATA_START_ADDRESS  | Global state header (volume, mode, BFO, etc.)
// 7-230       | 224B | -                          | State for all 28 bands (28 bands * 8 bytes each)
// 231-246     | 16 B | -                          | Global settings array `g_Settings`
// 247-252     | 6 B  | -                          | Mode-dependent settings `g_modeSettings`
//
// 260-279     | 20 B | EEPROM_FM_FAVORITES_START  | FM favorite station data (10 slots * 2 bytes/slot)
// 280         | 1 B  | EEPROM_FM_FAVORITES_COUNT  | Address storing the number of saved FM favorites (0-10)
//
// 1000        | 1 B  | EEPROM_VERSION_ADDRESS     | Firmware version for compatibility checks
// -----------------------------------------------------------------------------------------------------

// Core EEPROM
constexpr auto EEPROM_APP_ID = 235;
constexpr auto EEPROM_APP_ID_ADDRESS = 0;
constexpr auto EEPROM_VERSION_ADDRESS = 1000;
constexpr auto EEPROM_DATA_START_ADDRESS = 1;

// FM Favorites Storage
constexpr auto EEPROM_FM_FAVORITES_START = 260;
constexpr auto EEPROM_FM_FAVORITES_COUNT = 280;

// Behavior
constexpr auto SAVE_ON_IDLE_TIMEOUT = 30000UL;
constexpr auto DEFAULT_VOLUME = 25;
constexpr auto ADJUSTMENT_ACTIVE_TIMEOUT = 3000;
#define BAND_DELAY                2
#define VOLUME_DELAY              1 
constexpr auto MIN_ELAPSED_TIME = 100;
constexpr auto MIN_ELAPSED_RSSI_TIME = 150;
constexpr auto SEEK_TIME = 65535UL;         // 65535 ms = 65.535 seconds
constexpr auto SW_STEP_SPACING = 5;         // SW step spacing in kHz (1, 5, 9, 10) only supported values. 5 kHz is optimal
constexpr auto LW_MW_STEP_SPACING = 1;      // for LW/MW a small step of 1kHz for grid alignment

// Display
#define DEFAULT_FONT FONT8X16POB
#define RST_PIN -1
#define RESET_PIN 12

// Hardware
#define BATTERY_VOLTAGE_PIN A2

// Amplifier MD8002A control
#define MD8002A_SHUTDOWN_PIN A3
#define AMP_DDR   DDRC
#define AMP_PORT  PORTC
#define AMP_BIT   3

constexpr auto STEREO_STATUS_BIT = 0;

// delay (in ms) to wait after the encoder stops turning before sending
// final frequency to the chip
// shorter delay feels more responsive but can increase I2C traffic if tuning slowly
constexpr auto FREQ_UPDATE_DELAY_MS = 30UL;

// if the frequency change (delta) since chip update exceeds this threshold in kHz,
// send the update immediately without waiting for the delay
// This prevents the receiver from "lagging" behind during fast tuning
constexpr auto FREQ_FORCE_UPDATE_THRESHOLD_KHZ = 50;

// protect the I2C bus from being flooded with commands, this sets the absolute minimum
// time that must pass between any two setFrequency calls ( safety rate limit)
constexpr auto MIN_SETFREQ_INTERVAL_MS = 25UL;

// Encoder Pins
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Button Pins
#define MODE_SWITCH       4 
#define BANDWIDTH_BUTTON  5
#define VOLUME_BUTTON     6
#define AVC_BUTTON        7
#define BAND_BUTTON       8 
#define SOFTMUTE_BUTTON   9
#define AGC_BUTTON       11
#define STEP_BUTTON      10
#define ENCODER_BUTTON   14

// For test only (don`t edit)

// Display options
#define ENABLE_SPLASH_SCREEN 0              // Set to 1 to show splash screen, 0 to disable
#define ENABLE_EEPROM_RESET_MSG 0           // Set to 1 to show "EEPROM RESET" message, 0 to disable
#define ANIMATE_SPLASH 0                    // Set to 1 to animate splash screen, 0 to disable

// IC options
#define ENABLE_FM_FAV 1                     // Set to 1 to use FM favorites, 0 to disable (must disable some other features to compile & work)
#define DISABLE_FM 0                        // not implemented yet

#define ENABLE_ADVANCED_BATTERY_LOGIC 0     // Set to 1 to enable advanced battery logic, 0 to disable (must disable some other features to compile & work)
#define ENABLE_BATTERY_MONITOR 1            // Set to 1 to enable battery monitoring, 0 to disable

// Debugging options
#define DEBUG_MODE 0                        // Set to 1 to enable debug mode, 0 to disable, (This use serial speed 9600)


#define buttonEvent                NULL
