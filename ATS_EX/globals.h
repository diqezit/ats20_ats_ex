#pragma once

// =================================================================================================
// Enumerations
// =================================================================================================

// NEW ENUM for Command Mode
enum CommandMode : uint8_t {
    CMD_NONE,
    CMD_VOLUME,
    CMD_STEP,
    CMD_BW,
    CMD_BAND,
    CMD_SLEEP
};

// Enum for convenient access to mode-dependent settings
enum ModeSettingType {
    MODE_SETTING_AGC,
    MODE_SETTING_SOFT_MUTE,
    MODE_SETTING_AVC,
    MODE_SETTINGS_COUNT // Counter for use in loops
};

enum ModeContext {
    MODE_CONTEXT_AM,
    MODE_CONTEXT_SSB,
    MODE_CONTEXT_COUNT
};

enum SettingType {
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

enum SettingsIndex {
    // --- Page 1: General & Sound ---
    ATT,
    ScanSwitch,     // SCN
    AutoVolControl, // AVC
    SoftMute,       // SM
    SoftMuteThr,    // SMT - Soft Mute SNR Threshold
    DeEmp,          // DE

    // --- Page 2: SSB & Visual ---
    BFO,
    SSM,
    SVC,
    CutoffFilter,   // COF
    Sync,           // SYN
    Brightness,     // SCR

    // --- Page 3: Hardware ---
    AntennaCap,     // CAP
    CPUSpeed,       // CPU
    BATT_PIN,       // BAP (Battery Pin Select)
    SWUnits,        // SWU
    RSSI_AM_Off,    // RSI
    DisplayOff,     // DIS (Display Off Timeout)

    SETTINGS_MAX
};

enum BandType : uint8_t {
    LW_BAND_TYPE,
    MW_BAND_TYPE,
    SW_BAND_TYPE,
    FM_BAND_TYPE
};

enum Modulations : uint8_t {
    AM,
    LSB,
    USB,
    CW,
    FM
};

// =================================================================================================
// Function Prototypes
// =================================================================================================

#if DEBUG_MODE
void initDebugUART();
void debugPrint_P(const char* str);
void debugPrintNum(int16_t num);
#endif

static void applyBandConfiguration(bool extraSSBReset = false);
static void bandSwitch(bool up, bool loadStoredFreq = true);
static void doCWSwitch();
static void applyBrightness();
static void loadSSBPatch();
static void showChargeOnDisplay();
static void showFrequencySeek(uint16_t freq);
static void resetCommandMode();
static void switchCommand(CommandMode mode);

void doAttenuation(int8_t v);
void doSoftMute(int8_t v);
void doSoftMuteThreshold(int8_t v);
void doBrightness(int8_t v);
void doSSBAVC(int8_t v = 0);
void doAvc(int8_t v);
void doSync(int8_t v = 0);
void doDeEmp(int8_t v = 0);
void doSWUnits(int8_t v = 0);
void doSSBSoftMuteMode(int8_t v = 0);
void doCutoffFilter(int8_t v);
void doCPUSpeed(int8_t v = 0);
void doBFOCalibration(int8_t v);
void doScanSwitch(int8_t v = 0);
void doRSSIAMOff(int8_t v = 0);
void doAntennaCapacitor(int8_t v = 0);
void doDisplayOff(int8_t v = 0);
void doBatteryPinSelect(int8_t v = 0);
void showSplashScreen();
void showStatus(bool cleanFreq = false);
void updateAndShowBattery(bool forceShow);
void updateStereoIndicator();

// =================================================================================================
// Macros & Constants
// =================================================================================================

// A macro to convert a 4-character string literal into a char array without a null terminator
#define PACK_STR4(s) {s[0], s[1], s[2], s[3]}

const uint8_t g_SettingsMaxPages = 3;       // pages number in settings menu
const int16_t CW_PITCH_OFFSET_HZ = 500;     // 500 Hz pitch for CW tone generation

#if ENABLE_FM_FAV
const uint8_t MAX_FM_FAVORITES = 10;
#endif

const uint8_t g_bandCount = 28;             // Number of bands for seamless coverage
const uint8_t g_lastBand = g_bandCount - 1;

// =================================================================================================
// Data Structures
// =================================================================================================

// "Source of Truth" for default values
// This is an immutable template for resetting settings
struct ModeDefaults {
    const int8_t agc;       // Default for Attenuation/AGC
    const int8_t soft_mute; // Default for Soft Mute
    const int8_t avc;       // Default for AVC Max Gain
};

struct SettingsItem {
    char name[4];
    int8_t param;
    uint8_t type;
    void (*manipulateCallback)(int8_t);
};

// defines all properties of a frequency band
// this unified structure is the core of the new elegant architecture
struct Band {
    // --- constant data, defined at compile time ---
    char name[4];
    uint16_t minimumFreq;
    uint16_t maximumFreq;
    BandType bandType;

    // --- variable state, loaded/saved to eeprom ---
    uint16_t currentFreq;
    int8_t stepIdxAM;
    int8_t stepIdxSSB;
    int8_t stepIdxFM;
    int8_t bwIdxAM;
    int8_t bwIdxSSB;
    int8_t bwIdxFM;
};

#if ENABLE_FM_FAV
struct FMFavorite {
    uint16_t frequency;
};
#endif

// Defines how a setting's parameter is converted into a text index
struct SwitchMapEntry {
    uint8_t baseIndex;
    bool inverted;                  // if true, the parameter is subtracted from the base index
};

// =================================================================================================
// Global Variables & Data Tables
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// System State & Flags
// -------------------------------------------------------------------------------------------------
long g_storeTime = millis();
bool g_voltagePinConnnected;
bool g_ssbLoaded;
bool g_stereoStatus;
bool autoDisplayOff;
bool g_displayOn = true;
volatile bool g_seekStop;    // violatile important here!
uint32_t g_lastAdjustmentTime;
uint16_t g_lastUserActivityTime; // time of the last user frequency change (IN SECONDS)
bool g_stateIsDirty;         // indicate if the state needs saving on idle

// -------------------------------------------------------------------------------------------------
// UI & Command State
// -------------------------------------------------------------------------------------------------
volatile CommandMode g_activeCommand;
bool g_settingsActive;
bool g_settingsDirty;
int8_t g_SettingSelected;
int8_t g_SettingsPage = 1;
bool g_SettingEditing;

#if ENABLE_FM_FAV
bool g_favoritesActive;
bool g_favoritesDirty;
uint8_t g_favoriteSelected;
uint8_t g_totalFavorites;
#endif

// -------------------------------------------------------------------------------------------------
// Radio State
// -------------------------------------------------------------------------------------------------
uint8_t g_signalQualityValue = 255; // Unified value for RSSI (all modes). 255 = invalidated.
uint32_t g_lastRSSIUpdate;
uint8_t g_muteVolume;
uint8_t g_volume = DEFAULT_VOLUME;
volatile uint8_t g_currentMode = FM;
int g_currentBFO;
extern uint8_t g_stableBatteryPercent;  // store table percentage for display
uint8_t g_lastSsbMode = LSB;            // last used sideband (LSB or USB)
uint8_t g_lastCWMode = LSB;             // last used CW sideband (LSB/USB)

//Frequency tracking
uint16_t g_currentFrequency;
uint16_t g_previousFrequency;
uint16_t g_lastSavedFrequency;
uint8_t g_seekDirection = 1;

//Special logic for fast and responsive frequency surfing
uint32_t g_lastFreqChange;
bool g_processFreqChange;
uint32_t g_lastSetFreqTime;

// -------------------------------------------------------------------------------------------------
// Encoder & Buttons
// -------------------------------------------------------------------------------------------------
volatile int g_encoderCount;
int g_safeEncoderMovement;

SimpleButton  btn_Bandwidth(BANDWIDTH_BUTTON);
SimpleButton  btn_BandUp(BAND_BUTTON);
SimpleButton  btn_BandDn(SOFTMUTE_BUTTON);
SimpleButton  btn_VolumeUp(VOLUME_BUTTON);
SimpleButton  btn_VolumeDn(AVC_BUTTON);
SimpleButton  btn_Encoder(ENCODER_BUTTON);
SimpleButton  btn_AGC(AGC_BUTTON);
SimpleButton  btn_Step(STEP_BUTTON);
SimpleButton  btn_Mode(MODE_SWITCH);

Rotary g_encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735_fixed g_si4735;

// -------------------------------------------------------------------------------------------------
// Mode-Dependent Settings
// -------------------------------------------------------------------------------------------------
// Source for default values, centralized here
const ModeDefaults defaultModeSettings[MODE_CONTEXT_COUNT] = {
    // [MODE_CONTEXT_AM]
    {.agc = 0, .soft_mute = 0, .avc = 90 },
    // [MODE_CONTEXT_SSB]
    {.agc = 0, .soft_mute = 0, .avc = 90 }
};

// "Live State" storage for mode-dependent settings
// This array is loaded from and saved to EEPROM
int8_t g_modeSettings[MODE_SETTINGS_COUNT][MODE_CONTEXT_COUNT];

// -------------------------------------------------------------------------------------------------
// General Settings
// -------------------------------------------------------------------------------------------------
// "UI Buffer" - A temporary buffer for the settings UI, stored in RAM
// It holds the live state of settings while the user is in the menu
// This buffer is populated from g_modeSettings upon entering the menu
// The initial values here are defaults and will be overwritten
SettingsItem g_Settings[] =
{
    // Page 1
    { "ATT", 0,  SettingType::ZeroAuto,   doAttenuation       },
    { "SCN", 1,  SettingType::Switch,     doScanSwitch        },
    { "AVC", 90, SettingType::Num,        doAvc               },
    { "SMA", 0,  SettingType::Num,        doSoftMute          },
    { "SMT", 0,  SettingType::Num,        doSoftMuteThreshold },
    { "DE ", 1,  SettingType::Switch,     doDeEmp             },

    // Page 2
    { "BFO", 0,  SettingType::Num,        doBFOCalibration    },
    { "SSM", 1,  SettingType::Switch,     doSSBSoftMuteMode   },
    { "SVC", 1,  SettingType::Switch,     doSSBAVC            },
    { "COF", 0,  SettingType::SwitchAuto, doCutoffFilter      },
    { "SYN", 0,  SettingType::Switch,     doSync              },
    { "SCR", 4,  SettingType::Num,        doBrightness        },

    // Page 3
    { "CAP", 0,  SettingType::Switch,     doAntennaCapacitor  },
    { "CPU", 0,  SettingType::Switch,     doCPUSpeed          },
    { "BAP", 0,  SettingType::Switch,     doBatteryPinSelect  },
    { "SWU", 0,  SettingType::Switch,     doSWUnits           },
    { "RSI", 1,  SettingType::Switch,     doRSSIAMOff         },
    { "DIS", 0,  SettingType::Switch,     doDisplayOff        },
};

// defines the text conversion rules ONLY for settings of type 'Switch'
// it is indexed here by the SettingsIndex enum
const PROGMEM SwitchMapEntry switch_setting_map[] = {
    // Page 1
    [ATT] =                 {0, false},
    [ScanSwitch] =          {2, true},
    [AutoVolControl] =      {0, false},
    [SoftMute] =            {0, false},
    [SoftMuteThr] =         {0, false},
    [DeEmp] =               {3, false},
    // Page 2
    [BFO] =                 {0, false},
    [SSM] =                 {7, false},
    [SVC] =                 {2, true},
    [CutoffFilter] =        {0, false},
    [Sync] =                {2, true},
    [Brightness] =          {0, false},
    // Page 3
    [AntennaCap] =          {1, false},
    [CPUSpeed] =            {9, false},
    [BATT_PIN] =            {0, false},
    [SWUnits] =             {5, false},
    [RSSI_AM_Off] =         {1, true},
    [DisplayOff] =          {0, false},
};

// -------------------------------------------------------------------------------------------------
// Band Definitions
// -------------------------------------------------------------------------------------------------

// SW sub band limits for seek
constexpr uint16_t SW_MIN_FREQ = 1710;
constexpr uint16_t SW_MAX_FREQ = 30000;

// we use an index to track the current band. band index 1 is mw.
int8_t g_bandIndex = 1;

// this array is now the single source of truth for all bands (reduse size flash too now)
// it is defined here directly
Band g_bandList[g_bandCount] = {
    // name,         min_freq,  max_freq, band_type,    current_freq, stepAM, stepSSB, stepFM, bwAM, bwSSB, bwFM
    { PACK_STR4("LW  "),   150,       521, LW_BAND_TYPE,   300,        2,      4,       1,      4,    4,     0 },
    { PACK_STR4("MW  "),   522,      1710, MW_BAND_TYPE,   522,        2,      4,       1,      4,    4,     0 },
    // --- SW sub bands ---
    { PACK_STR4("SW  "),  1710,      1810, SW_BAND_TYPE,  1750,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("160M"),  1810,      2000, SW_BAND_TYPE,  1850,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  2000,      2300, SW_BAND_TYPE,  2150,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("120M"),  2300,      2500, SW_BAND_TYPE,  2400,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  2500,      3200, SW_BAND_TYPE,  2800,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("90M "),  3200,      3400, SW_BAND_TYPE,  3300,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  3400,      3500, SW_BAND_TYPE,  3450,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("80M "),  3500,      3900, SW_BAND_TYPE,  3700,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("75M "),  3900,      4000, SW_BAND_TYPE,  3950,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  4000,      4750, SW_BAND_TYPE,  4400,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("60M "),  4750,      5060, SW_BAND_TYPE,  4850,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  5060,      5900, SW_BAND_TYPE,  5500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("49M "),  5900,      6200, SW_BAND_TYPE,  6000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  6200,      7000, SW_BAND_TYPE,  6500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("40M "),  7000,      7200, SW_BAND_TYPE,  7100,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("41M "),  7200,      9400, SW_BAND_TYPE,  7450,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("31M "),  9400,      9900, SW_BAND_TYPE,  9600,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  9900,     11600, SW_BAND_TYPE, 11000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("25M "), 11600,     12100, SW_BAND_TYPE, 11975,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("22M "), 12100,     13870, SW_BAND_TYPE, 13700,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("19M "), 13870,     15800, SW_BAND_TYPE, 15300,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("16M "), 15800,     18100, SW_BAND_TYPE, 17700,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("15M "), 18100,     21850, SW_BAND_TYPE, 21600,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("13M "), 21850,     26100, SW_BAND_TYPE, 25800,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("11M "), 26100,     30000, SW_BAND_TYPE, 27500,        1,      4,       1,      4,    4,     0 },
    // --- FM ---
    { PACK_STR4("    "),  6400,     10800, FM_BAND_TYPE,  8400,        1,      4,       1,      4,    4,     0 }
};

// -------------------------------------------------------------------------------------------------
// Bandwidth Tables
// -------------------------------------------------------------------------------------------------

// single PROGMEM block of null-terminated UI labels
const char bw_all_data[] PROGMEM =
"0.5 KHZ\0" "1.0 KHZ\0" "1.2 KHZ\0" "1.8 KHZ\0" "2.0 KHZ\0" "2.2 KHZ\0"
"2.5 KHZ\0" "3.0 KHZ\0" "4.0 KHZ\0" "6.0 KHZ\0" " AUTO  \0" "110 KHZ\0"
"84 KHZ \0" "60 KHZ \0" "40 KHZ \0";

// Maps UI index to an offset in `bw_all_data`
// Using 1-byte offsets (vs 2-byte pointers) is a key data size optimization
// The `* 8` is for readability, reflecting the 8-byte fixed string length
const uint8_t bw_ssb_map[] PROGMEM = { 0 * 8, 1 * 8, 2 * 8, 5 * 8, 7 * 8, 8 * 8 };
const uint8_t bw_am_map[]  PROGMEM = { 1 * 8, 3 * 8, 4 * 8, 6 * 8, 7 * 8, 8 * 8, 9 * 8 };
const uint8_t bw_fm_map[]  PROGMEM = { 10 * 8, 11 * 8, 12 * 8, 13 * 8, 14 * 8 };

// Hardware control: Translates a UI index to the Si473x register value
// Order is fixed by the chip API, not the UI display order
const uint8_t g_bwSSBIdx[] = { 4, 5, 0, 1, 2, 3 };
const uint8_t g_bwSSBMaxIdx = 5;
const uint8_t g_maxFilterAM = 6;
const uint8_t g_bwAMIdx[] = { 4, 5, 3, 6, 2, 1, 0 };

// -------------------------------------------------------------------------------------------------
// Tuning Step Tables
// -------------------------------------------------------------------------------------------------

// step strings padded to 4 chars to reduce mem usage
static const char step_lookup_table[][7] PROGMEM = {
    // AM Steps (indices 0-6)
    "1 KHZ ", "5 KHZ ", "9 KHZ ", "10 KHZ", "50 KHZ", "100KHZ", "1 MHZ ",
    // SSB Steps (indices 7-15)
    "10 HZ ", "25 HZ ", "50 HZ ", "100 HZ", "500 HZ", "1 KHZ ", "5 KHZ ", "9 KHZ ", "10 KHZ"
};

// Array with tuning steps. The structure is defined like - AM (in kHz), then SSB (in Hz)
int g_tabStep[] =
{
    // AM steps in KHz (Indices 0-6)
    1, 5, 9, 10,
    // Large AM steps in KHz
    50, 100, 1000,
    // SSB steps in Hz (Indices 7-15)
    10, 25, 50, 100, 500,
    // SSB steps converted to Hz for seamless integration (1k, 5k, 9k, 10k)
    1000, 5000, 9000, 10000
};
const uint8_t AM_STEPS_COUNT = 7;
const uint8_t SSB_STEPS_COUNT = 9;
const uint8_t SSB_STEP_OFFSET = 7;

int8_t g_tabStepFM[] = { 5, 10, 100 };
const int8_t g_lastStepFM = (sizeof(g_tabStepFM) / sizeof(int8_t)) - 1;

// -------------------------------------------------------------------------------------------------
// UI Text & Other Data
// -------------------------------------------------------------------------------------------------
// used by SettingParamToUI function to convert parameter values to display strings
const char PROGMEM paramTexts[][4] = {
  "AUT", " ON", "OFF", "50U", "75U", "KHZ", "MHZ",
  "RSS", "SNR", "100", "50%",
  "10M", "15M", "30M", "60M"
};

// Timeout values in seconds for the display-off feature, indexed by the setting parameter
const uint16_t T[5] PROGMEM = { 0, 600, 900, 1800, 3600 };

const char g_bandModeDesc[][4] = { "AM ", "LSB", "USB", "CW ", "FM " };

#if ENABLE_FM_FAV
FMFavorite g_fmFavorites[MAX_FM_FAVORITES];
#endif
