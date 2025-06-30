# ATS-20+ Firmware Modifications (diqezit's Fork)

This repository is a fork of the original `goshante/ats20_ats_ex` firmware, dedicated to custom modifications, bug fixes, and new features. All discussion regarding these new versions should take place here.

Your feedback and suggestions are welcome!

You can find the original project here: [goshante/ats20_ats_ex](https://github.com/goshante/ats20_ats_ex)

---

## Key Modifications

This fork introduces several key improvements over the original firmware:

*   **Audio Pop/Click Elimination:** A hardware and software mod to provide pop-free audio when switching modes (requires a minor physical modification).
*   **`MOD_NO_RDS` Firmware Series:** A complete firmware evolution that removes RDS to add an FM Favorites list and builds upon that with dozens of fixes and new features.

---

## Audio Pop/Click Elimination Mod

This modification completely eliminates the audio pops that occur when switching modes or bands. It requires a minor hardware change to connect the MD8002A amplifier's shutdown pin to the Arduino.

**This improvement only affects the speaker output**, as the headphone jack is connected before the amplifier.

![Diagram showing how to connect the pins](https://github.com/user-attachments/assets/681c515e-bbb1-4213-845b-1d2d178f52b2)

#### Main Changes:
*   **Hardware Muting:** Implemented hardware muting by controlling the MD8002A amplifier's shutdown pin (`A3`).
*   **Safe Amplifier Control:** Added functions to smoothly mute and unmute the speaker during operations that cause clicks.
*   **Memory Optimizations:** Moved bandwidth strings to PROGMEM, saving ~100 bytes of RAM and fixing display freezes.
*   **Added FM Stereo Indicator:** An asterisk `*` now appears when an FM stereo pilot tone is detected.
*   **Dynamic RSSI:** Added a dynamic RSSI indicator for FM mode.

![Image of the mod in action](https://github.com/user-attachments/assets/ea14916d-9369-43f2-b8e3-53cad67bef9d)
![Another image](https://github.com/user-attachments/assets/2aaa1542-ba01-4707-9ea2-e518e22eac6f)

---

## `MOD_NO_RDS` Firmware Series

This is an alternative firmware branch that **disables RDS** to implement an **FM station favorites list** and includes numerous other fixes and improvements. Below is the full changelog.

### `MOD_NO_RDS` (Initial Version)

This version introduces the ability to save your favorite FM stations.

#### Controls
*   **Open Favorites Menu:** Short press **MODE** button in FM mode.
*   **Navigate List:** Rotate encoder.
*   **Select Station:** Short press encoder.
*   **Delete Station:** Press **BW** button.
*   **Exit Menu:** Press **BAND UP/DOWN** or **MODE**.
*   **Save Current Station:** Long press **MODE** button.

#### Fixes & Extras
*   **Fixed FM Seek:** Corrected command conflicts and seek steps that caused audio drop-outs ([Issue #46](https://github.com/goshante/ats20_ats_ex/issues/46)).
*   **Fixed AM Seek Grid:** Aligned the 9 kHz AM grid for correct step intervals ([Issue #44](https://github.com/goshante/ats20_ats_ex/issues/44)).
*   **Memory Savings:** Simplified battery calculation and removed redundant functions to free up Flash memory.

![Image of the favorites menu](https://github.com/user-attachments/assets/f739a0fd-ab2a-4b07-ae84-2e2c5e6e79dc)
![Favorites menu screen](https://github.com/user-attachments/assets/34e9151e-504d-44f8-b910-d4e66e8fa439)

### `MOD_NO_RDS v2`

*   **(Reworked) Tuning Step Management:** Created separate step settings for AM/SSB, so adjustments in one mode do not affect the other.
*   **(Improved) Battery Level Display:** Uses an interpolation formula for a more accurate and smooth battery percentage reading.
*   **(Fixed) CW Reception Mode:** Introduced an automatic BFO offset (`+/- 500 Hz`) for more intuitive CW tuning ([Issue #15](https://github.com/goshante/ats20_ats_ex/issues/15)).
*   **(New) Smart SW Band ID:** Displays amateur/broadcast band names (e.g., "40m", "20m") instead of a generic "SW".

### `MOD_NO_RDS v3`

**(!) Attention:** This version was a major refactoring and should be considered experimental.

*   **(Changed) Context-Dependent Settings:** Tuning step, bandwidth, and gain are now saved independently for each mode (AM/SSB/FM) and band.
*   **(Improved) Battery Monitoring System:** Redesigned with an IIR filter and hysteresis for a stable, flicker-free reading.
*   **(Fixed) EEPROM Reliability:** Implemented a versioning system to automatically reset settings after a firmware update, preventing freezes.
*   **(Added) Animated Loading Indicator** on the startup screen.

### `MOD_NO_RDS v3.1`

*   **(Reworked) Main `loop()` Function:** Restructured the main program loop for improved stability and easier maintenance.
*   **(Fixed) Inverted BFO Logic in CW Mode:** Corrected a bug that swapped LSB/USB sidebands.
*   **(Improved) Expanded BFO Calibration Range:** Increased the range to **+/- 2.5 kHz** to compensate for significant crystal drift.
*   **(Fixed) Prevented BFO Hardware Overrun:** Reduced the internal BFO range to prevent exceeding the Si4732's hardware limits.

### `MOD_NO_RDS v3.2`

*   **(Improved) "Snap-to-Grid" Tuning for AM/FM:** The first encoder turn now automatically aligns the frequency to the nearest step grid point.
*   **(Reworked) Display Brightness Control:** Replaced with a simple **1-10 scale** and a curve for more visually linear brightness steps.
*   **(New) Configurable Antenna Capacitor:** Added a menu setting to enable/disable the RF input capacitor to optimize performance with different antennas.
*   **(Fixed) Attenuator System:** Set the attenuator to manual-only to eliminate I2C-induced digital hum from the AUTO AGC mode.

### `MOD_NO_RDS v3.3`

*   **(Optimized) Flash Size Reduction:** Reworked multiple functions, reducing the final firmware size by over 200 bytes.
*   **(Fixed) BFO Calculation:** Corrected a bug that caused incorrect frequency display with negative BFO values.
*   **(Fixed) FM Bandwidth Control:** The encoder direction for FM bandwidth selection is now correct (clockwise increases bandwidth).
*   **(Changed) Default ATT Mode:** The AUTO mode for attenuation is now the default setting again.
*   **(Improved) Code Structure:** Reworked button processing logic into smaller, more organized functions to simplify debugging. The user-facing functionality remains identical.

---

Thank you all for your participation and feedback!
