#pragma once

// =================================================================================================
// Defines.h - Global Constants, Memory Map, and Compile-Time Switches
// =================================================================================================
// EEPROM Memory Map
// This map defines the layout for all persistent data.
//
// Address Range | Allotted | Used     | Free    | Symbol(s)                      | Description
//---------------|----------|----------|---------|--------------------------------|----------------------------------
// 0             | 1 B      | 1 B      | 0 B     | EEPROM_APP_ID_ADDRESS          | Custom ID to validate data
// 1             | 1 B      | 1 B      | 0 B     | EEPROM_VERSION_ADDRESS         | Firmware version for compatibility
//
// 2 - 9         | 8 B      | 0 B      | 8 B     | (free)                         | Reserved / free space
//
// 10 - 25       | 16 B     | 7 B      | 9 B     | EEPROM_HEADER_START            | ReceiverHeader (7B) + reserve
//
// 26 - 289      | 264 B    | 264 B    | 0 B     | EEPROM_BANDS_START             | 44 bands state (44 * 6B)
//
// 290 - 319     | 30 B     | 30 B     | 0 B     | EEPROM_SETTINGS_START          | Settings params (SETTINGS_MAX=30)
//
// 320 - 328     | 9 B      | 9 B      | 0 B     | EEPROM_MODE_SETTINGS_START     | Mode-dependent settings (3*3)
//
// 329 - 428     | 100 B    | 100 B    | 0 B     | EEPROM_FAVORITES_START         | Favorites (20 * 5B)  (*see note)
// 429           | 1 B      | 1 B      | 0 B     | EEPROM_FAVORITES_COUNT         | Favorite count
//
// 430 - 1023    | 594 B    | 0 B      | 594 B   | (free)                         | Free space (ATmega328P EEPROM)
//
// =================================================================================================


// =================================================================================================
// --------------- Core EEPROM Validation ----------------------------------------------------------
// =================================================================================================

constexpr auto EEPROM_APP_ID = 235;
constexpr auto EEPROM_APP_ID_ADDRESS = 0;
constexpr auto EEPROM_VERSION_ADDRESS = 1;


// =================================================================================================
// --------------- EEPROM Block Start Addresses ----------------------------------------------------
// =================================================================================================
// ReceiverHeader:      7 bytes
// BandStatePacked:     6 bytes (x44 bands = 264 bytes)
// Settings:            30 bytes used (SETTINGS_MAX = 30 items * 1 byte each)
// ModeSettings:        9 bytes (MODE_SETTINGS_COUNT * MODE_CONTEXT_COUNT = 3 * 3)
// FavoriteStation:     5 bytes (x20 stations = 100 bytes)
// FavoritesCount:      1 byte

constexpr auto EEPROM_HEADER_START = 10;                // Used: 7B   (reserved block is 16B: 10..25)
constexpr auto EEPROM_BANDS_START = 26;                 // Used: 264B (26..289)
constexpr auto EEPROM_SETTINGS_START = 290;             // Used: 30B  (290..319)
constexpr auto EEPROM_MODE_SETTINGS_START = 320;        // Used: 9B   (320..328)
constexpr auto EEPROM_FAVORITES_START = 329;            // Used: 100B (329..428)
constexpr auto EEPROM_FAVORITES_COUNT = 429;            // Used: 1B   (429)

// Increment APP_VERSION to force EEPROM reset due to layout changes
constexpr auto APP_VERSION = 71;


// =================================================================================================
// --------------- UI Strings ----------------------------------------------------------------------
// =================================================================================================

#define APP_NAME_LINE1 F("ATS-20+ V7.1.1")
#define APP_NAME_LINE2 F("ATS EX")

#define APP_SPLASH_CREDITS_TEXT  APP_SPLASH_PAD "MOD_NO_RDS github.com/diqezit/ats20_ats_ex"

// scroll timing (ms) bigger = slower / longer
#define SPLASH_CREDITS_STEP_MS   140
#define SPLASH_CREDITS_TOTAL_MS  4000
#define APP_SPLASH_PAD "\x01\x01\x01\x01\x01"

// =================================================================================================
// --------------- I2C / Bus -----------------------------------------------------------------------
// =================================================================================================

// I2C SCL base rate for the shared bus OLED and Si4735
//
// 61.5 kHz keeps main harmonics away from the LW band and common 9 kHz channel centers
// At 16 MHz TWBR 122 gives real SCL about 61.5 kHz
constexpr uint32_t I2C_BASE_HZ = 61500UL;


// =================================================================================================
// --------------- Runtime Timing / Behavior -------------------------------------------------------
// =================================================================================================

constexpr auto SAVE_ON_IDLE_TIMEOUT = 15000UL;          // 15 seconds
constexpr auto DEFAULT_VOLUME = 25;

constexpr auto ADJUSTMENT_ACTIVE_TIMEOUT = 3000;
constexpr auto SETTINGS_MENU_TIMEOUT = 10000UL;

#define BAND_DELAY 2
constexpr auto MIN_ELAPSED_TIME = 100;

constexpr auto SEEK_TIME = 65535UL;                     // 65535 ms = 65.535 seconds

// RSSI / stereo polling timing
constexpr auto RSSI_POLL_INTERVAL_MS = 1000UL;          // How often to check RSSI/Stereo when idle.
constexpr auto RSSI_POLL_DELAY_AFTER_TUNE_MS = 500UL;   // Debounce delay after tuning to let signal settle.

// Encoder stop delay before committing the new frequency to the chip
constexpr auto FREQ_UPDATE_DELAY_MS = 50UL;

// If the frequency jump is large send immediately
constexpr auto FREQ_FORCE_UPDATE_THRESHOLD_KHZ = 50;

// Minimum time between setFrequency calls to protect I2C and CTS polling
constexpr auto MIN_SETFREQ_INTERVAL_MS = 40UL;


// =================================================================================================
// --------------- Hardware Pins -------------------------------------------------------------------
// =================================================================================================

// Display
#define RST_PIN   -1
#define RESET_PIN 12

// Amplifier MD8002A control
#define AMP_DDR   DDRC
#define AMP_PORT  PORTC
#define AMP_BIT   3

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
#define STEP_BUTTON      10
#define AGC_BUTTON       11
#define ENCODER_BUTTON   14


// =================================================================================================
// --------------- Compile-Time Switches -----------------------------------------------------------
// =================================================================================================

// Display options
#define ENABLE_SPLASH_SCREEN    1       // 1=show splash screen, 0=disable
#define ENABLE_EEPROM_RESET_MSG 1       // 1=show EEPROM reset message, 0=disable
#define ANIMATE_SPLASH          1       // 1=animate splash, 0=disable
#define ENABLE_SPLASH_CREDITS_SCROLL 1  // splash credits (scrolling) set 0 to disable

// Features
#define ENABLE_FAVORITES 1         // 1=Favorites enabled

#define ENABLE_BATTERY_MONITOR 1   // 1=battery monitor enabled

// Debug (see Utils.h)
#define DEBUG_MODE 0               // 1=enable debug output (9600 baud), 0=disable

// Test / experimental flag (project-specific)
#define TEST 1                     // Enables temporary test code paths / experiments

// SSB patch compression
#define PATCH_EX_SSB 1             // 1=highly compressed patch loader (recommended)

// EXPERIMENTAL: CW (Morse) decoder view
#define ENABLE_CW_DECODER 0

#define ENABLE_RDS_MINI  1         // 1=RDS RadioText on FM (requires ~500B Flash)

#define ENABLE_GAME 0              // 1=enable Pong mini game, 0=disable

#define ENABLE_SIGNAL_BAR 1        // 1=draw thin RSSI bar above frequency, 0=disable (~260B Flash)

// =================================================================================================
// --------------- Audio Enhancement Profile Constants ---------------------------------------------
// =================================================================================================
// Values are derived from experimental testing and the Si47XX AN332 programming guide


// -------------------------------------------------------------------------------------------------
// FM: Aggressive Soft Mute for Quiet Tuning
// -------------------------------------------------------------------------------------------------

// Property 0x1300: FM_SOFTMUTE_RATE
// Sets mute/unmute speed.
constexpr uint8_t FM_PROP_SOFTMUTE_RATE = 255;                 // Default: 64. Range: 1-255

// Property 0x1301: FM_SOFTMUTE_SLOPE
// Attenuation slope (dB attenuation per 1 dB SNR drop).
constexpr uint8_t FM_PROP_SOFTMUTE_SLOPE = 4;                  // Default: 2. Range: 0-63

// Property addresses for user-configurable soft mute values
#define FM_PROP_SOFTMUTE_MAX_ATTN_ADDR   0x1302
#define FM_PROP_SOFTMUTE_SNR_THRESH_ADDR 0x1303

// Property 0x1304 & 0x1305: FM_SOFTMUTE_RELEASE/ATTACK_RATE
// Undocumented in some docs, used here as an audio profile tweak.
constexpr uint16_t FM_PROP_SOFTMUTE_REL_RATE = 32700;
constexpr uint16_t FM_PROP_SOFTMUTE_ATT_RATE = 32700;

// FM Soft Mute defaults (AN332 safe defaults)
// Used as fallback when EEPROM contains invalid data (0xFF)
constexpr uint8_t FM_SOFT_MUTE_DEFAULT_ATT = 16;               // dB attenuation
constexpr uint8_t FM_SOFT_MUTE_DEFAULT_THR = 0;                // dB SNR threshold


// -------------------------------------------------------------------------------------------------
// FM: Hi-Cut Filter (AN332 mapping)
// -------------------------------------------------------------------------------------------------
// NOTE (AN332):
// - There is NO separate "enable" property at 0x1A00.
// - Hi-Cut is disabled when FM_HICUT_CUTOFF_FREQUENCY FREQ[2:0] == 0.
// - Properties 0x1A00..0x1A06 are fixed and must be written with reserved bits = 0.

// AN332 Hi-Cut property addresses
constexpr uint16_t FM_HICUT_SNR_HIGH_THRESHOLD_PROP = 0x1A00;   // default 24 dB
constexpr uint16_t FM_HICUT_SNR_LOW_THRESHOLD_PROP  = 0x1A01;   // default 15 dB
constexpr uint16_t FM_HICUT_ATTACK_RATE_PROP        = 0x1A02;   // default 0x4E20 (~3 ms)
constexpr uint16_t FM_HICUT_RELEASE_RATE_PROP       = 0x1A03;   // default 0x0014 (~3.3 s)
constexpr uint16_t FM_HICUT_MP_TRIGGER_PROP         = 0x1A04;   // default 20 %
constexpr uint16_t FM_HICUT_MP_END_PROP             = 0x1A05;   // default 60 %
constexpr uint16_t FM_HICUT_CUTOFF_PROP             = 0x1A06;   // default 0x0000 (disabled)

// AN332 defaults (values)
constexpr uint16_t FM_HICUT_SNR_HIGH_DEFAULT = 24;             // 0x0018
constexpr uint16_t FM_HICUT_SNR_LOW_DEFAULT  = 15;             // 0x000F
constexpr uint16_t FM_HICUT_ATTACK_DEFAULT   = 0x4E20;         // 20000 (~3 ms)

// 0x1A03: FM_HICUT_RELEASE_RATE (AN332 default 0x0014 ~3.3s)
constexpr uint16_t FM_HICUT_RELEASE_DEFAULT = 0x0014;

// 0x1A04 / 0x1A05: Multipath thresholds (AN332 defaults)
constexpr uint16_t FM_HICUT_MP_TRIGGER_DEFAULT = 20;
constexpr uint16_t FM_HICUT_MP_END_DEFAULT     = 60;

// 0x1A06: FM_HICUT_CUTOFF_FREQUENCY
// Bits 6:4 = MAXIMUM_AUDIO_FREQUENCY (0..7)
// Bits 2:0 = HICUT_TRANSITION_FREQUENCY (0..7); 0 disables Hi-Cut
//
// 0x0055 = (MAX_AUDIO=5 -> 6 kHz), (HICUT=5 -> 6 kHz), Hi-Cut enabled (since HICUT != 0)
constexpr uint16_t FM_PROP_HICUT_CUTOFF = 0x0055;              // Default: 0x0000 (Disabled)


// Legacy names kept for compatibility with older code paths that treated these as "profile values".
// They are NOT property addresses; use FM_HICUT_*_PROP constants for addresses.
//
// If your RadioControl.h uses the old table:
//   {0x1A00, FM_PROP_HICUT_ENABLE}
//   {0x1A01, FM_PROP_HICUT_WINDOW}
//   {0x1A02, FM_PROP_HICUT_SNR_THRESH}
// then these values will map correctly to AN332 fields.
constexpr uint16_t FM_PROP_HICUT_ENABLE    = 127;              // legacy alias: SNR_HIGH threshold (profile)
constexpr uint16_t FM_PROP_HICUT_WINDOW    = 127;              // legacy alias: SNR_LOW threshold  (profile)
constexpr uint16_t FM_PROP_HICUT_SNR_THRESH = FM_HICUT_ATTACK_DEFAULT; // legacy alias: ATTACK rate (profile)


// -------------------------------------------------------------------------------------------------
// FM: Experimental Noise Blanker (Si4742/43/44/45 class parts; may be ignored on some silicon)
// -------------------------------------------------------------------------------------------------

constexpr uint16_t FM_PROP_NB_REJ_THRESH       = 16;            // 0x1900
constexpr uint16_t FM_PROP_NB_ATT_RATE         = 24;            // 0x1901
constexpr uint16_t FM_PROP_NB_REL_RATE         = 64;            // 0x1902
constexpr uint16_t FM_PROP_NB_ADC_OVER_THRESH  = 300;           // 0x1903
constexpr uint16_t FM_PROP_NB_ADC_OVER_DELAY   = 170;           // 0x1904


// -------------------------------------------------------------------------------------------------
// AM: Experimental Noise Blanker (NB)
// -------------------------------------------------------------------------------------------------

#define AM_NB_DETECT_THRESHOLD_PROP  0x3900
#define AM_NB_INTERVAL_PROP          0x3901
#define AM_NB_RATE_PROP              0x3902
#define AM_NB_IIR_FILTER_PROP        0x3903
#define AM_NB_DELAY_PROP             0x3904

constexpr auto AM_NB_THRESHOLD_DEFAULT   = 12;
constexpr auto AM_NB_INTERVAL_DEFAULT    = 55;
constexpr auto AM_NB_RATE_DEFAULT        = 64;
constexpr auto AM_NB_IIR_FILTER_DEFAULT  = 300;
constexpr auto AM_NB_DELAY_DEFAULT       = 172;


// p. 318 Rev. v 0.8
// Recommended by SiLabs for classic C40-like performance on D60 chips
constexpr uint16_t AM_SOFT_MUTE_SLOPE_PROP = 0x3301;
constexpr uint16_t AM_SOFT_MUTE_SLOPE_RECOMMENDED = 2;


// =================================================================================================
// --------------- Short-Wave AFC (AM on SW) profiles (Si47xx AN332) --------------------------------
// =================================================================================================

constexpr uint16_t AM_AFC_SW_PULL_IN_RANGE_PROP = 0x3104; // pull-in
constexpr uint16_t AM_AFC_SW_LOCK_IN_RANGE_PROP = 0x3105; // lock-in

// Profile 1 PPM defaults
constexpr uint16_t AM_AFC_SW_PULL_IN_RANGE_VAL = 0x21F7;  // 115 ppm
constexpr uint16_t AM_AFC_SW_LOCK_IN_RANGE_VAL = 0x2DF5;  // 85 ppm

// Profile IDs
constexpr uint8_t  SW_AFC_PROFILE_OFF       = 0;
constexpr uint8_t  SW_AFC_PROFILE_PPM       = 1;
constexpr uint8_t  SW_AFC_PROFILE_HZ_NORMAL = 2;
constexpr uint8_t  SW_AFC_PROFILE_HZ_AGGR   = 3;

// Fixed Hz windows
constexpr uint16_t SW_AFC_PULL_HZ_NORMAL = 1600;
constexpr uint16_t SW_AFC_LOCK_HZ_NORMAL = 1200;
constexpr uint16_t SW_AFC_PULL_HZ_AGGR   = 2000;
constexpr uint16_t SW_AFC_LOCK_HZ_AGGR   = 1500;


// =================================================================================================
// --------------- FM Multipath Blend (AN332) ------------------------------------------------------
// =================================================================================================

// 0x1808 FM_BLEND_MULTIPATH_STEREO_THRESHOLD (default 20)
constexpr uint16_t FM_MP_STEREO_THR_DEFAULT = 20;

// 0x1809 FM_BLEND_MULTIPATH_MONO_THRESHOLD (default 60)
constexpr uint16_t FM_MP_MONO_THR_DEFAULT = 60;

// 0x180A FM_BLEND_MULTIPATH_ATTACK_RATE (default 0x0FA0 ~16ms)
constexpr uint16_t FM_MP_ATTACK_DEFAULT = 0x0FA0;

// 0x180B FM_BLEND_MULTIPATH_RELEASE_RATE (default 0x0028 ~1.64s)
constexpr uint16_t FM_MP_RELEASE_DEFAULT = 0x0028;


// =================================================================================================
// --------------- FM Stereo/Mono Blend Thresholds (AN332) -----------------------------------------
// =================================================================================================

// RSSI-based blend thresholds (dBµV)
constexpr uint16_t FM_BLEND_RSSI_STEREO_THRESHOLD_PROP = 0x1800;
constexpr uint16_t FM_BLEND_RSSI_MONO_THRESHOLD_PROP   = 0x1801;
constexpr uint8_t  FM_BLEND_RSSI_STEREO_DEFAULT        = 49;
constexpr uint8_t  FM_BLEND_RSSI_MONO_DEFAULT          = 30;

// SNR-based blend thresholds (dB)
constexpr uint16_t FM_BLEND_SNR_STEREO_THRESHOLD_PROP  = 0x1804;
constexpr uint16_t FM_BLEND_SNR_MONO_THRESHOLD_PROP    = 0x1805;
constexpr uint8_t  FM_BLEND_SNR_STEREO_DEFAULT         = 27;
constexpr uint8_t  FM_BLEND_SNR_MONO_DEFAULT           = 14;

// Multipath-based blend thresholds (addresses only; values use FM_MP_* defaults)
constexpr uint16_t FM_BLEND_MULTIPATH_STEREO_THRESHOLD_PROP = 0x1808;
constexpr uint16_t FM_BLEND_MULTIPATH_MONO_THRESHOLD_PROP   = 0x1809;


// =================================================================================================
// --------------- Utility Macros ------------------------------------------------------------------
// =================================================================================================

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_INDEX(a)  (ARRAY_SIZE(a) - 1)


// =================================================================================================
// --------------- Logic and Algorithm Constants ---------------------------------------------------
// =================================================================================================

// BFO rollover threshold (Hz)
constexpr int32_t BFO_ROLLOVER_MAX_HZ = 13000;
constexpr int16_t HZ_PER_KHZ = 1000;

// Standard FM channel spacing for most regions
constexpr uint8_t FM_SEEK_SPACING_KHZ = 10;

// Short delay after setting frequency before seek
constexpr uint16_t DEFAULT_SEEK_DELAY_MS = 30;

// AM seek spacing normalization
constexpr uint8_t AM_SEEK_STEP_DEFAULT_KHZ = 5;
constexpr uint8_t AM_SEEK_STEP_MAX_KHZ     = 10;

// Seek threshold properties (AN332)
constexpr uint16_t FM_SEEK_TUNE_SNR_THRESHOLD_PROP  = 0x1403;
constexpr uint16_t FM_SEEK_TUNE_RSSI_THRESHOLD_PROP = 0x1404;

constexpr uint8_t  FM_SEEK_SNR_THRESHOLD_VAL  = 2;               // default 3
constexpr uint8_t  FM_SEEK_RSSI_THRESHOLD_VAL = 5;               // default 20

constexpr uint16_t AM_SEEK_SNR_THRESHOLD_PROP  = 0x3403;
constexpr uint16_t AM_SEEK_RSSI_THRESHOLD_PROP = 0x3404;

constexpr uint8_t  AM_SEEK_SNR_THRESHOLD_VAL  = 3;               // default 5
constexpr uint8_t  AM_SEEK_RSSI_THRESHOLD_VAL = 10;              // default 25


// =================================================================================================
// --------------- SSB/CW Parameters ---------------------------------------------------------------
// =================================================================================================

// I2C speed for SSB patch loading (Wire.setClock request)
constexpr int32_t I2C_SSB_PATCH_SPEED_HZ = 500000;

// Delay after patch power-up before download
constexpr uint16_t PATCH_LOAD_DELAY_MS = 50;

// Base hardware step for SSB
constexpr uint8_t SSB_BASE_STEP_KHZ = 1;

// DSP AFC flags (library-level)
constexpr int8_t  SSB_DSP_AFC_OFF = 1;
constexpr int8_t  SSB_DSP_AFC_ON  = 0;

// AVC divider behavior when Sync is active
constexpr int8_t  SSB_AVC_DIVIDER_SYNC_OFF = 0;
constexpr int8_t  SSB_AVC_DIVIDER_SYNC_ON  = 3;


// =================================================================================================
// --------------- Mode and State Management -------------------------------------------------------
// =================================================================================================

constexpr uint8_t INVALID_RSSI_VALUE = 255;

// FM de-emphasis (menu uses 0/1, hardware expects 1/2)
constexpr int8_t  DEEMPHASIS_50_US = 1;
constexpr int8_t  DEEMPHASIS_75_US = 2;


// =================================================================================================
// --------------- Attenuator (AGC) Settings -------------------------------------------------------
// =================================================================================================

constexpr uint8_t MAX_ATTENUATION_FM_DB = 26;
constexpr uint8_t MAX_ATTENUATION_AM_DB = 37;

constexpr uint8_t AGC_ATT_INDEX_OFFSET = 1;


// =================================================================================================
// --------------- Soft Mute Settings --------------------------------------------------------------
// =================================================================================================

constexpr uint8_t SOFT_MUTE_MAX_ATTENUATION   = 32;
constexpr uint8_t SOFT_MUTE_MAX_SNR_THRESHOLD = 63;

constexpr uint8_t FM_SOFT_MUTE_MAX_ATTN_LEVEL = 31;
constexpr uint8_t FM_SOFT_MUTE_MAX_SNR_LEVEL  = 15;


// =================================================================================================
// --------------- Squelch Settings ----------------------------------------------------------------
// =================================================================================================

constexpr uint8_t SQUELCH_MAX_LEVEL = 60;


// =================================================================================================
// --------------- AVC Settings --------------------------------------------------------------------
// =================================================================================================

constexpr uint8_t AVC_MAX_INDEX = 10;
constexpr uint8_t AVC_MIN_INDEX = 0;


// =================================================================================================
// --------------- BFO Calibration -----------------------------------------------------------------
// =================================================================================================

constexpr int8_t  BFO_CALIBRATION_MIN = -25;            // in 100Hz steps
constexpr int8_t  BFO_CALIBRATION_MAX = 25;             // in 100Hz steps

constexpr int16_t BFO_CALIBRATION_MULTIPLIER = 100;


// =================================================================================================
// --------------- Cutoff Filter -------------------------------------------------------------------
// =================================================================================================

constexpr int8_t CUTOFF_FILTER_MAX_VALUE = 2;

constexpr int8_t CUTOFF_FILTER_AUDIO_TAPERED = 1;
constexpr int8_t CUTOFF_FILTER_HISS_REDUCED  = 0;


// =================================================================================================
// --------------- Display & CPU -------------------------------------------------------------------
// =================================================================================================

constexpr uint8_t BRIGHTNESS_MAX_LEVEL = 9;
constexpr uint8_t DISPLAY_OFF_TIMER_MAX_LEVEL = 4;

// CPU prescaler for deep sleep on display timeout
constexpr uint8_t CPU_PRESCALER_DEEP_SLEEP = 3;         // 2 MHz


// =================================================================================================
// --------------- Hardware and System -------------------------------------------------------------
// =================================================================================================

constexpr uint16_t ADC_CONNECTED_THRESHOLD = 300;
constexpr uint16_t SYSTEM_INIT_DELAY_MS = 500;

// Toggle between LSB (1) and USB (2)
constexpr int8_t SIDEBAND_TOGGLE_LSB_USB = 3;
