#pragma once
// ====================================================================================
//
// Globals.h
//
// Central runtime state + data tables
// 
// ====================================================================================

#include "Arduino.h"
#include <avr/pgmspace.h>

// helper to check if the band type
// used to limit max AM step index as larger steps (like 9/10kHz)
#define IS_LW_MW(bt) ((bt)==LW_BAND_TYPE || (bt)==MW_BAND_TYPE)

// timed checks
#define now_ms()            (uint32_t)millis()
#define since_ms(t)         (now_ms() - (uint32_t)(t))
#define passed_ms(t,d)      (since_ms(t) >= (uint32_t)(d))

#define RETURN_IF_SETTINGS_ACTIVE() do { if (g_settingsActive) return; } while(0)

// A macro to convert a 4-character string literal into a char array without a null terminator
#define PACK_STR4(s) {s[0], s[1], s[2], s[3]}

const uint8_t g_SettingsMaxPages = 6;       // pages number in settings menu

#if ENABLE_FAVORITES
const uint8_t MAX_FAVORITES = 20;
#endif

#include "SettingsData.h"

// First SW segment index used as SW master when SWLink is enabled
constexpr uint8_t SW_MASTER_BAND_INDEX = 2;

// =================================================================================================
// Enums
// =================================================================================================

enum CommandMode : uint8_t {
    CMD_NONE,
    CMD_VOLUME,
    CMD_STEP,
    CMD_BW,
    CMD_BAND,
    CMD_SLEEP
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
// Prototypes (cross-module visibility)
// =================================================================================================

#if DEBUG_MODE
void initDebugUART();
void debugPrint_P(const char* str);
void debugPrintNum(int16_t num);
#endif

void siSetProperty(uint16_t prop_addr, uint16_t prop_val);
void syncActiveStateToBand();
void loadActiveStateFromBand();
static void syncPreviousFreq();
static void saveLastFreq();
void syncModeDependentSettings(bool load);
static inline void initModeSettingsDefaults(void);

static inline void noteUserActivity();
static void setCpuPrescaler(uint8_t prescaler);
static inline uint16_t freqDelta16(uint16_t, uint16_t);
static inline bool freqRateLimitOk(uint32_t);
static inline bool freqTimeElapsed(uint32_t);
static inline bool freqForceUpdate(uint16_t);
static inline void applyCompensatedVolume();
static inline bool amRssiPollingAllowed(uint16_t);
uint16_t currentCmdTimeoutMs();
static inline bool shouldSaveStateOnIdle(uint16_t);
static inline uint16_t displayTimeoutS(uint8_t);
static inline void persistModeSetting(ModeSettingType, SettingsIndex);
static inline uint8_t settingsPageStart(uint8_t page);
static inline void settingsEnter();
static inline void settingsExitAndSave();

static bool isSSB();

int16_t getAndResetEncoderCount(volatile int16_t& counter);
bool processEncoderActions(int16_t movement);
void processButtonEvents();
static void updateEncoderState();
static void switchSettings();
static void switchSettingsPage();
static void switchCommand(CommandMode mode);
static void resetCommandMode();
static void wakeUpDisplayIfNeeded();

static void doSeek();
static void cycleAmSsbCwModes();
static void doFrequencyTune(int16_t delta);
static void doFrequencyTuneSSB(int16_t encoder_delta);
static void doVolume(int8_t v);
static void doStep(int8_t v);
static void doBandwidth(uint8_t v);
static void bandSwitch(bool up, bool loadStoredFreq = true);
static void doCWSwitch();

void applyI2CSpeed();

#if ENABLE_FAVORITES
static inline bool favoriteExists(uint16_t f, uint8_t m);
static inline void compactFavoritesFrom(uint8_t start);
static inline void fixFavoriteSelectionAfterDelete();
static inline bool favoriteNeedsFullReset(BandType, BandType, bool, bool);
static void handleFavoritesTimeout();
static void handleFavoritesMenu(int16_t movement);
static bool addFavorite();
static void deleteFavorite();
static void saveFavorites();
static void loadFavorites();
void tuneToSelectedFavorite();
#endif

#if defined(ENABLE_SIGNAL_BAR) && ENABLE_SIGNAL_BAR
void smDrawSignalBar(uint8_t rssi);
#endif

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

#if ENABLE_CW_DECODER
static void handleVolumeDownShortPress();
static void cwViewEnter();
static void cwViewExit();
static inline void cwViewTask();
#endif

static void applyBandConfiguration(bool extraSSBReset = false);
static void setAmpState(bool on);
static void applyBrightness();
static void loadSSBPatch();

static void handleDelayedFrequencyUpdate();
static void handleSignalAndStereoUpdates(uint32_t, uint16_t);
static inline void handleCommandTimeout(uint16_t);
static inline void handleSettingsSave(uint16_t);
static inline void checkDisplayTimeout();
static void handlePeriodicTasks();

#if ENABLE_RDS_MINI
bool rdsMiniUiEnabled();
void rdsMiniToggleUi();
void rdsMiniTask(uint32_t);
#else
inline void rdsMiniTask(uint32_t) {}
#endif

// =================================================================================================
// Core data structures (non-settings)
// =================================================================================================

struct Band {
    char name[4];
    uint16_t minimumFreq;
    uint16_t maximumFreq;
    BandType bandType;

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
struct __attribute__((packed)) FavoriteStation {
    uint16_t frequency;
    uint8_t  modulation;
    int16_t  bfo;
};
#endif

// =================================================================================================
// Runtime flags/state
// =================================================================================================

bool g_voltagePinConnected = false;
bool g_ssbLoaded = false;
bool g_stereoStatus = false;
bool autoDisplayOff = false;
bool g_squelchCutoff = false;
bool g_displayOn = true;

// Used by seek callback / interrupt-driven stop logic
volatile bool g_seekStop = false;

uint16_t g_lastAdjustmentTime = 0;    // low16 millis wrap-safe for short UI timeouts
uint16_t g_lastUserActivityTime = 0;  // seconds
bool g_stateIsDirty = false;

// UI / menu state
volatile CommandMode g_activeCommand = CMD_NONE;

bool g_settingsActive = false;
bool g_settingsDirty = false;
uint8_t g_SettingSelected = 0;
uint8_t g_SettingsPage = 1;
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

// =================================================================================================
// Radio state
// =================================================================================================

// Number of bands for seamless coverage - array size for Band g_bandList[g_bandCount] must be consistent
const uint8_t g_bandCount = 44;
const uint8_t g_lastBand = g_bandCount - 1;

uint8_t g_signalQualityValue = 255;
uint16_t g_lastRSSIUpdate = 0;

uint8_t g_muteVolume = 0;
uint8_t g_volume = DEFAULT_VOLUME;

uint8_t g_currentMode = FM;
int16_t g_currentBFO = 0;

int16_t g_savedSsbBfo[g_bandCount] = { 0 };
extern uint8_t g_stableBatteryPercent;

uint8_t g_lastSsbMode = LSB;
uint8_t g_lastCWMode = LSB;

uint16_t g_currentFrequency = 0;
uint16_t g_previousFrequency = 0;
uint16_t g_lastSavedFrequency = 0;
uint8_t g_seekDirection = 1;

uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = false;
uint16_t g_lastSetFreqTime = 0;

// =================================================================================================
// Encoder + buttons + radio object
// =================================================================================================

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

// =================================================================================================
// Bands map
// =================================================================================================

// SW scan limits (seek uses wider limits than a single sub-band)
constexpr uint16_t SW_MIN_FREQ = 1710;
constexpr uint16_t SW_MAX_FREQ = 30000;

// AM-family absolute tuning limits (LW/MW/SW, excludes FM)
// Used by SSB/BFO normalization to avoid invalid kHz (e.g. 149 or 30001),
// which would break band lookup and desync UI/state
constexpr uint16_t SSB_MODE_MIN_FREQ = 150;
constexpr uint16_t SSB_MODE_MAX_FREQ = 30000;

// to track the current band where is 1 = MW 
uint8_t g_bandIndex = 1; // so that muls doesn spread across the code and without sbc r17, r17, uint is needed

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
    { PACK_STR4("160m"),     1800,    2000, SW_BAND_TYPE,  1900, BD },  // 160m amateur
    { PACK_STR4("SW  "),     2001,    2299, SW_BAND_TYPE,  2100, BD },

    { PACK_STR4("120m"),     2300,    2495, SW_BAND_TYPE,  2400, BD },  // 120m broadcast
    { PACK_STR4("SW  "),     2496,    3199, SW_BAND_TYPE,  2800, BD },

    { PACK_STR4("90m "),     3200,    3399, SW_BAND_TYPE,  3300, BD },  // 90m broadcast
    { PACK_STR4("SW  "),     3400,    3499, SW_BAND_TYPE,  3450, BD },

    { PACK_STR4("80m "),     3500,    3899, SW_BAND_TYPE,  3700, BD },  // 80m amateur
    { PACK_STR4("75m "),     3900,    3999, SW_BAND_TYPE,  3950, BD },
    { PACK_STR4("SW  "),     4000,    4749, SW_BAND_TYPE,  4500, BD },

    { PACK_STR4("60m "),     4750,    5060, SW_BAND_TYPE,  4850, BD },  // 60m broadcast
    { PACK_STR4("SW  "),     5061,    5350, SW_BAND_TYPE,  5200, BD },
    { PACK_STR4("60H "),     5351,    5366, SW_BAND_TYPE,  5357, BD },  // 60m amateur (WRC-15)
    { PACK_STR4("SW  "),     5367,    5899, SW_BAND_TYPE,  5500, BD },

    { PACK_STR4("49m "),     5900,    6200, SW_BAND_TYPE,  6000, BD },  // 49m broadcast
    { PACK_STR4("SW  "),     6201,    6999, SW_BAND_TYPE,  6500, BD },

    { PACK_STR4("40m "),     7000,    7200, SW_BAND_TYPE,  7100, BD },  // 40m amateur
    { PACK_STR4("41m "),     7201,    7450, SW_BAND_TYPE,  7300, BD },
    { PACK_STR4("SW  "),     7451,    9399, SW_BAND_TYPE,  8000, BD },

    { PACK_STR4("31m "),     9400,    9900, SW_BAND_TYPE,  9600, BD },  // 31m broadcast
    { PACK_STR4("SW  "),     9901,   10099, SW_BAND_TYPE, 10000, BD },
    { PACK_STR4("30m "),    10100,   10150, SW_BAND_TYPE, 10136, BD },  // 30m amateur
    { PACK_STR4("SW  "),    10151,   11599, SW_BAND_TYPE, 11000, BD },

    { PACK_STR4("25m "),    11600,   12100, SW_BAND_TYPE, 11900, BD },  // 25m broadcast
    { PACK_STR4("SW  "),    12101,   13569, SW_BAND_TYPE, 13000, BD },

    { PACK_STR4("22m "),    13570,   13870, SW_BAND_TYPE, 13700, BD },  // 22m broadcast
    { PACK_STR4("SW  "),    13871,   13999, SW_BAND_TYPE, 13900, BD },

    { PACK_STR4("20m "),    14000,   14350, SW_BAND_TYPE, 14200, BD },  // 20m amateur
    { PACK_STR4("SW  "),    14351,   15099, SW_BAND_TYPE, 15000, BD },

    { PACK_STR4("19m "),    15100,   15800, SW_BAND_TYPE, 15400, BD },  // 19m broadcast
    { PACK_STR4("SW  "),    15801,   17479, SW_BAND_TYPE, 17000, BD },
    { PACK_STR4("16m "),    17480,   18067, SW_BAND_TYPE, 17600, BD },  // 16m broadcast

    { PACK_STR4("17m "),    18068,   18168, SW_BAND_TYPE, 18100, BD },  // 17m amateur
    { PACK_STR4("SW  "),    18169,   20999, SW_BAND_TYPE, 19500, BD },

    { PACK_STR4("15m "),    21000,   21450, SW_BAND_TYPE, 21200, BD },  // 15m amateur
    { PACK_STR4("13m "),    21451,   21850, SW_BAND_TYPE, 21600, BD },  // 13m broadcast
    { PACK_STR4("SW  "),    21851,   24889, SW_BAND_TYPE, 23000, BD },

    { PACK_STR4("12m "),    24890,   24990, SW_BAND_TYPE, 24940, BD },  // 12m amateur
    { PACK_STR4("SW  "),    24991,   26099, SW_BAND_TYPE, 25600, BD },
    { PACK_STR4("CB  "),    26100,   27860, SW_BAND_TYPE, 27200, BD },  // CB radio
    { PACK_STR4("10m "),    27861,   30000, SW_BAND_TYPE, 28500, BD },  // 10m amateur

    // --- FM broadcast band ---
    { PACK_STR4("    "),     6400,   10800, FM_BAND_TYPE,  8400, BD }
};

#undef BD
#undef BM

// =================================================================================================
// Bandwidth tables (PROGMEM)
// =================================================================================================

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

// =================================================================================================
// Step tables (PROGMEM + RAM)
// =================================================================================================

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

// Timeout values in seconds for the display-off feature, indexed by the setting parameter
const uint16_t T[5] PROGMEM = { 0, 600, 900, 1800, 3600 };

extern const char g_bandModeDesc[][4];

#if ENABLE_FAVORITES
FavoriteStation g_favorites[MAX_FAVORITES];
#endif
