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
// 10 - 19       | 10 B     | 6 B      | EEPROM_HEADER_START            | Global state header (volume, mode etc.)
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
constexpr auto SETTINGS_MENU_TIMEOUT = 10000UL;
#define BAND_DELAY                2
constexpr auto MIN_ELAPSED_TIME = 100;
constexpr auto SEEK_TIME = 65535UL;         // 65535 ms = 65.535 seconds

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



// =================================================================================================
// --------------- FM Audio Enhancement Profile Constants ------------------------------------------
// =================================================================================================
// Defines a curated audio profile for FM reception
// Values are derived from experimental testing from users and Si47XX AN332 programming guide

// --- Aggressive Soft Mute for Quiet Tuning ---
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

// Property 0x1302: FM_SOFT_MUTE_MAX_ATTENUATION
// Sets maximum attenuation amount when soft mute is fully engaged
// A higher value results in a deeper mute. 22dB is used for near-silence
const auto FM_PROP_SOFTMUTE_MAX_ATTN = 22;              // Default: 16 dB. Range: 0-31

// Property 0x1304 & 0x1305: FM_SOFTMUTE_ATTACK/DECAY_RATE
// Undocumented properties for fine control over attack/decay times
// High values ensure the mute engages and disengages rapidly
const auto FM_PROP_SOFTMUTE_ATT_RATE = 32700;           // Default: Unknown
const auto FM_PROP_SOFTMUTE_DEC_RATE = 32700;           // Default: Unknown

// Property 0x1303: (Undocumented, possibly release rate)
// This value was found through experimentation for a balanced response
const auto FM_PROP_SOFTMUTE_REL_RATE = 4;


// --- Hi-Cut Filter as a "Warm Sound" Equalizer ---
// Goal: Reduce high-frequency harshness to suit the small speaker
// Mechanism: The dynamic hi-cut filter is re-purposed as a static audio filter
// It is forced active to tailor audio output for the speaker's physical limitations
// Safe Range: The CUTOFF value is critical. Changing it will alter the audio tone

// Property 0x1A00: FM_HICUT_ENABLE
// Enables or disables hi-cut functionality. Forced ON to act as an EQ.
const auto FM_PROP_HICUT_ENABLE = 1;                    // 1 = On, 0 = Off

// Property 0x1A02: FM_HICUT_SNR_HIGH_THRESHOLD
// SNR level where the hi-cut filter starts to engage
// A low value ensures the filter is active on almost all signals for a consistent audio profile
const auto FM_PROP_HICUT_SNR_THRESH = 10;               // Default: 24 dB. Range: 0-127

// Property 0x1A06: FM_HICUT_CUTOFF_FREQUENCY
// This setting controls the audio tone. It has two parts:
// - Bits 6:4: Maximum Audio Frequency. Sets a hard limit on the audio path.
// - Bits 2:0: Hi-Cut Transition Frequency. Sets frequency for filter attenuation.
// Value 0x0055 (binary ...0101 0101) translates to:
// - Max Audio = 2 (3 kHz). Audio above 3 kHz is sharply cut. This removes piercing highs.
// - Hi-Cut Freq = 5 (5 kHz). This softens upper mid-range frequencies.
const auto FM_PROP_HICUT_CUTOFF = 0x0055;               // Default: 0x0000 (Disabled)

// Other Hi-Cut properties for filter behavior. These values ensure a fast and stable response.
const auto FM_PROP_HICUT_WINDOW = 1;                    // Property 0x1A01. Filter response parameter
const auto FM_PROP_HICUT_ATT_RATE = 32760;              // Property 0x1A03. Fast attack rate
const auto FM_PROP_HICUT_REL_RATE = 1;                  // Property 0x1A04. (Undocumented)
const auto FM_PROP_HICUT_MPX_THRESH = 100;              // Property 0x1A05. (Undocumented)


// --- Experimental Noise Blanker ---
// Potentially reduce impulse noise from sources like car ignitions
// These properties configure a digital filter to detect and suppress short noise spikes
// The feature is undocumented for Si473x but present in related chips
// These values are experimental. Safest fallback is setting all to 0
// Testing shows no audio degradation when enabled
const auto FM_PROP_NB_REJ_THRESH = 0;                   // Property 0x1900
const auto FM_PROP_NB_ATT_RATE = 48;                    // Property 0x1901
const auto FM_PROP_NB_REL_RATE = 64;                    // Property 0x1902
const auto FM_PROP_NB_ADC_OVER_THRESH = 300;            // Property 0x1903
const auto FM_PROP_NB_ADC_OVER_DELAY = 125;             // Property 0x1904
