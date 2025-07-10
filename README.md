### **ATS-20+ Firmware Modifications (diqezit's Fork)**

This repository is a fork of the original `goshante/ats20_ats_ex` firmware, dedicated to custom modifications, bug fixes, and new features. All discussion regarding these new versions should take place here.

Your feedback and suggestions are welcome!
Please put star to raise the firmware higher up on the list, thanks :)

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
------------------------------------------------------------------------------------------------------------

#### `MOD_NO_RDS v2`
*   **[Improved]** Separate tuning step management for AM/SSB, so adjustments in one mode do not affect the other.
*   **[Improved]** Battery level display now uses an interpolation formula for a more accurate and smooth percentage reading.
*   **[Fixed]** CW reception mode now uses an automatic BFO offset (`+/- 500 Hz`) for more intuitive tuning ([Issue #15](https://github.com/goshante/ats20_ats_ex/issues/15)).
*   **[New]** Smart SW Band ID displays amateur/broadcast band names (e.g., "40m," "20m") instead of a generic "SW."
*   
------------------------------------------------------------------------------------------------------------

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
*   
------------------------------------------------------------------------------------------------------------

#### `MOD_NO_RDS v3.2`
*   **[Improved]** "Snap-to-Grid" Tuning for AM/FM: The first encoder turn now automatically aligns the frequency to the nearest step grid point.
*   **[Reworked]** Display brightness control was replaced with a simple **1-10 scale** and a curve for more visually linear brightness steps.
*   **[New]** Configurable antenna capacitor: Added a menu setting to enable/disable the RF input capacitor to optimize performance with different antennas.
*   **[Fixed]** The attenuator system was set to manual-only to eliminate I2C-induced digital hum from the AUTO AGC mode.
*   
------------------------------------------------------------------------------------------------------------

#### `MOD_NO_RDS v3.3`
*   **[Optimized]** Flash size reduction: Reworked multiple functions, reducing the final firmware size by over 200 bytes.
*   **[Fixed]** BFO calculation bug that caused incorrect frequency display with negative BFO values.
*   **[Fixed]** FM bandwidth control: The encoder direction for FM bandwidth selection is now correct (clockwise increases bandwidth).
*   **[Changed]** The default ATT mode is now **AUTO** again.
*   **[Improved]** Code structure: Button processing logic was reworked into smaller, more organized functions to simplify debugging. The user-facing functionality remains identical.
*   
------------------------------------------------------------------------------------------------------------

#### `MOD_NO_RDS v3.4`

*   **[Fixed] Runaway Encoder Bug in Command Mode:** Solved a critical issue where turning the encoder while in `CMD_BAND` (or other command modes) could cause a continuous, unstoppable loop of band/step/bw     changes. The main loop and encoder processing logic have been reworked to correctly handle state and prevent this bug.
*   **[Fixed] FM Band Stability:** Resolved a major bug that caused the receiver to freeze or display invalid data (`655.35 MHz`, incorrect step/bw) when switching to or using the FM band. This was traced to an incorrect EEPROM reset procedure.
*   **[Fixed] SSB Step Persistence:** Corrected an issue where the SSB tuning step was being reset to its default value every time the mode was activated. The step size is now correctly preserved for each band.
*   **[Improved] FM Band UI:** The on-screen text inversion for `CMD_BAND` mode now works correctly for the FM band, inverting the top-line "FM" text instead of a non-existent bottom-line name.
*   **[Fixed] RSSI Display on FM:** Corrected the logic in the periodic task handler to ensure the RSSI value is now reliably displayed and updated while on the FM band.

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.0**
`(!) Attention: This version introduces a new EEPROM data structure and still in free-test mode. 
	All saved settings, including volume and FM favorites (etc), will be reset to factory defaults upon the first boot.`

*   **[New] Per-Band State Saving:** The receiver now remembers the last used frequency, tuning step, and bandwidth for each of the 28 individual bands (LW, MW, all SW sub-bands, FM). 
	This state is automatically saved and persists after power-off.
	
*   **[New] Seamless Band Coverage:** The band list has been re-architected to provide continuous tuning from 150 kHz to 30 MHz. 
	"Gaps" between traditional bands are now filled with generic "SW" segments, allowing exploration of the entire spectrum.
	
*   **[Changed] Unified Band Switching:** The band switching logic is now universal for all bands. 
	Pressing `Band Up/Down` cycles sequentially through the entire list (e.g., from "10m" to "FM" and from "LW" to "FM").
	
*   **[Fixed] SSB State Persistence:** The tuning step for SSB mode is no longer reset when switching from AM, allowing custom step sizes to be saved per band. 	
	The bandwidth setting is also now intelligently carried over between AM and SSB modes.
	
*   **[Fixed] Amplifier Pop/Click:** Resolved the audible pop when switching between FM and AM/SW bands by implementing a correct power-down/power-up sequence for the audio amplifier.

*   **[Fixed] Multiple UI & Tuning Bugs:** Addressed critical bugs that caused system freezes, display of invalid data (`65535 MHz`), 
	and incorrect band boundary behavior during the refactoring process.
	
*   **[Refactor] Brightness Control:** Replaced the static look-up table (LUT) for brightness with an on-the-fly calculation. 
	The new implementation uses optimized fixed-point integer math to generate a perceptually linear gamma curve, embedding the logic directly into the function.
	
*   **[Improved] EEPROM Protection:** Removed the aggressive 10-second save timer. 
	System now saves state after 30 seconds of tuning inactivity, or immediately upon switching bands and modes.
	
*   **[Optimized] FM Favorites Saving:** Disabled immediate saving on each change. Favorites list is now written to memory in a single batch upon exiting the menu.

*   **[Optimized] UI Event Handling:** Refactored encoder button handler (handleEncoderButton) to use a switch-case on a computed context.

*   **[Optimized]** Refactored `showFrequency` to simplify the logic for calculating the decimal point position, slightly reducing Flash and stack usage.

*   **[Optimized] Bandwidth String Storage:** Replaced the system of multiple individual strings and pointer tables for bandwidth labels with a single packed string array (`bw_all_data`) and compact index tables.
	This change reduces Flash memory usage by eliminating redundant null-terminators and pointer overhead.
		
*   **[Refactor] Safe Band Name Handling:** Introduced a dedicated helper function (`getBandName`) to safely retrieve band names. 
	Corrects a potential vulnerability where using `PACK_STR4` macro could lead to buffer over-reads.

*   **[New] Smart Power Management (AGC Button):** The `AGC` button now toggles the display and automatically reduces CPU speed to 8MHz when the screen is off to conserve power.

*   **[Optimized] Flash Savings:** Replaced expensive division/modulo math in the FM Favorites menu with a lightweight (`sw_div`)
	
	
*   [Global BFO calibration]: How is a compromise. 
    If your receiver has a different frequency drift on 160 meters than on 10 meters, you will only be able to calibrate one band perfectly. 
    The others will have a margin of error that you can adjust to suit you.


------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.1**

`(!) Attention: This version has major stability update.`

*   **[Fixed & Improved] Seamless Wide-Band Seek:** Reworked the seek function. It now starts from the current frequency (not the band edge) and scans seamlessly across all AM/SW/LW bands. 
	This also fixes a critical bug that could corrupt saved band-states after an interrupted seek

*   **[Fixed] BFO State Logic:** BFO offset is now correctly reset to 0 upon switching bands, preventing fine-tuning adjustments from one station from incorrectly carrying over to another

*   **[Hardening] System Stability:** Added a safety check on boot to validate the `CPUSpeed` setting from EEPROM, preventing a potential device hang from corrupted data

*   **[Optimized] Critical Flash Savings:** Freed over 250 bytes by replacing expensive 32-bit and division/modulo math (`splitFreq`) 
	with 16-bit. This was the key to fitting all new fixes.
	
------------------------------------------------------------------------------------------------------------
	

### **MOD_NO_RDS v4.2**

`(!) Attention: This version includes code optimizations.`

*   **[Optimized] `convertToChar` Rework:** Сhange alone freed up **32 bytes** of critical flash memory and improved rendering performance.

*   **[Optimized] `ilen` Function:** The digit-length measurement function (`ilen`) refactored by compact logic and reducing firmware size.

*   **[Optimized] `doSeek()`: Reworked the FM frequency rounding logic to use a more direct modulo operation, saving 6 bytes of flash memory.

*   **[Optimized] Settings Handlers:** Unified 9 duplicate toggle functions into a single `toggleSetting()`, saving 40 bytes. 
	Combined with previous binary toggle optimization - total saved: **62 bytes**.
	
*   **[Optimized] `SettingParamToUI`:** Refactored to eliminate duplicate code paths, saving **22 bytes**.

*   **[Optimized] Button Handlers Refactoring:**
    *   `handleEncoderButton()`: Refactored logic to a more efficient "Guard Clause" style, saving **14 bytes**.
    *   `handleModeButton()`: Refactored logic by adding a "Guard Clause" to eliminate redundant checks, saving **8 bytes**.
    *   `handleFavoritesMenuButtons()`: Refactored by removing temporary variables, saving **20 bytes**.
    *   `processEncoderForSettings()`: Replaced the `min()` macro with a direct ternary operator, saving **4 bytes**.
    *   Centralized button processing logic to eliminate redundant checks, improving code structure and saving **4 bytes**.
	
*   **[Refactored] RSSI Subsystem Overhaul:**
    *   **[Feature]** Added a universal signal quality indicator (RSSI) for all modes.
    *   **[Fix]** Implemented a flag-triggered update mechanism for the AM/SSB RSSI meter to eliminate audio artifacts during idle listening while ensuring value updates after user interaction.
    *   **[Optimized]** Overhauled RSSI logic for AM/SSB modes, saving an additional **74 bytes**. The feature is now fully disabled when set to 'Off', improving performance.


------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v4.3**

`(!) Attention: This version includes code optimizations and a critical bug fix in SSB mode.`

*   **[Fixed] Critical SSB Tuning Bug:** Corrected an issue where the hardware tuner would get "stuck" during SSB tuning. 
	`doFrequencyTuneSSB` now correctly sends a `setFrequency()` command to the chip after a BFO rollover.

*   **[Fixed] RSSI Display Overflow:** Corrected a UI layout bug where RSSI values of 100 or more would break the display. 
	Value now capped at 99 within the `updateSignalQuality` function before being rendered

*   **[Refactored] EEPROM I/O Subsystem:** Centralized all EEPROM functions and optimized logic with `inline` helpers, saving **46 bytes** of firmware space

*   **[Refactored] Code Structure:** Grouped related functions into `Subsystem` blocks for improved readability

*   **[Optimized] `showFav()`:** Extracted item drawing logic into an `inline` helper to reduce Flash size


------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v4.4**

`(!) Attention: This version includes significant stability fixes, code refactoring, and flash memory optimizations.`

*   **[Fixed & Improved] Core Receiver Stability:**
    *   **CW Sideband Switching:** Corrected a major regression in `doCWSwitch` that caused signal loss and BFO resets. Full functionality from v3.4 is now restored.
    *   **AM RSSI Stability:** Implemented a new sampling strategy for AM RSSI to provide a stable on-screen value and track slow fading. 
		The value is now read once after a 1-second AGC settle time and then refreshed periodically (in 10 seconds), minimizing noise during active listening.

*   **[Optimized] Flash Memory & Code Structure:**
    *   **Major Optimization:** Removed a redundant API call (`setSeekAmSpacing`) from the AM configuration routine, resulting in a **38-byte** flash savings
    *   **Arithmetic & Logic:** Replaced `volumeUp/Down` calls with direct arithmetic in `doVolume()` (saving 14 bytes), replaced costly boolean arithmetic in `handleAgcButton()` (saving 4 bytes), and used a manual `abs()` in `updateStablePercent()` (saving 4 bytes).
    *   **Code Consolidation:** Eliminated duplicate function calls within `doStep()` and other minor logic blocks, saving an additional 10 bytes.

*   **[Refactored] Code Readability:**
    *   The main periodic task handler (`handlePeriodicTasks`) was split in inline helpers (`handleSignalAndStereoUpdates`, `handleSettingsSave`, etc.).
    *   Signal quality logic was separating into `getAmSignalValue` and `getFmSignalValue` helpers too.


------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v4.5**

`(!) Attention: Continues optimization effort, focusing on low-level code refinement to free up critical Flash memory.`

*   **[Optimized] Flash Memory & Code Structure:**
    *   **Frequency Display Overhaul (32 bytes):** rewrote `showFrequency` function to resolve a critical logic bug where the `SWUnits=MHz` setting incorrectly affected
  MW/LW band display.
    *   **Direct Display Rendering (26 bytes):** Refactored the `showChargeOnDisplay` function to print the battery percentage directly to the display,
  eliminating need for temporary string buffer and `convertToChar` helper. **26-byte** flash savings.
    *   **Specialized Logic (8 bytes):** Replaced generic `doSwitchLogic` function with `toggleSetting` call in `doCWSwitch` for binary (LSB/USB) switching.

*   **[UI] Display Layout Realignment:**
    *   Re-arranged on-screen indicators (Band/Step top, Mode/BW bottom) to align with physical button positions.
    *   Relocated RSSI indicator to the bottom row and adjusted all element X-coordinates to resolve text overlap with long values (e.g., `160m`, `Step:100k`).


![AM_MODE](https://github.com/user-attachments/assets/6a0405aa-bffc-4cf9-940a-ea39cfad03d2)

![FM_MODE](https://github.com/user-attachments/assets/c0d6a85d-cabf-4a38-bb76-36ddd5e49711)

------------------------------------------------------------------------------------------------------------



### **MOD_NO_RDS v4.6 (Final Release)**

`(!) Attention: Settings have been re-ordered. An EEPROM reset will occur on first boot.`

*   **[New] Quick Sync Toggle:** Long-pressing the `MODE` button in any SSB/CW mode now toggles the `Sync` setting, 
	providing fast access to this key feature without entering the menu, as was previously realized in AGC button.

*   **[New] Robust Seeking:** Seek function timeout has been extended to its maximum practical value (~32 seconds) 
	to ensure complete band scans without premature termination on quiet bands. User action is now the only way to stop a seek.

*   **[Reworked] Main Screen UI & Rendering:**
    *   Volume indicator is now right-aligned and includes an integrated separator (`'|'`) for a more consistent layout with the step parameter.
    *   Fixed a bug where the separator was not cleared correctly after changing tuning steps.
    *   Corrected an issue where only the volume value, not the separator, was highlighted (inverted) during editing.

*   **[UI] Settings Menu Rework:**
    *   Re-ordered and sorted all settings into logical groups (like General, SSB/CW, Hardware) for faster navigation.

*   **[Fixed] Stale RSSI Display:**
    *   RSSI value is now correctly cleared on mode switch (e.g., from FM to AM), preventing "ghost" values from being displayed.

*   **[Optimized] `showStep` Function (-20 bytes):**
    *   Rewrote step display logic, which also fixed a bug with incorrect CW steps and resulted in a **20-byte flash saving.**

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.7**

*   **[Optimized] `calculateRawPercent` Function (-14 bytes):**
    *   Rewritten for maximum flash efficiency using a compressed data `struct` (8-bit offsets).
    *   Hardcoded min/max edge-case values to eliminate runtime calculations.
    *   Downgraded interpolation math to `uint16_t`, achieving a **14-byte total saving.**

*   **[Fixed] IIR Filter Initialization:**
    *   Corrected the IIR filter logic for battery ADC readings to properly handle its uninitialized state (`-1`) on first boot, 
		ensuring stable and predictable readings from the start.

*   **[Optimized] AM Mode Step Handling (-4 bytes):**
    *   Refactored the `doStep` function to eliminate a redundant array lookup when setting the frequency and seek spacing for AM modes.
    *   This optimization streamlines register usage and results in a **4-byte flash saving.**

*   **[Fixed] VFO Tuning Logic at Band Edges:**
    *   Refactored `doFrequencyTune()`, `bandSwitch()`, and `performBfoRolloverWithBandCheck()` to prevent frequency jumps when tuning across band boundaries with the encoder.
    *   Tuning now rolls over seamlessly (e.g., 3200->3201 kHz like before v4.x), ensuring continuous VFO operation in all modes (AM/SSB/CW). 
		Jumping to a band's stored frequency is now isolated to the `Band+` function.
	
*   [Fixed/Optimized] `showFrequency()` Rendering (-20 bytes): Stabilized the display rendering during rapid tuning to prevent freezes. 
	This was achieved by refactoring core logic into helper functions, which also improves code clarity.
	
*   [Optimized] Step & Volume UI (-30 bytes): Replaced dynamic step generation with a PROGMEM lookup table (`step_lookup_table`), 
	saving flash space. Volume display was updated for consistent UI style.

------------------------------------------------------------------------------------------------------------


Thank you all for your participation and feedback
