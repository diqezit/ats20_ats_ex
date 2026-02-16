#pragma once
// ====================================================================================
//
// SettingsData.h
//
// Central place for ALL settings data:
//
//   - Settings enums and structs
//   - DEFAULT_MODE_SETTINGS (factory defaults for mode-dependent settings)
//   - g_modeSettings[][] (mode-dependent settings storage; synced with EEPROM)
//   - g_SettingsParams[] (settings values buffer; menu edits this array)
//   - g_SettingsMeta[] (PROGMEM metadata: names, types, callbacks)
//   - switch_setting_map[] + paramTexts[][] (PROGMEM tables used by UI formatting)
//   - Navigation tables for column-first cursor movement
//
// Business logic (do*() callbacks) is in SettingsHandlers.h
// 
// ====================================================================================

#include "Arduino.h"
#include <avr/pgmspace.h>

// =================================================================================================
// Settings enums
// =================================================================================================

// Mode-dependent setting slots inside g_modeSettings[][]
enum ModeSettingType : uint8_t {
    MODE_SETTING_AGC,
    MODE_SETTING_SOFT_MUTE,
    MODE_SETTING_AVC,
    MODE_SETTINGS_COUNT
};

// Context selector for mode-dependent settings
// CW does not have its own context; it inherits LSB/USB.
enum ModeContext : uint8_t {
    MODE_CONTEXT_AM,
    MODE_CONTEXT_LSB,
    MODE_CONTEXT_USB,
    MODE_CONTEXT_COUNT = 3
};

// How a setting value is represented/handled in UI
enum SettingType : uint8_t {
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

// Global list of settings indices (used for array indexing + EEPROM layout)
enum SettingsIndex : uint8_t {
    // --- Page 1: Core Audio & RF ---
    ATT,
    AutoVolControl,
    SQL,
    SoftMute,
    SoftMuteThr,
    AMNoiseBlanker,

    // --- Page 2: SSB & CW ---
    BFO,
    SSM,
    SVC,
    CutoffFilter,
    Sync,
    CWPitch,

    // --- Page 3: FM & Advanced Audio ---
    DeEmp,
    FMAudioProfile,
    ForceMono,
    FmSmAtt,
    FmSmThr,
    SWAFC,

    // --- Page 4: Display & UI ---
    Brightness,
    SMeter,
    SWUnits,
    DisplayOff,
    RSSI_AM_Off,
    NAV,

    // --- Page 5: Hardware Configuration ---
    AntennaCap,
    CPUSpeed,
    BATT_PIN,
    ScanSwitch,
    FmVolAdjust,
    SWLink,

    SETTINGS_MAX
};

// =================================================================================================
// Settings structs
// =================================================================================================

// Factory defaults for g_modeSettings[][] initialization
struct ModeDefaults {
    const int8_t agc;
    const int8_t soft_mute;
    const int8_t avc;
};

// Switch formatting rule (maps param -> paramTexts[] index)
struct SwitchMapEntry {
    uint8_t baseIndex;
    bool inverted;
};

// =================================================================================================
// Function pointer types for PROGMEM access
// =================================================================================================

typedef void (*SettingCallback)(int8_t);
typedef bool (*ActiveCheckFunc)();

// =================================================================================================
// Forward declarations for callbacks (defined in SettingsHandlers.h)
// =================================================================================================

void doAttenuation(int8_t v);
void doAvc(int8_t v);
void doSquelch(int8_t v);
void doSoftMute(int8_t v);
void doSoftMuteThreshold(int8_t v);
void doAMNoiseBlanker(int8_t v);

void doBFOCalibration(int8_t v);
void doSSBSoftMuteMode(int8_t v);
void doSSBAVC(int8_t v);
void doCutoffFilter(int8_t v);
void doSync(int8_t v);
void doCWPitch(int8_t v);

void doDeEmp(int8_t v);
void doFMAudioProfile(int8_t v);
void doForceMono(int8_t v);
void doFmSoftMuteAtt(int8_t v);
void doFmSoftMuteThr(int8_t v);
void doSwAfcProfile(int8_t v);

void doBrightness(int8_t v);
void doSMeter(int8_t v);
void doSWUnits(int8_t v);
void doDisplayOff(int8_t v);
void doRSSIAMOff(int8_t v);
void doNavStyle(int8_t v);

void doAntennaCapacitor(int8_t v);
void doCPUSpeed(int8_t v);
void doBatteryPinSelect(int8_t v);
void doScanSwitch(int8_t v);
void doFmVolAdjust(int8_t v);
void doSwLink(int8_t v);

// Forward declarations for applicability predicates (defined in SettingsLogic.h)
static bool isAlwaysActive();
static bool isAMFamilyActive();
static bool isSSBActive();
static bool __attribute__((noinline)) isFMActive();

// =================================================================================================
// Settings metadata (names, defaults, types, callbacks)
// =================================================================================================

struct SettingMeta {
    char name[4];
    int8_t defaultVal;
    uint8_t type;
    SettingCallback callback;
    ActiveCheckFunc isActive;
};

const SettingMeta g_SettingsMeta[SETTINGS_MAX] PROGMEM = {
    // --- Page 1: Core Audio & RF ---
    { "ATT", 0,  ZeroAuto,   doAttenuation,       isAlwaysActive   },
    { "AVC", 10, Num,        doAvc,               isAMFamilyActive },
    { "SQL", 0,  Num,        doSquelch,           isAlwaysActive   },
    { "SMA", 0,  Num,        doSoftMute,          isAMFamilyActive },
    { "SMT", 0,  Num,        doSoftMuteThreshold, isAMFamilyActive },
    { "ANB", 0,  Switch,     doAMNoiseBlanker,    isAMFamilyActive },

    // --- Page 2: SSB & CW ---
    { "BFO", 0,  Num,        doBFOCalibration,    isAMFamilyActive },
    { "SSM", 1,  Switch,     doSSBSoftMuteMode,   isSSBActive      },
    { "SVC", 1,  Switch,     doSSBAVC,            isSSBActive      },
    { "COF", 0,  SwitchAuto, doCutoffFilter,      isSSBActive      },
    { "SYN", 0,  Switch,     doSync,              isSSBActive      },
    { "CWP", 7,  Num,        doCWPitch,           isAMFamilyActive },

    // --- Page 3: FM & Advanced Audio ---
    { "DE ", 0,  Switch,     doDeEmp,             isFMActive       },
    { "FMP", 1,  Switch,     doFMAudioProfile,    isFMActive       },
    { "FMO", 0,  Switch,     doForceMono,         isFMActive       },
    { "FSA", 22, Num,        doFmSoftMuteAtt,     isFMActive       },
    { "FST", 0,  Num,        doFmSoftMuteThr,     isFMActive       },
    { "SWA", 0,  Num,        doSwAfcProfile,      isAMFamilyActive },

    // --- Page 4: Display & UI ---
    { "SCR", 4,  Num,        doBrightness,        isAlwaysActive   },
    { "SPT", 0,  Switch,     doSMeter,            isAlwaysActive   },
    { "SWU", 0,  Switch,     doSWUnits,           isAMFamilyActive },
    { "DIS", 0,  Switch,     doDisplayOff,        isAlwaysActive   },
    { "RSI", 1,  Switch,     doRSSIAMOff,         isAMFamilyActive },
    { "NAV", 0,  Switch,     doNavStyle,          isAlwaysActive   },

    // --- Page 5: Hardware Configuration ---
    { "CAP", 0,  Switch,     doAntennaCapacitor,  isAlwaysActive   },
    { "CPU", 0,  Switch,     doCPUSpeed,          isAlwaysActive   },
    { "BAP", 0,  Switch,     doBatteryPinSelect,  isAlwaysActive   },
    { "SCN", 1,  Switch,     doScanSwitch,        isAlwaysActive   },
    { "FVA", 0,  Num,        doFmVolAdjust,       isAlwaysActive   },
    { "SWL", 0,  Switch,     doSwLink,            isAlwaysActive   },
};

// RAM: Only mutable parameter values (menu edits this buffer)
int8_t g_SettingsParams[SETTINGS_MAX];

// Toggles a binary setting (0 or 1)
static void toggleSetting(uint8_t settingIndex) {
    // All callers use this only for true switch params (0/1)
    g_SettingsParams[settingIndex] ^= 1;
}

// Precomputed page start indices
// Index 0 unused, pages are 1-based
const uint8_t g_pageStartIdx[6] PROGMEM = { 0, 0, 6, 12, 18, 24 };

// =================================================================================================
// Settings API (RAM access)
// =================================================================================================

template<typename T>
static inline int8_t getSettingParam(T idx) {
    return g_SettingsParams[(uint8_t)idx];
}

template<typename T>
static inline void setSettingParam(T idx, int8_t val) {
    g_SettingsParams[(uint8_t)idx] = val;
}

static inline int8_t& settingRef(SettingsIndex idx) {
    return g_SettingsParams[(uint8_t)idx];
}

// =================================================================================================
// Accessors for PROGMEM metadata
// =================================================================================================

inline void getSettingName(uint8_t idx, char* buf) {
    memcpy_P(buf, g_SettingsMeta[idx].name, 4);
}

inline uint8_t getSettingType(uint8_t idx) {
    return pgm_read_byte(&g_SettingsMeta[idx].type);
}

inline int8_t getSettingDefault(uint8_t idx) {
    return (int8_t)pgm_read_byte(&g_SettingsMeta[idx].defaultVal);
}

inline void callSettingCallback(uint8_t idx, int8_t v) {
    SettingCallback cb = (SettingCallback)pgm_read_ptr(&g_SettingsMeta[idx].callback);
    if (cb) cb(v);
}

inline bool isSettingActive(uint8_t idx) {
    ActiveCheckFunc fn = (ActiveCheckFunc)pgm_read_ptr(&g_SettingsMeta[idx].isActive);
    return fn ? fn() : true;
}

// =================================================================================================
// Initialization helper
// =================================================================================================

// Initialize all settings to their default values from PROGMEM
// Called on first boot or after EEPROM reset
inline void initSettingsDefaults() {
    for (uint8_t i = 0; i < SETTINGS_MAX; i++) {
        g_SettingsParams[i] = getSettingDefault(i);
    }
}

// =================================================================================================
// Mode-dependent settings defaults + live storage
// =================================================================================================

// Defaults for mode-dependent settings
// EEPROM erased state is 0xFF which is -1 for int8_t,
// so code can detect "uninitialized" and apply these defaults
static constexpr ModeDefaults DEFAULT_MODE_SETTINGS = { 0, 0, 10 };

// Live storage (EEPROM <-> RAM)
int8_t g_modeSettings[MODE_SETTINGS_COUNT][MODE_CONTEXT_COUNT];

// =================================================================================================
// Navigation tables for Column-first cursor movement
// =================================================================================================
// NAV affects ONLY cursor movement order, NOT visual layout
// Visual layout is ALWAYS Row-first: 0,1 / 2,3 / 4,5 per page
//
// Column-first cursor path on page:
//   [0] [1]       1st --- 4th
//   [2] [3]       2nd --- 5th
//   [4] [5]       3rd --- 6th

// Full page pattern (6 items) left column first, then right
#define NAV_PAGE6_ORDER(B)   (B)+0, (B)+2, (B)+4, (B)+1, (B)+3, (B)+5
#define NAV_PAGE6_REVERSE(B) (B)+0, (B)+3, (B)+1, (B)+4, (B)+2, (B)+5

// Partial page pattern (5 items) page 5 has no slot 29
#define NAV_PAGE5_ORDER(B)   (B)+0, (B)+2, (B)+4, (B)+1, (B)+3
#define NAV_PAGE5_REVERSE(B) (B)+0, (B)+3, (B)+1, (B)+4, (B)+2

// Navigation position to Physical settings index
const uint8_t g_navColFirstOrder[SETTINGS_MAX] PROGMEM = {
    NAV_PAGE6_ORDER(0),
    NAV_PAGE6_ORDER(6),
    NAV_PAGE6_ORDER(12),
    NAV_PAGE6_ORDER(18),
    NAV_PAGE6_ORDER(24)
};

// Physical settings index to Navigation position
const uint8_t g_navColFirstReverse[SETTINGS_MAX] PROGMEM = {
    NAV_PAGE6_REVERSE(0),
    NAV_PAGE6_REVERSE(6),
    NAV_PAGE6_REVERSE(12),
    NAV_PAGE6_REVERSE(18),
    NAV_PAGE6_REVERSE(24)
};

#undef NAV_PAGE6_ORDER
#undef NAV_PAGE6_REVERSE
#undef NAV_PAGE5_ORDER
#undef NAV_PAGE5_REVERSE

// get next setting index with wrap-around based on NAV mode
static inline uint8_t getNextSettingIndex(uint8_t current, int16_t delta) {
    int16_t next;

    if (getSettingParam(NAV) == 0) {
        // linear order
        next = (int16_t)current + delta;
    } else {
        // column order
        next = (int16_t)pgm_read_byte(&g_navColFirstReverse[current]) + delta;
    }

    while (next < 0) next += SETTINGS_MAX;
    while (next >= SETTINGS_MAX) next -= SETTINGS_MAX;

    // mapping back to physical index for column ord
    if (getSettingParam(NAV) != 0) {
        return pgm_read_byte(&g_navColFirstOrder[(uint8_t)next]);
    }

    return (uint8_t)next;
}

// =================================================================================================
// Switch formatting mapping tables
// =================================================================================================

const SwitchMapEntry switch_setting_map[SETTINGS_MAX] PROGMEM = {
    /* ATT            */ {0,  false},
    /* AutoVolControl */ {0,  false},
    /* SQL            */ {0,  false},
    /* SoftMute       */ {0,  false},
    /* SoftMuteThr    */ {0,  false},
    /* AMNoiseBlanker */ {1,  false},

    /* BFO            */ {0,  false},
    /* SSM            */ {7,  false},
    /* SVC            */ {2,  true},
    /* CutoffFilter   */ {0,  false},
    /* Sync           */ {2,  true},
    /* CWPitch        */ {0,  false},

    /* DeEmp          */ {3,  false},
    /* FMAudioProfile */ {1,  false},
    /* ForceMono      */ {2,  true},
    /* FmSmAtt        */ {0,  false},
    /* FmSmThr        */ {0,  false},
    /* SWAFC          */ {2,  true},

    /* Brightness     */ {0,  false},

    /* SMeter         */ {17, false},   // 0..3: "RSS" "SPT" "R+B" "S+B"

    /* SWUnits        */ {5,  false},
    /* DisplayOff     */ {0,  false},
    /* RSSI_AM_Off    */ {1,  false},
    /* NAV            */ {15, false},

    /* AntennaCap     */ {1,  false},
    /* CPUSpeed       */ {9,  false},
    /* BATT_PIN       */ {0,  false},
    /* ScanSwitch     */ {2,  true},
    /* FmVolAdjust    */ {0,  false},
    /* SWLink         */ {2,  true},
};

// UI texts (PROGMEM), fixed width 3 + '\0'
const char paramTexts[][4] PROGMEM = {
  "AUT", " ON", "OFF", " 50", " 75", "kHz", "MHz",
  "RSS", "SNR", "100", "50%",
  "10m", "15m", "30m", "60m",
  "ROW", "COL",

  // SMeter UI mode (SMeter = 0..3)
  "RSS", "SPT", "R+B", "S+B"
};
