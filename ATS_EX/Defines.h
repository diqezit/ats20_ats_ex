#pragma once

// =================================================================================================
// Defines.h - Global Constants, Memory Map, and Compile-Time Switches
// =================================================================================================
// EEPROM Memory Map
// This map defines the layout for all persistent data.
// A compact layout saves space and simplifies block operations.
// There must be a gap of 10 bytes between sections
//
// Address Range | Allotted | Used     | Free    | Symbol(s)                      | Description
//---------------|----------|----------|---------|--------------------------------|----------------------------------
// 0             | 1 B      | 1 B      | 0 B     | EEPROM_APP_ID_ADDRESS          | Custom ID to validate data
// 1             | 1 B      | 1 B      | 0 B     | EEPROM_VERSION_ADDRESS         | Firmware version for compatibility
//
// 10 - 25       | 16 B     | 6 B      | 10 B    | EEPROM_HEADER_START            | Global state header
//
// 26 - 193      | 168 B    | 168 B    | 0 B     | EEPROM_BANDS_START             | 28 bands state (28 * 6)
//
// 204 - 232     | 29 B     | 29 B     | 0 B     | EEPROM_SETTINGS_START          | g_Settings (29 items)
//
// 243 - 251     | 9 B      | 9 B      | 0 B     | EEPROM_MODE_SETTINGS_START     | Mode-dependent settings (3*3)
//
// 261 - 360     | 100 B    | 100 B    | 0 B     | EEPROM_FAVORITES_START         | Favorites (20 * 5)
// 370           | 1 B      | 1 B      | 0 B     | EEPROM_FAVORITES_COUNT         | Favorite count
// =================================================================================================

// --- Core EEPROM Validation ---
constexpr auto EEPROM_APP_ID = 235;
constexpr auto EEPROM_APP_ID_ADDRESS = 0;
constexpr auto EEPROM_VERSION_ADDRESS = 1;

// --- Data Block Start Addresses ---
// These addresses are calculated manually to avoid include-order issues
// A gap is reserved between blocks for future expansion
//
// BLOCK SIZES FOR CALCULATION:
// ReceiverHeader:      6 bytes
// BandStatePacked:     6 bytes (x28 bands = 168 bytes)
// Settings:            29 bytes (SETTINGS_MAX = 29 items * 1 byte each)
// ModeSettings:        9 bytes (MODE_SETTINGS_COUNT * MODE_CONTEXT_COUNT = 3 * 3)
// FavoriteStation:     5 bytes (x20 stations = 100 bytes)
// FavoritesCount:      1 byte

constexpr auto EEPROM_HEADER_START = 10;                // Size: 6   End: 15
constexpr auto EEPROM_BANDS_START = 26;                 // Size: 168 End: 193
constexpr auto EEPROM_SETTINGS_START = 204;             // Size: 29  End: 232
constexpr auto EEPROM_MODE_SETTINGS_START = 243;        // Size: 9,  End: 251
constexpr auto EEPROM_FAVORITES_START = 261;            // Size: 100 End: 360
constexpr auto EEPROM_FAVORITES_COUNT = 370;            // Size: 1   End: 370

// Increment APP_VERSION to force EEPROM reset due to layout changes
constexpr auto APP_VERSION = 67;

// Centralizes user-facing strings on main screen
#define APP_NAME_LINE1 F("ATS-20+ V6.7.3")

// Behavior
constexpr auto SAVE_ON_IDLE_TIMEOUT = 15000UL;          // 15 seconds
constexpr auto DEFAULT_VOLUME = 25;
constexpr auto ADJUSTMENT_ACTIVE_TIMEOUT = 3000;
constexpr auto SETTINGS_MENU_TIMEOUT = 10000UL;
#define BAND_DELAY                2
constexpr auto MIN_ELAPSED_TIME = 100;
constexpr auto SEEK_TIME = 65535UL;                     // 65535 ms = 65.535 seconds

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
#define ANIMATE_SPLASH 1                    // Set to 1 to animate splash screen, 0 to disable

// IC options - must disable some other features to compile & work properly
#define ENABLE_FAVORITES 1                  // Set to 1 to use unified favorites, 0 to disable
#define DISABLE_FM 0                        // not implemented yet

#define ENABLE_ADVANCED_BATTERY_LOGIC 1     // Set to 1 to enable advanced battery logic, 0 to disable
#define ENABLE_BATTERY_MONITOR 1            // Set to 1 to enable battery monitoring, 0 to disable

// Debugging options - only for test purposes & port monotoring - see Utils.h
#define DEBUG_MODE 0                        // Set to 1 to enable debug mode, 0 to disable, (This use serial speed 9600)

// Set to 1 to enable the highly compressed SSB patch loading system
// This advanced method significantly reduces firmware size
// Enabled  (1) Saves 108 bytes of Flash memory compared to the original compressed patch
// Disabled (0) Falls back to a less optimized format
// Strongly recommended to keep enabled
#define PATCH_EX_SSB 1


// EXPERIMENTAL: CW (Morse code) to text decoder
// To use - at first connect speaker audio via a 2.2 µF capacitor to analog pin A6
// While in CW mode, a long-press on the MODE button will toggle the decoder view
// Must be disable (ENABLE_FAVORITES 0) at first to compile (30720 bytes)
// See CW_decoder.h for details
#define ENABLE_CW_DECODER 0


// =================================================================================================
// --------------- Audio Enhancement Profile Constants ---------------------------------------------
// =================================================================================================
// Defines curated audio profiles for both FM and AM/SSB modes
// Values are derived from experimental testing and the Si47XX AN332 programming guide

// --- FM: Aggressive Soft Mute for Quiet Tuning ---
// Eliminate static hiss when tuning between stations
// Properties are set for instant reaction and deep attenuation when SNR drops
// These values are aggressive. Reducing them will result in a softer mute
// Extremes might cause audio pumping on fading signals

// Property 0x1300: FM_SOFTMUTE_RATE
// Sets mute/unmute speed. Maximum value provides instantaneous action
const auto FM_PROP_SOFTMUTE_RATE = 255;                 // Default: 64. Range: 1-255

// Property 0x1301: FM_SOFTMUTE_SLOPE
// Configures attenuation slope (dB attenuation per 1 dB SNR drop)
// A higher value causes faster audio fade-out as signal weakens
const auto FM_PROP_SOFTMUTE_SLOPE = 4;                  // Default: 2. Range: 0-63

// Property addresses for user-configurable soft mute values
#define FM_PROP_SOFTMUTE_MAX_ATTN_ADDR   0x1302
#define FM_PROP_SOFTMUTE_SNR_THRESH_ADDR 0x1303

// Property 0x1304 & 0x1305: FM_SOFTMUTE_RELEASE/ATTACK_RATE
// These undocumented properties likely control release/attack rates
// High values are used to ensure the mute engages and disengages rapidly
const auto FM_PROP_SOFTMUTE_REL_RATE = 32700;           // Property 0x1304 (Assumed Release Rate)
const auto FM_PROP_SOFTMUTE_ATT_RATE = 32700;           // Property 0x1305 (Assumed Attack Rate)


// --- FM: Hi-Cut Filter as a "Warm Sound" Equalizer ---
// Goal: Reduce high-frequency harshness to suit the small speaker
// Mechanism: The dynamic hi-cut filter is re-purposed as a static audio filter
// It is forced active to tailor audio output for the speaker's physical limitations
// Safe Range: The CUTOFF value is critical. Changing it will alter the audio tone

// Property 0x1A00: FM_HICUT_ENABLE
// Enables or disables hi-cut functionality. Forced ON to act as an EQ.
const auto FM_PROP_HICUT_ENABLE = 1;                    // 1 = On, 0 = Off

// Property 0x1A00: FM_HICUT_SNR_HIGH_THRESHOLD
// SNR level where the hi-cut filter starts to engage
// A low value ensures the filter is active on almost all signals for a consistent audio profile
const auto FM_PROP_HICUT_SNR_THRESH = 10;               // Property Address 0x1A00, Default: 24 dB

// Property 0x1A06: FM_HICUT_CUTOFF_FREQUENCY
// This setting controls the audio tone. It has two parts:
// - Bits 6:4: Maximum Audio Frequency. Sets a hard limit on the audio path
// - Bits 2:0: Hi-Cut Transition Frequency. Sets frequency for filter attenuation
// Value 0x0055 (binary ...0101 0101) translates to:
// - Max Audio = 2 (3 kHz). Audio above 3 kHz is sharply cut. This removes piercing highs
// - Hi-Cut Freq = 5 (6 kHz). This softens upper mid-range frequencies
const auto FM_PROP_HICUT_CUTOFF = 0x0055;               // Property Address 0x1A06, Default: 0x0000 (Disabled)

// Other Hi-Cut properties that define filter behavior. These values ensure a fast,
// stable response when the filter is active, preventing audio pumping or instability
const auto FM_PROP_HICUT_WINDOW = 1;                    // Property 0x1A01: Sets filter response window for stability
const auto FM_PROP_HICUT_ATT_RATE = 32760;              // Property 0x1A02: Fast attack rate ensures immediate filter action
const auto FM_PROP_HICUT_REL_RATE = 1;                  // Property 0x1A03: Fast release rate prevents pumping on signal recovery
const auto FM_PROP_HICUT_MPX_THRESH = 100;              // Property 0x1A05: High threshold to ignore multipath effects on filter


// --- FM: Experimental Noise Blanker ---
// Potentially reduces impulse noise from sources like car ignitions
// Default values from AN332 are used here for base configuration
const auto FM_PROP_NB_REJ_THRESH = 16;                  // Property 0x1900: Default=16dB. Threshold to detect a noise spike. Set to 0 to disable
const auto FM_PROP_NB_ATT_RATE = 24;                    // Property 0x1901: Default=24us. Duration for which the signal is blanked
const auto FM_PROP_NB_REL_RATE = 64;                    // Property 0x1902: Default=64 (6.4kHz). Max rate of blanking events
const auto FM_PROP_NB_ADC_OVER_THRESH = 300;            // Property 0x1903: Default=300 (465Hz). Bandwidth of noise floor estimator
const auto FM_PROP_NB_ADC_OVER_DELAY = 170;             // Property 0x1904: Default=170us. Delay before applying blanking


// --- AM: Experimental Noise Blanker (NB) ---
// These properties configure the Noise Blanker for AM/SSB modes
// It is disabled by default and can be enabled via the settings menu
// Default values from AN32 are used for base config
#define AM_NB_DETECT_THRESHOLD_PROP  0x3900             // Property Address for NB Threshold
#define AM_NB_INTERVAL_PROP          0x3901             // Property Address for NB Interval
#define AM_NB_RATE_PROP              0x3902             // Property Address for NB Rate
#define AM_NB_IIR_FILTER_PROP        0x3903             // Property Address for NB IIR Filter
#define AM_NB_DELAY_PROP             0x3904             // Property Address for NB Delay

// Default=12dB. Threshold for detecting an impulse noise spike
// Setting this to 0 disables the Noise Blanker feature. Range: 0-90
constexpr auto AM_NB_THRESHOLD_DEFAULT = 12;

// Default=55us. The duration for which the original audio is replaced
// with interpolated samples after a noise spike is detected. Range: 15-110
constexpr auto AM_NB_INTERVAL_DEFAULT = 55;

// Default=64 (6.4kHz). The maximum rate at which the noise blanker
// is allowed to activate, preventing excessive signal processing. Range: 1-64
constexpr auto AM_NB_RATE_DEFAULT = 64;

// Default=300 (465Hz). The bandwidth of the filter used to estimate the
// noise floor, which is the baseline for detecting spikes. Range: 300-1600
constexpr auto AM_NB_IIR_FILTER_DEFAULT = 300;

// Default=172us. The delay before the blanking is applied, allowing the
// system to accurately identify the noise impulse. Range: 125-219
constexpr auto AM_NB_DELAY_DEFAULT = 172;


// p. 318 Rev. v 0.8
// Recommended by SiLabs for classic C40-like performance on D60 chips
constexpr uint16_t AM_SOFT_MUTE_SLOPE_PROP = 0x3301;
constexpr uint16_t AM_SOFT_MUTE_SLOPE_RECOMMENDED = 2;

// =================================================================================================
// --------------- Short-Wave AFC (AM on SW) profiles (Si47xx AN332) -------------------------------
// -------------------------------------------------------------------------------------------------
//   Configure AFC pull-in and lock-in ranges on SW in AM mode to improve capture and hold
//
//   0x3104 AM_AFC_SW_PULL_IN_RANGE  pull-in range
//   0x3105 AM_AFC_SW_LOCK_IN_RANGE  lock-in range
//
// PPM defaults
//   115 ppm -> 0x21F7
//   85  ppm -> 0x2DF5
//
//   Written only when band is SW and mode is AM
//   On older silicon the write is ignored
//
// Profiles (g_Settings[SWAFC].param)
//   0 OFF
//   1 PPM defaults
//   2 Fixed Hz  pull 1600  lock 1200
//   3 Fixed Hz  pull 2000  lock 1500
//
// Fixed Hz conversion
//   reg = round(1000 * freq_kHz / window_Hz) clamped to 1..0xFFFF
//
//   Keep pull-in >= lock-in
//   Does not calibrate the dial
//   Default SWA is 0 OFF
// =================================================================================================

constexpr uint16_t AM_AFC_SW_PULL_IN_RANGE_PROP = 0x3104; // pull-in
constexpr uint16_t AM_AFC_SW_LOCK_IN_RANGE_PROP = 0x3105; // lock-in

// Profile 1 PPM defaults
constexpr uint16_t AM_AFC_SW_PULL_IN_RANGE_VAL = 0x21F7;  // 8695
constexpr uint16_t AM_AFC_SW_LOCK_IN_RANGE_VAL = 0x2DF5;  // 11765

// Profile IDs
constexpr uint8_t  SW_AFC_PROFILE_OFF = 0;
constexpr uint8_t  SW_AFC_PROFILE_PPM = 1;
constexpr uint8_t  SW_AFC_PROFILE_HZ_NORMAL = 2;
constexpr uint8_t  SW_AFC_PROFILE_HZ_AGGR = 3;

// Fixed Hz windows
constexpr uint16_t SW_AFC_PULL_HZ_NORMAL = 1600;
constexpr uint16_t SW_AFC_LOCK_HZ_NORMAL = 1200;
constexpr uint16_t SW_AFC_PULL_HZ_AGGR = 2000;
constexpr uint16_t SW_AFC_LOCK_HZ_AGGR = 1500;

// FM multipath blending: automatic cleanup when reflections (multipath) break the stereo image
// No manual mono/stereo switching needed; in good conditions it is transparent
// Works by measuring reflections as MULT (0..100) and blending stereo to mono when MULT is high
//
// Logic (simple):
// If MULT <= x  -> keep 100% stereo
// If x < MULT < y -> smoothly blend stereo to mono
// If MULT >= y -> force 100% mono
//
// Timing:
// Attack: speed of stereo->mono (fast to hide artifacts)
// Release: speed of mono->stereo (slower to avoid pumping)
//
// MULT readout and multipath interrupts are guaranteed on D60 only

// 0x1808 FM_BLEND_MULTIPATH_STEREO_THRESHOLD
// below => 100% stereo
// above => start blend
// Default=20 (0x0014) Range=0–100
const auto FM_MP_STEREO_THR_DEFAULT = 20;

// 0x1809 FM_BLEND_MULTIPATH_MONO_THRESHOLD
// above => 100% mono
// Default=60 (0x003C)
// Range=0–100
const auto FM_MP_MONO_THR_DEFAULT = 60;

// 0x180A FM_BLEND_MULTIPATH_ATTACK_RATE
// stereo→mono attack ATTACK=65536/time_ms
// Default=0x0FA0 (~16 ms)
// Range=0 (disabled), 1–32767
const auto FM_MP_ATTACK_DEFAULT = 0x0FA0;

// 0x1A03 FM_HICUT_RELEASE_RATE
// rate to increase hi‑cut transition freq
// RELEASE=65536/time_ms
// Default=0x0014 (~3.3 s)
// Range=0 (disabled), 1–32767
const auto FM_HICUT_RELEASE_DEFAULT = 0x0014;

// 0x180B FM_BLEND_MULTIPATH_RELEASE_RATE mono→stereo release
// RELEASE=65536/time_ms
// Default=0x0028 (~1.64 s)
// Range=0 (disabled), 1–32767
const auto FM_MP_RELEASE_DEFAULT = 0x0028;

// 0x1A04 FM_HICUT_MULTIPATH_TRIGGER_THRESHOLD
// MULT at which hi‑cut starts band‑limiting
// Default=20 Range=0–100
const auto FM_HICUT_MP_TRIGGER_DEFAULT = 20;

// 0x1A05 FM_HICUT_MULTIPATH_END_THRESHOLD
// MULT at which hi‑cut reaches maximum band‑limiting
// Default=60 Range=0–100
const auto FM_HICUT_MP_END_DEFAULT = 60;

// =================================================================================================
// --------------- LOGIC AND ALGORITHM CONSTANTS ---------------------------------------------------
// =================================================================================================

// --- Utility Macros ---
// Gets the number of elements in a static array
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
// Gets the last valid index of a zero-based array
#define MAX_INDEX(a) (ARRAY_SIZE(a) - 1)


// --- Tuning and Seek Parameters ---

// Defines max BFO deviation before it rolls over into main frequency for seamless tuning
constexpr int32_t BFO_ROLLOVER_MAX_HZ = 13000;
constexpr int16_t HZ_PER_KHZ = 1000;

// Standard FM channel spacing for most regions
constexpr uint8_t FM_SEEK_SPACING_KHZ = 10;

// Short delay after setting frequency to allow hardware to settle before seeking
constexpr uint16_t DEFAULT_SEEK_DELAY_MS = 30;

// Default to 5kHz for AM seek if user-selected step is not hardware-supported
constexpr uint8_t AM_SEEK_STEP_DEFAULT_KHZ = 5;
constexpr uint8_t AM_SEEK_STEP_MAX_KHZ = 10;

// See AN332 for property details
constexpr uint16_t FM_SEEK_TUNE_SNR_THRESHOLD_PROP = 0x1403;
constexpr uint16_t FM_SEEK_TUNE_RSSI_THRESHOLD_PROP = 0x1404;

// Lower thresholds help find weaker stations during seek
constexpr uint8_t  FM_SEEK_SNR_THRESHOLD_VAL = 2;               // Default: 3
constexpr uint8_t  FM_SEEK_RSSI_THRESHOLD_VAL = 5;              // Default: 20
constexpr uint16_t AM_SEEK_SNR_THRESHOLD_PROP = 0x3403;
constexpr uint16_t AM_SEEK_RSSI_THRESHOLD_PROP = 0x3404;
constexpr uint8_t  AM_SEEK_SNR_THRESHOLD_VAL = 3;               // Default: 5
constexpr uint8_t  AM_SEEK_RSSI_THRESHOLD_VAL = 10;             // Default: 25

// --- SSB/CW Parameters ---

// I2C speed for SSB patch loading
// Higher speeds (500-800kHz) can shorten patch load time, but speeds above
// 500kHz have caused lock-ups on some chips. 500kHz is a safe default
constexpr int32_t I2C_SSB_PATCH_SPEED_HZ = 500000;

// Brief pause after power-up command before sending patch data
constexpr uint16_t PATCH_LOAD_DELAY_MS = 50;
constexpr uint8_t SSB_BASE_STEP_KHZ = 1;

// Values to toggle the Si4735 internal DSP Automatic Frequency Control
constexpr int8_t  SSB_DSP_AFC_OFF = 1;
constexpr int8_t  SSB_DSP_AFC_ON = 0;

// Values to control AVC behavior when DSP AFC (Sync) is active
constexpr int8_t  SSB_AVC_DIVIDER_SYNC_OFF = 0;
constexpr int8_t  SSB_AVC_DIVIDER_SYNC_ON = 3;

// --- Mode and State Management ---

// Special value to indicate RSSI is not currently valid or available
constexpr uint8_t INVALID_RSSI_VALUE = 255;

// FM de-emphasis values for different broadcast regions
constexpr int8_t  DEEMPHASIS_50_US = 1;                         // Europe etc
constexpr int8_t  DEEMPHASIS_75_US = 2;                         // Americas

// --- Attenuator (AGC) Settings ---

// Max hardware attenuation levels for FM and AM modes
constexpr uint8_t MAX_ATTENUATION_FM_DB = 26;
constexpr uint8_t MAX_ATTENUATION_AM_DB = 37;

// Chip attenuation index is user value minus one
constexpr uint8_t AGC_ATT_INDEX_OFFSET = 1;

// --- Soft Mute Settings ---

// Max attenuation level for AM soft mute feature
constexpr uint8_t SOFT_MUTE_MAX_ATTENUATION = 32;

// Max SNR threshold for AM soft mute activation
constexpr uint8_t SOFT_MUTE_MAX_SNR_THRESHOLD = 63;

// Max adjustment levels for FM soft mute settings
constexpr uint8_t FM_SOFT_MUTE_MAX_ATTN_LEVEL = 31;
constexpr uint8_t FM_SOFT_MUTE_MAX_SNR_LEVEL = 15;


// --- Squelch Settings ---

// Defines the maximum RSSI threshold for the Squelch setting
constexpr uint8_t SQUELCH_MAX_LEVEL = 60;

// --- AVC Settings ---

// Defines the adjustment range for Automatic Volume Control max gain
// Maps index 0-10 to IC 473x gain AVC values 12-90
// Check getAvcValueFromIndex in RadioControl.h for details
constexpr uint8_t AVC_MAX_INDEX = 10;
constexpr uint8_t AVC_MIN_INDEX = 0;

// --- BFO Calibration ---

// Range for user BFO calibration to compensate for crystal inaccuracies
constexpr int8_t  BFO_CALIBRATION_MIN = -25;            // in 100Hz steps
constexpr int8_t  BFO_CALIBRATION_MAX = 25;             // in 100Hz steps

// Multiplier to convert BFO setting param to Hz
constexpr int16_t BFO_CALIBRATION_MULTIPLIER = 100;

// --- Cutoff Filter ---

// Defines the number of available cutoff filter options
constexpr int8_t CUTOFF_FILTER_MAX_VALUE = 2;

// These are API values for setSSBSidebandCutoffFilter
constexpr int8_t CUTOFF_FILTER_AUDIO_TAPERED = 1;
constexpr int8_t CUTOFF_FILTER_HISS_REDUCED = 0;        // The more aggressive filter value

// --- Display & CPU ---

// Defines the 0-9 range for the brightness setting
constexpr uint8_t BRIGHTNESS_MAX_LEVEL = 9;

// Defines the number of available display-off timer settings
constexpr uint8_t DISPLAY_OFF_TIMER_MAX_LEVEL = 4;

// CPU prescaler for deep power save on display timeout
constexpr uint8_t CPU_PRESCALER_DEEP_SLEEP = 3;         // Corresponds to 2 MHz

// --- Hardware and System ---

// ADC threshold to detect if battery measurement pin is connected
constexpr uint16_t ADC_CONNECTED_THRESHOLD = 300;

// Delay for system initialization to allow components to stabilize
constexpr uint16_t SYSTEM_INIT_DELAY_MS = 500;

// Simple math to toggle between LSB (1) and USB (2)
constexpr int8_t   SIDEBAND_TOGGLE_LSB_USB = 3;
