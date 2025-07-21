#pragma once

// ------------------------------------------
// ------- Battery Monitoring Subsystem -----
// ------------------------------------------

// --- HARDWARE REQUIREMENT ---
// For battery level display, the BATTERY_VOLTAGE_PIN (look in defs.h) must be
// connected to the midpoint of a voltage divider made of two 10kΩ resistors
//
//   VCC (PREFER after power switch!) --- [10kΩ] --- (BATTERY_VOLTAGE_PIN) --- [10kΩ] --- GND
//
// The firmware is calibrated specifically for this 1:2 divider
// Using other resistor values will result in inaccurate battery readings


#if ENABLE_BATTERY_MONITOR
uint8_t g_stableBatteryPercent = 100;

// key points of Li-Ion discharge curve and pre-calculated ranges forinterpolation formula
#define BATT_ADC_FULL        647 // fully charged battery                               (4.15V)
#define BATT_ADC_SHOULDER    616 // initial voltage drop flattens out                   (3.95V)
#define BATT_ADC_MID         577 // in the middle of the long flat discharge plateau    (3.70V)
#define BATT_ADC_KNEE        530 // plateau ends and voltage plummets                   (3.40V)
#define BATT_ADC_EMPTY       491 // battery is empty                                    (3.15V)

#define BATT_PCT_AT_SHOULDER 82  // % capacity SHOULDER voltage
#define BATT_PCT_AT_MID      47  // % capacity MID voltage
#define BATT_PCT_AT_KNEE     7   // % capacity KNEE voltage

#define PCT_DELTA_TOP_DROP      (100 - BATT_PCT_AT_SHOULDER)
#define ADC_DELTA_TOP_DROP      (BATT_ADC_FULL - BATT_ADC_SHOULDER)
#define PCT_DELTA_UPPER_PLATEAU (BATT_PCT_AT_SHOULDER - BATT_PCT_AT_MID)
#define ADC_DELTA_UPPER_PLATEAU (BATT_ADC_SHOULDER - BATT_ADC_MID)
#define PCT_DELTA_LOWER_PLATEAU (BATT_PCT_AT_MID - BATT_PCT_AT_KNEE)
#define ADC_DELTA_LOWER_PLATEAU (BATT_ADC_MID - BATT_ADC_KNEE)
#define PCT_DELTA_FINAL_DROP    (BATT_PCT_AT_KNEE - 0)
#define ADC_DELTA_FINAL_DROP    (BATT_ADC_KNEE - BATT_ADC_EMPTY)

#define BATT_DISPLAY_UPDATE_INTERVAL_MS 10000

// https://github.com/diqezit/ats20_ats_ex/issues/16

// useful capacity estimate by modeling the battery non-linear discharge curve
static inline uint8_t calculateRawPercent(uint16_t adc_value) {
    if (adc_value >= BATT_ADC_FULL)  return 100;
    if (adc_value <= BATT_ADC_EMPTY) return 0;

    if (adc_value > BATT_ADC_SHOULDER) {
        return BATT_PCT_AT_SHOULDER + ((uint16_t)(adc_value - BATT_ADC_SHOULDER) * PCT_DELTA_TOP_DROP) / ADC_DELTA_TOP_DROP;
    }
    if (adc_value > BATT_ADC_MID) {
        return BATT_PCT_AT_MID + ((uint16_t)(adc_value - BATT_ADC_MID) * PCT_DELTA_UPPER_PLATEAU) / ADC_DELTA_UPPER_PLATEAU;
    }
    if (adc_value > BATT_ADC_KNEE) {
        return BATT_PCT_AT_KNEE + ((uint16_t)(adc_value - BATT_ADC_KNEE) * PCT_DELTA_LOWER_PLATEAU) / ADC_DELTA_LOWER_PLATEAU;
    }
    return 0 + ((uint16_t)(adc_value - BATT_ADC_EMPTY) * PCT_DELTA_FINAL_DROP) / ADC_DELTA_FINAL_DROP;
}

#if ENABLE_ADVANCED_BATTERY_LOGIC
static uint8_t g_percentChangeCounter = 0;
static int16_t g_averageADC = -1;

static inline void applyPercentUpdate(uint8_t newPercent) {
    g_stableBatteryPercent = newPercent;
    g_percentChangeCounter = 0;
}
#endif

// Update internal stable battery percentage
static inline void updateStablePercent() {
    if (!g_voltagePinConnnected) return;

    int sample = analogRead(BATTERY_VOLTAGE_PIN);

    if (sample <= 0) sample = BATT_ADC_EMPTY; // if disconnected

#if ENABLE_ADVANCED_BATTERY_LOGIC
    if (g_averageADC == -1) g_averageADC = sample;

    // filter to prevent the displayed percentage from jumping around
    g_averageADC = (7 * g_averageADC + sample) >> 3;

    uint8_t currentRawPercent = calculateRawPercent(g_averageADC);
    int8_t diff = currentRawPercent - g_stableBatteryPercent;

    // hysteresis to prevent flickering and handle normal discharge
    if (diff == 0) {
        g_percentChangeCounter = 0;
    } else if (diff > 2 || diff < -2) {
        applyPercentUpdate(currentRawPercent);
    } else if (++g_percentChangeCounter >= 5) {
        applyPercentUpdate(currentRawPercent);
    }
#else
    g_stableBatteryPercent = calculateRawPercent(sample);
#endif
}

// Public interface for the battery monitoring subsystem. Updates the internal
// state and shows it on the display if the timer has elapsed or if forced.
void updateAndShowBattery(bool forceShow) {
    if (!g_voltagePinConnnected) return;
    updateStablePercent();
    static uint32_t lastChargeShow = 0;
    if ((millis() - lastChargeShow) > BATT_DISPLAY_UPDATE_INTERVAL_MS || forceShow) {
        showChargeOnDisplay();
        lastChargeShow = millis();
    }
}
#endif
