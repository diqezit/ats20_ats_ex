#pragma once

// =================================================================================================
// EEPROM Memory Map
// This map describes how data is organized in the receiver's non-volatile memory.
//
// Address Range | Allotted | Used     | Symbol(s)                      | Description
//---------------|----------|----------|--------------------------------|----------------------------------
// 0             | 1 B      | 1 B      | EEPROM_APP_ID_ADDRESS          | Custom ID to validate data structure
// 1             | 1 B      | 1 B      | EEPROM_VERSION_ADDRESS         | Firmware version for compatibility
//
// 10 - 19       | 10 B     | 7 B      | EEPROM_HEADER_START            | Global state header (volume, mode etc.)
// 20 - 243      | 224 B    | 224 B    | EEPROM_BANDS_START             | State for all 28 bands (28 * 8 bytes)
// 250 - 299     | 50 B     | ~18 B    | EEPROM_SETTINGS_START          | Global settings array `g_Settings` 
// 300 - 319     | 20 B     | 6 B      | EEPROM_MODE_SETTINGS_START     | Mode-dependent settings
//
// 400 - 419     | 20 B     | 20 B     | EEPROM_FM_FAVORITES_START      | FM favorite stations (10 * 2 bytes)
// 420           | 1 B      | 1 B      | EEPROM_FM_FAVORITES_COUNT      | Count of saved FM favorites
// =================================================================================================

// --- Core EEPROM Validation ---
constexpr auto EEPROM_APP_ID = 235;
constexpr auto EEPROM_APP_ID_ADDRESS = 0;
constexpr auto EEPROM_VERSION_ADDRESS = 1;

// --- Data Block Start Addresses ---
constexpr auto EEPROM_HEADER_START = 10;
constexpr auto EEPROM_BANDS_START = 20;
constexpr auto EEPROM_SETTINGS_START = 250;
constexpr auto EEPROM_MODE_SETTINGS_START = 300;
constexpr auto EEPROM_FM_FAVORITES_START = 400;
constexpr auto EEPROM_FM_FAVORITES_COUNT = 420;

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

// for signal polling timing
constexpr auto RSSI_POLL_INTERVAL_MS = 1000UL;          // How often to check RSSI/Stereo when idle.
constexpr auto RSSI_POLL_DELAY_AFTER_TUNE_MS = 500UL;   // Debounce delay after tuning to let signal settle.

// Display
#define RST_PIN -1
#define RESET_PIN 12

// Amplifier MD8002A control
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

// Display options
#define ENABLE_SPLASH_SCREEN 1              // Set to 1 to show splash screen, 0 to disable
#define ENABLE_EEPROM_RESET_MSG 1           // Set to 1 to show "EEPROM RESET" message, 0 to disable
#define ANIMATE_SPLASH 0                    // Set to 1 to animate splash screen, 0 to disable

// IC options
#define ENABLE_FM_FAV 1                     // Set to 1 to use FM favorites, 0 to disable (must disable some other features to compile & work)
#define DISABLE_FM 0                        // not implemented yet

#define ENABLE_ADVANCED_BATTERY_LOGIC 1     // Set to 1 to enable advanced battery logic, 0 to disable (must disable some other features to compile & work)
#define ENABLE_BATTERY_MONITOR 1            // Set to 1 to enable battery monitoring, 0 to disable

// Debugging options
#define DEBUG_MODE 0                        // Set to 1 to enable debug mode, 0 to disable, (This use serial speed 9600)

// Set to 1 to enable compilation of the SSB patch loading functions overridden in SI4735_fixed.h
// These functions may offer better performance than the original library.
// Set to 0 to disable them and fall back to the base library methods - off for save 36 bytes
#define PATCH_EX_SSB 0

#define buttonEvent                NULL
