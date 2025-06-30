#pragma once

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
void doCWSwitch(int8_t v = 0);
void updateAndShowBattery(bool forceShow);


// A macro to convert a 4-character string literal into a char array without a null terminator
#define PACK_STR4(s) {s[0], s[1], s[2], s[3]}

const uint8_t g_SettingsMaxPages = 3;
const int16_t CW_PITCH_OFFSET_HZ = 500; // 500 Hz pitch for CW tone generation
const uint8_t MAX_FM_FAVORITES = 10;


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

// "Source of Truth" for default values
// This is an immutable template for resetting settings
struct ModeDefaults {
    const int8_t agc;       // Default for Attenuation/AGC
    const int8_t soft_mute; // Default for Soft Mute
    const int8_t avc;       // Default for AVC Max Gain
};

enum SettingType
{
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

struct SettingsItem
{
    char name[4];
    int8_t param;
    uint8_t type;
    void (*manipulateCallback)(int8_t);
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
    SETTINGS_MAX
};

enum BandType : uint8_t
{
    LW_BAND_TYPE,
    MW_BAND_TYPE,
    SW_BAND_TYPE,
    FM_BAND_TYPE
};

struct Band
{
    uint16_t minimumFreq;
    uint16_t maximumFreq;
    uint16_t currentFreq;
    // -- New fields to store per-mode settings
    int8_t stepIdxAM;
    int8_t stepIdxSSB;
    int8_t stepIdxFM;
    int8_t bwIdxAM;
    int8_t bwIdxSSB;
    int8_t bwIdxFM;
};

enum Modulations : uint8_t
{
    AM,
    LSB,
    USB,
    CW,
    FM
};

struct FMFavorite {
    uint16_t frequency;
};


long g_storeTime = millis();
bool g_voltagePinConnnected = false;
bool g_ssbLoaded = false;
bool g_stereoStatus = false;
bool g_displayOn = true;
bool g_seekStop = false;
uint32_t g_lastAdjustmentTime = 0;

bool g_cmdVolume = false;
bool g_cmdStep = false;
bool g_cmdBw = false;
bool g_cmdBand = false;

bool g_settingsActive = false;
bool g_settingsDirty = false;
int8_t g_SettingSelected = 0;
int8_t g_SettingsPage = 1;
bool g_SettingEditing = false;

uint8_t g_currentRSSI = 0;
uint32_t g_lastRSSIUpdate = 0;
extern uint8_t g_stableBatteryPercent; // store table percentage for display

uint8_t g_muteVolume = 0;
uint8_t g_volume = DEFAULT_VOLUME;

volatile uint8_t g_currentMode = FM;
volatile uint8_t g_prevMode = FM;
int g_currentBFO = 0;

//Frequency tracking
uint16_t g_currentFrequency;
uint16_t g_previousFrequency;
uint8_t g_seekDirection = 1;
//Special logic for fast and responsive frequency surfing
uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = 0;

volatile int g_encoderCount = 0;
int g_safeEncoderMovement = 0;

bool g_favoritesActive = false;
uint8_t g_favoriteSelected = 0;
uint8_t g_totalFavorites = 0;


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


// "Live State" storage for mode-dependent settings
// This array is loaded from and saved to EEPROM
int8_t g_modeSettings[MODE_SETTINGS_COUNT][MODE_CONTEXT_COUNT];

const ModeDefaults defaultModeSettings[MODE_CONTEXT_COUNT] = {
    // [MODE_CONTEXT_AM]
    {.agc = 0, .soft_mute = 0, .avc = 90 },
    // [MODE_CONTEXT_SSB]
    {.agc = 0, .soft_mute = 0, .avc = 90 }
};

// used by SettingParamToUI function to convert parameter values to display strings
const char PROGMEM paramTexts[][4] = {
  "AUT", "On ", "Off", "50u", "75u", "kHz", "MHz",
  "RSS", "SNR", "LSB", "USB", "100", "50%"
};

// "UI Buffer" - a temporary buffer for the settings UI
// It is populated from g_modeSettings upon entering the menu
SettingsItem g_Settings[] =
{
    { "ATT", 0,  SettingType::ZeroAuto,     doAttenuation     },
    { "SM ", 0,  SettingType::Num,          doSoftMute        },
    { "SVC", 1,  SettingType::Switch,       doSSBAVC          },
    { "SYN", 0,  SettingType::Switch,       doSync            },
    { "DE",  1,  SettingType::Switch,       doDeEmp           },
    { "AVC", 0,  SettingType::Num,          doAvc             },
    { "SCR", 10, SettingType::Num,          doBrightness      },
    { "SWU", 0,  SettingType::Switch,       doSWUnits         },
    { "SSM", 1,  SettingType::Switch,       doSSBSoftMuteMode },
    { "COF", 0,  SettingType::SwitchAuto,   doCutoffFilter    },
    { "CPU", 0,  SettingType::Switch,       doCPUSpeed        },
    { "BFO", 0,  SettingType::Num,          doBFOCalibration  },
    { "UNI", 1,  SettingType::Switch,       doUnitsSwitch     },
    { "SCN", 1,  SettingType::Switch,       doScanSwitch      },
    { "CW ", 0,  SettingType::Switch,       doCWSwitch        },
};


// For SSB - using PROGMEM to save RAM
const char bw_ssb_0[] PROGMEM = "0.5k";
const char bw_ssb_1[] PROGMEM = "1.0k";
const char bw_ssb_2[] PROGMEM = "1.2k";
const char bw_ssb_3[] PROGMEM = "2.2k";
const char bw_ssb_4[] PROGMEM = "3.0k";
const char bw_ssb_5[] PROGMEM = "4.0k";
const char* const bw_ssb_table[] PROGMEM = { bw_ssb_0, bw_ssb_1, bw_ssb_2, bw_ssb_3, bw_ssb_4, bw_ssb_5 };
int8_t g_bwIndexSSB = 4;
const uint8_t g_bwSSBIdx[] = { 4, 5, 0, 1, 2, 3 };
const uint8_t g_bwSSBMaxIdx = 5;

const char bw_am_0[] PROGMEM = "1.0k";
const char bw_am_1[] PROGMEM = "1.8k";
const char bw_am_2[] PROGMEM = "2.0k";
const char bw_am_3[] PROGMEM = "2.5k";
const char bw_am_4[] PROGMEM = "3.0k";
const char bw_am_5[] PROGMEM = "4.0k";
const char bw_am_6[] PROGMEM = "6.0k";
const char* const bw_am_table[] PROGMEM = { bw_am_0, bw_am_1, bw_am_2, bw_am_3, bw_am_4, bw_am_5, bw_am_6 };
int8_t g_bwIndexAM = 4;
const uint8_t g_maxFilterAM = 6;
const uint8_t g_bwAMIdx[] = { 4, 5, 3, 6, 2, 1, 0 };

const char bw_fm_0[] PROGMEM = "AUTO";
const char bw_fm_1[] PROGMEM = "110k";
const char bw_fm_2[] PROGMEM = " 84k";
const char bw_fm_3[] PROGMEM = " 60k";
const char bw_fm_4[] PROGMEM = " 40k";
const char* const bw_fm_table[] PROGMEM = { bw_fm_0, bw_fm_1, bw_fm_2, bw_fm_3, bw_fm_4 };
int8_t g_bwIndexFM = 0;

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

// Separated state variables for step index storage
int8_t g_stepIndexAM = 3;   // Stores the current step index ONLY for AM mode (range 0..6)
int8_t g_stepIndexSSB = 0;  // Stores the current step index ONLY for SSB mode (range 0..8)

int8_t g_tabStepFM[] = { 5, 10, 100 };
int8_t g_FMStepIndex = 1;
const int8_t g_lastStepFM = (sizeof(g_tabStepFM) / sizeof(int8_t)) - 1;

const char bandTags[][3] = { "LW", "MW", "  ", "  " };

// https://github.com/goshante/ats20_ats_ex/issues/44
Band g_bandList[] =
{
    // FreqMin,      FreqMax,  FreqCurrent,stepAM,stepSSB, stepFM,    bwAM, bwSSB,  bwFM
    /* LW */ { LW_LIMIT_LOW,             520,          300,     2,      4,       1,      4,    4,     0 }, // Default: 9k,  500Hz, 100k | 3.0k, 3.0k, AUTO
    /* MW */ { 450,                     1710,         1080,     3,      4,       1,      4,    4,     0 }, // Default: 10k, 500Hz, 100k | 3.0k, 3.0k, AUTO
    /* SW */ { SW_LIMIT_LOW,   SW_LIMIT_HIGH, SW_LIMIT_LOW,     1,      4,       1,      4,    4,     0 }, // Default: 5k,  500Hz, 100k | 3.0k, 3.0k, AUTO
    /* FM */ { 6400,                   10800,         8400,     1,      4,       1,      4,    4,     0 }  // Default: --,     --, 100k |   --,   --, AUTO
};

uint16_t SWSubBands[] =
{
    SW_LIMIT_LOW,  // 160 Meter
    3500, // 80 Meter
    4500,
    5600,
    6800, // 40 Meter
    7200, // 41 Meter
    8500,
    10000, // 30 Meter
    11200,
    13400,
    14000, // 20 Meter
    15000,
    17200,
    18000, // 17 Meter
    21000, // 15 Meter
    21400, // 13 Meter
    24890, // 12 Meter
    CB_LIMIT_LOW, // CB Band (11 Meter)
    CB_LIMIT_HIGH  // 10 Meter
};
const uint8_t g_SWSubBandCount = sizeof(SWSubBands) / sizeof(uint16_t);

// Array of SW sub-band names, with broadcasting bands filled in
const uint8_t band_names_packed[][4] PROGMEM = {
    PACK_STR4("160m"), PACK_STR4("80m "), PACK_STR4("60m "), PACK_STR4("49m "),
    PACK_STR4("40m "), PACK_STR4("41m "), PACK_STR4("31m "), PACK_STR4("30m "),
    PACK_STR4("25m "), PACK_STR4("22m "), PACK_STR4("20m "), PACK_STR4("19m "),
    PACK_STR4("16m "), PACK_STR4("17m "), PACK_STR4("15m "), PACK_STR4("13m "),
    PACK_STR4("12m "), PACK_STR4("CB  "), PACK_STR4("10m ")
};

const uint8_t g_lastBand = (sizeof(g_bandList) / sizeof(Band)) - 1;
int8_t g_bandIndex = 1;

const char g_bandModeDesc[][4] = { "AM ", "LSB", "USB", "CW ", "FM " };

FMFavorite g_fmFavorites[MAX_FM_FAVORITES];

char _literal_EmptyLine[17] = "                ";
