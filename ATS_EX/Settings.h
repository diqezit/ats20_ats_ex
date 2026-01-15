#pragma once
// ====================================================================================
//
// Settings.h
//
// Central place for ALL settings data:
//
//   - Settings enums and structs
//   - DEFAULT_MODE_SETTINGS (factory defaults for mode-dependent settings)
//   - g_modeSettings[][] (mode-dependent settings storage; synced with EEPROM)
//   - g_SettingsParams[] (settings values buffer; menu edits this array)
//   - g_SettingsMeta[] (PROGMEM metadata: names, types, callbacks)
//   - switch_setting_map[] + paramTexts[][] (PROGMEM tables used by UI formatting)
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
// Forward declarations (callbacks + applicability predicates)
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

static inline bool isAlwaysActive();
static inline bool isAMFamilyActive();
static inline bool isSSBActive();
static inline bool isFMActive();

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
    { "FST", 10, Num,        doFmSoftMuteThr,     isFMActive       },
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
};

// RAM: Only mutable parameter values (menu edits this buffer)
int8_t g_SettingsParams[SETTINGS_MAX];

// Toggles a binary setting (0 or 1)
static void toggleSetting(uint8_t settingIndex) {
    g_SettingsParams[settingIndex] = 1 - g_SettingsParams[settingIndex];
}

// =================================================================================================
// Settings API (RAM access)
// =================================================================================================

// Read setting parameter from RAM buffer
static inline int8_t getSettingParam(uint8_t idx) {
    return g_SettingsParams[idx];
}

// Write setting parameter to RAM buffer
static inline void setSettingParam(uint8_t idx, int8_t val) {
    g_SettingsParams[idx] = val;
}

// Enum-friendly overloads
static inline int8_t getSettingParam(SettingsIndex idx) {
    return g_SettingsParams[(uint8_t)idx];
}

static inline void setSettingParam(SettingsIndex idx, int8_t val) {
    g_SettingsParams[(uint8_t)idx] = val;
}

// Reference access (for doSwitchLogic and other in-place modifiers)
static inline int8_t& settingRef(uint8_t idx) {
    return g_SettingsParams[idx];
}

static inline int8_t& settingRef(SettingsIndex idx) {
    return g_SettingsParams[(uint8_t)idx];
}

// =================================================================================================
// accessors for PROGMEM metadata only to keep call sites clean
// =================================================================================================

// Read setting name into buffer (4 chars including null terminator area)
inline void getSettingName(uint8_t idx, char* buf) {
    memcpy_P(buf, g_SettingsMeta[idx].name, 4);
}

// Read setting type from PROGMEM
inline uint8_t getSettingType(uint8_t idx) {
    return pgm_read_byte(&g_SettingsMeta[idx].type);
}

// Read default value from PROGMEM
inline int8_t getSettingDefault(uint8_t idx) {
    return (int8_t)pgm_read_byte(&g_SettingsMeta[idx].defaultVal);
}

// Call the setting's manipulation callback
inline void callSettingCallback(uint8_t idx, int8_t v) {
    SettingCallback cb = (SettingCallback)pgm_read_ptr(&g_SettingsMeta[idx].callback);
    if (cb) cb(v);
}

// Check if setting is active in current mode
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
    /* SMeter         */ {2,  true},
    /* SWUnits        */ {5,  false},
    /* DisplayOff     */ {0,  false},
    /* RSSI_AM_Off    */ {1,  false},
    /* NAV            */ {15, false},

    /* AntennaCap     */ {1,  false},
    /* CPUSpeed       */ {9,  false},
    /* BATT_PIN       */ {0,  false},
    /* ScanSwitch     */ {2,  true},
    /* FmVolAdjust    */ {0,  false},
};

// UI texts (PROGMEM), fixed width 3 + '\0'
const char paramTexts[][4] PROGMEM = {
  "AUT", " ON", "OFF", " 50", " 75", "kHz", "MHz",
  "RSS", "SNR", "100", "50%",
  "10m", "15m", "30m", "60m",
  "ROW", "COL"
};
