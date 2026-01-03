#pragma once

// =================================================================================================
// Macros & Constants
// =================================================================================================

// helper to check if the band type
// used to limit max AM step index as larger steps (like 9/10kHz)
#define IS_LW_MW(bt) ((bt)==LW_BAND_TYPE || (bt)==MW_BAND_TYPE)

// to get the last valid index of a zero-based array
#define LEN(a) ((uint8_t)(sizeof(a) - 1))

// timed checks
#define now_ms()            (uint32_t)millis()
#define since_ms(t)         (now_ms() - (uint32_t)(t))
#define passed_ms(t,d)      (since_ms(t) >= (uint32_t)(d))

#define RETURN_IF_SETTINGS_ACTIVE() do { if (g_settingsActive) return; } while(0)

// A macro to convert a 4-character string literal into a char array without a null terminator
#define PACK_STR4(s) {s[0], s[1], s[2], s[3]}

const uint8_t g_SettingsMaxPages = 5;       // pages number in settings menu

#if ENABLE_FAVORITES
const uint8_t MAX_FAVORITES = 20;
#endif

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
    MODE_CONTEXT_LSB,
    MODE_CONTEXT_USB,
    MODE_CONTEXT_COUNT = 3  // Only 3 contexts since CW inherits from LSB/USB
};

enum SettingType {
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

enum SettingsIndex {
    // --- Page 1: Core Audio & RF ---
    ATT,            // ATT - Attenuation / AGC
    AutoVolControl, // AVC - Automatic Volume Control
    SQL,            // SQL - Squelch
    SoftMute,       // SM  - AM Soft Mute Attenuation
    SoftMuteThr,    // SMT - AM Soft Mute Threshold
    AMNoiseBlanker, // ANB - AM Noise Blanker

    // --- Page 2: SSB & CW ---
    BFO,            // BFO - BFO Calibration
    SSM,            // SSM - SSB Soft Mute
    SVC,            // SVC - SSB AVC Switch
    CutoffFilter,   // COF - SSB Cutoff Filter
    Sync,           // SYN - SSB Sync (DSP AFC)
    CWPitch,        // CWP - CW Pitch

    // --- Page 3: FM & Advanced Audio ---
    DeEmp,          // DE  - FM De-Emphasis
    FMAudioProfile, // FMP - FM Audio Profile (Speaker EQ)
    ForceMono,      // FMO - Force Mono Reception
    FmSmAtt,        // FSA - FM Soft Mute Attenuation
    FmSmThr,        // FST - FM Soft Mute Threshold
    SWAFC,          // SWA - SW AFC (AM on SW bands)

    // --- Page 4: Display & UI ---
    Brightness,     // SCR - Screen Brightness
    SMeter,         // SPT - S-Point / RSSI Display
    SWUnits,        // SWU - SW Units (kHz/MHz)
    DisplayOff,     // DIS - Display Off Timeout
    RSSI_AM_Off,    // RSI - Disable RSSI polling in AM
    NAV,            // NAV - Settings Navigation Style

    // --- Page 5: Hardware Configuration ---
    AntennaCap,     // CAP - Antenna Capacitor
    CPUSpeed,       // CPU - CPU Speed
    BATT_PIN,       // BAP - Battery Pin Select
    ScanSwitch,     // SCN - Scan Button Behavior
    FmVolAdjust,    // FVA - FM Volume Adjust

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
// --- Debugging ---
void initDebugUART();
void debugPrint_P(const char* str);
void debugPrintNum(int16_t num);
#endif

// --- Memory & State Sync ---
void syncActiveStateToBand();
void loadActiveStateFromBand();
void syncModeDependentSettings(bool load);
static inline void initModeSettingsDefaults(void);

// --- Core Utilities & Helpers ---
static inline void noteUserActivity();
static void setCpuPrescaler(uint8_t prescaler);
static inline uint16_t freqDelta16(uint16_t, uint16_t);
static inline bool freqRateLimitOk(uint32_t);
static inline bool freqTimeElapsed(uint32_t);
static inline bool freqForceUpdate(uint16_t);
static inline bool applySafeEncoderDeltaAndTune();
static inline void applyCompensatedVolume();
static inline bool amRssiPollingAllowed(uint32_t);
static inline uint32_t currentCmdTimeoutMs();
static inline bool shouldSaveStateOnIdle(uint16_t);
static inline uint16_t displayTimeoutS(uint8_t);
static inline void persistModeSetting(ModeSettingType, SettingsIndex);
static inline uint8_t settingsPageStart(uint8_t page);
static inline void settingsEnter();
static inline void settingsExitAndSave();
static bool isSSB();

// --- Input Handling ---
inline int16_t getAndResetEncoderCount(volatile int16_t& counter);
bool processEncoderActions(int16_t movement);
void processButtonEvents();
static void updateEncoderState();
static void switchSettings();
static void switchSettingsPage();
static void switchCommand(CommandMode mode);
static void resetCommandMode();
static void wakeUpDisplayIfNeeded();

// --- Direct Radio Control Actions ---
static void doSeek();
static void cycleAmSsbCwModes();
static void doFrequencyTuneSSB();
static void doFrequencyTune();
static void doVolume(int8_t v);
static void doStep(int8_t v);
static void doBandwidth(uint8_t v);
static void bandSwitch(bool up, bool loadStoredFreq = true);
static void doCWSwitch();

// --- Favorites Handling ---
#if ENABLE_FAVORITES
static inline bool favoriteExists(uint16_t f, uint8_t m);
static inline void compactFavoritesFrom(uint8_t start);
static inline void fixFavoriteSelectionAfterDelete();
static inline bool favoriteNeedsFullReset(BandType, BandType, bool, bool);
static void handleFavoritesTimeout();
static void handleFavoritesMenu(int16_t movement);
static void addFavorite();
static void deleteFavorite();
static void saveFavorites();
static void loadFavorites();
void tuneToSelectedFavorite();
#endif

// --- UI Drawing ---
static void DrawSetting(uint8_t idx, bool full);
#if ENABLE_FAVORITES
static void showFavorites(bool force_redraw = false);
#endif
void showSavedConfirmation();
static void showVolume();
static void showStep();
static void showBandwidth();
static void showModulation();
static void showSettingsTitle();
static void showSettings();
void showSplashScreen();
void showStatus(bool cleanFreq = false);
void rssiToSLevel(char* buffer, uint8_t rssi);
void updateAndShowBattery(bool forceShow);
void updateStereoIndicator();
static void showRfHints();
static void showChargeOnDisplay();
static void showFrequencySeek(uint16_t freq);
static void refreshCommandIndicators();

// --- CW Decoder hooks ---
#if ENABLE_CW_DECODER
static void handleVolumeDownShortPress();
static void cwViewEnter();
static void cwViewExit();
static inline void cwViewTask();
#endif

// --- Settings Handlers (Callbacks for the settings menu) ---
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
void doCWPitch(int8_t v);
void doSwAfcProfile(int8_t v);
void doScanSwitch(int8_t v = 0);
void doRSSIAMOff(int8_t v = 0);
void doAntennaCapacitor(int8_t v = 0);
void doDisplayOff(int8_t v = 0);
void doFMAudioProfile(int8_t v = 0);
void doAMNoiseBlanker(int8_t v = 0);
void doForceMono(int8_t v = 0);
void doBatteryPinSelect(int8_t v = 0);
void doSquelch(int8_t v);
void doFmSoftMuteAtt(int8_t v);
void doFmSoftMuteThr(int8_t v);
void doFmVolAdjust(int8_t v);
void doSMeter(int8_t v = 0);
void doNavStyle(int8_t v = 0);

// --- High-Level Configuration & System ---
static void applyBandConfiguration(bool extraSSBReset = false);
static void setAmpState(bool on);
static void applyBrightness();
static void loadSSBPatch();
static void resetEepromDelay();

// --- Periodic & Timed Tasks ---
static inline void handleSignalAndStereoUpdates();
static inline void handleCommandTimeout();
static inline void handleSettingsSave();
static inline void checkDisplayTimeout();
static void handlePeriodicTasks();

// --- Setting Applicability Checks Prototypes ---
static inline bool isAlwaysActive();
static inline bool isAMFamilyActive();
static inline bool isSSBActive();
static inline bool isFMActive();

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
    bool (*is_active)();
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
    int8_t bfoCal;
};

#if ENABLE_FAVORITES
struct FavoriteStation {
    uint16_t frequency;
    uint8_t  modulation;
    int16_t  bfo;
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
bool g_voltagePinConnected = false;
bool g_ssbLoaded = false;
bool g_stereoStatus = false;
bool autoDisplayOff = false;
bool g_squelchCutoff = false;
bool g_displayOn = true;
volatile bool g_seekStop = false;       // volatile important here!
uint32_t g_lastAdjustmentTime = 0;
uint16_t g_lastUserActivityTime = 0;    // time of the last user frequency change (IN SECONDS)
bool g_stateIsDirty = false;            // indicate if the state needs saving on idle

// -------------------------------------------------------------------------------------------------
// UI & Command State
// -------------------------------------------------------------------------------------------------
volatile CommandMode g_activeCommand = CMD_NONE;
bool g_settingsActive = false;
bool g_settingsDirty = false;
int8_t g_SettingSelected = 0;
int8_t g_SettingsPage = 1;
bool g_SettingEditing = false;

#if ENABLE_FAVORITES
bool g_favoritesActive = false;
bool g_favoritesDirty = false;
uint8_t g_favoriteSelected = 0;
uint8_t g_totalFavorites = 0;
#endif

#if ENABLE_CW_DECODER
bool g_cwViewActive = false;
#endif

// -------------------------------------------------------------------------------------------------
// Radio State
// -------------------------------------------------------------------------------------------------

// Number of bands for seamless coverage - msut array size for Band g_bandList[g_bandCount] be consistent
const uint8_t g_bandCount = 32;


const uint8_t g_lastBand = g_bandCount - 1;
uint8_t g_signalQualityValue = 255;     // Unified value for RSSI (all modes) where is 255 invalidated
uint32_t g_lastRSSIUpdate = 0;
uint8_t g_muteVolume = 0;
uint8_t g_volume = DEFAULT_VOLUME;
volatile uint8_t g_currentMode = FM;
int16_t g_currentBFO = 0;
int16_t g_savedSsbBfo[g_bandCount] = { 0 };  // cache of the last SSB BFO for each band (RAM, without EEPROM)
extern uint8_t g_stableBatteryPercent;       // store table percentage for display
uint8_t g_lastSsbMode = LSB;                 // last used sideband (LSB or USB)
uint8_t g_lastCWMode = LSB;                  // last used CW sideband (LSB/USB)

//Frequency tracking
uint16_t g_currentFrequency = 0;
uint16_t g_previousFrequency = 0;
uint16_t g_lastSavedFrequency = 0;
uint8_t g_seekDirection = 1;

//Special logic for fast and responsive frequency surfing
uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = false;
uint32_t g_lastSetFreqTime = 0;

// -------------------------------------------------------------------------------------------------
// Encoder & Buttons
// -------------------------------------------------------------------------------------------------
volatile int16_t g_encoderCount = 0;
volatile int16_t g_safeEncoderMovement = 0;

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
// All contexts use identical defaults
static constexpr ModeDefaults DEFAULT_MODE_SETTINGS = {
    .agc = 0, .soft_mute = 0, .avc = 10  // index 10 = max level (90)
};

// "Live State" storage for mode-dependent settings
// This array is loaded from and saved to EEPROM
int8_t g_modeSettings[MODE_SETTINGS_COUNT][MODE_CONTEXT_COUNT];


// --- Setting Applicability Checks ---
// These helpers determine if a setting is relevant in the current radio mode.
static inline bool isAlwaysActive() { return true; }
static inline bool isAMFamilyActive() { return g_currentMode != FM; }
static inline bool isSSBActive() { return isSSB(); }
static inline bool isFMActive() { return g_currentMode == FM; }


// -------------------------------------------------------------------------------------------------
// General Settings
// -------------------------------------------------------------------------------------------------
// "UI Buffer" - A temporary buffer for the settings UI, stored in RAM
// It holds the live state of settings while the user is in the menu
// This buffer is populated from g_modeSettings upon entering the menu
// The initial values here are defaults and will be overwritten
SettingsItem g_Settings[] =
{
    // --- Page 1: Core Audio & RF ---
    { "ATT", 0,  SettingType::ZeroAuto,   doAttenuation,       isAlwaysActive },
    { "AVC", 10, SettingType::Num,        doAvc,               isAMFamilyActive },
    { "SQL", 0,  SettingType::Num,        doSquelch,           isAlwaysActive },
    { "SMA", 0,  SettingType::Num,        doSoftMute,          isAMFamilyActive },
    { "SMT", 0,  SettingType::Num,        doSoftMuteThreshold, isAMFamilyActive },
    { "ANB", 0,  SettingType::Switch,     doAMNoiseBlanker,    isAMFamilyActive },

    // --- Page 2: SSB & CW ---
    { "BFO", 0,  SettingType::Num,        doBFOCalibration,    isAMFamilyActive },
    { "SSM", 1,  SettingType::Switch,     doSSBSoftMuteMode,   isSSBActive },
    { "SVC", 1,  SettingType::Switch,     doSSBAVC,            isSSBActive },
    { "COF", 0,  SettingType::SwitchAuto, doCutoffFilter,      isSSBActive },
    { "SYN", 0,  SettingType::Switch,     doSync,              isSSBActive },
    { "CWP", 2,  SettingType::Num,        doCWPitch,           isAMFamilyActive },

    // --- Page 3: FM & Advanced Audio ---
    { "DE ", 0,  SettingType::Switch,     doDeEmp,             isFMActive },
    { "FMP", 1,  SettingType::Switch,     doFMAudioProfile,    isFMActive },
    { "FMO", 0,  SettingType::Switch,     doForceMono,         isFMActive },
    { "FSA", 22, SettingType::Num,        doFmSoftMuteAtt,     isFMActive },
    { "FST", 10, SettingType::Num,        doFmSoftMuteThr,     isFMActive },
    { "SWA", 0,  SettingType::Num,        doSwAfcProfile,      isAMFamilyActive },

    // --- Page 4: Display & UI ---
    { "SCR", 4,  SettingType::Num,        doBrightness,        isAlwaysActive },
    { "SPT", 0,  SettingType::Switch,     doSMeter,            isAlwaysActive },
    { "SWU", 0,  SettingType::Switch,     doSWUnits,           isAMFamilyActive },
    { "DIS", 0,  SettingType::Switch,     doDisplayOff,        isAlwaysActive },
    { "RSI", 1,  SettingType::Switch,     doRSSIAMOff,         isAMFamilyActive },
    { "NAV", 0,  SettingType::Switch,     doNavStyle,          isAlwaysActive },

    // --- Page 5: Hardware Configuration ---
    { "CAP", 0,  SettingType::Switch,     doAntennaCapacitor,  isAlwaysActive },
    { "CPU", 0,  SettingType::Switch,     doCPUSpeed,          isAlwaysActive },
    { "BAP", 0,  SettingType::Switch,     doBatteryPinSelect,  isAlwaysActive },
    { "SCN", 1,  SettingType::Switch,     doScanSwitch,        isAlwaysActive },
    { "FVA", 0,  SettingType::Num,        doFmVolAdjust,       isAlwaysActive },
};

// defines the text conversion rules ONLY for settings of type 'Switch'
// it is indexed here by the SettingsIndex enum
const PROGMEM SwitchMapEntry switch_setting_map[] = {
    // --- Page 1: Core Audio & RF ---
    [ATT] = {0, false},
    [AutoVolControl] = {0, false},
    [SQL] = {0, false},
    [SoftMute] = {0, false},
    [SoftMuteThr] = {0, false},
    [AMNoiseBlanker] = {1, false},

    // --- Page 2: SSB & CW ---
    [BFO] = {0, false},
    [SSM] = {7, false},
    [SVC] = {2, true},
    [CutoffFilter] = {0, false},
    [Sync] = {2, true},
    [CWPitch] = {0, false},

    // --- Page 3: FM & Advanced Audio ---
    [DeEmp] = {3, false},
    [FMAudioProfile] = {1, false},
    [ForceMono] = {2, true},
    [FmSmAtt] = {0, false},
    [FmSmThr] = {0, false},
    [SWAFC] = {2, true},

    // --- Page 4: Display & UI ---
    [Brightness] = {0, false},
    [SMeter] = {2, true},
    [SWUnits] = {5, false},
    [DisplayOff] = {0, false},
    [RSSI_AM_Off] = {1, true},
    [NAV] = {15, false},

    // --- Page 5: Hardware Configuration ---
    [AntennaCap] = {1, false},
    [CPUSpeed] = {9, false},
    [BATT_PIN] = {0, false},
    [ScanSwitch] = {2, true},
    [FmVolAdjust] = {0, false},
};

// -------------------------------------------------------------------------------------------------
// Band Definitions
// -------------------------------------------------------------------------------------------------

// SW sub band limits for seek
constexpr uint16_t SW_MIN_FREQ = 1710;
constexpr uint16_t SW_MAX_FREQ = 30000;

// we use an index to track the current band. band index 1 is mw.
int8_t g_bandIndex = 1;

// Default step/bandwidth/bfo values for band initialization
// stepIdxAM, stepIdxSSB, stepIdxFM, bwIdxAM, bwIdxSSB, bwIdxFM, bfoCal
#define BD  1, 4, 1, 4, 4, 0, 0   // SW/FM bands (stepIdx=1 → 5 kHz step)
#define BM  2, 4, 1, 4, 4, 0, 0   // LW/MW bands (stepIdx=2 → 9 kHz step)

// Array single source of truth for all bands
// name, minFreq, maxFreq, bandType, defaultFreq, [step/bw/bfo defaults]
Band g_bandList[g_bandCount] = {
    //        name           min      max   type           freq   step/bw/bfo
    // --- LW/MW bands ---
    { PACK_STR4("LW  "),      150,     521, LW_BAND_TYPE,   300, BM },
    { PACK_STR4("MW  "),      522,    1710, MW_BAND_TYPE,   522, BM },

    // --- SW broadcast & amateur bands ---
    { PACK_STR4("SW  "),     1711,    1799, SW_BAND_TYPE,  1750, BD },
    { PACK_STR4("160m"),     1800,    1999, SW_BAND_TYPE,  1850, BD },  // 160m amateur

    { PACK_STR4("SW  "),     2000,    2495, SW_BAND_TYPE,  2400, BD },
    { PACK_STR4("SW  "),     2496,    3399, SW_BAND_TYPE,  2800, BD },

    { PACK_STR4("80m "),     3400,    3999, SW_BAND_TYPE,  3700, BD },  // 80m amateur
    { PACK_STR4("75m "),     4000,    4749, SW_BAND_TYPE,  4500, BD },  // 75m broadcast
    { PACK_STR4("60m "),     4750,    5059, SW_BAND_TYPE,  4850, BD },  // 60m broadcast

    { PACK_STR4("SW  "),     5060,    5350, SW_BAND_TYPE,  5200, BD },
    { PACK_STR4("60H "),     5351,    5367, SW_BAND_TYPE,  5357, BD },  // 60m amateur
    { PACK_STR4("SW  "),     5368,    5899, SW_BAND_TYPE,  5500, BD },

    { PACK_STR4("49m "),     5900,    6199, SW_BAND_TYPE,  6000, BD },  // 49m broadcast
    { PACK_STR4("41m "),     6200,    7299, SW_BAND_TYPE,  7100, BD },  // 41m broadcast
    { PACK_STR4("40m "),     7300,    7599, SW_BAND_TYPE,  7400, BD },  // 40m amateur
    { PACK_STR4("31m "),     7600,    9899, SW_BAND_TYPE,  9500, BD },  // 31m broadcast

    { PACK_STR4("25m "),     9900,   10099, SW_BAND_TYPE, 10000, BD },
    { PACK_STR4("30m "),    10100,   10150, SW_BAND_TYPE, 10136, BD },  // 30m amateur
    { PACK_STR4("25m "),    10151,   11599, SW_BAND_TYPE, 11000, BD },

    { PACK_STR4("22m "),    11600,   13569, SW_BAND_TYPE, 12500, BD },  // 22m broadcast

    { PACK_STR4("19m "),    13570,   13869, SW_BAND_TYPE, 13700, BD },  // 19m broadcast
    { PACK_STR4("16m "),    13870,   13999, SW_BAND_TYPE, 13950, BD },  // 16m broadcast

    { PACK_STR4("20m "),    14000,   14350, SW_BAND_TYPE, 14200, BD },  // 20m amateur
    { PACK_STR4("16m "),    14351,   15099, SW_BAND_TYPE, 15000, BD },  // 16m broadcast

    { PACK_STR4("15m "),    15100,   17899, SW_BAND_TYPE, 17500, BD },  // 15m amateur
    { PACK_STR4("13m "),    17900,   21449, SW_BAND_TYPE, 21200, BD },  // 13m broadcast

    { PACK_STR4("11m "),    21450,   21849, SW_BAND_TYPE, 21600, BD },  // 11m broadcast
    { PACK_STR4("SW  "),    21850,   24889, SW_BAND_TYPE, 23000, BD },  // SW

    { PACK_STR4("12m "),    24890,   26099, SW_BAND_TYPE, 25600, BD },  // 12m amateur
    { PACK_STR4("CB  "),    26100,   27860, SW_BAND_TYPE, 27200, BD },  // CB radio
    { PACK_STR4("10m "),    27861,   30000, SW_BAND_TYPE, 28500, BD },  // 10m amateur

    // --- FM broadcast band ---
    { PACK_STR4("    "),     6400,   10800, FM_BAND_TYPE,  8400, BD }
};

#undef BD
#undef BM

// -------------------------------------------------------------------------------------------------
// Bandwidth Tables
// -------------------------------------------------------------------------------------------------

// single PROGMEM block of null-terminated UI labels
const char bw_all_data[] PROGMEM =
"0.5 kHz\0" "1.0 kHz\0" "1.2 kHz\0" "1.8 kHz\0" "2.0 kHz\0" "2.2 kHz\0"
"2.5 kHz\0" "3.0 kHz\0" "4.0 kHz\0" "6.0 kHz\0" " AUTO  \0" "110 kHz\0"
"84 kHz \0" "60 kHz \0" "40 kHz \0";

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
// S‑Meter mapping + constants and PROGMEM helpers
// -------------------------------------------------------------------------------------------------

// dBuV thresholds for S0 through S9+50 on HF
static const uint8_t THR_HF[] PROGMEM = { 1,2,3,4,10,16,22,28,34,44,54,64,74,84,94 };
// dBuV thresholds for S3 through S9+50 on FM
static const uint8_t THR_FM[] PROGMEM = { 0,2,8,14,24,34,44,54,64,74 };
// Special non-linear S-point mapping for low signal FM
static const uint8_t FM_S4[] PROGMEM = { 3,6,7,8 };

static constexpr uint8_t LEN_HF = sizeof(THR_HF);
static constexpr uint8_t LEN_FM = sizeof(THR_FM);

static inline uint8_t CREAD(const uint8_t* p, uint8_t i) {
    return pgm_read_byte(&p[i]);
}

// -------------------------------------------------------------------------------------------------
// Tuning Step Tables
// -------------------------------------------------------------------------------------------------

// step strings padded to 4 chars to reduce mem usage
static const char step_lookup_table[][7] PROGMEM = {
    // AM Steps (indices 0-6)
    "1 kHz ", "5 kHz ", "9 kHz ", "10 kHz", "50 kHz", "100kHz", "1 MHz ",
    // SSB Steps (indices 7-15)
    "10 Hz ", "25 Hz ", "50 Hz ", "100 Hz", "500 Hz", "1 kHz ", "5 kHz ", "9 kHz ", "10 kHz"
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

// Pitch options for CW reception in Hz
const uint16_t cw_pitch_options_hz[] PROGMEM = { 500, 600, 700, 800 };

// -------------------------------------------------------------------------------------------------
// UI Text & Other Data
// -------------------------------------------------------------------------------------------------
// used by SettingParamToUI function to convert parameter values to display strings
const char PROGMEM paramTexts[][4] = {
  "AUT", " ON", "OFF", " 50", " 75", "kHz", "MHz",
  "RSS", "SNR", "100", "50%",
  "10m", "15m", "30m", "60m",
  "ROW", "COL"
};

// Timeout values in seconds for the display-off feature, indexed by the setting parameter
const uint16_t T[5] PROGMEM = { 0, 600, 900, 1800, 3600 };

extern const char g_bandModeDesc[][4];

#if ENABLE_FAVORITES
FavoriteStation g_favorites[MAX_FAVORITES];
#endif
