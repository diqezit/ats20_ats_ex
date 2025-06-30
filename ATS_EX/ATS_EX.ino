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

void showStatus(bool cleanFreq = false);
void applyBandConfiguration(bool extraSSBReset = false);
void updateStereoIndicator();

bool isSSB() {
    return g_currentMode > AM && g_currentMode < FM;
}

// --------------------------
// ------- Main logic -------
// --------------------------

#define APP_VERSION 118

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

    if (!(PINC & (1 << (ENCODER_BUTTON - 14))) || !(PINB & (1 << (AGC_BUTTON - 8)))) {
        EEPROM.write(EEPROM_VERSION_ADDRESS, 0);
        EEPROM.write(EEPROM_APP_ID_ADDRESS, 0);
        saveAllReceiverInformation();
        oled.setCursor(24, 2);
        oled.print(F("EEPROM"));
        oled.setCursor(32, 4);
        oled.print(F("RESET"));
        delay(1500);
    }
    else {

        oled.setCursor(20, 2);
        oled.print(F("ATS-EX 1.18"));

        oled.setCursor(32, 4);
        oled.print(F("Goshante"));

        delay(1500);

    }
    oled.clear();

    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);

    delay(500);

    if (EEPROM.read(EEPROM_VERSION_ADDRESS) == APP_VERSION && EEPROM.read(EEPROM_APP_ID_ADDRESS) == EEPROM_APP_ID)
        readAllReceiverInformation();
    else
        saveAllReceiverInformation();

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
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
    return event;
}

//This looks like it's better to remove them and use only simpleEvent
//But it's a part of a hack that allows us to save more flash image size
//uint8_t stepEvent(uint8_t event, uint8_t pin) {
//  return simpleEvent(event, pin);
//}

//uint8_t agcEvent(uint8_t event, uint8_t pin) {
//  return simpleEvent(event, pin);
//}

uint8_t modeEvent(uint8_t event, uint8_t pin) {
    if (BUTTONEVENT_ISLONGPRESS(event)) return event;

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

//EEPROM Save
void saveAllReceiverInformation() {
    uint8_t addr = EEPROM_DATA_START_ADDRESS;
    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);
    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);

    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0XFF);
    EEPROM.update(addr++, g_FMStepIndex);
    EEPROM.update(addr++, g_prevMode);
    EEPROM.update(addr++, g_bwIndexSSB);

    // Synchronize the active state before writing to EEPROM
    if (isSSB()) {
        g_bandList[g_bandIndex].currentStepIdx = g_stepIndexSSB;
    }
    else if (g_currentMode == AM) {
        g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;
    }

    for (uint8_t i = 0; i <= g_lastBand; i++) {
        EEPROM.update(addr++, (g_bandList[i].currentFreq >> 8));
        EEPROM.update(addr++, (g_bandList[i].currentFreq & 0xFF));
        // Writing the step index is now simple, as it's always correct for its mode
        EEPROM.update(addr++, g_bandList[i].currentStepIdx);
        EEPROM.update(addr++, g_bandList[i].bandwidthIdx);
    }

    for (uint8_t i = 0; i < SettingsIndex::SETTINGS_MAX; i++)
        EEPROM.update(addr++, g_Settings[i].param);

    // Save FM favorites
    saveFMFav();
}

//EEPROM Load
void readAllReceiverInformation() {
    uint8_t addr = EEPROM_DATA_START_ADDRESS;
    int8_t bwIdx;
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    g_currentMode = EEPROM.read(addr++);
    g_currentBFO = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
    g_FMStepIndex = EEPROM.read(addr++);
    g_prevMode = EEPROM.read(addr++);
    g_bwIndexSSB = EEPROM.read(addr++);

    for (uint8_t i = 0; i <= g_lastBand; i++) {
        g_bandList[i].currentFreq = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
        g_bandList[i].currentStepIdx = EEPROM.read(addr++);
        g_bandList[i].bandwidthIdx = EEPROM.read(addr++);
    }

    for (uint8_t i = 0; i < SettingsIndex::SETTINGS_MAX; i++)
        g_Settings[i].param = EEPROM.read(addr++);

    oled.setContrast(uint8_t(g_Settings[SettingsIndex::Brightness].param) * 2);

    g_previousFrequency = g_currentFrequency = g_bandList[g_bandIndex].currentFreq;

    // Initialize global state variables from the loaded data
    if (isSSB()) {
        g_stepIndexSSB = g_bandList[g_bandIndex].currentStepIdx;
    }
    else if (g_currentMode == AM) {
        g_stepIndexAM = g_bandList[g_bandIndex].currentStepIdx;
    }
    else { // FM
        g_FMStepIndex = g_bandList[g_bandIndex].currentStepIdx;
    }

    bwIdx = g_bandList[g_bandIndex].bandwidthIdx;

    if (isSSB()) {
        loadSSBPatch();
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[g_bwIndexSSB]);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM) {
        g_bwIndexAM = bwIdx;
        g_si4735.setBandwidth(g_bwAMIdx[g_bwIndexAM], 1);
    }
    else {
        g_bwIndexFM = bwIdx;
        g_si4735.setFmBandwidth(g_bwIndexFM);
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
    // Check if count changed
    uint8_t savedCount = EEPROM.read(EEPROM_FM_FAVORITES_COUNT);
    bool changed = (savedCount != g_totalFavorites);

    // Check if frequencies changed
    if (!changed && g_totalFavorites > 0) {
        uint16_t addr = EEPROM_FM_FAVORITES_START;
        for (uint8_t i = 0; i < g_totalFavorites; i++) {
            uint16_t savedFreq = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
            if (savedFreq != g_fmFavorites[i].frequency) {
                changed = true;
                break;
            }
        }
    }

    // Save only if changed
    if (!changed) return;

    EEPROM.update(EEPROM_FM_FAVORITES_COUNT, g_totalFavorites);
    uint16_t addr = EEPROM_FM_FAVORITES_START;
    for (uint8_t i = 0; i < MAX_FM_FAVORITES; i++) {
        if (i < g_totalFavorites) {
            EEPROM.update(addr++, g_fmFavorites[i].frequency >> 8);
            EEPROM.update(addr++, g_fmFavorites[i].frequency & 0xFF);
        }
        else {
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
    oled.print(F(" FM FAVORITES "));
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
        oled.setCursor(0, 3);
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

#if USE_RDS
    if (g_displayRDS)
        oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
#endif

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
    showCharge(true);
    showVolume();
    showRSSI();
}

void updateLowerDisplayLine() {
    oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
    showModulation();
    showStep();
    showCharge(true);
}

// Converts setting parameter value to UI display string
// Handles different setting types (Num, ZeroAuto, Switch, SwitchAuto)
// Uses PROGMEM table for text values to save memory
void SettingParamToUI(char* buf, uint8_t idx) {
    int8_t param = g_Settings[idx].param;
    uint8_t textIdx = 0xFF;

    switch (g_Settings[idx].type) {
    case SettingType::Num:
        // For numeric type, convert number to string
        convertToChar(buf, abs(param), 3);
        if (param < 0) buf[0] = '-';
        buf[3] = '\0';
        return;

    case SettingType::ZeroAuto:
        // Zero shows "AUT", other values show as numbers
        if (param == 0) textIdx = 0; // "AUT"
        else {
            convertToChar(buf, param, 3);
            buf[3] = '\0';
            return;
        }
        break;

    case SettingType::SwitchAuto:
        // Direct mapping: 0="AUT", 1="On ", 2="Off"
        textIdx = param;
        break;

    case SettingType::Switch:
        // Different switches have different text mappings
        if (idx == SettingsIndex::DeEmp)
            textIdx = 3 + param; // 0="50u", 1="75u"
        else if (idx == SettingsIndex::SWUnits)
            textIdx = 5 + param; // 0="kHz", 1="MHz"
        else if (idx == SettingsIndex::SSM)
            textIdx = 7 + param; // 0="RSS", 1="SNR"
        else if (idx == SettingsIndex::CWSwitch)
            textIdx = 9 + param; // 0="LSB", 1="USB"
        else if (idx == SettingsIndex::CPUSpeed)
            textIdx = 11 + param; // 0="100", 1="50%"
        else
            textIdx = 2 - param; // Generic: 0="Off", 1="On "
        break;
    }

    // Copy text from PROGMEM to buffer
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

//Switch between main screen and settings mode
void switchSettings() {
    oled.clear();
    if (g_settingsActive) {
        g_SettingsPage = 1;
        showSettingsTitle();
        g_SettingSelected = 0;
        g_SettingEditing = false;
        showSettings();
    }
    else {
        saveAllReceiverInformation();
        showStatus();
    }
}

//Draw curremt modulation
void showModulation() {
    oledPrint(g_bandModeDesc[g_currentMode], 0, 0, DEFAULT_FONT, g_cmdBand && g_currentMode == FM);
    oled.print(" ");
    updateStereoIndicator();
    showBandTag();
}

void updateStereoIndicator() {
    char c = ' ';
    if (isSSB() && g_Settings[SettingsIndex::Sync].param == 1)
        c = 'S';
    else if (g_currentMode == FM && g_stereoStatus)
        c = '*';

    oled.setCursor(24, 0);
    oled.print(c);
}

//Draw current band
void showBandTag() {
    if (g_sMeterOn || g_displayRDS || g_settingsActive)
        return;

    bool invert = g_cmdBand && g_currentMode != FM;

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

    oledPrint(buf, (128 - (8 * 2) + 2 - 6), 0, DEFAULT_FONT, g_cmdVolume);
}

// RSSI drawings
void showRSSI() {
    if (g_settingsActive || g_currentMode != FM)
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

//Draw battery charge
//This feature requires hardware mod
//Voltage divider made of two 10 KOhm resistors between + and GND of Li-Ion battery
//Solder it to A2 analog pin
void showCharge(bool forceShow)
{
    // Early exit if the voltage pin is not connected
    if (!g_voltagePinConnnected)
        return;

    static uint32_t lastChargeShow = 0;
    static int16_t averageSamples = -1;

    int sample = analogRead(BATTERY_VOLTAGE_PIN);

    if (averageSamples == -1 || sample < 0) {
        averageSamples = sample > 0 ? sample : 550;
    }
    averageSamples = (averageSamples + sample) >> 1;

    if ((millis() - lastChargeShow) > 10000 || forceShow)
    {
        constexpr const uint8_t rows = 10;
        static const PROGMEM uint16_t voltages[rows] = { 643, 620, 604, 581, 573, 558, 542, 503, 496, 488 };
        static const PROGMEM uint8_t percents[rows] = { 100, 95, 90, 80, 60, 40, 20, 15, 5, 0 };

        uint8_t batteryPercent;
        uint16_t currentSamples = averageSamples;

        if (currentSamples >= pgm_read_word(&voltages[0])) {
            batteryPercent = 100;
        }
        else if (currentSamples <= pgm_read_word(&voltages[rows - 1])) {
            batteryPercent = 0;
        }
        else {
            batteryPercent = 0;
            for (uint8_t i = 0; i < rows - 1; ++i)
            {
                uint16_t v_upper = pgm_read_word(&voltages[i]);
                uint16_t v_lower = pgm_read_word(&voltages[i + 1]);

                if (currentSamples >= v_lower && currentSamples <= v_upper)
                {
                    uint8_t p_upper = pgm_read_byte(&percents[i]);
                    uint8_t p_lower = pgm_read_byte(&percents[i + 1]);

                    uint16_t voltageDiff = v_upper - v_lower;
                    uint16_t percentageDiff = p_upper - p_lower;
                    uint16_t voltageOffset = currentSamples - v_lower;

                    batteryPercent = p_lower + (percentageDiff * voltageOffset + voltageDiff / 2) / voltageDiff;
                    break;
                }
            }
        }

        if (!g_settingsActive && !g_sMeterOn && !g_displayRDS) {
            char buf[4];
            if (batteryPercent >= 100) {
                buf[0] = '1'; buf[1] = '0'; buf[2] = '0'; buf[3] = '\0';
            }
            else {
                convertToChar(buf, batteryPercent, 2);
                buf[2] = '%';
                buf[3] = '\0';
            }
            oledPrint(buf, 102, 6, DEFAULT_FONT);
        }
        lastChargeShow = millis();
    }
}

#if USE_RDS
void showRDS() {
    static uint16_t lastUpdatedFreq = 0;
    static uint32_t lastUpdatedTime = millis();
    static bool succeed = false;

    if (g_currentMode != FM || !g_displayRDS || g_settingsActive) {
        lastUpdatedFreq = 0;
        g_rdsPrevLen = 0;
        succeed = false;
        g_rdsActiveInfo = 0;
        return;
    }

    if (millis() - lastUpdatedTime > 300)
        succeed = false;

    if (lastUpdatedFreq != g_currentFrequency || g_rdsSwitchPressed) {
        if (g_rdsSwitchPressed) {
            g_rdsActiveInfo++;
            if (g_rdsActiveInfo > RDSActiveInfo::ProgramInfo)
                g_rdsActiveInfo = RDSActiveInfo::StationName;
        }
        else {
            g_rdsActiveInfo = RDSActiveInfo::StationName;
            succeed = false;
        }
        g_rdsPrevLen = 0;
        oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
    }
    lastUpdatedFreq = g_currentFrequency;

    if (!succeed)
        g_si4735.getRdsStatus();

    if (!succeed && g_si4735.getRdsReceived() && g_si4735.getRdsSync() && g_si4735.getNumRdsFifoUsed() > 1) {
        g_RDSCells[RDSActiveInfo::StationName] = g_si4735.getRdsStationName();
        g_RDSCells[RDSActiveInfo::StationInfo] = g_si4735.getRdsStationInformation();
        g_RDSCells[RDSActiveInfo::ProgramInfo] = g_si4735.getRdsProgramInformation();
        g_RDSCells[RDSActiveInfo::StationInfo][17] = '\0';
        g_RDSCells[RDSActiveInfo::ProgramInfo][17] = '\0';
        succeed = true;
        lastUpdatedTime = millis();
    }
    else if (!g_rdsSwitchPressed && succeed)
        return;

    uint8_t len = strlen8(g_RDSCells[g_rdsActiveInfo]);

    if (len == 0 && !g_rdsSwitchPressed)
        return;

    oledPrint(g_RDSCells[g_rdsActiveInfo], 0, 6, DEFAULT_FONT);

    uint8_t toPrint = len == 0 ? 3 : (len < g_rdsPrevLen ? min(g_rdsPrevLen - len, 16 - len) : 0);
    char printChar = len == 0 ? '.' : ' ';
    for (uint8_t i = 0; i < toPrint; i++)
        oled.print(printChar);

    g_rdsPrevLen = len;
    g_rdsSwitchPressed = false;
}
#endif

//Draw steps (with units)
void showStep() {
    if (g_sMeterOn || g_displayRDS)
        return;

    char buf[5];
    if (g_currentMode == FM) {
        if (g_tabStepFM[g_FMStepIndex] == 100) {
            buf[0] = ' ';
            buf[1] = ' ';
            buf[2] = '1';
            buf[3] = 'M';
            buf[4] = 0x0;
        }
        else {
            convertToChar(buf, g_tabStepFM[g_FMStepIndex] * 10, 3);
            buf[3] = 'k';
            buf[4] = '\0';
        }
    }
    else if (isSSB()) {
        int stepValue = g_tabStep[SSB_STEP_OFFSET + g_stepIndexSSB];
        // If step is >= 1000 Hz, display it in kHz for better readability
        if (stepValue >= 1000) {
            convertToChar(buf, stepValue / 1000, 3);
            buf[3] = 'k';
            buf[4] = '\0';
        }
        else { // Otherwise, display in Hz
            convertToChar(buf, stepValue, 4);
        }
    }
    else { // AM
        int stepValue = g_tabStep[g_stepIndexAM];
        if (stepValue == 1000) {
            buf[0] = ' ';
            buf[1] = ' ';
            buf[2] = '1';
            buf[3] = 'M';
            buf[4] = 0x0;
        }
        else {
            convertToChar(buf, stepValue, 3);
            buf[3] = 'k';
            buf[4] = '\0';
        }
    }

    uint8_t off = 50;
    oledPrint("St:", off - 16, 6, DEFAULT_FONT, g_cmdStep);
    oledPrint(buf, off + 8, 6, DEFAULT_FONT, g_cmdStep);
}

void showSMeter() {
    static uint32_t sMeterUpdated = 0;
    if (millis() - sMeterUpdated < 100)
        return;

    g_si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = g_si4735.getCurrentRSSI();
    rssi = rssi > 64 ? 64 : rssi;

    int sMeterValue = rssi / (64 / 16);
    char buf[17];
    for (uint8_t i = 0; i < sizeof(buf) - 1; i++)
        buf[i] = i < sMeterValue ? '|' : ' ';
    buf[sizeof(buf) - 1] = 0x0;

    oledPrint(buf, 0, 6, DEFAULT_FONT);
    sMeterUpdated = millis();
}

//Draw bandwidth (Ignored for CW mode)
void showBandwidth() {
    char bw[5];

    if (isSSB()) {
        if (g_currentMode == CW) {
            bw[0] = '\0';
        }
        else {
            strcpy_P(bw, (char*)pgm_read_word(&(bw_ssb_table[g_bwIndexSSB])));
        }
    }
    else if (g_currentMode == AM) {
        strcpy_P(bw, (char*)pgm_read_word(&(bw_am_table[g_bwIndexAM])));
    }
    else {
        strcpy_P(bw, (char*)pgm_read_word(&(bw_fm_table[g_bwIndexFM])));
    }

    oledPrint(bw, 45, 0, DEFAULT_FONT, g_cmdBw);
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
        agcSetFunc();
        showFrequency();
        showBandTag();
    }
    else {
        // Save current band state before switching
        uint8_t currentBand = g_bandIndex;
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;

        // Save the correct step index based on the current mode
        if (g_currentMode == FM) {
            g_bandList[g_bandIndex].currentStepIdx = g_FMStepIndex;
        }
        else if (isSSB()) {
            g_bandList[g_bandIndex].currentStepIdx = g_stepIndexSSB;
        }
        else { // AM
            g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;
        }

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

        // Clear UI elements if active
        if (g_sMeterOn) {
            g_sMeterOn = false;
            oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
        }

#if USE_RDS
        if (g_displayRDS && g_currentMode != FM) {
            g_displayRDS = false;
            oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
        }
#endif

        // Reset BFO for SSB modes
        g_currentBFO = 0;
        if (isSSB())
            updateBFO();

        // Apply new band configuration
        applyBandConfiguration();
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

#if USE_RDS
void setRDSConfig(uint8_t bias) {
    g_si4735.setRdsConfig(1, bias, bias, bias, bias);
}
#endif

// ============ Refactor ==============

// Set up FM radio parameters including frequency limits, bandwidth, RDS, and de-emphasis
void configureFMMode() {
    g_currentMode = FM;
    g_stereoStatus = false;
    g_si4735.setFM(
        g_bandList[g_bandIndex].minimumFreq,
        g_bandList[g_bandIndex].maximumFreq,
        g_bandList[g_bandIndex].currentFreq,
        g_tabStepFM[g_bandList[g_bandIndex].currentStepIdx]);
    g_si4735.setSeekFmLimits(
        g_bandList[g_bandIndex].minimumFreq,
        g_bandList[g_bandIndex].maximumFreq);
    g_si4735.setSeekFmSpacing(10);
    g_ssbLoaded = false;
#if USE_RDS
    setRDSConfig(g_Settings[SettingsIndex::RDSError].param);
#endif
    g_si4735.setFifoCount(1);
    g_bwIndexFM = g_bandList[g_bandIndex].bandwidthIdx;
    g_si4735.setFmBandwidth(g_bwIndexFM);
    g_si4735.setFMDeEmphasis(
        (g_Settings[SettingsIndex::DeEmp].param == 0) ? 1 : 2);

    g_currentRSSI = 255;
}

// Initialize SSB mode with patch loading, BFO setup, filters, and audio bandwidth configuration
void configureSSBMode(uint16_t minFreq, uint16_t maxFreq, bool extraSSBReset) {
    if (g_bwIndexSSB >= g_bwSSBMaxIdx)
        g_bwIndexSSB = 4;

    g_currentBFO = 0;
    if (extraSSBReset)
        loadSSBPatch();

    g_si4735.setSSBAutomaticVolumeControl(g_Settings[SettingsIndex::SVC].param);

    // A base step of 1kHz is passed for the chip's internal seek logic.
    // Our encoder logic completely controls the actual tuning step.
    g_si4735.setSSB(
        minFreq,
        maxFreq,
        g_bandList[g_bandIndex].currentFreq,
        1, // Base step for the chip (1 kHz)
        (g_currentMode == CW) ? (g_Settings[SettingsIndex::CWSwitch].param + 1) : g_currentMode);

    updateSSBCutoffFilter();
    g_si4735.setSSBDspAfc(
        (g_Settings[SettingsIndex::Sync].param == 1) ? 0 : 1);
    g_si4735.setSSBAvcDivider(
        (g_Settings[SettingsIndex::Sync].param == 0) ? 0 : 3);
    g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
    g_si4735.setSSBAudioBandwidth(
        (g_currentMode == CW) ? g_bwSSBIdx[0] : g_bwSSBIdx[g_bwIndexSSB]);
    updateBFO();
    g_si4735.setSSBSoftMute(g_Settings[SettingsIndex::SSM].param);
}

// Switch to AM mode and configure bandwidth, soft mute, and frequency parameters
void configureAMMode(uint16_t minFreq, uint16_t maxFreq) {
    g_currentMode = AM;

    // Directly use the correct step value from g_tabStep via the bandList index
    g_si4735.setAM(
        minFreq,
        maxFreq,
        g_bandList[g_bandIndex].currentFreq,
        g_tabStep[g_bandList[g_bandIndex].currentStepIdx]);

    g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
    g_bwIndexAM = g_bandList[g_bandIndex].bandwidthIdx;
    g_si4735.setBandwidth(g_bwAMIdx[g_bwIndexAM], 1);
}

// Set AGC, AVC gain, and seek parameters shared between AM and SSB modes
void configureAMCommon(uint16_t minFreq, uint16_t maxFreq) {
    agcSetFunc();
    g_si4735.setAvcAmMaxGain(g_Settings[SettingsIndex::AutoVolControl].param);
    g_si4735.setSeekAmLimits(minFreq, maxFreq);

    // The seek spacing also uses the direct step value
    g_si4735.setSeekAmSpacing(g_tabStep[g_bandList[g_bandIndex].currentStepIdx]);
}

// Synchronize global frequency variables and adjust step indices for band restrictions
void updateFrequencyAndStepIndices() {
    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;

    // Load the step index into the corresponding global state variable
    if (isSSB()) {
        g_stepIndexSSB = g_bandList[g_bandIndex].currentStepIdx;
    }
    else if (g_currentMode == AM) {
        g_stepIndexAM = g_bandList[g_bandIndex].currentStepIdx;
    }
    else { // FM
        g_FMStepIndex = g_bandList[g_bandIndex].currentStepIdx;
    }
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

    // Tune antenna capacitor
    g_si4735.setTuneFrequencyAntennaCapacitor(static_cast<uint16_t>(nextIsFM));

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

    // update global frequency and step indices
    updateFrequencyAndStepIndices();

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

// ============= Refactor End ==============

//Step value regulation
void doStep(int8_t v) {
    if (g_currentMode == FM) {
        g_FMStepIndex = (v == 1) ? g_FMStepIndex + 1 : g_FMStepIndex - 1;
        if (g_FMStepIndex > g_lastStepFM)
            g_FMStepIndex = 0;
        else if (g_FMStepIndex < 0)
            g_FMStepIndex = g_lastStepFM;

        g_si4735.setFrequencyStep(g_tabStepFM[g_FMStepIndex]);
        g_bandList[g_bandIndex].currentStepIdx = g_FMStepIndex;
        g_si4735.setSeekFmSpacing(10);
        showStep();
    }
    else if (isSSB()) {
        // --- Isolated logic for SSB ---
        // This part now works with the new SSB_STEPS_COUNT
        g_stepIndexSSB = (v == 1) ? g_stepIndexSSB + 1 : g_stepIndexSSB - 1;
        if (g_stepIndexSSB >= SSB_STEPS_COUNT)
            g_stepIndexSSB = 0;
        else if (g_stepIndexSSB < 0)
            g_stepIndexSSB = SSB_STEPS_COUNT - 1;

        showStep();
    }
    else { // --- Isolated logic for AM ---
        g_stepIndexAM = (v == 1) ? g_stepIndexAM + 1 : g_stepIndexAM - 1;

        // Large steps are restricted ONLY for LW/MW bands.
        // All steps are available for SW band.
        const uint8_t max_am_idx = (g_bandIndex == LW_BAND_TYPE || g_bandIndex == MW_BAND_TYPE)
            ? 3 // Max step is 10 kHz (index 3) for LW/MW
            : (AM_STEPS_COUNT - 1); // All 7 steps are available for SW

        if (g_stepIndexAM > max_am_idx)
            g_stepIndexAM = 0;
        else if (g_stepIndexAM < 0)
            g_stepIndexAM = max_am_idx;

        g_si4735.setFrequencyStep(g_tabStep[g_stepIndexAM]);
        g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;
        g_si4735.setSeekAmSpacing(g_tabStep[g_stepIndexAM]);
        showStep();
    }
}

// Calculates and sets the final BFO 
// it combines user tuning, calibration, and an automatic offset for CW mode
// final value is inverted as required by the IC
void updateBFO() {
    int16_t finalBfo = g_currentBFO + (g_Settings[SettingsIndex::BFO].param * 10);

    if (g_currentMode == CW) {
        if (g_Settings[SettingsIndex::CWSwitch].param == 1) { // 1 = USB
            finalBfo += CW_PITCH_OFFSET_HZ;
        }
        else { // 0 = LSB
            finalBfo -= CW_PITCH_OFFSET_HZ;
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

void agcSetFunc() {
    uint8_t att = g_Settings[SettingsIndex::ATT].param;
    uint8_t disableAgc = att > 0;
    uint8_t agcNdx;
    if (att > 1)
        agcNdx = att - 1;
    else
        agcNdx = 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

//Settings: Attenuation
void doAttenuation(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::ATT].param, 0, 37, v);
    agcSetFunc();
}

//Settings: Soft Mute
void doSoftMute(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::SoftMute].param, 0, 32, v);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
}

//Settings: Brightness
void doBrightness(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::Brightness].param, 1, 125, v);
    oled.setContrast(uint8_t(g_Settings[SettingsIndex::Brightness].param) * 2);
}

//Settings: SSB AVC Switch
void doSSBAVC(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::SVC].param, 0, 1, v);

    if (isSSB()) {
        g_si4735.setSSBAutomaticVolumeControl(g_Settings[SettingsIndex::SVC].param);
        applyBandConfiguration(true);
    }
}

//Settings: Automatic Volume Control
void doAvc(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::AutoVolControl].param, 12, 90, v);

    if (g_currentMode != FM)
        g_si4735.setAvcAmMaxGain(g_Settings[SettingsIndex::AutoVolControl].param);
}

//Settings: Sync switch
void doSync(int8_t v = 0) {
    bool wasSettingsActive = g_settingsActive;
    doSwitchLogic(g_Settings[SettingsIndex::Sync].param, 0, 1, v);

    if (isSSB()) {
        g_si4735.setSSBDspAfc(g_Settings[SettingsIndex::Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[SettingsIndex::Sync].param == 0 ? 0 : 3);  //Set Sync mode
        applyBandConfiguration(true);

        if (wasSettingsActive) {
            showSettingsTitle();
            showSettings();
        }
    }
}

//Settings: FM DeEmp switch (50 or 75)
void doDeEmp(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::DeEmp].param, 0, 1, v);

    if (g_currentMode == FM)
        g_si4735.setFMDeEmphasis(g_Settings[SettingsIndex::DeEmp].param == 0 ? 1 : 2);
}

//Settings: SW Units
void doSWUnits(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::SWUnits].param, 0, 1, v);
}

//Settings: SW Units
void doSSBSoftMuteMode(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::SSM].param, 0, 1, v);

    if (isSSB())
        g_si4735.setSSBSoftMute(g_Settings[SettingsIndex::SSM].param);
}

//Settings: SSB Cutoff filter
void doCutoffFilter(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::CutoffFilter].param, 0, 2, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::CPUSpeed].param, 0, 1, v);

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();
}

//Settings: BFO Offset calibration
void doBFOCalibration(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::BFO].param, -60, 60, v);

    if (isSSB()) {
#if USE_RDS
        setRDSConfig(g_Settings[SettingsIndex::BFO].param);
#endif
        updateBFO();
    }
}

//Settings: Tune Frequency Antenna Capacitor
void doUnitsSwitch(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::UnitsSwitch].param, 0, 1, v);
}

//Settings: Scan button switch
void doScanSwitch(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::ScanSwitch].param, 0, 1, v);
}

//Settings: CW mode switch
void doCWSwitch(int8_t v = 0) {
    doSwitchLogic(g_Settings[SettingsIndex::CWSwitch].param, 0, 1, v);

    if (g_currentMode == CW)
        applyBandConfiguration(true);
}

#if USE_RDS
//Settings: RDS Error Level
void doRDSErrorLevel(int8_t v) {
    doSwitchLogic(g_Settings[SettingsIndex::RDSError].param, 0, 3, v);

    if (g_currentMode == FM)
        setRDSConfig(g_Settings[SettingsIndex::RDSError].param);
}


void doRDS() {
    g_displayRDS = !g_displayRDS;

    if (g_displayRDS) {
        g_sMeterOn = false;
        oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
        g_si4735.getRdsStatus();
        showRDS();
    }
    else
        updateLowerDisplayLine();
}
#endif

//Prevents repeatable code for flash image size saving
void doBandwidthLogic(int8_t& bwIndex, uint8_t upperLimit, uint8_t v) {
    doSwitchLogic(bwIndex, 0, upperLimit, v);
    g_bandList[g_bandIndex].bandwidthIdx = bwIndex;
}

//Bandwidth regulation logic
void doBandwidth(uint8_t v) {
    if (isSSB()) {
        doSwitchLogic(g_bwIndexSSB, 0, g_bwSSBMaxIdx, v);
        g_si4735.setSSBAudioBandwidth(g_bwSSBIdx[g_bwIndexSSB]);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM) {
        doBandwidthLogic(g_bwIndexAM, g_maxFilterAM, v);
        g_bandList[g_bandIndex].bandwidthIdx = g_bwIndexAM;
        g_si4735.setBandwidth(g_bwAMIdx[g_bwIndexAM], 1);
    }
    else {
        doBandwidthLogic(g_bwIndexFM, 4, v);
        g_bandList[g_bandIndex].bandwidthIdx = g_bwIndexFM;
        g_si4735.setFmBandwidth(g_bwIndexFM);
    }
    showBandwidth();
}

void switchCommand(bool* b, void (*showFunction)()) {
    static bool* prev = NULL;
    static void (*prevFunc)() = NULL;

    if (!b) {
        if (prev) {
            *prev = false;
            if (prevFunc)
                prevFunc();
            g_lastAdjustmentTime = 0;
            prev = NULL;
        }
        return;
    }

    bool last = *b;
    prev = b;
    prevFunc = showFunction;

    if (*b == false) {
        g_cmdVolume = false;
        g_cmdStep = false;
        g_cmdBw = false;
        g_cmdBand = false;
        g_lastAdjustmentTime = millis();
        showVolume();
        showStep();
        showBandwidth();
        showModulation();
    }
    else
        g_lastAdjustmentTime = 0;

    *b = !last;

    if (showFunction)
        showFunction();
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

void doFrequencyTune() {
    g_seekDirection = g_encoderCount == 1 ? 1 : 0;

    //Update frequency
    g_previousFrequency = g_currentFrequency;  //Force EEPROM update
    if (g_currentMode == FM) {
        g_currentFrequency += g_tabStepFM[g_FMStepIndex] * g_encoderCount;
#if USE_RDS
        if (g_displayRDS)
            oledPrint(_literal_EmptyLine, 0, 6, DEFAULT_FONT);
#endif
    }
    else {
        // This correctly uses g_stepIndexAM to get the step in kHz
        g_currentFrequency += g_tabStep[g_stepIndexAM] * g_encoderCount;
    }

    uint16_t bMin = g_bandList[g_bandIndex].minimumFreq, bMax = g_bandList[g_bandIndex].maximumFreq;

    //Special logic for fast and responsive frequency surfing
    if (g_currentFrequency > bMax)
        g_currentFrequency = bMin;
    else if (g_currentFrequency < bMin)
        g_currentFrequency = bMax;

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    g_processFreqChange = true;
    g_lastFreqChange = millis();

    showFrequency();
}

void resetLowerLine() {
    if (g_sMeterOn || g_displayRDS) {
        g_sMeterOn = false;
        g_displayRDS = false;
        updateLowerDisplayLine();
    }
}

//Special feature to make SSB feel like on expensive TECSUN receivers
//BFO is now part of main frequency in SSB mode
void doFrequencyTuneSSB() {
    const int BFOMax = 16000;
    // This logic now correctly gets the step value in Hz, including the new kHz-equivalent steps
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
        agcSetFunc();  //Re-apply to remove noize
        g_currentFrequency = g_si4735.getFrequency();
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    }

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency + (g_currentBFO / 1000);
    g_lastFreqChange = millis();
    g_previousFrequency = 0;  //Force EEPROM update
    if (!clampSSBBand())      //If we move outside of current band - switch it
        showFrequency();
}

// ============== Refactor =============

// key process for all keys
void processButtonEvents() {
    uint8_t evt;

    // Lambda to cleanly exit the favorites menu
    auto exitFavorites = [&]() {
        g_favoritesActive = false;
        oled.clear();
        showStatus();
        };

    // --- Encoder Button ---
    evt = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        if (g_lastAdjustmentTime) {
            switchCommand(NULL, NULL); // Cancel any active command mode
        }
        else if (g_settingsActive) {
            g_SettingEditing = !g_SettingEditing;
            DrawSetting(g_SettingSelected, true);
        }
        else if (g_favoritesActive) {
            if (g_totalFavorites) {
                g_currentFrequency = g_fmFavorites[g_favoriteSelected].frequency;
                g_si4735.setFrequency(g_currentFrequency);
            }
            exitFavorites();
        }
        else if (isSSB() || !g_Settings[SettingsIndex::ScanSwitch].param) {
            switchCommand(&g_cmdStep, showStep);
            resetLowerLine();
        }
        else if (g_currentMode == FM || g_currentMode == AM) {
            doSeek();
        }
    }

    // --- Bandwidth (BW) Button ---
    evt = btn_Bandwidth.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        if (g_favoritesActive) {
            delFav();
            oled.clear();
            showFav();
        }
        else if (!g_settingsActive && g_currentMode != CW) {
            switchCommand(&g_cmdBw, showBandwidth);
        }
    }

    // --- Band Up Button ---
    evt = btn_BandUp.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        if (g_favoritesActive) {
            exitFavorites();
        }
        else if (g_settingsActive) {
            switchSettingsPage();
        }
        else {
            resetLowerLine();
            switchCommand(&g_cmdBand, showModulation);
        }
    }

    // --- Band Down Button ---
    evt = btn_BandDn.checkEvent(bandEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        if (g_favoritesActive) {
            exitFavorites();
        }
        else {
            if (!g_settingsActive) switchCommand(NULL, NULL);
            g_settingsActive = !g_settingsActive;
            switchSettings();
        }
    }

    // --- Volume Up Button ---
    evt = btn_VolumeUp.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive && !g_muteVolume) {
        switchCommand(&g_cmdVolume, showVolume);
    }

    // --- Volume Down Button (Mute) ---
    evt = btn_VolumeDn.checkEvent(volumeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_favoritesActive && !g_cmdVolume) {
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

    // --- AGC Button (Display On/Off, Sync) ---
    evt = btn_AGC.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt) {
        if (!g_settingsActive || (g_settingsActive && !g_displayOn)) {
            g_displayOn = !g_displayOn;
            g_displayOn ? oled.on() : oled.off();
        }
    }
    else if (BUTTONEVENT_LONGPRESS == evt && !g_settingsActive && !g_favoritesActive && isSSB()) {
        doSync(1);
    }

    // --- Step Button (S-Meter) ---
    evt = btn_Step.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive && !g_favoritesActive) {
        switchCommand(&g_cmdStep, showStep);
        resetLowerLine();
    }
    else if (BUTTONEVENT_LONGPRESSDONE == evt && !g_settingsActive && !g_favoritesActive) {
        g_sMeterOn = !g_sMeterOn;
        if (g_sMeterOn) {
            g_displayRDS = false;
            showSMeter();
        }
        else {
            updateLowerDisplayLine();
        }
    }

    // --- Mode Button (Favorites, Modulation) ---
    evt = btn_Mode.checkEvent(modeEvent);
    if (BUTTONEVENT_SHORTPRESS == evt && !g_settingsActive) {
        if (g_favoritesActive) {
            exitFavorites();
        }
        else if (g_currentMode == FM) {
            g_favoritesActive = true;
            g_favoriteSelected = 0;
            oled.clear();
            showFav();
        }
        else {
            // This block handles modulation switching between AM, LSB, USB, CW
            uint8_t bw = (g_currentMode == AM) ? g_bwIndexAM : g_bwIndexSSB;
            g_bandList[g_bandIndex].currentFreq = g_currentFrequency;

            // Save the correct step index to the band structure BEFORE changing the mode
            if (isSSB()) { // Includes LSB, USB, CW
                g_bandList[g_bandIndex].currentStepIdx = g_stepIndexSSB;
            }
            else { // AM
                g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;
            }

            if (g_currentMode == CW) safeAmpOff();

            // When switching from AM to SSB, the patch must be loaded.
            if (g_currentMode == AM) {
                loadSSBPatch();
                g_bwIndexSSB = bw;
                g_processFreqChange = false;
            }

            g_currentMode = (g_currentMode + 1) % 4; // AM -> LSB -> USB -> CW -> AM

            // When switching back to AM, unload SSB patch flag
            if (g_currentMode == AM) {
                g_ssbLoaded = false;
                g_bwIndexAM = bw;
            }

            applyBandConfiguration();

            if (!g_ssbLoaded && g_currentMode == AM) safeAmpOn();
        }
    }
    else if (BUTTONEVENT_LONGPRESSDONE == evt && !g_settingsActive && g_currentMode == FM && !g_favoritesActive) {
        addFav();
        oled.setCursor(45, 3);
        oled.print(F("SAVED"));
        delay(500);
        showFrequency(true);
    }
}

// main loop program in process order
void loop() {
    bool skip = false;
    bool recent = millis() - g_lastFreqChange < 70;

    // Handle favorites menu navigation with bounds checking
    if (g_favoritesActive) {
        if (g_encoderCount) {
            if (g_totalFavorites > 0) {
                g_favoriteSelected = (g_favoriteSelected + g_encoderCount + g_totalFavorites) % g_totalFavorites;
                showFav();
            }
            g_encoderCount = 0;
        }
        processButtonEvents();
        return;
    }

    if (g_processFreqChange && !isSSB()) {
        if (!recent && !g_encoderCount) {
            g_si4735.setFrequency(g_currentFrequency);
            g_processFreqChange = false;
        }
        else if (recent && g_encoderCount) {
            doFrequencyTune();
            g_encoderCount = 0;
            return;
        }
    }

    if (millis() - g_lastFreqChange >= 500) {
        if (g_sMeterOn && !g_settingsActive) showSMeter();
        showCharge(false);

        if (!g_settingsActive && millis() - g_lastRSSIUpdate >= 1000) {
            g_lastRSSIUpdate = millis();
            if (g_currentMode == FM) {
                g_si4735.getCurrentReceivedSignalQuality();
                uint8_t rssi = g_si4735.getCurrentRSSI();
                if (g_currentRSSI != rssi || g_currentRSSI == 255) {
                    g_currentRSSI = rssi;
                    showRSSI();
                }
                if (millis() > 3000 && !g_sMeterOn && !g_displayRDS) {
                    bool stereo = g_si4735.getCurrentPilot();
                    if (g_stereoStatus != stereo) {
                        g_stereoStatus = stereo;
                        updateStereoIndicator();
                    }
                }
            }
        }
    }

    if (g_lastAdjustmentTime && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT)
        switchCommand(NULL, NULL);

    if (g_encoderCount) {
        if (g_lastAdjustmentTime) g_lastAdjustmentTime = millis();

        if (g_settingsActive) {
            if (!g_SettingEditing) {
                int8_t prev = g_SettingSelected;
                g_SettingSelected += g_encoderCount;
                uint8_t page = g_SettingsPage - 1;
                uint8_t max = min((page * 6) + 5, SettingsIndex::SETTINGS_MAX - 1);
                if (g_SettingSelected < page * 6) g_SettingSelected = max;
                else if (g_SettingSelected > max) g_SettingSelected = page * 6;
                DrawSetting(prev, true);
                DrawSetting(g_SettingSelected, true);
            }
            else {
                (*g_Settings[g_SettingSelected].manipulateCallback)(g_encoderCount);
                DrawSetting(g_SettingSelected, false);
                delay(MIN_ELAPSED_TIME);
            }
        }
        else if (g_cmdVolume) doVolume(g_encoderCount);
        else if (g_cmdStep) doStep(g_encoderCount);
        else if (g_cmdBw) doBandwidth(g_encoderCount);
        else if (g_cmdBand) bandSwitch(g_encoderCount == 1);
        else if (isSSB()) { doFrequencyTuneSSB(); skip = true; }
        else { doFrequencyTune(); skip = true; }

        g_encoderCount = 0;
        resetEepromDelay();
    }

    if (!skip) processButtonEvents();

    if (g_currentFrequency != g_previousFrequency && (millis() - g_storeTime) > STORE_TIME) {
        saveAllReceiverInformation();
        g_storeTime = millis();
        g_previousFrequency = g_currentFrequency;
    }

#if USE_RDS
    if (g_displayRDS && !g_settingsActive && !g_favoritesActive)
        showRDS();
#endif
}

//Overriding original main to save some space
int main(void) {
    init();
    setup();
    while (1)
        loop();
    return 0;
}