### **ATS-20+ Firmware Modifications (diqezit's Fork)**

This repository is a fork of the original `goshante/ats20_ats_ex` firmware, dedicated to custom modifications, bug fixes, and new features. All discussion regarding these new versions should take place here.

Your feedback and suggestions are welcome!

You can find the original project here: [goshante/ats20_ats_ex](https://github.com/goshante/ats20_ats_ex)

---

### **Key Modifications**

This fork introduces two main branches of improvements over the original firmware:

1.  **Audio Pop/Click Elimination (Hardware & Software Mod):** A modification that completely removes pops and clicks when switching modes. It requires a minor physical change to the receiver's circuit.
2.  **`MOD_NO_RDS` Firmware Series:** An alternative firmware branch where the RDS feature was removed to free up space for new functionality, including an FM Favorites menu, along with dozens of other fixes and improvements.

---

### **1. Audio Pop/Click Elimination Mod**

This modification completely eliminates the audio pops that occur when switching modes or bands. It requires a minor hardware change to connect the MD8002A amplifier's shutdown pin to the Arduino.

**Important:** This improvement only affects the speaker output, as the headphone jack is connected before the amplifier.

![Diagram showing how to connect the pins](https://github.com/user-attachments/assets/681c515e-bbb1-4213-845b-1d2d178f52b2)

#### Main Software Changes:
*   **Hardware Muting:** Implemented hardware muting by controlling the MD8002A amplifier's shutdown pin (`A3`).
*   **Safe Amplifier Control:** Added functions to smoothly mute and unmute the speaker during operations that cause clicks.
*   **Memory Optimization:** Moved bandwidth strings to PROGMEM, saving ~100 bytes of RAM and fixing display freezes.
*   **FM Stereo Indicator:** An asterisk `*` now appears when an FM stereo pilot tone is detected.
*   **Dynamic RSSI:** Added a dynamic RSSI indicator for FM mode.

![Image of the mod in action](https://github.com/user-attachments/assets/ea14916d-9369-43f2-b8e3-53cad67bef9d)
![Another image](https://github.com/user-attachments/assets/2aaa1542-ba01-4707-9ea2-e518e22eac6f)

---

### **2. `MOD_NO_RDS` Firmware Series**

This is an alternative firmware branch that **disables RDS** to implement an **FM station favorites list** and includes numerous other fixes and improvements. Below is the full changelog.

#### `MOD_NO_RDS` (Initial Version)

This version introduces the ability to save your favorite FM stations.

**Controls:**
*   **Open Favorites Menu:** Short press the **MODE** button in FM mode.
*   **Save Current Station:** Long press (hold) the **MODE** button.
*   **Navigate List:** Rotate the encoder.
*   **Select Station:** Short press the encoder.
*   **Delete Station:** Press the **BW** button.
*   **Exit Menu:** Press **BAND UP/DOWN** or **MODE**.

**Fixes & Extras:**
*   **[Fixed]** FM Seek: Corrected command conflicts and seek steps that caused audio drop-outs ([Issue #46](https://github.com/goshante/ats20_ats_ex/issues/46)).
*   **[Fixed]** AM Seek Grid: Aligned the 9 kHz AM grid for correct step intervals ([Issue #44](https://github.com/goshante/ats20_ats_ex/issues/44)).
*   **[Optimized]** Memory Savings: Simplified battery calculation and removed redundant functions to free up Flash memory.

![Image of the favorites menu](https://github.com/user-attachments/assets/f739a0fd-ab2a-4b07-ae84-2e2c5e6e79dc)
![Favorites menu screen](https://github.com/user-attachments/assets/34e9151e-504d-44f8-b910-d4e66e8fa439)

#### `MOD_NO_RDS v2`
*   **[Improved]** Separate tuning step management for AM/SSB, so adjustments in one mode do not affect the other.
*   **[Improved]** Battery level display now uses an interpolation formula for a more accurate and smooth percentage reading.
*   **[Fixed]** CW reception mode now uses an automatic BFO offset (`+/- 500 Hz`) for more intuitive tuning ([Issue #15](https://github.com/goshante/ats20_ats_ex/issues/15)).
*   **[New]** Smart SW Band ID displays amateur/broadcast band names (e.g., "40m," "20m") instead of a generic "SW."

#### `MOD_NO_RDS v3`
**(!) Attention:** This version was a major refactoring and should be considered experimental.

*   **[Changed]** Context-dependent settings: Tuning step, bandwidth, and gain are now saved independently for each mode (AM/SSB/FM) and band.
*   **[Improved]** The battery monitoring system was redesigned with an IIR filter and hysteresis for a stable, flicker-free reading.
*   **[Fixed]** EEPROM reliability: Implemented a versioning system to automatically reset settings after a firmware update, preventing freezes.
*   **[New]** Animated loading indicator on the startup screen.

#### `MOD_NO_RDS v3.1`
*   **[Reworked]** The main `loop()` function was restructured for improved stability and easier maintenance.
*   **[Fixed]** Inverted BFO logic in CW mode, which caused a swap of LSB/USB sidebands.
*   **[Improved]** Expanded BFO calibration range to **+/- 2.5 kHz** to compensate for significant crystal drift.
*   **[Fixed]** Prevented BFO hardware overrun by reducing the internal BFO range to stay within the Si4732's hardware limits.

#### `MOD_NO_RDS v3.2`
*   **[Improved]** "Snap-to-Grid" Tuning for AM/FM: The first encoder turn now automatically aligns the frequency to the nearest step grid point.
*   **[Reworked]** Display brightness control was replaced with a simple **1-10 scale** and a curve for more visually linear brightness steps.
*   **[New]** Configurable antenna capacitor: Added a menu setting to enable/disable the RF input capacitor to optimize performance with different antennas.
*   **[Fixed]** The attenuator system was set to manual-only to eliminate I2C-induced digital hum from the AUTO AGC mode.

#### `MOD_NO_RDS v3.3`
*   **[Optimized]** Flash size reduction: Reworked multiple functions, reducing the final firmware size by over 200 bytes.
*   **[Fixed]** BFO calculation bug that caused incorrect frequency display with negative BFO values.
*   **[Fixed]** FM bandwidth control: The encoder direction for FM bandwidth selection is now correct (clockwise increases bandwidth).
*   **[Changed]** The default ATT mode is now **AUTO** again.
*   **[Improved]** Code structure: Button processing logic was reworked into smaller, more organized functions to simplify debugging. The user-facing functionality remains identical.

---

Thank you all for your participation and feedback
