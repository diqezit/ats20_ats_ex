// ----------------------------------------------------------------------
// ATS_EX (Extended) Firmware for ATS-20 and ATS-20+ receivers.
// Based on PU2CLR sources.
// Inspired by closed-source swling.ru firmware.
// For more information check README file in my github repository:
// http://github.com/goshante/ats20_ats_ex
// ----------------------------------------------------------------------
// By Goshante
// 02.2024
// http://github.com/goshante
// ----------------------------------------------------------------------
// MOD_NO_RDS_v4.1 by diqezit
// More info for this mod you can get below
// https://github.com/diqezit/ats20_ats_ex
// ----------------------------------------------------------------------
// Si4704/05/06/3x FM Receiver Programming:
// – Hardware interface control (I2C signal mappings, GPIO functions)
// – Software command set (register definitions, status reads/writes)
// – Configuration workflows (tuning, volume, seek, power modes and more..)
// Ref here https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN332.pdf
// ----------------------------------------------------------------------
// Using the work of 
// https://github.com/esp32-si4732/ats-mini
// https://github.com/G8PTN/ATS_MINI
// ----------------------------------------------------------------------

#include <SI4735.h>
#include <EEPROM.h>
#include <Tiny4kOLED.h>
#include <PixelOperatorBold.h>

#include "font14x24sevenSeg.h"
#include "Rotary.h"
#include "SimpleButton.h"
#include "patch_ssb_compressed.h"

#include "defs.h"
#include "globals.h"
#include "Utils.h"

static bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// --------------------------
// ------- Main logic -------
// --------------------------

constexpr auto APP_VERSION = 41;

// Helper function to get the current context (AM or SSB/CW)
static ModeContext getModeContext() {
    if (isSSB() || g_currentMode == CW) {
        return MODE_CONTEXT_SSB;
    }
    return MODE_CONTEXT_AM;
}

// Initializes mode-dependent settings to their default values
static void initializeDefaultModeSettings() {
    for (uint8_t i = 0; i < MODE_CONTEXT_COUNT; i++) {
        g_modeSettings[MODE_SETTING_AGC][i] = defaultModeSettings[i].agc;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][i] = defaultModeSettings[i].soft_mute;
        g_modeSettings[MODE_SETTING_AVC][i] = defaultModeSettings[i].avc;
    }
}

//Initialize controller
void setup() {
    safeAmpOff();

    DDRB |= (1 << DDB5);
    DDRD &= ~((1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B));
    PORTD |= (1 << ENCODER_PIN_A) | (1 << ENCODER_PIN_B);

    g_voltagePinConnnected = analogRead(BATTERY_VOLTAGE_PIN) > 300;

    oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
    oled.clear();
    oled.on();
    oled.setFont(DEFAULT_FONT);

    // Force EEPROM reset if specific buttons are held on startup
    if (!(PINC & (1 << (ENCODER_BUTTON - 14))) || !(PINB & (1 << (AGC_BUTTON - 8)))) {
        // Invalidate version to trigger reset logic
        EEPROM.write(EEPROM_VERSION_ADDRESS, 0);
    }
    else {
#if ENABLE_SPLASH_SCREEN
        showSplashScreen();
#endif
    }

    // The rest of the setup is common for both paths
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);

    delay(500);

    // Load configuration from EEPROM or initialize with defaults
    readAllReceiverInformation();

    loadFMFav();

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();

    applyBandConfiguration();
    g_currentFrequency = g_previousFrequency = g_si4735.getFrequency();
    g_si4735.setVolume(g_volume);

    oled.clear();
    showStatus();

    safeAmpOn();
}

// Startup screen
void showSplashScreen() {
    oled.clear();
    oled.setFont(DEFAULT_FONT);

    oled.setCursor(18, 1);
    oled.print(F("ATS-20+ 1.18"));

    oled.setCursor(34, 3);
    oled.print(F("Goshante"));

    for (int i = 0; i < 21; i++) {

        oled.setCursor(i * 6, 6);
        oled.print('-');

        delay(70);
    }

    delay(200);
    oled.clear();
}

// off amplifier md8002a
static void safeAmpOff() {

    AMP_DDR |= (1 << AMP_BIT);   // OUTPUT
    AMP_PORT |= (1 << AMP_BIT);  // HIGH
}

// on amplifier md8002a
static void safeAmpOn() {
    AMP_PORT &= ~(1 << AMP_BIT); // LOW
}

static uint8_t volumeEvent(uint8_t event, uint8_t pin) {
    if (g_muteVolume) {
        if (!BUTTONEVENT_ISDONE(event)) {
            if ((BUTTONEVENT_SHORTPRESS != event) || (VOLUME_BUTTON == pin))
                doVolume(1);
        }
    }

    if (!g_muteVolume) {
#if (0 != VOLUME_DELAY)
#if (VOLUME_DELAY > 1)
        static uint8_t count;
        if (BUTTONEVENT_FIRSTLONGPRESS == event) {
            count = 0;
        }
#endif
        if (BUTTONEVENT_ISLONGPRESS(event))
            if (BUTTONEVENT_LONGPRESSDONE != event) {
#if (VOLUME_DELAY > 1)
                if (count++ == 0)
#endif
                    doVolume(VOLUME_BUTTON == pin ? 1 : -1);
#if (VOLUME_DELAY > 1)
                count = count % VOLUME_DELAY;
#endif
            }
#else
        if (BUTTONEVENT_FIRSTLONGPRESS == event)
            event = BUTTONEVENT_SHORTPRESS;
#endif
    }
    return event;
}

static uint8_t simpleEvent(uint8_t event, uint8_t pin) {
    // If the event is from the Mode button in FM mode, pass it through unmodified.
    if (pin == MODE_SWITCH && g_currentMode == FM) {
        return event;
    }

    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
    return event;
}

// This function handles the button events for band switching.
static uint8_t bandEvent(uint8_t event, uint8_t pin) {
#if (0 != BAND_DELAY)
    static uint8_t count;
    if (BUTTONEVENT_ISLONGPRESS(event) && !g_settingsActive) {
        if (BUTTONEVENT_LONGPRESSDONE != event) {
            if (BUTTONEVENT_FIRSTLONGPRESS == event) {
                count = 0;
            }
            // Process only for the designated band buttons
            if ((pin == BAND_BUTTON || pin == SOFTMUTE_BUTTON) && count++ == 0) {
                bandSwitch(pin == BAND_BUTTON);
            }
            count = count % BAND_DELAY;
        }
    }
#else
    // This logic handles the case where long press is disabled in defs.h
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
#endif
    return event;
}

// Handle encoder direction
static void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        g_encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
        g_seekStop = true;
    }
}

//Saves more flash image size
static void updateSSBCutoffFilter() {
    uint8_t idx = g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB];
    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter((idx == 0 || idx == 4 || idx == 5) ? 0 : 1);
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

// most state is already in the band list
// only need to sync the single live frequency variable
static void syncActiveStateToBand() {
    const Band& current_band = g_bandList[g_bandIndex];

    // update only band stored frequency if the current live frequency
    // is within the valid range for this band
    if (g_currentFrequency >= current_band.minimumFreq && g_currentFrequency <= current_band.maximumFreq) {
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    }
}

// functions will read step/bw directly from the band list
// only need to load the frequency into the single live variable
static void loadActiveStateFromBand() {
    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
}

// writes the state of a single band to a specific eeprom address
static void writeBandStateToEEPROM(uint16_t addr, const Band& band) {
    EEPROM.update(addr + 0, band.currentFreq >> 8);
    EEPROM.update(addr + 1, band.currentFreq & 0xFF);
    EEPROM.update(addr + 2, band.stepIdxAM);
    EEPROM.update(addr + 3, band.stepIdxSSB);
    EEPROM.update(addr + 4, band.stepIdxFM);
    EEPROM.update(addr + 5, band.bwIdxAM);
    EEPROM.update(addr + 6, band.bwIdxSSB);
    EEPROM.update(addr + 7, band.bwIdxFM);
}

// reads the state of a single band from a specific eeprom address
static void readBandStateFromEEPROM(uint16_t addr, Band& band) {
    band.currentFreq = (EEPROM.read(addr + 0) << 8) | EEPROM.read(addr + 1);
    band.stepIdxAM = EEPROM.read(addr + 2);
    band.stepIdxSSB = EEPROM.read(addr + 3);
    band.stepIdxFM = EEPROM.read(addr + 4);
    band.bwIdxAM = EEPROM.read(addr + 5);
    band.bwIdxSSB = EEPROM.read(addr + 6);
    band.bwIdxFM = EEPROM.read(addr + 7);
}

// handles saving all receiver state to EEPROM
static void saveAllReceiverInformation(bool full_save = true) {
    syncActiveStateToBand();

    if (!full_save && g_currentFrequency == g_lastSavedFrequency) {
        g_stateIsDirty = false;
        return;
    }

    // write version/ID to validate data on next boot, preventing corruption after updates
    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);
    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);

    uint16_t addr = EEPROM_DATA_START_ADDRESS;

    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0xFF);
    EEPROM.update(addr++, g_prevMode);

    const uint8_t band_state_size = 8;

    if (full_save) {
        // Writes all receiver data sequentially
        for (uint8_t i = 0; i <= g_lastBand; i++) {
            writeBandStateToEEPROM(addr + (i * band_state_size), g_bandList[i]);
        }

        addr += ((g_lastBand + 1) * band_state_size);

        for (uint8_t i = 0; i < SETTINGS_MAX; i++) {
            EEPROM.update(addr++, g_Settings[i].param);
        }

        for (uint8_t i = 0; i < MODE_SETTINGS_COUNT; i++) {
            for (uint8_t j = 0; j < MODE_CONTEXT_COUNT; j++) {
                EEPROM.update(addr++, g_modeSettings[i][j]);
            }
        }

        saveFMFav();
    }
    else {
        // Writes ONLY the current band's state to minimize EEPROM wear
        // Calculate the precise address for the current band's data slot and write to it
        uint16_t band_addr = addr + (g_bandIndex * band_state_size);
        writeBandStateToEEPROM(band_addr, g_bandList[g_bandIndex]);
    }

    g_lastSavedFrequency = g_currentFrequency;
}

// handles loading all receiver state from EEPROM
static void readAllReceiverInformation() {
    // if stored app id or version mismatch, the eeprom data format is considered incompatible
    // this triggers a reset to default settings to prevent data corruption
    if (EEPROM.read(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID || EEPROM.read(EEPROM_VERSION_ADDRESS) != APP_VERSION) {
        oled.clear();
        oled.setFont(DEFAULT_FONT);
        oled.setCursor(0, 2);
        oled.print(F("  EEPROM RESET"));
        delay(2000);
        // g_bandList is already initialized with defaults from globals.h
        // we just need to save these initial states to eeprom
        initializeDefaultModeSettings();
        g_totalFavorites = 0;
        saveAllReceiverInformation(true);

        // аfter resetting, we MUST NOT proceed to read from EEPROM
        // g_bandList in RAM already holds the correct default values
        loadActiveStateFromBand();
        applyBandConfiguration();
        return;
    }

    uint16_t addr = EEPROM_DATA_START_ADDRESS;
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    // ensure g_bandIndex is within valid bounds after reading from eeprom
    if (g_bandIndex > g_lastBand) g_bandIndex = 1; // default to MW

    g_currentMode = EEPROM.read(addr++);
    g_currentBFO = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
    g_prevMode = EEPROM.read(addr++);

    // read only the variable state fields for each band from eeprom
    const uint8_t band_state_size = 8;
    for (uint8_t i = 0; i <= g_lastBand; i++) {
        readBandStateFromEEPROM(addr + (i * band_state_size), g_bandList[i]);
    }

    addr += ((g_lastBand + 1) * band_state_size);

    for (uint8_t i = 0; i < SETTINGS_MAX; i++)
        g_Settings[i].param = EEPROM.read(addr++);

    if (g_Settings[SettingsIndex::CPUSpeed].param > 1)
        g_Settings[SettingsIndex::CPUSpeed].param = 0;
    
    for (uint8_t i = 0; i < MODE_SETTINGS_COUNT; i++) {
        for (uint8_t j = 0; j < MODE_CONTEXT_COUNT; j++) {
            g_modeSettings[i][j] = EEPROM.read(addr++);
        }
    }

    applyBrightness();
    loadActiveStateFromBand();
    g_previousFrequency = g_currentFrequency;
    if (isSSB()) {
        loadSSBPatch();
    }
    applyBandConfiguration();

    g_lastSavedFrequency = g_currentFrequency;
}

//For saving features
static void resetEepromDelay() {
    g_storeTime = millis();
    g_previousFrequency = 0;
}

// ====== FM save station logic begin ======

// Save favorites FM stations to EEPROM
static void saveFMFav() {
    EEPROM.update(EEPROM_FM_FAVORITES_COUNT, g_totalFavorites);

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < MAX_FM_FAVORITES; i++) {
        if (i < g_totalFavorites) {
            uint16_t freq = g_fmFavorites[i].frequency;
            EEPROM.update(addr++, freq >> 8);
            EEPROM.update(addr++, freq & 0xFF);
        }
        else {
            // Fill unused slots with value 0xFFFFFF
            EEPROM.update(addr++, 0xFF);
            EEPROM.update(addr++, 0xFF);
        }
    }
}

// Load favorites from EEPROM
static void loadFMFav() {
    g_totalFavorites = EEPROM.read(EEPROM_FM_FAVORITES_COUNT);

    // Protection against uninitialized EEPROM
    if (g_totalFavorites == 0xFF || g_totalFavorites > MAX_FM_FAVORITES) {
        g_totalFavorites = 0;
        saveFMFav();  // Initialize with empty list
        return;
    }

    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < g_totalFavorites; i++) {
        uint16_t freq = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
        // Validate FM frequency range (64.0-108.0 MHz)
        if (freq >= 6400 && freq <= 10800) {
            g_fmFavorites[i].frequency = freq;
        }
        else {
            // If data is corrupted, reset the list
            g_totalFavorites = 0;
            saveFMFav();
            return;
        }
    }
}

// Add current frequency to RAM and set dirty flag
static void addFav() {
    if (g_totalFavorites >= MAX_FM_FAVORITES) return;
    for (uint8_t i = 0; i < g_totalFavorites; i++)
        if (g_fmFavorites[i].frequency == g_currentFrequency) return;

    g_fmFavorites[g_totalFavorites++].frequency = g_currentFrequency;
    g_favoritesDirty = true;
}

// Delete selected favorite from RAM and set dirty flag
static void delFav() {
    if (!g_totalFavorites) return;

    for (uint8_t i = g_favoriteSelected; i < g_totalFavorites - 1; i++)
        g_fmFavorites[i] = g_fmFavorites[i + 1];

    g_totalFavorites--;
    if (g_totalFavorites && g_favoriteSelected >= g_totalFavorites)
        g_favoriteSelected = g_totalFavorites - 1;

    g_favoritesDirty = true;
}

// Display favorites menu
static void showFav() {
    oled.setCursor(0, 0);
    oled.invertOutput(true);
    oled.print(F("  FM FAVORITES  "));
    oled.invertOutput(false);

    if (!g_totalFavorites) {
        oled.setCursor(30, 3);
        oled.print(F("NO SAVED"));
        return;
    }

    uint8_t start = (g_favoriteSelected >> 1) << 1;
    uint8_t end = start + 2;
    if (end > g_totalFavorites) end = g_totalFavorites;

    for (uint8_t i = start; i < end; i++) {
        oled.setCursor(0, 2 + ((i - start) << 1));
        oled.print(i == g_favoriteSelected ? '>' : ' ');
        oled.print('0');
        oled.print(i + 1);
        oled.print(':');

        uint16_t f_copy = g_fmFavorites[i].frequency;

        uint8_t megahertz = sw_div(f_copy, 100);

        if (megahertz < 100) oled.print(' ');
        if (megahertz < 10) oled.print(' ');
        oled.print(megahertz);
        oled.print('.');

        uint8_t first_decimal = sw_div(f_copy, 10);

        oled.print(first_decimal);

        oled.print(F(" MHz  "));
    }

    if (end == start + 1) {
        oled.setCursor(0, 4);
        for (uint8_t j = 16; j; j--) oled.print(' ');
    }

    oled.setCursor(0, 6);
    oled.print(' ');
    oled.print(g_favoriteSelected + 1);
    oled.print('/');
    oled.print(g_totalFavorites);
    oled.print(F(" DEL:BW "));
}

// ====== End of FM station save logic ======

//Draw frequency.
//BFO and main frequency produce actual frequency that is displayed on LCD
//Too sensitive logic, do not change
static void showFrequency(bool cleanDisplay = false) {
    if (g_settingsActive)
        return;

    char freqDisplay[7];
    static uint8_t prevLen = 0;
    uint16_t khzBFO, tailBFO;
    bool ssbMode = isSSB();
    uint8_t off = (ssbMode ? -5 : 4) + 8;
    const char* unit = "kHz";

    // Get the type of the current band 1 time only
    BandType currentBandType = g_bandList[g_bandIndex].bandType;

    if (currentBandType == FM_BAND_TYPE) {
        convertToChar(freqDisplay, g_currentFrequency, 5, 3, '.', '/');
        unit = "MHz";
    }
    else {
        if (!ssbMode) {
            uint8_t dot_pos = 0;
            if (currentBandType == SW_BAND_TYPE && g_Settings[SettingsIndex::SWUnits].param == 1) {
                dot_pos = 2;
                unit = "MHz";
            }
            convertToChar(freqDisplay, g_currentFrequency, 5, dot_pos, '.', '/');
        }
        else {
            splitFreq(khzBFO, tailBFO);
            convertToChar(freqDisplay, khzBFO, ilen(khzBFO));
        }
    }

    uint8_t len = ssbMode ? ilen(khzBFO) : ilen(g_currentFrequency);

    if (cleanDisplay) {
        // oled.setcursor is not needed as oledprint handles cursor placement
        oledPrint("/////////", 0, 3, FONT14X24SEVENSEG);
    }
    else if (ssbMode && len > prevLen && len == 5) 
        oledPrint("   ", 102, 4, DEFAULT_FONT);
    
    oledPrint(freqDisplay, off, 3, FONT14X24SEVENSEG);

    if (ssbMode) {
        oled.print('.');
        if (tailBFO < 10) 
            oled.print('0');
        oled.print(tailBFO);

        if (len != prevLen && len < prevLen) 
            oledPrint("/");
    }

    if (g_Settings[SettingsIndex::UnitsSwitch].param == 1 && (!ssbMode || len < 5))
        oledPrint(unit, 102, 4, DEFAULT_FONT);

    prevLen = len;
}

//This function is called by station seek logic
static void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;

    if (g_currentMode == FM) {
        // Only round for display, don't tune during seek
        g_currentFrequency = (freq / 10) * 10;
    }

    // Don't update band list during seek to avoid conflicts
    showFrequency();
}

static bool checkStopSeeking() {
    return g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

// Handles the low-level interaction with the Si4735 chip to perform a seek
static inline uint16_t executeHardwareSeek() {
    g_si4735.setFrequency(g_currentFrequency);
    delay(30);

    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        g_si4735.setSeekAmLimits(150, 30000);
    }

    g_seekStop = false;
    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);

    delay(50);
    return g_si4735.getFrequency();
}

// Manages the seek process and updates the application state.
static void doSeek() {
    g_currentFrequency = executeHardwareSeek();

    if (g_bandList[g_bandIndex].bandType != FM_BAND_TYPE) {
        for (uint8_t i = 0; i < g_bandCount; i++) {
            if (g_bandList[i].bandType != FM_BAND_TYPE &&
                g_currentFrequency >= g_bandList[i].minimumFreq &&
                g_currentFrequency <= g_bandList[i].maximumFreq) {
                g_bandIndex = i;
                break;
            }
        }
        const Band& new_band = g_bandList[g_bandIndex];
        g_si4735.setSeekAmLimits(new_band.minimumFreq, new_band.maximumFreq);
    }
    else { // FM band
        uint16_t rounded = (g_currentFrequency / 10) * 10;
        if (rounded != g_currentFrequency) {
            g_currentFrequency = rounded;
            g_si4735.setFrequency(g_currentFrequency);
        }
    }

    syncActiveStateToBand();
    showStatus(true);
    resetEepromDelay();
}

//Update and draw main screen UI.
//basicUpdate - update minimum as possible
//cleanFreq   - force clean frequency line
void showStatus(bool cleanFreq) {
    showFrequency(cleanFreq);
    showModulation();
    showStep();
    showBandwidth();
    updateAndShowBattery(true);
    showVolume();
    showRSSI();
}

static void updateLowerDisplayLine() {
    oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
    showModulation();
    showStep();
    updateAndShowBattery(true);
}

// Converts setting parameter value to UI display string
// Handles different setting types (Num, ZeroAuto, Switch, SwitchAuto)
static void SettingParamToUI(char* buf, uint8_t idx) {
    int8_t param = g_Settings[idx].param;
    uint8_t textIdx;

    switch (g_Settings[idx].type) {
    case SettingType::Num:
        if (idx == SettingsIndex::Brightness) param += 1;
        convertToChar(buf, abs(param), 3);
        if (param < 0) buf[0] = '-';
        buf[3] = '\0';
        return;

    case SettingType::ZeroAuto:
        if (param == 0) {
            textIdx = 0; // "AUT"
        }
        else {
            convertToChar(buf, param, 3);
            buf[3] = '\0';
            return;
        }
        break;

    case SettingType::SwitchAuto:
        textIdx = param;
        break;

    case SettingType::Switch: {
        // Read conversion rules directly from the PROGMEM map
        uint8_t base = pgm_read_byte(&switch_setting_map[idx].baseIndex);
        bool inv = pgm_read_byte(&switch_setting_map[idx].inverted);

        if (inv) {
            textIdx = base - param;
        }
        else {
            textIdx = base + param;
        }
        break;
    }
    }

    strcpy_P(buf, paramTexts[textIdx]);
}

// If full false - update only value
static void DrawSetting(uint8_t idx, bool full) {
    if (!g_settingsActive)
        return;

    char buf[5];
    uint8_t place = idx - ((g_SettingsPage - 1) * 6);
    uint8_t yOffset = place > 2 ? (place - 3) * 2 : place * 2;
    uint8_t xOffset = place > 2 ? 60 : 0;
    if (full)
        oledPrint(g_Settings[idx].name, 5 + xOffset, 2 + yOffset, DEFAULT_FONT, idx == g_SettingSelected && !g_SettingEditing);
    SettingParamToUI(buf, idx);
    oledPrint(buf, 35 + xOffset, 2 + yOffset, DEFAULT_FONT, idx == g_SettingSelected && g_SettingEditing);
}

//Update and draw settings UI
static void showSettings() {
    for (uint8_t i = 0; i < 6 && i + ((g_SettingsPage - 1) * 6) < SettingsIndex::SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * 6), true);
}

static void showSettingsTitle() {
    oledPrint("   SETTINGS  ", 0, 0, DEFAULT_FONT, true);
    oled.invertOutput(true);
    oled.print(uint8_t(g_SettingsPage));
    oled.print("/");
    oled.print(uint8_t(g_SettingsMaxPages));
    oled.invertOutput(false);
}

static void switchSettingsPage() {
    g_SettingsPage++;
    g_SettingsPage = (g_SettingsPage > g_SettingsMaxPages) ? 1 : g_SettingsPage;
    g_SettingSelected = 6 * (g_SettingsPage - 1);
    g_SettingEditing = false;
    oled.clear();
    showSettingsTitle();
    showSettings();
}

// Helper to sync settings between the g_Settings buffer and g_modeSettings storage.
static inline void syncModeDependentSettings(bool load) {
    ModeContext modeCtx = getModeContext();
    if (load) {
        g_Settings[ATT].param = g_modeSettings[MODE_SETTING_AGC][modeCtx];
        g_Settings[SoftMute].param = g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx];
        g_Settings[AutoVolControl].param = g_modeSettings[MODE_SETTING_AVC][modeCtx];
    }
    else {
        g_modeSettings[MODE_SETTING_AGC][modeCtx] = g_Settings[ATT].param;
        g_modeSettings[MODE_SETTING_SOFT_MUTE][modeCtx] = g_Settings[SoftMute].param;
        g_modeSettings[MODE_SETTING_AVC][modeCtx] = g_Settings[AutoVolControl].param;
    }
}

//Switch between main screen and settings mode
static void switchSettings() {
    oled.clear();
    if (g_settingsActive) {
        // Entering settings menu
        syncModeDependentSettings(true);

        g_SettingsPage = 1;
        showSettingsTitle();
        g_SettingSelected = 0;
        g_SettingEditing = false;
        showSettings();
    }
    else {
        // Exiting settings menu
        syncModeDependentSettings(false);

        g_settingsDirty = true;

        // Commit all changes to EEPROM and return to the main screen
        saveAllReceiverInformation();
        showStatus();
    }
}

//Draw curremt modulation
static void showModulation() {
    // Draw the mode description (AM, LSB, etc.)
    // The text should only be inverted if the command is BAND AND we are on the FM band
    // For all other bands, the inversion is handled by showBandTag() for the bottom line
    oledPrint(g_bandModeDesc[g_currentMode], 0, 0, DEFAULT_FONT,
        g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);

    oled.print(' ');
    updateStereoIndicator();

    // Call showBandTag, which will draw the band name (or empty space for FM)
    // and handle its own inversion logic for non-FM bands
    showBandTag();
}

void updateStereoIndicator() {
    char c = (isSSB() && g_Settings[SettingsIndex::Sync].param == 1) ? 'S' :
        ((g_currentMode == FM && g_stereoStatus) ? '*' : ' ');

    oled.setCursor(24, 0);
    oled.print(c);
}

// gets the band name from the Band structure into a C-string
static void getBandName(char* buffer, uint8_t band_idx) {
    memcpy(buffer, g_bandList[band_idx].name, 4);
    buffer[4] = '\0'; // ensure null 
}

//Draw current band
static void showBandTag() {
    if (g_settingsActive) return;

    bool invert = (g_activeCommand == CMD_BAND && g_bandList[g_bandIndex].bandType != FM_BAND_TYPE);

    static char name_buffer[5];
    getBandName(name_buffer, g_bandIndex);

    oledPrint(name_buffer, 0, 6, DEFAULT_FONT, invert);
}

//Draw volume level
static void showVolume() {
    if (g_settingsActive)
        return;

    char buf[3];
    if (g_muteVolume == 0)
        convertToChar(buf, g_si4735.getCurrentVolume(), 2, 0, 0);
    else {
        buf[0] = ' ';
        buf[1] = 'M';
        buf[2] = 0;
    }

    oledPrint(buf, (128 - (8 * 2) + 2 - 6), 0, DEFAULT_FONT, g_activeCommand == CMD_VOLUME);
}

// RSSI drawings
static void showRSSI() {
    if (g_settingsActive || g_favoritesActive || g_currentMode != FM)
        return;

    oled.setCursor(84, 0);
    if (g_currentRSSI == 255)
        oled.print("--|");
    else {
        if (g_currentRSSI < 10) oled.print(' ');
        oled.print(g_currentRSSI);
        oled.print('|');
    }
}

// ====== Battery Monitoring Subsystem ========

uint8_t g_stableBatteryPercent = 100;
static uint8_t g_percentChangeCounter = 0;
static int16_t g_averageADC = -1;

// Calculates the "raw" battery percentage from an ADC value using interpolation.
// It uses the original lookup table for consistency.
static uint8_t calculateRawPercent(uint16_t adc_value) {
    constexpr const uint8_t rows = 10;
    // Original voltage table from Goshante's firmware.
    static const PROGMEM uint16_t voltages[rows] = { 643, 620, 604, 581, 573, 558, 542, 503, 496, 488 };
    static const PROGMEM uint8_t percents[rows] = { 100, 95, 90, 80, 60, 40, 20, 15, 5, 0 };

    if (adc_value >= pgm_read_word(&voltages[0])) return 100;
    if (adc_value <= pgm_read_word(&voltages[rows - 1])) return 0;

    for (uint8_t i = 0; i < rows - 1; ++i) {
        uint16_t v_upper = pgm_read_word(&voltages[i]);
        uint16_t v_lower = pgm_read_word(&voltages[i + 1]);
        if (adc_value >= v_lower && adc_value <= v_upper) {
            uint8_t p_upper = pgm_read_byte(&percents[i]);
            uint8_t p_lower = pgm_read_byte(&percents[i + 1]);
            return p_lower + ((uint32_t)(adc_value - v_lower) * (p_upper - p_lower)) / (v_upper - v_lower);
        }
    }
    return 0;
}

// Updates the internal stable battery percentage. Applies an IIR filter to ADC
// readings and uses hysteresis logic to prevent display flicker.
static void updateStablePercent() {
    if (!g_voltagePinConnnected) return;

    int sample = analogRead(BATTERY_VOLTAGE_PIN);
    if (g_averageADC == -1) {
        g_averageADC = sample > 0 ? sample : 550;
    }
    g_averageADC = (3 * g_averageADC + sample) >> 2;

    uint8_t currentRawPercent = calculateRawPercent(g_averageADC);

    // Hysteresis threshold to prevent flicker between adjacent percentage values.
    const uint8_t PERCENT_HYSTERESIS_THRESHOLD = 2;
    const uint8_t CONFIRMATION_COUNT = 5;

    if (abs(currentRawPercent - g_stableBatteryPercent) > PERCENT_HYSTERESIS_THRESHOLD) {
        g_stableBatteryPercent = currentRawPercent;
        g_percentChangeCounter = 0;
    }
    else {
        if (currentRawPercent != g_stableBatteryPercent) {
            if (++g_percentChangeCounter >= CONFIRMATION_COUNT) {
                g_stableBatteryPercent = currentRawPercent;
                g_percentChangeCounter = 0;
            }
        }
        else {
            g_percentChangeCounter = 0;
        }
    }
}

// Renders the stable battery percentage value on the display.
static void showChargeOnDisplay() {
    if (g_settingsActive) return;

    char buf[4];
    if (g_stableBatteryPercent >= 100) {
        buf[0] = '1'; buf[1] = '0'; buf[2] = '0'; buf[3] = '\0';
    }
    else {
        convertToChar(buf, g_stableBatteryPercent, 2);
        buf[2] = '%';
        buf[3] = '\0';
    }
    oledPrint(buf, 102, 6, DEFAULT_FONT);
}

//Draw battery charge
//This feature requires hardware mod
//Voltage divider made of two 10 KOhm resistors between + and GND of Li-Ion battery
//Solder it to A2 analog pin
//
// Public interface for the battery monitoring subsystem. Updates the internal
// state and shows it on the display if the timer has elapsed or if forced.
void updateAndShowBattery(bool forceShow) {
    if (!g_voltagePinConnnected) return;

    updateStablePercent();

    static uint32_t lastChargeShow = 0;

    if ((millis() - lastChargeShow) > 10000 || forceShow) {
        showChargeOnDisplay();
        lastChargeShow = millis();
    }
}

// ====== Battery Monitoring Subsystem END ========

// formats a raw step value into a human-readable string for the display
// conversions to "kHz", "Hz", and a special "1M" case for 1000 kHz
static void formatStepValue(char* buf, int stepValue, bool isKhz) {
    if (isKhz && stepValue == 1000) {
        strcpy_P(buf, PSTR("  1M"));
    }
    else if (isKhz) {
        convertToChar(buf, stepValue, 3);
        buf[3] = 'k';
        buf[4] = '\0';
    }
    else { // Hz for SSB
        if (stepValue >= 1000) {
            convertToChar(buf, stepValue / 1000, 3);
            buf[3] = 'k';
            buf[4] = '\0';
        }
        else {
            convertToChar(buf, stepValue, 4);
        }
    }
}

// displays the current tuning step based on the mode and the active band's state
static void showStep() {
    char buf[5];
    int stepValue;
    bool isKhz = true;

    const Band& current_band = g_bandList[g_bandIndex];

    if (g_currentMode == FM) {
        stepValue = (g_tabStepFM[current_band.stepIdxFM] == 100) ? 1000 : g_tabStepFM[current_band.stepIdxFM] * 10;
    }
    else if (isSSB()) {
        stepValue = g_tabStep[SSB_STEP_OFFSET + current_band.stepIdxSSB];
        isKhz = false;
    }
    else { // AM
        stepValue = g_tabStep[current_band.stepIdxAM];
    }

    formatStepValue(buf, stepValue, isKhz);
    uint8_t off = 50;
    oledPrint("St:", off - 16, 6, DEFAULT_FONT, g_activeCommand == CMD_STEP);
    oledPrint(buf, off + 8, 6, DEFAULT_FONT, g_activeCommand == CMD_STEP);
}

// displays the current bandwidth based on the mode and the active band's state
static void showBandwidth() {
    char bw[5];
    bw[4] = '\0';
    const Band& current_band = g_bandList[g_bandIndex];
    const uint8_t* table_ptr = nullptr;
    uint8_t index = 0;

    if (isSSB()) {
        if (g_currentMode == CW) {
            bw[0] = '\0'; // no label for CW
        }
        else {
            table_ptr = bw_ssb_map;
            index = current_band.bwIdxSSB;
        }
    }
    else if (g_currentMode == AM) {
        table_ptr = bw_am_map;
        index = current_band.bwIdxAM;
    }
    else { // FM
        table_ptr = bw_fm_map;
        index = current_band.bwIdxFM;
    }

    if (table_ptr) {
        // read base offset from the PROGMEM index table
        uint8_t offset = pgm_read_byte(&table_ptr[index]);
        // copy 4 characters from the main data string using the offset
        for (uint8_t i = 0; i < 4; i++) {
            bw[i] = pgm_read_byte(&bw_all_data[offset + i]);
        }
    }
    oledPrint(bw, 45, 0, DEFAULT_FONT, g_activeCommand == CMD_BW);
}

// switches band index and immediately applies the new band's default state
static void bandSwitch(bool up) {
    syncActiveStateToBand(); // Save current frequency to RAM
    markStateAsDirty();

    uint8_t oldBandIndex = g_bandIndex;
    g_currentBFO = 0;

    if (up) {
        g_bandIndex = (g_bandIndex + 1) % g_bandCount;
    }
    else {
        g_bandIndex = (g_bandIndex == 0) ? (g_bandCount - 1) : (g_bandIndex - 1);
    }

    loadActiveStateFromBand();

    g_lastSavedFrequency = g_currentFrequency;

    BandType oldType = g_bandList[oldBandIndex].bandType;
    BandType newType = g_bandList[g_bandIndex].bandType;

    if (oldType == newType && newType != FM_BAND_TYPE) {
        // fast for seamless transitions within AM/SW bands
        g_si4735.setFrequency(g_currentFrequency);
        showFrequency(false);
        showBandTag();
        showStep();
        showBandwidth();
    }
    else {
        // long for major mode changes (like to/from FM - in AM/LW/MW (SSB too)
        applyBandConfiguration();
    }
}

// This function is required for using SSB. Si473x controllers do not support SSB by-default.
// But we can patch internal RAM of Si473x with special patch to make it work in SSB mode.
// Patch must be applied every time we enable SSB after AM or FM.
static void loadSSBPatch() {
    safeAmpOff();

    g_si4735.setI2CFastModeCustom(500000);

    g_si4735.queryLibraryId();

    g_si4735.patchPowerUp();
    delay(50);
    g_si4735.downloadCompressedPatch(ssb_patch_content, sizeof(ssb_patch_content), cmd_0x15, sizeof(cmd_0x15));

    // use bw from the current band's state
    g_si4735.setSSBConfig(g_bwSSBIdx[g_bandList[g_bandIndex].bwIdxSSB], 1, 0, 1, 0, 1);
    g_si4735.setI2CStandardMode();

    g_ssbLoaded = true;

    // line that reset the step here with index has been removed
    // allows the step setting for SSB to persist for each band individually for now

    safeAmpOn();
}

// Set up FM radio parameters including frequency limits, bandwidth, and de-emphasis
static void configureFMMode() {
    g_currentMode = FM;
    g_stereoStatus = false;

    // Get all parameters from the current band's state
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setFM(
        current_band.minimumFreq,
        current_band.maximumFreq,
        current_band.currentFreq,
        g_tabStepFM[current_band.stepIdxFM]);

    g_si4735.setSeekFmLimits(
        current_band.minimumFreq,
        current_band.maximumFreq);

    g_si4735.setSeekFmSpacing(10);

    // Set custom seek thresholds to improve seek on weak stations
    g_si4735.setProperty(0x1403, 2);  // FM_SEEK_TUNE_SNR_THRESHOLD (Default: 3)
    g_si4735.setProperty(0x1404, 9);  // FM_SEEK_TUNE_RSSI_THRESHOLD (Default: 20)

    g_ssbLoaded = false;
    g_si4735.setFifoCount(1);
    g_si4735.setFmBandwidth(current_band.bwIdxFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[DeEmp].param == 0) ? 1 : 2);

    g_currentRSSI = 255;
}

// Initialize SSB mode with patch loading, BFO setup, filters, and audio bandwidth configuration
static void configureSSBMode(uint16_t minFreq, uint16_t maxFreq, bool extraSSBReset) {
    Band& current_band = g_bandList[g_bandIndex];

    if (current_band.bwIdxSSB >= g_bwSSBMaxIdx)
        current_band.bwIdxSSB = 4;

    g_currentBFO = 0;
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);

    g_si4735.setSSB(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? (g_Settings[CWSwitch].param + 1) : g_currentMode);

    updateSSBCutoffFilter();
    g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
    g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);

    // Use SoftMute setting from storage for SSB
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_SSB]);

    // Use bandwidth index from the current band state
    g_si4735.setSSBAudioBandwidth((g_currentMode == CW) ? g_bwSSBIdx[0] : g_bwSSBIdx[current_band.bwIdxSSB]);
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

// Switch to AM mode and configure bandwidth, soft mute, and frequency parameters
static void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;
    const Band& current_band = g_bandList[g_bandIndex];

    g_si4735.setAM(
        minFreq,
        maxFreq,
        current_band.currentFreq,
        g_tabStep[current_band.stepIdxAM]);

    // Use SoftMute setting from storage for AM
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_AM]);
    g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);
}

// Set AGC, AVC gain, and seek parameters shared between AM and SSB modes
static void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    // Apply AVC MAX GAIN from storage for the current mode
    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);

    // Set seek limits and spacing
    g_si4735.setSeekAmLimits(minFreq, maxFreq);
    // For seek spacing, always use the AM step, as SSB does not have hardware seek
    g_si4735.setSeekAmSpacing(g_tabStep[g_bandList[g_bandIndex].stepIdxAM]);
}

// AGC hardware control
static inline void setAgcHardware(int8_t att_val) {
    bool disableAgc = att_val > 0;
    // attenuation index for the chip is one less than the parameter valu
    // if att_val is 0 (auto) or 1 (manual, 0dB), the index sent to the chip is 0
    uint8_t agcNdx = (att_val > 1) ? (att_val - 1) : 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

// Applies AGC settings based on current mode and stored values
static void applyAgcSettings() {
    ModeContext modeCtx = getModeContext();
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];

    setAgcHardware(att_val);
}

// Main band switching logic that coordinates mode transitions and amplifier control
void applyBandConfiguration(bool extraSSBReset) {
    // if transitioning between FM and pure AM
    bool prevWasFM = (g_currentMode == FM);
    // use bandType field instead of comparing g_bandIndex to an enum value
    bool nextIsFM = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE);
    bool nextIsPureAM = !nextIsFM && !g_ssbLoaded;
    bool switchingBetweenFMandAM = (prevWasFM && nextIsPureAM) || (!prevWasFM && nextIsFM);

    // power down amplifier if switching modes
    if (switchingBetweenFMandAM)
        safeAmpOff();

    loadActiveStateFromBand();

    // Tune antenna capacitor
    uint8_t cap_value = (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);

    if (nextIsFM) {
        configureFMMode();
    }
    else {
        // AM frequency limits
        // now must ALWAYS use the specific limits from the g_bandList  entry
        uint16_t minFreq = g_bandList[g_bandIndex].minimumFreq;
        uint16_t maxFreq = g_bandList[g_bandIndex].maximumFreq;

        if (g_ssbLoaded) {
            configureSSBMode(minFreq, maxFreq, extraSSBReset);
        }
        else {
            configureAMMode(minFreq, maxFreq);
        }

        // Common AM/SSB post-processing
        configureAMCommon(minFreq, maxFreq);
    }

    applyAgcSettings();

    if (!g_settingsActive) {
        // Clear OLED buffer
        oled.clear();
        // then draw status and flush buffer
        showStatus(true);
    }

    resetEepromDelay();

    // Re-enable amplifier if modes toggled
    if (switchingBetweenFMandAM)
        safeAmpOn();
}

// handles tuning step adjustment, updates the current band's state, and applies it to the chip
static void doStep(int8_t v) {
    Band& current_band = g_bandList[g_bandIndex];

    if (g_currentMode == FM) {
        doSwitchLogic(current_band.stepIdxFM, 0, g_lastStepFM, v);
        g_si4735.setFrequencyStep(g_tabStepFM[current_band.stepIdxFM]);
        g_si4735.setSeekFmSpacing(10);
        showStep();
    }
    else if (isSSB()) {
        doSwitchLogic(current_band.stepIdxSSB, 0, SSB_STEPS_COUNT - 1, v);
        showStep();
    }
    else { // AM
        const uint8_t max_am_idx = (current_band.bandType == LW_BAND_TYPE || current_band.bandType == MW_BAND_TYPE) ? 3 : (AM_STEPS_COUNT - 1);
        doSwitchLogic(current_band.stepIdxAM, 0, max_am_idx, v);
        g_si4735.setFrequencyStep(g_tabStep[current_band.stepIdxAM]);
        g_si4735.setSeekAmSpacing(g_tabStep[current_band.stepIdxAM]);
        showStep();
    }
}

// Corrected CW BFO offset logic to match standard radio behavior
// The Si4735 IC requires an inverted BFO value, so the math is reversed here to compensate
// To get a positive BFO offset for USB, the value must be negative before the final inversion
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static void updateBFO() {

    int16_t finalBfo = g_currentBFO + (g_Settings[BFO].param * 100);

    if (g_currentMode == CW) {
        if (g_Settings[CWSwitch].param == 1) { // 1 = USB
            finalBfo -= CW_PITCH_OFFSET_HZ;
        }
        else { // 0 = LSB
            finalBfo += CW_PITCH_OFFSET_HZ;
        }
    }

    g_si4735.setSSBBfo(finalBfo * -1);
}

//Volume control
static void doVolume(int8_t v) {
    if (g_muteVolume) {
        g_si4735.setVolume(g_muteVolume);
        g_muteVolume = 0;
    }
    else {
        if (v == 1)
            g_si4735.volumeUp();
        else
            g_si4735.volumeDown();
    }
    showVolume();
}

//Helps to save more flash image size
static void doSwitchLogic(int8_t& param, int8_t low, int8_t high, int8_t step) {
    param += step;
    if (param < low)
        param = high;
    else if (param > high)
        param = low;
}

//Settings: Attenuation
void doAttenuation(int8_t v) {
    uint8_t max_att_value = (g_currentMode == FM) ? 26 : 37;
    doSwitchLogic(g_Settings[ATT].param, 0, max_att_value, v);

    setAgcHardware(g_Settings[ATT].param);
}

//Settings: Soft Mute
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SoftMute].param, 0, 32, v);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SoftMute].param);
}

// helper for brightness calculating the value by using integer (low flash consume)
// on edit have a white display, so don`t know what it will look like for you
static void applyBrightness() {
    uint8_t s = g_Settings[Brightness].param;

    // f(s) = 0.75*s^2 + 2.0*s. coefficients are scaled by 256 here
    uint8_t contrast_value = ((uint32_t)s * ((uint16_t)s * 192 + 512)) >> 8;
    // add the base value of 1 to map to the final contrast range [1, 80]
    oled.setContrast(contrast_value + 1);
}

//Settings: Brightness
void doBrightness(int8_t v) {
    int8_t new_setting = g_Settings[Brightness].param + v;

    // clamp the value of to the [0, 9]
    new_setting = constrain(new_setting, 0, 9);

    g_Settings[Brightness].param = new_setting;
    applyBrightness();
}

//Settings: SSB AVC Switch
void doSSBAVC(int8_t v = 0) {
    doSwitchLogic(g_Settings[SVC].param, 0, 1, v);

    if (isSSB()) {
        g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);
        applyBandConfiguration(true);
    }
}

//Settings: Automatic Volume Control
void doAvc(int8_t v) {
    doSwitchLogic(g_Settings[AutoVolControl].param, 12, 90, v);

    if (g_currentMode != FM)
        g_si4735.setAvcAmMaxGain(g_Settings[AutoVolControl].param);
}

//Settings: Sync switch
void doSync(int8_t v = 0) {
    bool wasSettingsActive = g_settingsActive;
    doSwitchLogic(g_Settings[Sync].param, 0, 1, v);

    if (isSSB()) {
        g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);  //Set Sync mode
        applyBandConfiguration(true);

        if (wasSettingsActive) {
            showSettingsTitle();
            showSettings();
        }
    }
}

//Settings: FM DeEmp switch (50 or 75)
void doDeEmp(int8_t v = 0) {
    doSwitchLogic(g_Settings[DeEmp].param, 0, 1, v);

    if (g_currentMode == FM)
        g_si4735.setFMDeEmphasis(g_Settings[DeEmp].param == 0 ? 1 : 2);
}

//Settings: SW Units
void doSWUnits(int8_t v = 0) {
    doSwitchLogic(g_Settings[SWUnits].param, 0, 1, v);
}

//Settings: SW Units
void doSSBSoftMuteMode(int8_t v = 0) {
    doSwitchLogic(g_Settings[SSM].param, 0, 1, v);

    if (isSSB())
        g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

//Settings: SSB Cutoff filter
void doCutoffFilter(int8_t v) {
    doSwitchLogic(g_Settings[CutoffFilter].param, 0, 2, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v = 0) {
    doSwitchLogic(g_Settings[CPUSpeed].param, 0, 1, v);

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[CPUSpeed].param;
    interrupts();
}

// Settings: BFO Offset calibration
void doBFOCalibration(int8_t v) {
    // Expanded range to -25..+25. With a x100 multiplier in updateBFO(),
    // this provides a +/- 2.5kHz calibration range in 100Hz step
    doSwitchLogic(g_Settings[BFO].param, -25, 25, v);

    if (isSSB()) {
        updateBFO();
    }
}

//Settings: Tune Frequency Antenna Capacitor
void doUnitsSwitch(int8_t v) {
    doSwitchLogic(g_Settings[UnitsSwitch].param, 0, 1, v);
}

//Settings: Scan button switch
void doScanSwitch(int8_t v = 0) {
    doSwitchLogic(g_Settings[ScanSwitch].param, 0, 1, v);
}

//Settings: CW mode switch
void doCWSwitch(int8_t v = 0) {
    syncActiveStateToBand();
    doSwitchLogic(g_Settings[CWSwitch].param, 0, 1, v);

    if (g_currentMode == CW)
        applyBandConfiguration(false);
}

//Settings: Auto Antenna Capacitor
void doAntennaCapacitor(int8_t v) {
    doSwitchLogic(g_Settings[AntennaCap].param, 0, 1, v);
}

// handles bandwidth adjustment and updates the current band's state
static void doBandwidth(uint8_t v) {
    Band& current_band = g_bandList[g_bandIndex];

    if (isSSB()) {
        doSwitchLogic(current_band.bwIdxSSB, 0, sizeof(bw_ssb_map) - 1, v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[current_band.bwIdxSSB]);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM) {
        doSwitchLogic(current_band.bwIdxAM, 0, sizeof(bw_am_map) - 1, v);
        g_si4735.setBandwidth(g_bwAMIdx[current_band.bwIdxAM], 1);
    }
    else { // FM
        doSwitchLogic(current_band.bwIdxFM, 0, sizeof(bw_fm_map) - 1, -v);
        g_si4735.setFmBandwidth(current_band.bwIdxFM);
    }
    showBandwidth();
}

// switch command modes
static void switchCommand(CommandMode mode) {
    if (g_activeCommand != mode) { // Switching to a new mode or activating one
        g_activeCommand = mode;
        g_lastAdjustmentTime = millis();
    }
    else { // Pressing the same button again to deactivate
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
    }

    // Refresh all indicators to show the new state
    showVolume();
    showStep();
    showBandwidth();
    showModulation();
}

//  helper to reset any active command mode
static void resetCommandMode() {
    if (g_activeCommand != CMD_NONE) {
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
        showVolume();
        showStep();
        showBandwidth();
        showModulation();
    }
}

// register user activity and mark the state as dirty before save in EEPROM
static inline void markStateAsDirty() {
    g_lastUserActivityTime = millis();
    g_stateIsDirty = true;
}

// handles frequency tuning for am/fm
static void doFrequencyTune() {
    g_seekDirection = g_encoderCount > 0;
    Band& current_band = g_bandList[g_bandIndex];
    uint16_t step;

    if (current_band.bandType == FM_BAND_TYPE) {
        step = g_tabStepFM[current_band.stepIdxFM];
    }
    else { // am
        step = g_tabStep[current_band.stepIdxAM];
    }

    // 32 integer need here for calculations to prevent underflow on band edges!
    int32_t temp_freq = g_currentFrequency;
    temp_freq += (int16_t)step * g_encoderCount;
    g_encoderCount = 0;

    // calculate then check
    if (temp_freq > current_band.maximumFreq) {
        bandSwitch(true);
        return;
    }
    if (temp_freq < current_band.minimumFreq) {
        bandSwitch(false);
        return;
    }

    // if safe, commit the new frequency and align it to the grid (snap)
    g_currentFrequency = (uint16_t)temp_freq;
    uint16_t remainder = g_currentFrequency % step;
    if (remainder != 0) {
        if (g_seekDirection) {
            g_currentFrequency += (step - remainder);
        }
        else {
            g_currentFrequency -= remainder;
        }
    }

    g_processFreqChange = true;
    g_lastFreqChange = millis();
    showFrequency();
    syncActiveStateToBand();
    markStateAsDirty();
}

// performs bfo rollover with integrated boundary checks and a max bfo limit
// this is the core of the stability system for ssb tuning
// returns true if a band switch occurred, false otherwise
// see: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
static inline bool performBfoRolloverWithBandCheck(uint16_t* freq, int32_t* bfo) {
    const int32_t BFOMax = 13000;
    Band& current_band = g_bandList[g_bandIndex];

    // clamp bfo to the maximum allowed range first for stability
    if (*bfo > BFOMax) *bfo = BFOMax;
    if (*bfo < -BFOMax) *bfo = -BFOMax;

    // then perform the reliable rollover with integrated "emergency brake" checks
    while (*bfo >= 1000) {
        (*freq)++;
        if (*freq >= current_band.maximumFreq) {
            bandSwitch(true);
            return true;
        }
        *bfo -= 1000;
    }

    while (*bfo <= -1000) {
        (*freq)--;
        if (*freq < current_band.minimumFreq) {
            bandSwitch(false);
            return true;
        }
        *bfo += 1000;
    }

    return false;
}

// handles ssb tuning using the definitive "atomic step with integrated checks" architecture
static void doFrequencyTuneSSB() {
    if (g_encoderCount == 0) return;

    // calculate the potential new bfo in a temporary variable
    int32_t temp_bfo = g_currentBFO;
    uint16_t temp_freq = g_currentFrequency;

    temp_bfo += (int32_t)g_tabStep[SSB_STEP_OFFSET + g_bandList[g_bandIndex].stepIdxSSB] * g_encoderCount;
    g_encoderCount = 0;

    // perform rollover and boundary checks; exit if a band switch happened
    if (performBfoRolloverWithBandCheck(&temp_freq, &temp_bfo)) return;

    // if we survived all checks, commit the new state
    g_currentFrequency = temp_freq;
    g_currentBFO = temp_bfo;

    updateBFO();
    syncActiveStateToBand();
    g_lastFreqChange = millis();
    g_previousFrequency = 0;
    showFrequency();
    markStateAsDirty();
}

// helper functions for processButtonEvents
static inline void handleEncoderButton() {
    uint8_t evt = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    // determine the current ui context to use in a switch-case
    uint8_t context = 0;
    if (g_activeCommand != CMD_NONE)      context = 1;
    else if (g_settingsActive)            context = 2;
    else if (g_favoritesActive)           context = 3;
    else if (isSSB() || !g_Settings[ScanSwitch].param) context = 4;
    else                                  context = 5;

    switch (context) {
    case 1:
        resetCommandMode();
        break;
    case 2:
        g_SettingEditing = !g_SettingEditing;
        DrawSetting(g_SettingSelected, true);
        break;
    case 3:
        if (g_totalFavorites) {
            g_currentFrequency = g_fmFavorites[g_favoriteSelected].frequency;
            g_si4735.setFrequency(g_currentFrequency);
        }
        exitFavoritesMenu();
        break;
    case 4:
        switchCommand(CMD_STEP);
        break;
    case 5:
        doSeek();
        break;
    }
}

static inline void handleBandwidthButton() {
    // bandwidth (bw) button handler
    uint8_t evt = btn_Bandwidth.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_favoritesActive) {
        // in favorites menu, this button deletes the selected favorite
        delFav();
        oled.clear();
        showFav();
    }
    else if (!g_settingsActive && g_currentMode != CW) {
        // in main screen, it enters bandwidth adjustment mode (not available for cw)
        switchCommand(CMD_BW);
    }
}

static inline void handleBandUpButton() {
    // band up button handler
    uint8_t evt = btn_BandUp.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_favoritesActive) {
        // any navigation button exits the favorites menu
        exitFavoritesMenu();
    }
    else if (g_settingsActive) {
        // in settings menu, it switches to the next page
        switchSettingsPage();
    }
    else {
        // in main screen, it enters band selection mode
        switchCommand(CMD_BAND);
    }
}

static inline void handleBandDownButton() {
    // band down button (settings) handler
    uint8_t evt = btn_BandDn.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_favoritesActive) {
        // any navigation button exits the favorites menu
        exitFavoritesMenu();
    }
    else {
        // this button's primary role is to toggle the main settings menu
        resetCommandMode(); // ensure no command is active when entering settings
        g_settingsActive = !g_settingsActive;
        switchSettings();
    }
}

static inline void handleVolumeUpButton() {
    // volume up button handler
    uint8_t evt = btn_VolumeUp.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive && !g_muteVolume) {
        switchCommand(CMD_VOLUME);
    }
}

static inline void handleVolumeDownButton() {
    // volume down button (mute) handler
    uint8_t evt = btn_VolumeDn.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_favoritesActive && g_activeCommand != CMD_VOLUME) {
        // toggles mute, saves the current volume to restore it later
        uint8_t vol = g_si4735.getCurrentVolume();
        if (vol && !g_muteVolume) {
            g_muteVolume = vol;
            g_si4735.setVolume(0);
        }
        else if (g_muteVolume) {
            g_si4735.setVolume(g_muteVolume);
            g_muteVolume = 0;
        }
        showVolume();
    }
}

// AGC button press for display and CPU speed toggle
static inline void handleAgcButton() {
    uint8_t evt = btn_AGC.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    // not allow toggling OFF while in settings menu
    if (g_settingsActive && g_displayOn) return;

    g_displayOn = !g_displayOn;
    uint8_t new_prescaler = !g_displayOn + (g_displayOn * g_Settings[SettingsIndex::CPUSpeed].param);

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = new_prescaler;
    interrupts();

    g_displayOn ? oled.on() : oled.off();
}

static inline void handleStepButton() {
    // step button handler
    uint8_t evt = btn_Step.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive) {
        switchCommand(CMD_STEP);
    }
}

// handles the complex logic of cycling through AM, LSB, USB, and CW modes
static inline void cycleAmSsbCwModes() {
    Band& current_band = g_bandList[g_bandIndex];
    // store the bandwidth index to carry it over between am/ssb
    int8_t bw = (g_currentMode == AM) ? current_band.bwIdxAM : current_band.bwIdxSSB;
    syncActiveStateToBand();

    //saveAllReceiverInformation(false); // Save the state of the departing mode to EEPROM
    markStateAsDirty();

    if (g_currentMode == CW) safeAmpOff();

    // when switching from am to ssb for the first time, the patch must be loaded
    if (g_currentMode == AM) {
        loadSSBPatch();
        current_band.bwIdxSSB = bw;
        g_processFreqChange = false;
    }

    // cycle through am -> lsb -> usb -> cw -> am
    g_currentMode = (g_currentMode + 1) % 4;

    if (g_currentMode == AM) {
        g_ssbLoaded = false;
        current_band.bwIdxAM = bw;
    }

    applyBandConfiguration();

    if (!g_ssbLoaded && g_currentMode == AM) safeAmpOn();
}

static inline void processModeButtonShortPress() {
    if (g_favoritesActive) {
        // if in favorites menu, exit it.
        g_favoritesActive = false;
        oled.clear();
        showStatus();
        return;
    }

    if (g_bandList[g_bandIndex].bandType == FM_BAND_TYPE) {
        // if on FM band, enter favorites menu.
        g_favoritesActive = true;
        g_favoriteSelected = 0;
        oled.clear();
        showFav();
        return;
    }

    // if not in favorites and not on FM, cycle modulation
    cycleAmSsbCwModes();
}

// handles mode button presses, dispatching tasks based on the current context
static inline void handleModeButton() {
    uint8_t evt = btn_Mode.checkEvent(simpleEvent);

    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive) {
        processModeButtonShortPress();
    }
    else if (BUTTONEVENT_LONGPRESSDONE == evt && !g_settingsActive &&
        g_bandList[g_bandIndex].bandType == FM_BAND_TYPE && !g_favoritesActive) {
        // long press on FM adds the current station to favorites
        addFav();
        oled.setCursor(45, 3);
        oled.print(F("SAVED"));
        delay(500);
        showFrequency(true);
    }
}

// exit the favorites menu and return to the main status screen
static inline void exitFavoritesMenu() {
    if (g_favoritesDirty) {
        saveFMFav();
        g_favoritesDirty = false;
    }

    g_favoritesActive = false;
    oled.clear();
    showStatus();
}

// key process for all keys
static void processButtonEvents() {
    handleEncoderButton();
    handleBandwidthButton();
    handleBandUpButton();
    handleBandDownButton();
    handleVolumeUpButton();
    handleVolumeDownButton();
    handleAgcButton();
    handleStepButton();
    handleModeButton();
}

// Safely reads the accumulated encoder value from the interrupt context.
static void updateEncoderState() {
    if (g_encoderCount) {
        noInterrupts();
        g_safeEncoderMovement += g_encoderCount;
        g_encoderCount = 0;
        interrupts();
    }
}

// Handles all user input and display updates when in the Favorites menu.
static void handleFavoritesMenu() {
    if (g_safeEncoderMovement) {
        if (g_totalFavorites > 0) {
            g_favoriteSelected = (g_favoriteSelected + g_safeEncoderMovement + g_totalFavorites) % g_totalFavorites;
            showFav();
        }
        g_safeEncoderMovement = 0;
    }
    processButtonEvents();
}

// handles encoder movement within the settings menu
static inline void processEncoderForSettings(int encoder_delta) {
    if (!g_SettingEditing) {
        int8_t prev = g_SettingSelected;
        g_SettingSelected += encoder_delta;
        uint8_t page = g_SettingsPage - 1;
        uint8_t max = min((page * 6) + 5, SettingsIndex::SETTINGS_MAX - 1);
        if (g_SettingSelected < page * 6) g_SettingSelected = max;
        else if (g_SettingSelected > max) g_SettingSelected = page * 6;
        DrawSetting(prev, true);
        DrawSetting(g_SettingSelected, true);
    }
    else {
        (*g_Settings[g_SettingSelected].manipulateCallback)(encoder_delta);
        DrawSetting(g_SettingSelected, false);
        delay(MIN_ELAPSED_TIME);
    }
}

// handles encoder movement for main screen commands
static inline bool processEncoderForCommands(int encoder_delta) {
    switch (g_activeCommand) {
    case CMD_VOLUME:
        doVolume(encoder_delta);
        break;
    case CMD_STEP:
        doStep(encoder_delta);
        break;
    case CMD_BW:
        doBandwidth(encoder_delta);
        break;
    case CMD_BAND:
        bandSwitch(encoder_delta > 0);
        g_safeEncoderMovement = 0;
        g_encoderCount = 0;
        return true;
    case CMD_NONE:
        g_encoderCount = encoder_delta;
        if (isSSB()) {
            doFrequencyTuneSSB();
        }
        else {
            doFrequencyTune();
        }
        g_safeEncoderMovement = 0;
        g_encoderCount = 0;
        return true;
    }
    return false;
}

// Handles encoder actions by dispatching to the appropriate handler
static bool processEncoderActions() {
    if (g_activeCommand != CMD_NONE) {
        g_lastAdjustmentTime = millis();
    }

    bool was_tuning_event = false;

    if (g_settingsActive) {
        processEncoderForSettings(g_safeEncoderMovement);
    }
    else {
        was_tuning_event = processEncoderForCommands(g_safeEncoderMovement);
    }

    g_safeEncoderMovement = 0;
    g_encoderCount = 0;
    resetEepromDelay();
    return was_tuning_event;
}

// Handles the delayed frequency update for AM/FM to prevent flooding the chip
static void handleDelayedFrequencyUpdate() {
    if (!g_processFreqChange || isSSB()) {
        return;
    }

    // if there new encoder movement, process it immediately to stay responsive
    if (g_safeEncoderMovement) {
        g_encoderCount = g_safeEncoderMovement;
        g_safeEncoderMovement = 0;
        doFrequencyTune();
        return;
    }

    // if there no new movement and enough time has passed, send the last frequency to the chip
    bool recent = millis() - g_lastFreqChange < 70;
    if (!recent) {
        g_si4735.setFrequency(g_currentFrequency);
        g_processFreqChange = false;
    }
}

// Runs all periodic, time-based tasks like RSSI updates and EEPROM saves.
static void handlePeriodicTasks() {
    if (millis() - g_lastFreqChange >= 500) {
        if (!g_settingsActive && millis() - g_lastRSSIUpdate >= 1000) {
            g_lastRSSIUpdate = millis();
            if (g_currentMode == FM) {
                g_si4735.getCurrentReceivedSignalQuality();
                uint8_t rssi = g_si4735.getCurrentRSSI();
                if (g_currentRSSI != rssi || g_currentRSSI == 255) {
                    g_currentRSSI = rssi;
                    showRSSI();
                }
                if (millis() > 3000) {
                    bool stereo = g_si4735.getCurrentPilot();
                    if (g_stereoStatus != stereo) {
                        g_stereoStatus = stereo;
                        updateStereoIndicator();
                    }
                }
            }
        }
    }

    if (g_lastAdjustmentTime && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT) {
        resetCommandMode();
    }

    updateAndShowBattery(false);

    // Save if global settings were changed (e.g., in the settings menu)
    if (g_settingsDirty) {
        saveAllReceiverInformation(true);
        g_settingsDirty = false;
    }
}

// main loop program in process order
void loop() {
    updateEncoderState();

    if (g_favoritesActive) {
        handleFavoritesMenu();
        return;
    }

    handleDelayedFrequencyUpdate();

    bool frequencyTuned = false;
    if (g_safeEncoderMovement) {
        frequencyTuned = processEncoderActions();
    }

    // process buttons only if the encoder was not used for a major tuning event
    if (!frequencyTuned) {
        processButtonEvents();
    }

    if (g_lastAdjustmentTime && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT) {
        resetCommandMode();
    }

    handlePeriodicTasks();

    // Check if we need to save the state due to user inactivity (idle)
    if (g_stateIsDirty && (millis() - g_lastUserActivityTime > SAVE_ON_IDLE_TIMEOUT)) {
        saveAllReceiverInformation(false);
        g_stateIsDirty = false;
    }
}

//Overriding original main to save some space
int main(void) {
    init();
    setup();
    while (1)
        loop();
    return 0;
}