#pragma once

// =================================================================================================
// Function Prototypes
// =================================================================================================

void applyBandConfiguration(bool extraSSBReset = false);
void doAttenuation(int8_t v);
void doSoftMute(int8_t v);
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
void doUnitsSwitch(int8_t v = 0);
void doScanSwitch(int8_t v = 0);
void doRSSIAMOff(int8_t v = 0);
void doCWSwitch(int8_t v = 0);
void doAntennaCapacitor(int8_t v = 0);
void showSplashScreen();
void showStatus(bool cleanFreq = false);
void updateAndShowBattery(bool forceShow);
void updateStereoIndicator();

// =================================================================================================
// Macros & Constants
// =================================================================================================

// A macro to convert a 4-character string literal into a char array without a null terminator
#define PACK_STR4(s) {s[0], s[1], s[2], s[3]}

const uint8_t g_SettingsMaxPages = 3;
const int16_t CW_PITCH_OFFSET_HZ = 500;     // 500 Hz pitch for CW tone generation
const uint8_t MAX_FM_FAVORITES = 10;
const uint8_t g_bandCount = 28;             // Number of bands for seamless coverage
const uint8_t g_lastBand = g_bandCount - 1;

// =================================================================================================
// Enumerations
// =================================================================================================

// NEW ENUM for Command Mode
enum CommandMode : uint8_t
{
    CMD_NONE,
    CMD_VOLUME,
    CMD_STEP,
    CMD_BW,
    CMD_BAND,
    CMD_SLEEP
};

// Enum for convenient access to mode-dependent settings
enum ModeSettingType
{
    MODE_SETTING_AGC,
    MODE_SETTING_SOFT_MUTE,
    MODE_SETTING_AVC,
    MODE_SETTINGS_COUNT // Counter for use in loops
};

enum ModeContext
{
    MODE_CONTEXT_AM,
    MODE_CONTEXT_SSB,
    MODE_CONTEXT_COUNT
};

enum SettingType
{
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

enum SettingsIndex
{
    ATT,
    SoftMute,
    SVC,
    Sync,
    DeEmp,
    AutoVolControl,
    Brightness,
    SWUnits,
    SSM,
    CutoffFilter,
    CPUSpeed,
    BFO,
    UnitsSwitch,
    ScanSwitch,
    CWSwitch,
    AntennaCap,
    RSSI_AM_Off,
    SETTINGS_MAX
};

enum BandType : uint8_t
{
    LW_BAND_TYPE,
    MW_BAND_TYPE,
    SW_BAND_TYPE,
    FM_BAND_TYPE
};

enum Modulations : uint8_t
{
    AM,
    LSB,
    USB,
    CW,
    FM
};

// =================================================================================================
// Data Structures
// =================================================================================================

// "Source of Truth" for default values
// This is an immutable template for resetting settings
struct ModeDefaults
{
    const int8_t agc;       // Default for Attenuation/AGC
    const int8_t soft_mute; // Default for Soft Mute
    const int8_t avc;       // Default for AVC Max Gain
};

struct SettingsItem
{
    char name[4];
    int8_t param;
    uint8_t type;
    void (*manipulateCallback)(int8_t);
};

// defines all properties of a frequency band
// this unified structure is the core of the new elegant architecture
struct Band
{
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

struct FMFavorite
{
    uint16_t frequency;
};

// Defines how a setting's parameter is converted into a text index
struct SwitchMapEntry
{
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
bool g_voltagePinConnnected = false;
bool g_ssbLoaded = false;
bool g_stereoStatus = false;
bool g_displayOn = true;
bool g_seekStop = false;
uint32_t g_lastAdjustmentTime = 0;
uint32_t g_lastUserActivityTime = 0; // time of the last user frequency change
bool g_stateIsDirty = false;         // indicate if the state needs saving on idle

// -------------------------------------------------------------------------------------------------
// UI & Command State
// -------------------------------------------------------------------------------------------------
volatile CommandMode g_activeCommand = CMD_NONE;
bool g_settingsActive = false;
bool g_settingsDirty = false;
int8_t g_SettingSelected = 0;
int8_t g_SettingsPage = 1;
bool g_SettingEditing = false;
bool g_favoritesActive = false;
bool g_favoritesDirty = false;
uint8_t g_favoriteSelected = 0;
uint8_t g_totalFavorites = 0;

// -------------------------------------------------------------------------------------------------
// Radio State
// -------------------------------------------------------------------------------------------------
bool g_forceRssiUpdate = true;      // Flag to trigger a one-time RSSI update "kick" in AM/SSB.
uint8_t g_signalQualityValue = 255; // Unified value for RSSI (all modes). 255 = invalidated.
uint32_t g_lastRSSIUpdate = 0;
uint8_t g_muteVolume = 0;
uint8_t g_volume = DEFAULT_VOLUME;
volatile uint8_t g_currentMode = FM;
volatile uint8_t g_prevMode = FM;
int g_currentBFO = 0;
extern uint8_t g_stableBatteryPercent; // store table percentage for display

//Frequency tracking
uint16_t g_currentFrequency;
uint16_t g_previousFrequency;
uint16_t g_lastSavedFrequency = 0;
uint8_t g_seekDirection = 1;

//Special logic for fast and responsive frequency surfing
uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = 0;

// -------------------------------------------------------------------------------------------------
// Encoder & Buttons
// -------------------------------------------------------------------------------------------------
volatile int g_encoderCount = 0;
int g_safeEncoderMovement = 0;

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
SI4735 g_si4735;

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
    //  { Name, Default Param,     Behavior,             Callback          }
        { "ATT", 0,            SettingType::ZeroAuto,   doAttenuation     },
        { "SM ", 0,            SettingType::Num,        doSoftMute        },
        { "SVC", 1,            SettingType::Switch,     doSSBAVC          },
        { "SYN", 0,            SettingType::Switch,     doSync            },
        { "DE",  1,            SettingType::Switch,     doDeEmp           },
        { "AVC", 90,           SettingType::Num,        doAvc             },
        { "SCR", 4,            SettingType::Num,        doBrightness      },
        { "SWU", 0,            SettingType::Switch,     doSWUnits         },
        { "SSM", 1,            SettingType::Switch,     doSSBSoftMuteMode },
        { "COF", 0,            SettingType::SwitchAuto, doCutoffFilter    },
        { "CPU", 0,            SettingType::Switch,     doCPUSpeed        },
        { "BFO", 0,            SettingType::Num,        doBFOCalibration  },
        { "UNI", 1,            SettingType::Switch,     doUnitsSwitch     },
        { "SCN", 1,            SettingType::Switch,     doScanSwitch      },
        { "CW ", 0,            SettingType::Switch,     doCWSwitch        },
        { "CAP", 0,            SettingType::Switch,     doAntennaCapacitor},
        { "RSI", 1,            SettingType::Switch,     doRSSIAMOff       },
};

// defines the text conversion rules ONLY for settings of type 'Switch'
// it is indexed here by the SettingsIndex enum
const PROGMEM SwitchMapEntry switch_setting_map[] = {
    [ATT] = {0, false},                     // Ignored, type is ZeroAuto
    [SoftMute] = {0, false},                // Ignored, type is Num
    [SVC] = {2, true},
    [Sync] = {2, true},
    [DeEmp] = {3, false},
    [AutoVolControl] = {0, false},          // Ignored, type is Num
    [Brightness] = {0, false},              // Ignored, type is Num
    [SWUnits] = {5, false},
    [SSM] = {7, false},
    [CutoffFilter] = {0, false},            // Ignored, type is SwitchAuto
    [CPUSpeed] = {11, false},
    [BFO] = {0, false},                     // Ignored, type is Num
    [UnitsSwitch] = {2, true},
    [ScanSwitch] = {2, true},
    [CWSwitch] = {9, false},
    [AntennaCap] = {1, false},
    [RSSI_AM_Off] = {1, true}
};

// -------------------------------------------------------------------------------------------------
// Band Definitions
// -------------------------------------------------------------------------------------------------
// we use an index to track the current band. band index 1 is mw.
int8_t g_bandIndex = 1;

// this array is now the single source of truth for all bands (reduse size flash too now)
// it is defined here directly
Band g_bandList[g_bandCount] = {
    // name,         min_freq,  max_freq, band_type,    current_freq, stepAM, stepSSB, stepFM, bwAM, bwSSB, bwFM
    { PACK_STR4("LW  "),   150,       520, LW_BAND_TYPE,   300,        2,      4,       1,      4,    4,     0 },
    { PACK_STR4("MW  "),   520,      1710, MW_BAND_TYPE,  1080,        3,      4,       1,      4,    4,     0 },
    // --- SW sub bands ---
    { PACK_STR4("SW  "),  1710,      1810, SW_BAND_TYPE,  1750,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("160m"),  1810,      2000, SW_BAND_TYPE,  1850,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  2000,      2300, SW_BAND_TYPE,  2150,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("120m"),  2300,      2500, SW_BAND_TYPE,  2400,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  2500,      3200, SW_BAND_TYPE,  2800,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("90m "),  3200,      3400, SW_BAND_TYPE,  3300,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  3400,      3500, SW_BAND_TYPE,  3450,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("80m "),  3500,      3900, SW_BAND_TYPE,  3700,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("75m "),  3900,      4000, SW_BAND_TYPE,  3950,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  4000,      4750, SW_BAND_TYPE,  4400,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("60m "),  4750,      5060, SW_BAND_TYPE,  4850,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  5060,      5900, SW_BAND_TYPE,  5500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("49m "),  5900,      6200, SW_BAND_TYPE,  6000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  6200,      7000, SW_BAND_TYPE,  6500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("40m "),  7000,      7300, SW_BAND_TYPE,  7150,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("41m "),  7300,      9400, SW_BAND_TYPE,  8500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("31m "),  9400,      9900, SW_BAND_TYPE,  9600,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("SW  "),  9900,     10100, SW_BAND_TYPE, 10000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("30m "), 10100,     10150, SW_BAND_TYPE, 10120,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("25m "), 10150,     18068, SW_BAND_TYPE, 14000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("17m "), 18068,     18168, SW_BAND_TYPE, 18100,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("16m "), 18168,     21000, SW_BAND_TYPE, 19500,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("15m "), 21000,     21450, SW_BAND_TYPE, 21200,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("13m "), 21450,     28000, SW_BAND_TYPE, 25000,        1,      4,       1,      4,    4,     0 },
    { PACK_STR4("10m "), 28000,     30000, SW_BAND_TYPE, 28400,        1,      4,       1,      4,    4,     0 },
    // --- FM ---
    { PACK_STR4("    "),  6400,     10800, FM_BAND_TYPE,   8400,        1,      4,       1,      4,    4,     0 }
};

// -------------------------------------------------------------------------------------------------
// Bandwidth Tables
// -------------------------------------------------------------------------------------------------
// single string array in PROGMEM holds all bandwidth labels to save Flash
const char bw_all_data[] PROGMEM =
"0.5k" "1.0k" "1.2k" "1.8k" "2.0k" "2.2k" "2.5k" "3.0k"
"4.0k" "6.0k" "AUTO" "110k" " 84k" " 60k" " 40k";

// more compact (1 byte per entry) than a early table of pointers (2 bytes per entry)
const uint8_t bw_ssb_map[] PROGMEM = { 0 * 4, 1 * 4, 2 * 4, 5 * 4, 7 * 4, 8 * 4 };
const uint8_t bw_am_map[] PROGMEM = { 1 * 4, 3 * 4, 4 * 4, 6 * 4, 7 * 4, 8 * 4, 9 * 4 };
const uint8_t bw_fm_map[] PROGMEM = { 10 * 4, 11 * 4, 12 * 4, 13 * 4, 14 * 4 };

// arrays for chip configuration. They map the our UI index to configure IC Si473x
const uint8_t g_bwSSBIdx[] = { 4, 5, 0, 1, 2, 3 };
const uint8_t g_bwSSBMaxIdx = 5;
const uint8_t g_maxFilterAM = 6;
const uint8_t g_bwAMIdx[] = { 4, 5, 3, 6, 2, 1, 0 };

// -------------------------------------------------------------------------------------------------
// Tuning Step Tables
// -------------------------------------------------------------------------------------------------
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
  "AUT", "On ", "Off", "50u", "75u", "kHz", "MHz",
  "RSS", "SNR", "LSB", "USB", "100", "50%"
};

const char g_bandModeDesc[][4] = { "AM ", "LSB", "USB", "CW ", "FM " };

FMFavorite g_fmFavorites[MAX_FM_FAVORITES]; 
