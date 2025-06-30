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
// MOD_NO_RDS_v3.4 by diqezit
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

void showSplashScreen();
void showStatus(bool cleanFreq = false);
void applyBandConfiguration(bool extraSSBReset = false);
void updateStereoIndicator();

bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// --------------------------
// ------- Main logic -------
// --------------------------

#define APP_VERSION 34

// Helper function to get the current context (AM or SSB/CW)
ModeContext getModeContext() {
    if (isSSB() || g_currentMode == CW) {
        return MODE_CONTEXT_SSB;
    }
    return MODE_CONTEXT_AM;
}

// Initializes mode-dependent settings to their default values
void initializeDefaultModeSettings() {
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
        showSplashScreen();
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
void safeAmpOff() {

    AMP_DDR |= (1 << AMP_BIT);   // OUTPUT
    AMP_PORT |= (1 << AMP_BIT);  // HIGH
}

// on amplifier md8002a
void safeAmpOn() {
    AMP_PORT &= ~(1 << AMP_BIT); // LOW
}

uint8_t volumeEvent(uint8_t event, uint8_t pin) {
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

uint8_t simpleEvent(uint8_t event, uint8_t pin) {
    // If the event is from the Mode button in FM mode, pass it through unmodified.
    if (pin == MODE_SWITCH && g_currentMode == FM) {
        return event;
    }

    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
    return event;
}

// This function handles the button events for band switching.
uint8_t bandEvent(uint8_t event, uint8_t pin) {
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
void rotaryEncoder() {
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus) {
        g_encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
        g_seekStop = true;
    }
}

//Saves more flash image size
void updateSSBCutoffFilter() {
    uint8_t idx = g_bwSSBIdx[g_bwIndexSSB];
    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter((idx == 0 || idx == 4 || idx == 5) ? 0 : 1);
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

void syncActiveStateToBand() {
    Band& current_band = g_bandList[g_bandIndex];
    current_band.currentFreq = g_currentFrequency;
    current_band.stepIdxAM = g_stepIndexAM;
    current_band.stepIdxSSB = g_stepIndexSSB;
    current_band.stepIdxFM = g_FMStepIndex;
    current_band.bwIdxAM = g_bwIndexAM;
    current_band.bwIdxSSB = g_bwIndexSSB;
    current_band.bwIdxFM = g_bwIndexFM;
}

void loadActiveStateFromBand() {
    const Band& current_band = g_bandList[g_bandIndex];
    g_currentFrequency = current_band.currentFreq;
    g_stepIndexAM = current_band.stepIdxAM;
    g_stepIndexSSB = current_band.stepIdxSSB;
    g_FMStepIndex = current_band.stepIdxFM;
    g_bwIndexAM = current_band.bwIdxAM;
    g_bwIndexSSB = current_band.bwIdxSSB;
    g_bwIndexFM = current_band.bwIdxFM;
}

// handles saving all receiver state to EEPROM
void saveAllReceiverInformation(bool full_save = true) {
    syncActiveStateToBand();

    uint16_t addr = EEPROM_DATA_START_ADDRESS;
    // save version first to validate data structure on next boot
    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);
    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);

    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0xFF);
    EEPROM.update(addr++, g_prevMode);

    uint8_t start_band = full_save ? 0 : g_bandIndex;
    uint8_t end_band = full_save ? g_lastBand : g_bandIndex;
    for (uint8_t i = start_band; i <= end_band; i++) {
        EEPROM.put(addr + (i * sizeof(Band)), g_bandList[i]);
    }

    addr += ((g_lastBand + 1) * sizeof(Band));

    for (uint8_t i = 0; i < SETTINGS_MAX; i++)
        EEPROM.update(addr++, g_Settings[i].param);

    for (uint8_t i = 0; i < MODE_SETTINGS_COUNT; i++) {
        for (uint8_t j = 0; j < MODE_CONTEXT_COUNT; j++) {
            EEPROM.update(addr++, g_modeSettings[i][j]);
        }
    }

    saveFMFav();
}

// handles loading all receiver state from EEPROM
void readAllReceiverInformation() {
    // if stored app id or version mismatch, the eeprom data format is considered incompatible
    // this triggers a reset to default settings to prevent data corruption
    if (EEPROM.read(EEPROM_APP_ID_ADDRESS) != EEPROM_APP_ID || EEPROM.read(EEPROM_VERSION_ADDRESS) != APP_VERSION) {
        oled.clear();
        oled.setFont(DEFAULT_FONT);
        oled.setCursor(0, 2);
        oled.print(F("  EEPROM RESET"));
        delay(2000);
        initializeDefaultModeSettings();
        g_totalFavorites = 0;
        saveAllReceiverInformation(true);
        return;
    }

    uint16_t addr = EEPROM_DATA_START_ADDRESS;
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    g_currentMode = EEPROM.read(addr++);
    g_currentBFO = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
    g_prevMode = EEPROM.read(addr++);

    for (uint8_t i = 0; i <= g_lastBand; i++) {
        EEPROM.get(addr + (i * sizeof(Band)), g_bandList[i]);
    }

    addr += ((g_lastBand + 1) * sizeof(Band));

    for (uint8_t i = 0; i < SETTINGS_MAX; i++)
        g_Settings[i].param = EEPROM.read(addr++);

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
}

//For saving features
void resetEepromDelay() {
    g_storeTime = millis();
    g_previousFrequency = 0;
}

// ====== FM save station logic begin ======

// Save favorites FM stations to EEPROM
void saveFMFav() {
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
void loadFMFav() {
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
void addFav() {
    if (g_totalFavorites >= MAX_FM_FAVORITES) return;
    for (uint8_t i = 0; i < g_totalFavorites; i++)
        if (g_fmFavorites[i].frequency == g_currentFrequency) return;

    g_fmFavorites[g_totalFavorites++].frequency = g_currentFrequency;
    saveFMFav();  // Save immediately
}

// Delete selected favorite from RAM and set dirty flag
void delFav() {
    if (!g_totalFavorites) return;

    for (uint8_t i = g_favoriteSelected; i < g_totalFavorites - 1; i++)
        g_fmFavorites[i] = g_fmFavorites[i + 1];

    g_totalFavorites--;
    if (g_totalFavorites && g_favoriteSelected >= g_totalFavorites)
        g_favoriteSelected = g_totalFavorites - 1;

    saveFMFav();  // Save immediately
}

// Display favorites menu
void showFav() {
    oled.setCursor(0, 0);
    oled.invertOutput(true);
    oled.print(F("  FM FAVORITES  "));
    oled.invertOutput(false);

    if (!g_totalFavorites) {
        oled.setCursor(30, 3);
        oled.print(F("NO SAVED"));

        return;
    }

    uint8_t start = (g_favoriteSelected >> 1) << 1;  // Faster than / 2 * 2
    uint8_t end = start + 2;
    if (end > g_totalFavorites) end = g_totalFavorites;

    for (uint8_t i = start; i < end; i++) {
        oled.setCursor(0, 2 + ((i - start) << 1));
        oled.print(i == g_favoriteSelected ? '>' : ' ');
        oled.print('0');
        oled.print(i + 1);
        oled.print(':');

        uint16_t f = g_fmFavorites[i].frequency;
        uint8_t m = f / 100;

        if (m < 100) oled.print(' ');
        if (m < 10) oled.print(' ');
        oled.print(m);
        oled.print('.');
        oled.print((f % 100) / 10);
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
void showFrequency(bool cleanDisplay = false) {
    if (g_settingsActive)
        return;

    char freqDisplay[7];
    static uint8_t prevLen = 0;
    uint16_t khzBFO, tailBFO;
    bool ssbMode = isSSB();
    uint8_t off = (ssbMode ? -5 : 4) + 8;
    const char* unit = "kHz";

    if (g_bandIndex == FM_BAND_TYPE) {
        convertToChar(freqDisplay, g_currentFrequency, 5, 3, '.', '/');
        unit = "MHz";
    }
    else {
        if (g_bandIndex == SW_BAND_TYPE)
            showBandTag();

        if (!ssbMode) {
            bool swMhz = g_Settings[SettingsIndex::SWUnits].param == 1;
            convertToChar(freqDisplay, g_currentFrequency, 5, (g_bandIndex == SW_BAND_TYPE && swMhz) ? 2 : 0, '.', '/');
            if (g_bandIndex == SW_BAND_TYPE && swMhz)
                unit = "MHz";
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
void showFrequencySeek(uint16_t freq) {
    g_currentFrequency = freq;

    if (g_currentMode == FM) {
        // Only round for display, don't tune during seek
        g_currentFrequency = (freq / 10) * 10;
    }

    // Don't update band list during seek to avoid conflicts
    showFrequency();
}

bool checkStopSeeking() {
    return g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

void doSeek() {
    // Need these calls when library is not modified
    if (g_seekDirection)
        g_si4735.frequencyUp();
    else
        g_si4735.frequencyDown();

    g_seekStop = false;
    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);

    // Proper sync after seek completion
    delay(50);
    g_currentFrequency = g_si4735.getFrequency();

    // For FM, ensure frequency is on 100 kHz boundary
    if (g_currentMode == FM) {
        uint16_t rounded = (g_currentFrequency / 10) * 10;
        if (rounded != g_currentFrequency) {
            g_currentFrequency = rounded;
            g_si4735.setFrequency(g_currentFrequency);
        }
    }

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    showFrequency();
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

void updateLowerDisplayLine() {
    oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
    showModulation();
    showStep();
    updateAndShowBattery(true);
}

// Converts setting parameter value to UI display string
// Handles different setting types (Num, ZeroAuto, Switch, SwitchAuto)
void SettingParamToUI(char* buf, uint8_t idx) {
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
void DrawSetting(uint8_t idx, bool full) {
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
void showSettings() {
    for (uint8_t i = 0; i < 6 && i + ((g_SettingsPage - 1) * 6) < SettingsIndex::SETTINGS_MAX; i++)
        DrawSetting(i + ((g_SettingsPage - 1) * 6), true);
}

void showSettingsTitle() {
    oledPrint("   SETTINGS  ", 0, 0, DEFAULT_FONT, true);
    oled.invertOutput(true);
    oled.print(uint8_t(g_SettingsPage));
    oled.print("/");
    oled.print(uint8_t(g_SettingsMaxPages));
    oled.invertOutput(false);
}

void switchSettingsPage() {
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
void switchSettings() {
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
void showModulation() {
    oledPrint(g_bandModeDesc[g_currentMode], 0, 0, DEFAULT_FONT, g_activeCommand == CMD_BAND && g_currentMode == FM);
    oled.print(' ');
    updateStereoIndicator();
    showBandTag();
}

void updateStereoIndicator() {
    char c = (isSSB() && g_Settings[SettingsIndex::Sync].param == 1) ? 'S' :
        ((g_currentMode == FM && g_stereoStatus) ? '*' : ' ');

    oled.setCursor(24, 0);
    oled.print(c);
}

//Draw current band
void showBandTag() {
    if (g_settingsActive)
        return;

    bool invert = (g_activeCommand == CMD_BAND) && g_currentMode != FM;

    if (g_bandIndex == SW_BAND_TYPE) {
        uint16_t freq = g_currentFrequency;
        if (isSSB()) {
            freq += (g_currentBFO / 1000);
        }

        for (int8_t i = g_SWSubBandCount - 1; i >= 0; i--) {
            if (freq >= SWSubBands[i]) {
                // Create a temporary buffer to build the string from PROGMEM
                char sw_tag_buffer[5];
                for (uint8_t j = 0; j < 4; j++) {
                    sw_tag_buffer[j] = pgm_read_byte(&band_names_packed[i][j]);
                }
                sw_tag_buffer[4] = '\0'; // Null-terminate the string

                // Use the oledPrint helper, which correctly handles inversion
                oledPrint(sw_tag_buffer, 0, 6, DEFAULT_FONT, invert);
                return;
            }
        }
    }

    // For LW/MW, the helper function handles inversion.
    if (!isSSB()) {
        oledPrint(bandTags[g_bandIndex], 0, 6, DEFAULT_FONT, invert);
    }
}

//Draw volume level
void showVolume() {
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
void showRSSI() {
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

// determines the active tuning step based on 
// the current modulation mode (AM, FM, SSB)
void showStep() {
    char buf[5];
    int stepValue;
    bool isKhz = true;

    if (g_currentMode == FM) {
        stepValue = (g_tabStepFM[g_FMStepIndex] == 100) ? 1000 : g_tabStepFM[g_FMStepIndex] * 10;
    }
    else if (isSSB()) {
        stepValue = g_tabStep[SSB_STEP_OFFSET + g_stepIndexSSB];
        isKhz = false;
    }
    else { // AM
        stepValue = g_tabStep[g_stepIndexAM];
    }

    formatStepValue(buf, stepValue, isKhz);

    uint8_t off = 50;
    oledPrint("St:", off - 16, 6, DEFAULT_FONT, g_activeCommand == CMD_STEP);
    oledPrint(buf, off + 8, 6, DEFAULT_FONT, g_activeCommand == CMD_STEP);
}

//Draw bandwidth (Ignored for CW mode)
void showBandwidth() {
    char bw[5];
    const char* const* table_ptr = NULL;
    int8_t index = 0;

    if (isSSB()) {
        if (g_currentMode == CW) {
            bw[0] = '\0';
        }
        else {
            table_ptr = bw_ssb_table;
            index = g_bwIndexSSB;
        }
    }
    else if (g_currentMode == AM) {
        table_ptr = bw_am_table;
        index = g_bwIndexAM;
    }
    else { // FM
        table_ptr = bw_fm_table;
        index = g_bwIndexFM;
    }

    // Perform the copy operation only if a table was selected (handles CW mode)
    if (table_ptr != NULL) {
        strcpy_P(bw, (char*)pgm_read_word(&table_ptr[index]));
    }

    oledPrint(bw, 45, 0, DEFAULT_FONT, g_activeCommand == CMD_BW);
}

uint16_t getNextSWSuBband(bool up) {
    uint16_t freq = g_currentFrequency;
    if (isSSB())
        freq += g_currentBFO / 1000;

    for (uint8_t i = 0; i < g_SWSubBandCount; i++) {
        uint8_t n = g_SWSubBandCount - 1 - i;
        if (!up && SWSubBands[n] < freq)
            return SWSubBands[n];
        else if (up && SWSubBands[i] > freq)
            return SWSubBands[i];
    }

    return 0;
}

// Switch radio band up/down with special frequency rules for SW transitions
void bandSwitch(bool up) {
    uint16_t nextSW = getNextSWSuBband(up);

    if (g_bandIndex == SW_BAND_TYPE && nextSW != 0) {
        // Switch between SW sub-bands
        g_currentFrequency = nextSW;
        g_currentBFO = 0;
        if (isSSB())
            updateBFO();
        g_si4735.setFrequency(nextSW);
        showFrequency();
        showBandTag();
    }
    else {
        // Sync the state of the band we are LEAVING into the RAM structure
        // The actual save to EEPROM will happen later in loop()
        syncActiveStateToBand();

        uint8_t currentBand = g_bandIndex;

        // Calculate next band index
        if (up) {
            if (g_bandIndex < g_lastBand) g_bandIndex++;
            else g_bandIndex = 0;
        }
        else {
            if (g_bandIndex > 0) g_bandIndex--;
            else g_bandIndex = g_lastBand;
        }

        // Apply special frequency logic for band transitions
        if (g_bandIndex == SW_BAND_TYPE) {
            if (currentBand == FM_BAND_TYPE) {
                g_bandList[g_bandIndex].currentFreq = up ? SW_LIMIT_LOW : SW_LIMIT_HIGH;
            }
            else if (currentBand == MW_BAND_TYPE && up) {
                g_bandList[g_bandIndex].currentFreq = SW_LIMIT_LOW;
            }
        }

        // Reset BFO for SSB modes
        g_currentBFO = 0;
        if (isSSB())
            updateBFO();

        // Apply new band configuration
        applyBandConfiguration();

        // Mark settings as dirty to ensure the new band's state is saved eventually
        g_settingsDirty = true;
    }
}

// This function is required for using SSB. Si473x controllers do not support SSB by-default.
// But we can patch internal RAM of Si473x with special patch to make it work in SSB mode.
// Patch must be applied every time we enable SSB after AM or FM.
void loadSSBPatch() {
    safeAmpOff();

    g_si4735.setI2CFastModeCustom(500000);

    g_si4735.queryLibraryId();

    g_si4735.patchPowerUp();
    delay(50);
    g_si4735.downloadCompressedPatch(ssb_patch_content, sizeof(ssb_patch_content), cmd_0x15, sizeof(cmd_0x15));
    g_si4735.setSSBConfig(g_bwSSBIdx[g_bwIndexSSB], 1, 0, 1, 0, 1);
    g_si4735.setI2CStandardMode();

    g_ssbLoaded = true;
    // Reset the SSB step index to its default value
    g_stepIndexSSB = 0;

    safeAmpOn();
}

// Set up FM radio parameters including frequency limits, bandwidth, RDS, and de-emphasis
void configureFMMode() {
    g_currentMode = FM;
    g_stereoStatus = false;
    g_si4735.setFM(
        g_bandList[g_bandIndex].minimumFreq,
        g_bandList[g_bandIndex].maximumFreq,
        g_bandList[g_bandIndex].currentFreq,
        g_tabStepFM[g_FMStepIndex]);
    g_si4735.setSeekFmLimits(
        g_bandList[g_bandIndex].minimumFreq,
        g_bandList[g_bandIndex].maximumFreq);
    g_si4735.setSeekFmSpacing(10);

    // Set custom seek thresholds to improve performance on weak stations.
    g_si4735.setProperty(0x1403, 2);  // FM_SEEK_TUNE_SNR_THRESHOLD (Default: 3)
    g_si4735.setProperty(0x1404, 9);  // FM_SEEK_TUNE_RSSI_THRESHOLD (Default: 20)

    g_ssbLoaded = false;
    g_si4735.setFifoCount(1);
    g_si4735.setFmBandwidth(g_bwIndexFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[DeEmp].param == 0) ? 1 : 2);

    g_currentRSSI = 255;
}

// Initialize SSB mode with patch loading, BFO setup, filters, and audio bandwidth configuration
void configureSSBMode(uint16_t minFreq, uint16_t maxFreq, bool extraSSBReset) {
    if (g_bwIndexSSB >= g_bwSSBMaxIdx)
        g_bwIndexSSB = 4;

    g_currentBFO = 0;
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SVC].param);

    g_si4735.setSSB(
        minFreq,
        maxFreq,
        g_bandList[g_bandIndex].currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? (g_Settings[CWSwitch].param + 1) : g_currentMode);

    updateSSBCutoffFilter();
    g_si4735.setSSBDspAfc(g_Settings[Sync].param == 1 ? 0 : 1);
    g_si4735.setSSBAvcDivider(g_Settings[Sync].param == 0 ? 0 : 3);

    // Use SoftMute setting from storage for SSB
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_SSB]);

    g_si4735.setSSBAudioBandwidth((g_currentMode == CW) ? g_bwSSBIdx[0] : g_bwSSBIdx[g_bwIndexSSB]);
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SSM].param);
}

// Switch to AM mode and configure bandwidth, soft mute, and frequency parameters
void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;

    g_si4735.setAM(
        minFreq,
        maxFreq,
        g_bandList[g_bandIndex].currentFreq,
        g_tabStep[g_stepIndexAM]);

    // Use SoftMute setting from storage for AM
    g_si4735.setAmSoftMuteMaxAttenuation(g_modeSettings[MODE_SETTING_SOFT_MUTE][MODE_CONTEXT_AM]);

    g_si4735.setBandwidth(g_bwAMIdx[g_bwIndexAM], 1);
}

// Set AGC, AVC gain, and seek parameters shared between AM and SSB modes
void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    ModeContext modeCtx = getModeContext();

    // Apply AVC MAX GAIN from storage for the current mode
    g_si4735.setAvcAmMaxGain(g_modeSettings[MODE_SETTING_AVC][modeCtx]);

    // Set seek limits and spacing
    g_si4735.setSeekAmLimits(minFreq, maxFreq);
    g_si4735.setSeekAmSpacing(g_tabStep[g_stepIndexAM]);
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
void applyAgcSettings() {
    ModeContext modeCtx = getModeContext();
    int8_t att_val = g_modeSettings[MODE_SETTING_AGC][modeCtx];

    setAgcHardware(att_val);
}

// Main band switching logic that coordinates mode transitions and amplifier control
void applyBandConfiguration(bool extraSSBReset) {
    // if transitioning between FM and pure AM
    bool prevWasFM = (g_currentMode == FM);
    bool nextIsFM = (g_bandIndex == FM_BAND_TYPE);
    bool nextIsPureAM = !nextIsFM && !g_ssbLoaded;
    bool switchingBetweenFMandAM = (prevWasFM && nextIsPureAM) || (!prevWasFM && nextIsFM);

    // power down amplifier if switching modes
    if (switchingBetweenFMandAM)
        safeAmpOff();

    loadActiveStateFromBand();

    // Tune antenna capacitor
    uint8_t cap_value = (g_bandIndex == FM_BAND_TYPE) ? 1 : g_Settings[AntennaCap].param;
    g_si4735.setTuneFrequencyAntennaCapacitor(cap_value);

    if (nextIsFM) {
        configureFMMode();
    }
    else {
        // AM frequency limits
        uint16_t minFreq = g_bandList[g_bandIndex].minimumFreq;
        uint16_t maxFreq = g_bandList[g_bandIndex].maximumFreq;
        if (g_bandIndex == SW_BAND_TYPE) {
            minFreq = SW_LIMIT_LOW;
            maxFreq = SW_LIMIT_HIGH;
        }

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

//Step value regulation
void doStep(int8_t v) {
    if (g_currentMode == FM) {
        doSwitchLogic(g_FMStepIndex, 0, g_lastStepFM, v);
        g_si4735.setFrequencyStep(g_tabStepFM[g_FMStepIndex]);
        g_si4735.setSeekFmSpacing(10);
        showStep();
    }
    else if (isSSB()) {
        // --- Isolated logic for SSB ---
        doSwitchLogic(g_stepIndexSSB, 0, SSB_STEPS_COUNT - 1, v);
        showStep();
    }
    else { // --- Isolated logic for AM ---
        const uint8_t max_am_idx = (g_bandIndex == LW_BAND_TYPE || g_bandIndex == MW_BAND_TYPE)
            ? 3 // Max step is 10 kHz (index 3) for LW/MW
            : (AM_STEPS_COUNT - 1); // All 7 steps are available for SW

        doSwitchLogic(g_stepIndexAM, 0, max_am_idx, v);
        g_si4735.setFrequencyStep(g_tabStep[g_stepIndexAM]);
        g_si4735.setSeekAmSpacing(g_tabStep[g_stepIndexAM]);
        showStep();
    }
}

// Corrected CW BFO offset logic to match standard radio behavior
// The Si4735 IC requires an inverted BFO value, so the math is reversed here to compensate
// To get a positive BFO offset for USB, the value must be negative before the final inversion
// See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
void updateBFO() {

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
void doVolume(int8_t v) {
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
void doSwitchLogic(int8_t& param, int8_t low, int8_t high, int8_t step) {
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

// Applies the brightness setting to the OLED display using the stored value
void applyBrightness() {
    // cheap non-linear mapping
    int16_t s = g_Settings[Brightness].param;
    uint8_t contrast_value = 5 + (s * s * 3);
    oled.setContrast(contrast_value);
}

//Settings: Brightness
void doBrightness(int8_t v) {
    // Manage 10 steps, from 0 to 9 internally
    int8_t new_setting = g_Settings[Brightness].param + v;

    // Clamp to the 10 steps range [0, 9]
    if (new_setting > 9) new_setting = 9;
    if (new_setting < 0) new_setting = 0;

    g_Settings[Brightness].param = new_setting;

    // Ask the system to apply the new brightness value
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

//Bandwidth regulation logic
void doBandwidth(uint8_t v) {
    if (isSSB()) {
        doSwitchLogic(g_bwIndexSSB, 0, g_bwSSBMaxIdx, v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[g_bwIndexSSB]);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM) {
        doSwitchLogic(g_bwIndexAM, 0, g_maxFilterAM, v);
        g_si4735.setBandwidth(g_bwAMIdx[g_bwIndexAM], 1);
    }
    else { // FM
        // The table is ordered wide-to-narrow, so a right turn (v=+1) should decrease the index
        doSwitchLogic(g_bwIndexFM, 0, 4, -v);
        g_si4735.setFmBandwidth(g_bwIndexFM);
    }
    showBandwidth();
}

// switch command modes
void switchCommand(CommandMode mode) {
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
void resetCommandMode() {
    if (g_activeCommand != CMD_NONE) {
        g_activeCommand = CMD_NONE;
        g_lastAdjustmentTime = 0;
        showVolume();
        showStep();
        showBandwidth();
        showModulation();
    }
}

bool clampSSBBand() {
    uint16_t freq = g_currentFrequency + (g_currentBFO / 1000);
    auto bfoReset = [&]() {
        g_currentBFO = 0;
        updateBFO();
        showFrequency(true);
        showModulation();
        };

    bool upd = false;
    if (freq > g_bandList[g_bandIndex].maximumFreq) {
        g_currentFrequency = g_bandList[g_bandIndex].minimumFreq;
        upd = true;
    }
    else if (freq < g_bandList[g_bandIndex].minimumFreq) {
        g_currentFrequency = g_bandList[g_bandIndex].maximumFreq;
        upd = true;
    }

    if (upd) {
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
        g_si4735.setFrequency(g_currentFrequency);
        bfoReset();
        return true;
    }

    return false;
}

// Handles frequency tuning for AM/FM with step alignment.
void doFrequencyTune() {
    g_seekDirection = g_encoderCount == 1 ? 1 : 0;
    g_previousFrequency = g_currentFrequency;

    uint16_t step;
    if (g_currentMode == FM) {
        step = g_tabStepFM[g_FMStepIndex];
    }
    else { // am
        step = g_tabStep[g_stepIndexAM];
    }

    // handle snap-to-grid and regular step as mutually exclusive actions
    // this structure fixes a bug in the boundary wrap-around logic
    uint16_t remainder = g_currentFrequency % step;
    if (remainder != 0 && g_encoderCount != 0) {
        // snap to grid
        if (g_encoderCount > 0) {
            g_currentFrequency += (step - remainder);
        }
        else { // g_encoderCount < 0
            g_currentFrequency -= remainder;
        }
    }
    else {
        // regular step
        g_currentFrequency += (int16_t)step * g_encoderCount;
    }

    // reset encoder delta for the next loop cycle
    g_encoderCount = 0;

    uint16_t bMin = g_bandList[g_bandIndex].minimumFreq, bMax = g_bandList[g_bandIndex].maximumFreq;

    // use seek direction instead of encoder count for boundary checks
    // this is more reliable with large deltas from fast spins
    if (g_currentFrequency >= bMax && g_seekDirection == 1)
        g_currentFrequency = bMin;
    else if (g_currentFrequency < bMin && g_seekDirection == 0)
        g_currentFrequency = bMax;
    else if (g_currentFrequency >= bMax)
        g_currentFrequency = bMax;
    else if (g_currentFrequency < bMin)
        g_currentFrequency = bMin;

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    g_processFreqChange = true;
    g_lastFreqChange = millis();

    showFrequency();
}

// Special feature to make SSB feel like on expensive TECSUN receivers
// BFO is now part of main frequency in SSB mode
void doFrequencyTuneSSB() {

    // Reduced BFOMax from 16000 to 13000 to create a safe buffer
    // This prevents the total BFO offset (tuning + calibration + CW tone)
    // from exceeding the Si4732's hardware limit of +/- 16383 Hz
    // See: https://github.com/goshante/ats20_ats_ex/issues/42#issuecomment-3015265184
    const int BFOMax = 13000;

    int step = g_tabStep[SSB_STEP_OFFSET + g_stepIndexSSB] * g_encoderCount;
    int newBFO = g_currentBFO + step;
    int redundant = 0;

    if (newBFO > BFOMax) {
        redundant = (newBFO / BFOMax) * BFOMax;
        g_currentFrequency += redundant / 1000;
        newBFO -= redundant;
    }
    else if (newBFO < -BFOMax) {
        redundant = ((abs(newBFO) / BFOMax) * BFOMax);
        g_currentFrequency -= redundant / 1000;
        newBFO += redundant;
    }

    g_currentBFO = newBFO;
    updateBFO();

    if (redundant != 0) {
        g_si4735.setFrequency(g_currentFrequency);
        // Re-apply AGC settings after a large frequency jump
        applyAgcSettings();

        g_currentFrequency = g_si4735.getFrequency();
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    }

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency + (g_currentBFO / 1000);
    g_lastFreqChange = millis();
    g_previousFrequency = 0;  //Force EEPROM update
    if (!clampSSBBand())      //If we move outside of current band - switch it
        showFrequency();
}

// helper functions for processButtonEvents
// declared as static inline to hint the compiler, avoids function call overhead
// this refactoring improves code readability with a minimal impact on flash size

static inline void handleEncoderButton() {
    // encoder button handler
    uint8_t evt = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS != evt) return;

    if (g_activeCommand != CMD_NONE) {
        // if a command mode is active, encoder button cancels it
        resetCommandMode();
    }
    else if (g_settingsActive) {
        // in settings menu, it toggles editing mode for the selected item
        g_SettingEditing = !g_SettingEditing;
        DrawSetting(g_SettingSelected, true);
    }
    else if (g_favoritesActive) {
        // in favorites menu, it tunes to the selected favorite and exits
        if (g_totalFavorites) {
            g_currentFrequency = g_fmFavorites[g_favoriteSelected].frequency;
            g_si4735.setFrequency(g_currentFrequency);
        }
        // original lambda 'exitFavorites' inlined here to save flash
        exitFavoritesMenu();
    }
    else if (isSSB() || !g_Settings[ScanSwitch].param) {
        // default action: enter step adjustment mode, seek is disabled in ssb
        switchCommand(CMD_STEP);
    }
    else if (g_currentMode == FM || g_currentMode == AM) {
        // in am/fm, if scan is enabled in settings, it starts the seek function
        doSeek();
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

static inline void handleAgcButton() {
    // agc button (display on/off) handler
    uint8_t evt = btn_AGC.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        // toggles the oled display on and off
        if (!g_settingsActive || (g_settingsActive && !g_displayOn)) {
            g_displayOn = !g_displayOn;
            g_displayOn ? oled.on() : oled.off();
        }
    }
}

static inline void handleStepButton() {
    // step button handler
    uint8_t evt = btn_Step.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive) {
        switchCommand(CMD_STEP);
    }
}

static inline void handleModeButton() {
    // mode button (favorites, modulation) handler
    uint8_t evt = btn_Mode.checkEvent(simpleEvent);

    // short press logic
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive) {
        if (g_favoritesActive) {
            g_favoritesActive = false;
            oled.clear();
            showStatus();
        }
        else if (g_currentMode == FM) {
            // in fm mode, this button enters the favorites menu
            g_favoritesActive = true;
            g_favoriteSelected = 0;
            oled.clear();
            showFav();
        }
        else {
            // in am/ssb/cw modes, it cycles through the available modulations
            uint8_t bw = (g_currentMode == AM) ? g_bwIndexAM : g_bwIndexSSB;
            syncActiveStateToBand();

            if (g_currentMode == CW) safeAmpOff();

            // when switching from am to ssb for the first time, the patch must be loaded
            if (g_currentMode == AM) {
                loadSSBPatch();
                g_bwIndexSSB = bw;
                g_processFreqChange = false;
            }

            // cycle through am(0), lsb(1), usb(2), cw(3)
            g_currentMode = (g_currentMode + 1) % 4;

            // when switching back to am, unload the ssb patch flag to return to normal am operation
            if (g_currentMode == AM) {
                g_ssbLoaded = false;
                g_bwIndexAM = bw;
            }

            applyBandConfiguration();

            if (!g_ssbLoaded && g_currentMode == AM) safeAmpOn();
        }
    }
    // long press logic (only for fm mode)
    else if (BUTTONEVENT_LONGPRESSDONE == evt && !g_settingsActive && g_currentMode == FM && !g_favoritesActive) {
        // in fm mode, a long press adds the current station to favorites
        addFav();
        oled.setCursor(45, 3);
        oled.print(F("SAVED"));
        delay(500);
        showFrequency(true);
    }
}

// exit the favorites menu and return to the main status screen
static inline void exitFavoritesMenu() {
    g_favoritesActive = false;
    oled.clear();
    showStatus();
}

// key process for all keys
void processButtonEvents() {
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
void updateEncoderState() {
    if (g_encoderCount) {
        noInterrupts();
        g_safeEncoderMovement += g_encoderCount;
        g_encoderCount = 0;
        interrupts();
    }
}

// Handles all user input and display updates when in the Favorites menu.
void handleFavoritesMenu() {
    if (g_safeEncoderMovement) {
        if (g_totalFavorites > 0) {
            g_favoriteSelected = (g_favoriteSelected + g_safeEncoderMovement + g_totalFavorites) % g_totalFavorites;
            showFav();
        }
        g_safeEncoderMovement = 0;
    }
    processButtonEvents();
}

// Handles encoder actions for settings, commands, and frequency tuning.
// Returns true if the action was a frequency tune, false otherwise.
bool processEncoderActions() {
    if (g_lastAdjustmentTime && g_activeCommand != CMD_NONE) g_lastAdjustmentTime = millis();

    g_encoderCount = g_safeEncoderMovement;

    if (g_settingsActive) {
        if (!g_SettingEditing) {
            int8_t prev = g_SettingSelected;
            g_SettingSelected += g_safeEncoderMovement;
            uint8_t page = g_SettingsPage - 1;
            uint8_t max = min((page * 6) + 5, SettingsIndex::SETTINGS_MAX - 1);
            if (g_SettingSelected < page * 6) g_SettingSelected = max;
            else if (g_SettingSelected > max) g_SettingSelected = page * 6;
            DrawSetting(prev, true);
            DrawSetting(g_SettingSelected, true);
        }
        else {
            (*g_Settings[g_SettingSelected].manipulateCallback)(g_safeEncoderMovement);
            DrawSetting(g_SettingSelected, false);
            delay(MIN_ELAPSED_TIME);
        }
    }
    else {
        switch (g_activeCommand) {
        case CMD_VOLUME:
            doVolume(g_safeEncoderMovement);
            break;
        case CMD_STEP:
            doStep(g_safeEncoderMovement);
            break;
        case CMD_BW:
            doBandwidth(g_safeEncoderMovement);
            break;
        case CMD_BAND:
            bandSwitch(g_safeEncoderMovement == 1);
            break;
        case CMD_NONE: // If no command mode is active, tune frequency
            if (isSSB()) {
                doFrequencyTuneSSB();
            }
            else {
                doFrequencyTune();
            }
            g_safeEncoderMovement = 0;
            g_encoderCount = 0;
            resetEepromDelay();
            return true; // This replaces skip = true
        }
    }

    g_safeEncoderMovement = 0;
    g_encoderCount = 0;
    resetEepromDelay();
    return false;
}

// Handles the delayed frequency update for AM/FM to prevent flooding the chip.
void handleDelayedFrequencyUpdate() {
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
void handlePeriodicTasks() {
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

    // Final, correct, and centralized save logic.
    if ((millis() - g_storeTime) > STORE_TIME) {
        if (g_settingsDirty || g_currentFrequency != g_previousFrequency) {
            saveAllReceiverInformation(false);
            g_settingsDirty = false;
            g_previousFrequency = g_currentFrequency;
        }
        g_storeTime = millis();
    }
}

// main loop program in process order
void loop() {
    updateEncoderState();

    if (g_favoritesActive) {
        // Handle favorites menu navigation with bounds checking
        handleFavoritesMenu();
        return; // Exit loop early when in favorites menu
    }

    // --- Main operating mode ---

    handleDelayedFrequencyUpdate();

    bool frequencyTuned = false;
    if (g_safeEncoderMovement) {
        frequencyTuned = processEncoderActions();
    }

    // Process buttons only if the encoder was not used for frequency tuning in this cycle
    if (!frequencyTuned) {
        processButtonEvents();
    }

    // Check for command mode timeout
    if (g_lastAdjustmentTime && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT) {
        resetCommandMode();
    }

    handlePeriodicTasks();
}

//Overriding original main to save some space
int main(void) {
    init();
    setup();
    while (1)
        loop();
    return 0;
}