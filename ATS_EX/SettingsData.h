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
//   - g_SettingsMeta[] (PROGMEM: name, default, flags, textBase, max, callback)
//   - paramTexts[][] (PROGMEM labels used by UI formatting)
//   - Navigation tables for column-first cursor movement
//
// Business logic (do*() callbacks) is in SettingsHandlers.h
//
// ====================================================================================

#include "Arduino.h"
#include <avr/pgmspace.h>

#define SETTINGS_CB(name)        void name(int8_t v)
#define SETTING_PARAM(idx)       g_SettingsParams[(uint8_t)(idx)]
#define META_LPM(idx, field)     pgm_read_byte(&g_SettingsMeta[(idx)].field)

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

// Applicability category packed into SettingMeta.flags (bits 2..3)
enum ActiveCat : uint8_t {
    ACT_ALWAYS,
    ACT_AM,
    ACT_SSB,
    ACT_FM
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

    // --- Page 6: Advanced RF ---
    SsbAgcSpeed,
    AmSmRate,

    SETTINGS_MAX
};

// Skip EEPROM clamp for this slot
// (uint8_t)val > 255 is never true, so signed values like BFO stay intact
enum : uint8_t { SETTING_NO_CLAMP = 255 };

// =================================================================================================
// Settings structs
// =================================================================================================

// Factory defaults for g_modeSettings[][] initialization
struct ModeDefaults {
    const int8_t agc;
    const int8_t soft_mute;
    const int8_t avc;
};

// =================================================================================================
// Function pointer types for PROGMEM access
// =================================================================================================

typedef void (*SettingCallback)(int8_t);

// =================================================================================================
// Forward declarations for callbacks (defined in SettingsHandlers.h)
// =================================================================================================

SETTINGS_CB(doAttenuation);
SETTINGS_CB(doAvc);
SETTINGS_CB(doSquelch);
SETTINGS_CB(doSoftMute);
SETTINGS_CB(doSoftMuteThreshold);
SETTINGS_CB(doAMNoiseBlanker);

SETTINGS_CB(doBFOCalibration);
SETTINGS_CB(doSSBSoftMuteMode);
SETTINGS_CB(doSSBAVC);
SETTINGS_CB(doCutoffFilter);
SETTINGS_CB(doSync);
SETTINGS_CB(doCWPitch);

SETTINGS_CB(doDeEmp);
SETTINGS_CB(doFMAudioProfile);
SETTINGS_CB(doForceMono);
SETTINGS_CB(doFmSoftMuteAtt);
SETTINGS_CB(doFmSoftMuteThr);
SETTINGS_CB(doSwAfcProfile);

SETTINGS_CB(doBrightness);
SETTINGS_CB(doSMeter);
SETTINGS_CB(doSWUnits);
SETTINGS_CB(doDisplayOff);
SETTINGS_CB(doRSSIAMOff);
SETTINGS_CB(doNavStyle);

SETTINGS_CB(doAntennaCapacitor);
SETTINGS_CB(doCPUSpeed);
SETTINGS_CB(doBatteryPinSelect);
SETTINGS_CB(doScanSwitch);
SETTINGS_CB(doFmVolAdjust);
SETTINGS_CB(doSwLink);

SETTINGS_CB(doSsbAgcSpeed);
SETTINGS_CB(doAmSmRate);

#undef SETTINGS_CB

// Forward declarations for applicability predicates (defined in SettingsLogic.h)
static bool isAMFamilyActive();
static bool isSSBActive();
static bool NOINLINE isFMActive();

// =================================================================================================
// SSB RF AGC Attack and Release Rate Tables
// =================================================================================================
// Maps menu indices (Fast Medium Slow) to Si4735-D60 register values
// Rate (dB/s) = 5600 / Value Higher value = slower response
// Attack (0x3700): 4 (1400 dB/s) 8 (700 dB/s) 16 (350 dB/s)
// Release (0x3701): 24 (233 dB/s) 60 (93 dB/s) 140 (40 dB/s)
PGM_U8(ssb_agc_attack_tbl,  4,  8,  16);
PGM_U8(ssb_agc_release_tbl, 24, 60, 140);

// =================================================================================================
// AM / SSB Soft Mute Rate Table
// =================================================================================================
// Maps menu indices (Fast Medium Slow) to Soft Mute rate register (0x3300)
// Rate (dB/s) = Value x 4.35 Higher value = faster response
// Values: 128 (556 dB/s) 64 (278 dB/s default) 32 (139 dB/s)
PGM_U8(am_sm_rate_tbl, 128, 64, 32);

// =================================================================================================
// Settings metadata (names, defaults, types, callbacks)
// =================================================================================================

// Packed UI / applicability bits for one setting slot
//
//   bit  7 6 5 4 3 2 1 0
//        - - - I A A T T
//
//   T T  type      SettingType: ZeroAuto / Num / Switch / SwitchAuto
//   A A  active    ActiveCat:   ALWAYS / AM / SSB / FM   (grey-out)
//   I    inverted  0: textIdx = textBase + param
//                  1: textIdx = textBase - param   (ON/OFF polarity)
//   -    reserved  must stay 0
//
// textBase is a separate byte: first paramTexts[] index for this slot.
// Num slots usually keep textBase = 0 (numeric path, not switch labels)
#define SF_TYPE_MASK    0x03
#define SF_ACTIVE_SHIFT 2
#define SF_ACTIVE_MASK  0x0C
#define SF_INVERTED     0x10

#define SN(s) { s[0], s[1], s[2] }
#define SF(type, act, inv) \
    ((uint8_t)((uint8_t)(type) | ((uint8_t)(act) << 2) | ((uint8_t)(inv) << 4)))
#define META(nm, def, type, act, inv, tbase, maxv, cb) \
    { SN(nm), (def), SF(type, act, inv), (tbase), (maxv), (cb) }

struct SettingMeta {
    char            name[3];     // without '\0'; getSettingName() appends it
    int8_t          defaultVal;
    uint8_t         flags;
    uint8_t         textBase;
    uint8_t         maxVal;      // EEPROM clamp; SETTING_NO_CLAMP = skip
    SettingCallback callback;
};

#if defined(__AVR__) && !defined(__INTELLISENSE__)
static_assert(sizeof(SettingMeta) == 9, "SettingMeta must be 9 bytes");
#endif

const SettingMeta g_SettingsMeta[SETTINGS_MAX] PROGMEM = {
    // --- Page 1: Core Audio & RF ---
    META("ATT",  0, ZeroAuto,   ACT_ALWAYS, 0,  0, MAX_ATTENUATION_AM_DB,       doAttenuation      ),
    META("AVC", 10, Num,        ACT_AM,     0,  0, AVC_MAX_INDEX,               doAvc              ),
    META("SQL",  0, Num,        ACT_ALWAYS, 0,  0, SQUELCH_MAX_LEVEL,           doSquelch          ),
    META("SMA",  0, Num,        ACT_AM,     0,  0, SOFT_MUTE_MAX_ATTENUATION,   doSoftMute         ),
    META("SMT",  0, Num,        ACT_AM,     0,  0, SOFT_MUTE_MAX_SNR_THRESHOLD, doSoftMuteThreshold),
    META("ANB",  0, Switch,     ACT_AM,     0,  1, 1,                           doAMNoiseBlanker   ),

    // --- Page 2: SSB & CW ---
    META("BFO",  0, Num,        ACT_AM,     0,  0, SETTING_NO_CLAMP,            doBFOCalibration   ),
    META("SSM",  1, Switch,     ACT_SSB,    0,  7, 1,                           doSSBSoftMuteMode  ),
    META("SVC",  1, Switch,     ACT_SSB,    1,  2, 1,                           doSSBAVC           ),
    META("COF",  0, SwitchAuto, ACT_SSB,    0,  0, CUTOFF_FILTER_MAX_VALUE,     doCutoffFilter     ),
    META("SYN",  0, Switch,     ACT_SSB,    1,  2, 1,                           doSync             ),
    META("CWP",  7, Num,        ACT_AM,     0,  0, 8,                           doCWPitch          ),

    // --- Page 3: FM & Advanced Audio ---
    META("DE ",  0, Switch,     ACT_FM,     0,  3, 1,                           doDeEmp            ),
    META("FMP",  1, Switch,     ACT_FM,     0,  1, 1,                           doFMAudioProfile   ),
    META("FMO",  0, Switch,     ACT_FM,     1,  2, 1,                           doForceMono        ),
    META("FSA", 22, Num,        ACT_FM,     0,  0, FM_SOFT_MUTE_MAX_ATTN_LEVEL, doFmSoftMuteAtt    ),
    META("FST",  0, Num,        ACT_FM,     0,  0, FM_SOFT_MUTE_MAX_SNR_LEVEL,  doFmSoftMuteThr    ),
    META("SWA",  0, Num,        ACT_AM,     1,  2, SW_AFC_PROFILE_HZ_AGGR,      doSwAfcProfile     ),

    // --- Page 4: Display & UI ---
    META("SCR",  4, Num,        ACT_ALWAYS, 0,  0, BRIGHTNESS_MAX_LEVEL,        doBrightness       ),
    META("SPT",  0, Switch,     ACT_ALWAYS, 0, 17, 3,                           doSMeter           ),   // 0..3: "RSS" "SPT" "R+B" "S+B"
    META("SWU",  0, Switch,     ACT_AM,     0,  5, 1,                           doSWUnits          ),
    META("DIS",  0, Switch,     ACT_ALWAYS, 0,  0, DISPLAY_OFF_TIMER_MAX_LEVEL, doDisplayOff       ),
    META("RSI",  1, Switch,     ACT_AM,     0,  1, 1,                           doRSSIAMOff        ),
    META("NAV",  0, Switch,     ACT_ALWAYS, 0, 15, 1,                           doNavStyle         ),

    // --- Page 5: Hardware Configuration ---
    META("CAP",  0, Switch,     ACT_ALWAYS, 0,  1, 1,                           doAntennaCapacitor ),
    META("CPU",  0, Switch,     ACT_ALWAYS, 0,  9, 1,                           doCPUSpeed         ),
    META("BAP",  0, Switch,     ACT_ALWAYS, 0,  0, 1,                           doBatteryPinSelect ),
    META("SCN",  1, Switch,     ACT_ALWAYS, 1,  2, 1,                           doScanSwitch       ),
    META("FVA",  0, Num,        ACT_ALWAYS, 0,  0, 15,                          doFmVolAdjust      ),
    META("SWL",  0, Switch,     ACT_ALWAYS, 1,  2, 1,                           doSwLink           ),

    // --- Page 6: Advanced RF ---
    META("AGS",  1, Switch,     ACT_SSB,    0, 21, 2,                           doSsbAgcSpeed      ),
    META("SMR",  1, Switch,     ACT_AM,     0, 21, 2,                           doAmSmRate         ),
};

#undef META
#undef SN
#undef SF

// RAM: Only mutable parameter values (menu edits this buffer)
int8_t g_SettingsParams[SETTINGS_MAX];

// Toggles a binary setting (0 or 1)
static void toggleSetting(uint8_t settingIndex) {
    // All callers use this only for true switch params (0/1)
    SETTING_PARAM(settingIndex) ^= 1;
}

// Precomputed page start indices
// Index 0 unused, pages are 1-based
const uint8_t g_pageStartIdx[7] PROGMEM = { 0, 0, 6, 12, 18, 24, 30 };

// =================================================================================================
// Settings API (RAM access)
// =================================================================================================

template<typename T>
static inline int8_t getSettingParam(T idx) {
    return SETTING_PARAM(idx);
}

template<typename T>
static inline void setSettingParam(T idx, int8_t val) {
    SETTING_PARAM(idx) = val;
}

static inline int8_t& settingRef(SettingsIndex idx) {
    return SETTING_PARAM(idx);
}

// =================================================================================================
// Accessors for PROGMEM metadata
// =================================================================================================

static uint8_t NOINLINE settingFlags(uint8_t idx) {
    return META_LPM(idx, flags);
}

inline void getSettingName(uint8_t idx, char* buf) {
    memcpy_P(buf, g_SettingsMeta[idx].name, 3);
    buf[3] = '\0';
}

inline uint8_t getSettingType(uint8_t idx) {
    return settingFlags(idx) & SF_TYPE_MASK;
}

inline bool getSettingInverted(uint8_t idx) {
    return settingFlags(idx) & SF_INVERTED;
}

inline uint8_t getSettingTextBase(uint8_t idx) {
    return META_LPM(idx, textBase);
}

inline int8_t getSettingDefault(uint8_t idx) {
    return (int8_t)META_LPM(idx, defaultVal);
}

inline uint8_t getSettingMax(uint8_t idx) {
    return META_LPM(idx, maxVal);
}

inline void callSettingCallback(uint8_t idx, int8_t v) {
    ((SettingCallback)pgm_read_ptr(&g_SettingsMeta[idx].callback))(v);
}

inline bool isSettingActive(uint8_t idx) {
    switch ((settingFlags(idx) & SF_ACTIVE_MASK) >> SF_ACTIVE_SHIFT) {
        case ACT_AM:  return isAMFamilyActive();
        case ACT_SSB: return isSSBActive();
        case ACT_FM:  return isFMActive();
        default:      return true;
    }
}

#undef META_LPM
#undef SETTING_PARAM
#undef SF_TYPE_MASK
#undef SF_ACTIVE_SHIFT
#undef SF_ACTIVE_MASK
#undef SF_INVERTED

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
    NAV_PAGE6_ORDER(24),
    30, 31
};

// Physical settings index to Navigation position
const uint8_t g_navColFirstReverse[SETTINGS_MAX] PROGMEM = {
    NAV_PAGE6_REVERSE(0),
    NAV_PAGE6_REVERSE(6),
    NAV_PAGE6_REVERSE(12),
    NAV_PAGE6_REVERSE(18),
    NAV_PAGE6_REVERSE(24),
    30, 31
};

#undef NAV_PAGE6_ORDER
#undef NAV_PAGE6_REVERSE
#undef NAV_PAGE5_ORDER
#undef NAV_PAGE5_REVERSE

// get next setting index with wrap-around based on NAV mode
static inline uint8_t getNextSettingIndex(uint8_t current, int16_t delta) {
    int16_t next;
    const bool col = getSettingParam(NAV) != 0;

    if (!col) {
        // linear order
        next = (int16_t)current + delta;
    } else {
        // column order
        next = (int16_t)pgm_read_byte(&g_navColFirstReverse[current]) + delta;
    }

    while (next < 0) next += SETTINGS_MAX;
    while (next >= SETTINGS_MAX) next -= SETTINGS_MAX;

    // mapping back to physical index for column ord
    if (col) {
        return pgm_read_byte(&g_navColFirstOrder[(uint8_t)next]);
    }

    return (uint8_t)next;
}

// =================================================================================================
// Switch formatting mapping tables
// =================================================================================================

// UI texts (PROGMEM), fixed width 3 + '\0'
const char paramTexts[][4] PROGMEM = {
  "AUT", " ON", "OFF", " 50", " 75", "kHz", "MHz",
  "RSS", "SNR", "100", "50%",
  "10m", "15m", "30m", "60m",
  "ROW", "COL",
  "RSS", "SPT", "R+B", "S+B",
  "FST", "NRM", "SLW"
};
