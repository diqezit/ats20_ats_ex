
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

### **MOD_NO_RDS v1.0.0**

---

## Added

- **FM station favorites list**
  - Compile-time flag: RDS disabled to free memory for favorites storage
  - Save up to 10 favorite FM stations with frequency and name
  - Favorites menu accessible via short press **MODE** in FM mode
  - Save current station via long press **MODE**
  - Navigate list with encoder rotation
  - Select station with short encoder press
  - Delete station with **BW** button
  - Exit menu via **BAND UP/DOWN** or **MODE**

---

## Fixed

- **FM Seek audio drop-outs**
  - Corrected command conflicts and seek steps that caused audio interruptions during FM seek
  - ([Issue #46](https://github.com/goshante/ats20_ats_ex/issues/46))

- **AM Seek 9 kHz grid alignment**
  - Aligned the 9 kHz AM grid for correct step intervals
  - ([Issue #44](https://github.com/goshante/ats20_ats_ex/issues/44))

---

## Optimizations

- **Memory savings**
  - Simplified battery calculation logic
  - Removed redundant functions to free up Flash memory

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v2.0.0**

---

## Added

- **Smart SW Band ID display**
  - Displays amateur/broadcast band names (e.g., `40m`, `20m`) instead of generic `SW`
  - Automatic detection based on current frequency

---

## Fixed

- **CW reception BFO offset**
  - CW mode now uses automatic BFO offset (`+/- 500 Hz`) for more intuitive tuning
  - ([Issue #15](https://github.com/goshante/ats20_ats_ex/issues/15))

---

## Changed

- **Separate tuning step management for AM/SSB**
  - AM and SSB modes now maintain independent tuning step settings
  - Adjustments in one mode no longer affect the other

- **Battery level display accuracy**
  - Now uses an interpolation formula for more accurate and smooth percentage reading
  - Replaces previous stepped/threshold-based calculation

------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v3.0.0**

**(!) Attention:** This version was a major refactoring and should be considered experimental.

---

## Added

- **Animated loading indicator**
  - Displays on the startup screen during initialization

---

## Fixed

- **EEPROM reliability**
  - Implemented a versioning system to automatically reset settings after firmware update
  - Prevents freezes caused by incompatible stored settings

---

## Changed

- **Context-dependent settings**
  - Tuning step, bandwidth, and gain are now saved independently for each mode (AM/SSB/FM) and band

- **Battery monitoring system redesign**
  - Redesigned with IIR filter and hysteresis for stable, flicker-free reading

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v3.1.0**

---

## Fixed

- **Inverted BFO logic in CW mode**
  - Caused a swap of LSB/USB sidebands

- **BFO hardware overrun prevention**
  - Reduced internal BFO range to stay within Si4732 hardware limits

---

## Changed

- **Main `loop()` function restructure**
  - Reworked for improved stability and easier maintenance

- **Expanded BFO calibration range**
  - Now supports **+/- 2.5 kHz** to compensate for significant crystal drift

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v3.2.0**

---

## Added

- **Configurable antenna capacitor**
  - Added menu setting to enable/disable RF input capacitor
  - Optimizes performance with different antennas

---

## Fixed

- **Attenuator system digital hum**
  - Set to manual-only to eliminate I2C-induced digital hum from AUTO AGC mode

---

## Changed

- **"Snap-to-Grid" tuning for AM/FM**
  - First encoder turn now automatically aligns frequency to nearest step grid point

- **Display brightness control rework**
  - Replaced with simple **1-10 scale** and curve for more visually linear brightness steps

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v3.3.0**

---

## Fixed

- **BFO calculation bug**
  - Caused incorrect frequency display with negative BFO values

- **FM bandwidth control direction**
  - Encoder direction for FM bandwidth selection now correct (clockwise increases bandwidth)

---

## Changed

- **Default ATT mode**
  - Now **AUTO** again

- **Button processing logic rework**
  - Reworked into smaller, more organized functions to simplify debugging
  - User-facing functionality remains identical

---

## Optimizations

- **Flash size reduction**
  - Reworked multiple functions, reducing final firmware size by over 200 bytes

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v3.4.0**

---

## Fixed

- **Runaway encoder bug in Command Mode**
  - Solved critical issue where turning encoder in `CMD_BAND` (or other command modes) could cause continuous, unstoppable loop of band/step/bw changes
  - Main loop and encoder processing logic reworked to correctly handle state

- **FM band stability**
  - Resolved major bug causing receiver to freeze or display invalid data (`655.35 MHz`, incorrect step/bw) when switching to FM band
  - Traced to incorrect EEPROM reset procedure

- **SSB step persistence**
  - Corrected issue where SSB tuning step was reset to default value every time mode was activated
  - Step size now correctly preserved for each band

- **RSSI display on FM**
  - Corrected logic in periodic task handler to ensure RSSI value is reliably displayed and updated on FM band

---

## Changed

- **FM band UI improvement**
  - On-screen text inversion for `CMD_BAND` mode now works correctly for FM band
  - Inverts top-line "FM" text instead of non-existent bottom-line name

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.0.0**

`(!) Attention: This version introduces a new EEPROM data structure and still in free-test mode. All saved settings, including volume and FM favorites (etc), will be reset to factory defaults upon the first boot.`

---

## Added

- **Per-Band State Saving**
  - Receiver now remembers last used frequency, tuning step, and bandwidth for each of the 28 individual bands (LW, MW, all SW sub-bands, FM)
  - State is automatically saved and persists after power-off

- **Seamless Band Coverage**
  - Band list re-architected to provide continuous tuning from 150 kHz to 30 MHz
  - "Gaps" between traditional bands now filled with generic "SW" segments, allowing exploration of the entire spectrum

- **Smart Power Management (AGC Button)**
  - `AGC` button now toggles the display and automatically reduces CPU speed to 8MHz when screen is off to conserve power

- **Global BFO calibration**
  - How is a compromise
  - If your receiver has a different frequency drift on 160 meters than on 10 meters, you will only be able to calibrate one band perfectly
  - The others will have a margin of error that you can adjust to suit you

---

## Fixed

- **SSB State Persistence**
  - Tuning step for SSB mode no longer reset when switching from AM, allowing custom step sizes to be saved per band
  - Bandwidth setting now intelligently carried over between AM and SSB modes

- **Amplifier Pop/Click**
  - Resolved audible pop when switching between FM and AM/SW bands by implementing correct power-down/power-up sequence for audio amplifier

- **Multiple UI & Tuning Bugs**
  - Addressed critical bugs that caused system freezes, display of invalid data (`65535 MHz`), and incorrect band boundary behavior during refactoring process

---

## Changed

- **Unified Band Switching**
  - Band switching logic now universal for all bands
  - Pressing `Band Up/Down` cycles sequentially through entire list (e.g., from "10m" to "FM" and from "LW" to "FM")

- **Brightness Control refactor**
  - Replaced static look-up table (LUT) with on-the-fly calculation
  - New implementation uses optimized fixed-point integer math to generate perceptually linear gamma curve

- **EEPROM Protection improvement**
  - Removed aggressive 10-second save timer
  - System now saves state after 30 seconds of tuning inactivity, or immediately upon switching bands and modes

- **Safe Band Name Handling**
  - Introduced dedicated helper function (`getBandName`) to safely retrieve band names
  - Corrects potential vulnerability where `PACK_STR4` macro could lead to buffer over-reads

---

## Optimizations

- **FM Favorites Saving**
  - Disabled immediate saving on each change
  - Favorites list now written to memory in single batch upon exiting menu

- **UI Event Handling**
  - Refactored encoder button handler (`handleEncoderButton`) to use switch-case on computed context

- **`showFrequency` refactor**
  - Simplified logic for calculating decimal point position, slightly reducing Flash and stack usage

- **Bandwidth String Storage**
  - Replaced system of multiple individual strings and pointer tables for bandwidth labels with single packed string array (`bw_all_data`) and compact index tables
  - Reduces Flash memory usage by eliminating redundant null-terminators and pointer overhead

- **Flash Savings**
  - Replaced expensive division/modulo math in FM Favorites menu with lightweight (`sw_div`)

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.1.0**

`(!) Attention: This version has major stability update.`

---

## Fixed

- **Seamless Wide-Band Seek**
  - Reworked seek function
  - Now starts from current frequency (not band edge) and scans seamlessly across all AM/SW/LW bands
  - Also fixes critical bug that could corrupt saved band-states after interrupted seek

- **BFO State Logic**
  - BFO offset now correctly reset to 0 upon switching bands
  - Prevents fine-tuning adjustments from one station from incorrectly carrying over to another

- **System Stability hardening**
  - Added safety check on boot to validate `CPUSpeed` setting from EEPROM
  - Prevents potential device hang from corrupted data

---

## Optimizations

- **Critical Flash Savings**
  - Freed over 250 bytes by replacing expensive 32-bit and division/modulo math (`splitFreq`) with 16-bit
  - This was the key to fitting all new fixes

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.2.0**

`(!) Attention: This version includes code optimizations.`

---

## Optimizations

- **`convertToChar` Rework**
  - Change alone freed up **32 bytes** of critical flash memory and improved rendering performance

- **`ilen` Function**
  - Digit-length measurement function refactored by compact logic and reducing firmware size

- **`doSeek()` FM rounding**
  - Reworked FM frequency rounding logic to use more direct modulo operation, saving 6 bytes of flash memory

- **Settings Handlers**
  - Unified 9 duplicate toggle functions into single `toggleSetting()`, saving 40 bytes
  - Combined with previous binary toggle optimization - total saved: **62 bytes**

- **`SettingParamToUI`**
  - Refactored to eliminate duplicate code paths, saving **22 bytes**

- **Button Handlers Refactoring**
  - `handleEncoderButton()`: Refactored logic to more efficient "Guard Clause" style, saving **14 bytes**
  - `handleModeButton()`: Refactored logic by adding "Guard Clause" to eliminate redundant checks, saving **8 bytes**
  - `handleFavoritesMenuButtons()`: Refactored by removing temporary variables, saving **20 bytes**
  - `processEncoderForSettings()`: Replaced `min()` macro with direct ternary operator, saving **4 bytes**
  - Centralized button processing logic to eliminate redundant checks, improving code structure and saving **4 bytes**

- **RSSI Subsystem Overhaul**
  - **[Feature]** Added universal signal quality indicator (RSSI) for all modes
  - **[Fix]** Implemented flag-triggered update mechanism for AM/SSB RSSI meter to eliminate audio artifacts during idle listening while ensuring value updates after user interaction
  - **[Optimized]** Overhauled RSSI logic for AM/SSB modes, saving additional **74 bytes**. Feature now fully disabled when set to 'Off', improving performance

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.3.0**

`(!) Attention: This version includes code optimizations and a critical bug fix in SSB mode.`

---

## Fixed

- **Critical SSB Tuning Bug**
  - Corrected issue where hardware tuner would get "stuck" during SSB tuning
  - `doFrequencyTuneSSB` now correctly sends `setFrequency()` command to chip after BFO rollover

- **RSSI Display Overflow**
  - Corrected UI layout bug where RSSI values of 100 or more would break display
  - Value now capped at 99 within `updateSignalQuality` function before being rendered

---

## Optimizations

- **EEPROM I/O Subsystem refactor**
  - Centralized all EEPROM functions and optimized logic with `inline` helpers, saving **46 bytes** of firmware space

- **Code Structure refactor**
  - Grouped related functions into `Subsystem` blocks for improved readability

- **`showFav()` optimization**
  - Extracted item drawing logic into `inline` helper to reduce Flash size

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.4.0**

`(!) Attention: This version includes significant stability fixes, code refactoring, and flash memory optimizations.`

---

## Fixed

- **CW Sideband Switching**
  - Corrected major regression in `doCWSwitch` that caused signal loss and BFO resets
  - Full functionality from v3.4 now restored

- **AM RSSI Stability**
  - Implemented new sampling strategy for AM RSSI to provide stable on-screen value and track slow fading
  - Value now read once after 1-second AGC settle time and then refreshed periodically (in 10 seconds), minimizing noise during active listening

---

## Changed

- **`handlePeriodicTasks` refactor**
  - Split into inline helpers (`handleSignalAndStereoUpdates`, `handleSettingsSave`, etc.)

- **Signal quality logic separation**
  - Separated into `getAmSignalValue` and `getFmSignalValue` helpers

---

## Optimizations

- **Major Optimization**
  - Removed redundant API call (`setSeekAmSpacing`) from AM configuration routine, resulting in **38-byte** flash savings

- **Arithmetic & Logic**
  - Replaced `volumeUp/Down` calls with direct arithmetic in `doVolume()` (saving 14 bytes)
  - Replaced costly boolean arithmetic in `handleAgcButton()` (saving 4 bytes)
  - Used manual `abs()` in `updateStablePercent()` (saving 4 bytes)

- **Code Consolidation**
  - Eliminated duplicate function calls within `doStep()` and other minor logic blocks, saving additional 10 bytes

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.5.0**

`(!) Attention: Continues optimization effort, focusing on low-level code refinement to free up critical Flash memory.`

---

## Fixed

- **Frequency Display Overhaul (32 bytes)**
  - Rewrote `showFrequency` function to resolve critical logic bug where `SWUnits=MHz` setting incorrectly affected MW/LW band display

---

## Changed

- **Display Layout Realignment**
  - Re-arranged on-screen indicators (Band/Step top, Mode/BW bottom) to align with physical button positions
  - Relocated RSSI indicator to bottom row and adjusted all element X-coordinates to resolve text overlap with long values (e.g., `160m`, `Step:100k`)

---

## Optimizations

- **Direct Display Rendering (26 bytes)**
  - Refactored `showChargeOnDisplay` function to print battery percentage directly to display
  - Eliminated need for temporary string buffer and `convertToChar` helper

- **Specialized Logic (8 bytes)**
  - Replaced generic `doSwitchLogic` function with `toggleSetting` call in `doCWSwitch` for binary (LSB/USB) switching

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.6.0**

`(!) Attention: Settings have been re-ordered. An EEPROM reset will occur on first boot.`

---

## Added

- **Quick Sync Toggle**
  - Long-pressing `MODE` button in any SSB/CW mode now toggles `Sync` setting
  - Provides fast access to key feature without entering menu, as was previously realized in AGC button

- **Robust Seeking**
  - Seek function timeout extended to maximum practical value (~32 seconds) to ensure complete band scans without premature termination on quiet bands
  - User action now the only way to stop a seek

---

## Fixed

- **Stale RSSI Display**
  - RSSI value now correctly cleared on mode switch (e.g., from FM to AM), preventing "ghost" values from being displayed

---

## Changed

- **Main Screen UI & Rendering rework**
  - Volume indicator now right-aligned and includes integrated separator (`'|'`) for more consistent layout with step parameter
  - Fixed bug where separator was not cleared correctly after changing tuning steps
  - Corrected issue where only volume value, not separator, was highlighted (inverted) during editing

- **Settings Menu Rework**
  - Re-ordered and sorted all settings into logical groups (like General, SSB/CW, Hardware) for faster navigation

---

## Optimizations

- **`showStep` Function (-20 bytes)**
  - Rewrote step display logic, which also fixed bug with incorrect CW steps and resulted in **20-byte flash saving**

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.7.0**

---

## Fixed

- **IIR Filter Initialization**
  - Corrected IIR filter logic for battery ADC readings to properly handle uninitialized state (`-1`) on first boot
  - Ensures stable and predictable readings from start

- **VFO Tuning Logic at Band Edges**
  - Refactored `doFrequencyTune()`, `bandSwitch()`, and `performBfoRolloverWithBandCheck()` to prevent frequency jumps when tuning across band boundaries with encoder
  - Tuning now rolls over seamlessly (e.g., 3200->3201 kHz like before v4.x), ensuring continuous VFO operation in all modes (AM/SSB/CW)
  - Jumping to band's stored frequency now isolated to `Band+` function

- **`showFrequency()` Rendering (-20 bytes)**
  - Stabilized display rendering during rapid tuning to prevent freezes
  - Achieved by refactoring core logic into helper functions, which also improves code clarity

---

## Optimizations

- **`calculateRawPercent` Function (-14 bytes)**
  - Rewritten for maximum flash efficiency using compressed data `struct` (8-bit offsets)
  - Hardcoded min/max edge-case values to eliminate runtime calculations
  - Downgraded interpolation math to `uint16_t`, achieving **14-byte total saving**

- **AM Mode Step Handling (-4 bytes)**
  - Refactored `doStep` function to eliminate redundant array lookup when setting frequency and seek spacing for AM modes
  - Streamlines register usage and results in **4-byte flash saving**

- **Step & Volume UI (-30 bytes)**
  - Replaced dynamic step generation with PROGMEM lookup table (`step_lookup_table`), saving flash space
  - Volume display updated for consistent UI style

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.8.0**

---

## Added

- **Software Debounce to Encoder Handling**
  - Introduced 10ms debounce in `updateEncoderState` to filter out rattling from cheap encoders, improving reliability without bloating ISR
  - Uses `millis()` for timing, ensuring atomic access with `noInterrupts()`

---

## Fixed

- **`snapToNewStep` helper**
  - Added with ternary for continuous rollover without jump on up/down, fixing bug in SSB tuning

---

## Changed

- **Enhanced `doFrequencyTune` and `performBfoRolloverWithBandCheck`**
  - For seamless continuous tuning across bands on encoder rotation
  - Sets freq to new band edge without loading saved value

---

## Optimizations

- **`getModeContext` Function (-4 bytes)**
  - Removed redundant `|| g_currentMode == CW` condition, as already covered by `isSSB()` (CW included in LSB/USB/CW range per enum Modulations)
  - Simplifies logic without changing behavior, reducing branching in assembly

- **`doCWSwitch` Function (-2 bytes)**
  - Removed dead condition `if (actual_direction == 0) return;`, as `toggleSetting` always changes parameter (0->1 or 1->0), making `actual_direction` always +/-1
  - Preserves seamless CW sideband switching and frequency compensation

- **Refactored `bandSwitch` (-8 bytes)**
  - With delta for DRY up/down logic, saving 8 bytes Flash while preserving wrap-around and config behavior

- **Removed redundant rounding in `showFrequencySeek` (-36 bytes)**
  - For FM seek, saving 36 bytes Flash while preserving frequency display and seek logic

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.9.0**

---

## Fixed

- **Critical tuning lag resolved**
  - Re-architected frequency update system to be adaptive and robust
  - New logic eliminates station skipping during fast encoder rotation by implementing `force_update` trigger that sends frequency to chip whenever UI delta exceeds 50 kHz
  - 25ms I2C rate-limiter now protects bus from flooding
  - Core fix to state management: `g_previousFrequency` now correctly synchronized only after command is sent, making delta calculation accurate and reliable
  - Necessary flash space freed by optimizing and removing non-essential features (animated splash screen, unused global variables)

---

## Changed

- **AM Seek Logic with Fixed 1 kHz Step (-40–50 bytes)**
  - Eliminated `seekAmMapping()` entirely
  - Seek spacing now fixed at 1 kHz for all AM modes (including SW/LW/MW), decoupling from `stepIdxAM` for consistent, fine-grained scanning
  - Integrated directly into `executeHardwareSeek()` via `setSeekAmSpacing(1)`
  - FM remains unchanged

---

## Optimizations

- **Refactored `doStep` Function (-20–30 bytes)**
  - Removed seek spacing updates (`setSeekFmSpacing(10)` for FM and `seekAmMapping()` for AM), as spacing now fixed in `executeHardwareSeek()`
  - Simplified LW/MW max index check with ternary operator, reducing branching while maintaining step limits

- **Refactored `configureAMCommon` Function (-10–15 bytes)**
  - Dropped `seekAmMapping()` call, as spacing is fixed
  - Uncommented AM-specific thresholds (SNR=0, RSSI=25) to enhance sensitivity for weak signals

- **Enhanced `doSeek` Function for Reliability**
  - Retained bounds checking and SW sub-band update loop, optimized for fixed-step operation
  - FM rounding logic preserved for compatibility
  - Added ternary-based step grid alignment to correct off-grid frequencies (e.g., 610 %9 !=0), skipped on encoder interrupt to avoid resets
  - Implemented ternary wrap-around for seamless SW scanning (min to max on down, max to min on up), resolving edge stalls

- **Refactored SSB/CW and Band Handling Functions (-30–50 bytes)**
  - Refactored `configureSSBMode` by commenting out `// g_currentBFO = 0;`, avoiding unnecessary BFO reset on SSB mode entry
  - Simplified `bandEvent` by removing rate-limiting counter and modulo operations; band switching now occurs on every long-press
  - Optimized `performBfoRolloverWithBandCheck` and `doFrequencyTuneSSB` by eliminating always-false return values, inlining boundary conditions, streamlining variable handling and encoder reset

- **Refactored FM Favorites Handling (-15–25 bytes)**
  - Simplified `saveFMFav` loop to write only active favorites (up to `g_totalFavorites`), eliminating else-branch for unused slots
  - Refactored `loadFMFav` by removing per-frequency validation in loop, relying on firmware-written data validity

- **Refactored Battery Monitoring Subsystem (-50–100 bytes)**
  - Wrapped advanced features (IIR filter, hysteresis, counters) in `#if ENABLE_ADVANCED_BATTERY_LOGIC` for conditional compilation
  - Defaults to minimal direct-calculation mode to save space
  - Inlined `base_voltage` (488) and hardcoded thresholds in loop, reducing declarations while keeping PROGMEM table intact

- **Replaced `strcpy_P` with direct PROGMEM print in `showStep`**
  - Minor flash savings

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.10.0**

---

## Fixed

- **Critical SSB Tuning Jump at Band Edges due to Faulty Snap Logic**
  - Resolved critical bug causing abrupt frequency jump when tuning downwards in SSB mode across band boundary
  - Was logical flaw in `snapToNewStep` function where unit mismatch (comparing frequency in `kHz` with step value in `Hz`) led to incorrect remainder calculation and massive erroneous frequency adjustment
  - Function re-architected to be robust and context-aware:
    - Now correctly **skips snapping for steps < 1 kHz** (preserving smooth tuning)
    - **Scales larger steps to kHz** before calculation
    - Includes **underflow check** to prevent `uint16_t` wrap-around

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v4.11.0**

---

## Changed

- **Dynamic Seek Spacing in AM Modes**
  - Updated `executeHardwareSeek` to use band-specific spacing:
    - LW/MW -> `LW_MW_STEP_SPACING`
    - SW -> `SW_STEP_SPACING`
  - Replaces fixed `AM_STEP_SPACING` for accurate seeking on LW/MW using 1kHz step

- **Modularized functions into static inline helpers for readability (no size impact)**
  - `processEncoderForSettings` -> `navigateSettingsPage`
  - `handleDelayedFrequencyUpdate` -> `performFrequencyUpdateCheck`
  - `setup()` -> `initHardwarePins`, `initOLED`, etc.
  - `cycleAmSsbCwModes` -> `prepareModeSwitch`, `performModeCycle`, `finalizeModeSwitch`
  - `doFrequencyTuneSSB` -> `prepareSSBTune`, `performSSBRollover`, `finalizeSSBTune`
  - Verified no changes or regressions via boundary tests

---

## Optimizations

- **SSB tuning refactor (-6 bytes)**
  - Saved 6 bytes via modular optimization

------------------------------------------------------------------------------------------------------------
### **MOD_NO_RDS v5.0.0**

`(!) Attention: This release only impacts the rendering UI / Display workflow components; all receiver logic functions remain untouched.`

---

## Optimizations

- **Significant memory optimization**
  - Achieved by disabling unnecessary fonts (thanks to den3rats) and transitioning to custom implementation instead of tiny4koled

- **Common custom 6x8 font**
  - Library now utilizes common custom 6x8 font, striking balance between readability and freeing up additional space for notifications

- **Buffer-based segmental frequency rendering**
  - Replaced heavy 3-page font with buffer-based segmental frequency rendering, resulting in substantial increase in available memory

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.1.0**

`GyverOLED.h / Utils.h / ATS_EX.ino`

---

## Added

- **`printInverted` templated helper function**
  - Introduced to centralize text inversion logic, simplifying UI code

- **Long-press STEP button for sideband toggle**
  - Implemented long-press on STEP button to toggle between LSB and USB when in SSB mode, providing quick sideband selection

---

## Fixed

- **Menu selection visual distortion**
  - Text inversion replaced by `>` indicator, which preserves font appearance and eliminates visual distortion

---

## Changed

- **Enhanced brightness control**
  - Adjustment now uses non-linear curve across full 1-255 contrast range

- **Reworked mode button logic**
  - More intuitive AM -> SSB -> CW cycle
  - Receiver now remembers last used SSB mode (LSB/USB)

- **Battery indicator accuracy**
  - Now much more accurate with 0% level set to safe 3.15V

---

## Optimizations

- **GyverOLED.h rendering code**
  - Refined with faster bitwise math and more compact `offsetof` data access to reduce flash size and increase speed
  - Removed dead code and unused function parameters

- **ATS_EX.ino refactoring**
  - Resulted in measurable decrease in final flash memory footprint, freeing up crucial space

- **Battery monitoring subsystem**
  - Refactored by eliminating duplicated code path, resulting in smaller flash memory footprint

- **Amplifier control functions merge**
  - Merged `safeAmpOff` and `safeAmpOn` into single `setAmpState(bool on)` with `attribute((always_inline))`
  - Cutting FLASH by 20 bytes via reduced call overhead

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.2.0**

`ATS_EX.ino`

---

## Added

- **Long-press STEP button in CW mode**
  - Enabled to toggle sideband in CW mode, mirroring functionality in SSB for more consistent experience

- **CW mode sideband display**
  - Now displays active sideband (L/U) on main screen, providing clear visual feedback for operator

---

## Fixed

- **Critical SSB tuning bug at band edges**
  - BFO rollover logic refactored for immediate and seamless band switching
  - Eliminates "stuck" frequency issue when tuning down

- **Tuning loop in AM mode at band boundaries**
  - Frequency no longer gets stuck when tuning across band edges (e.g., from MW to LW)

- **Visual artifact on frequency display**
  - Eliminated parts of "kHz" unit remaining on screen when tuning from 4-digit to 5-digit frequency in SSB mode

---

## Changed

- **AM/FM tuning logic robustness**
  - "Snap to step" feature disabled during band switch to prevent frequency distortion
  - Ensures smooth and predictable transition

- **CW mode SYNC behavior**
  - Now aligns with true receivers: SYNC feature fully disabled (functionally and visually) when in CW mode

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.3.0**

`ATS_EX.ino / Globals.h`

---

## Added

- **Auto Display-Off Timer**
  - New "DIS" setting on page 3 of menu allows configuring timer (10, 15, 30, 60 min)
  - Automatically turns off display and reduces CPU speed during inactivity, saving power

- **`SI4735_fixed.h` Override Class**
  - Introduced for non-invasive fixes
  - Created new class `SI4735_fixed` that inherits from base `SI4735` library for clean performance improvements without modifying original source code
  - Primary fix: inefficient `seekStationProgress` function overridden with corrected logic to initiate hardware seek only once and then poll for status
  - Results in much faster and more responsive station search

---

## Changed

- **Band Limits for Universal 9/10 kHz Grid Alignment**
  - Adjusted LW to 150-521 kHz (default freq 300 kHz)
  - MW to 522-1710 kHz (default freq 522 kHz, stepIdxAM=2 for 9 kHz)
  - Data-only change ensures better alignment with both 9 kHz and 10 kHz steps, supporting regional tuning

- **Battery Logic Refactored**
  - PROGMEM data table for battery monitoring removed
  - Reduced firmware size while maintaining non-linear battery model
  - Battery logic moved to separate file `Battery.h`
  - Adjusted ADC smoothing filter to be less sensitive to transient noise

---

## Optimizations

- **Core architecture optimized for code size**
  - Inactivity tracking logic switched from 32-bit milliseconds to 16-bit seconds
  - Use of compact 16-bit arithmetic minimizing memory footprint of new feature

- **Multiple core functions refactored**
  - Key UI functions (`SettingParamToUI`, `handleSwitchParam`, `DrawSetting`) and main tuning logic (`doFrequencyTune`) rewritten
  - Replaced conditional branching with more efficient arithmetic and data-driven lookups
  - Results in significantly more compact firmware

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.4.0**

`ATS_EX.ino / SI4735_fixed.h / Globals.h / Defines.h`

---

## Added

- **Soft Mute SNR Threshold Setting (SMT)**
  - Can now fine-tune signal-to-noise ratio (SNR) threshold (0-63db) at which soft mute feature activates

---

## Fixed

- **Audio Interruption During AM RSSI Updates**
  - Previous polling mechanism in AM mode relied on `setFrequency` command that caused audible mute or "pop" in audio
  - By leveraging dedicated `AM_RSQ_STATUS` command from chip's documentation, RSSI now updated seamlessly
  - Listening experience in AM now uninterrupted

- **AGC/AVC Logic**
  - Fixed logical inconsistency where "AVC" (Automatic Volume Control) setting could conflict with manual attenuator (ATT) settings
  - AVC now correctly applied only when main AGC is active

---

## Changed

- **"Soft Update" for AM RSSI**
  - `softAmRssiUpdate()` added to `SI4735_fixed`
  - Function encapsulates non-interrupting command (`AM_RSQ_STATUS`) to poll for fresh signal strength value

- **Restructured SW Band Map**
  - Replaced generic "SW" bands with specific broadcast bands (22m, 19m, 11m)
  - Preserved 28-band limit by adjusting adjacent band boundaries to ensure seamless tuning coverage

- **Robust EEPROM Memory Map**
  - EEPROM storage system completely overhauled to ensure settings safe across future firmware updates
  - Previously, adding new setting could cause data shift, corrupting everything saved after it
  - New system uses block-based map with fixed addresses, so each data type has own protected space

---

## Optimizations

- **Simplified Signal Polling Logic**
  - With new reliable "soft update" method in place, legacy `g_forceRssiUpdate` flag and associated timer (`updateAmRssiCountdown`) rendered obsolete and removed

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.5.0**

`ATS_EX.ino / Globals.h / Defines.h / Battery.h`

---

## Added

- **CW sideband state persistence**
  - Selected CW sideband (LSB/USB) now saved to EEPROM and restored on startup
  - Provides consistent experience similar to SSB mode

- **"Battery Pin" (BAP) setting**
  - Users can now select between ADC pins A1 and A2 for battery voltage monitoring
  - Makes firmware compatible with different hardware revisions without needing to recompile
  - Selection saved to EEPROM

---

## Fixed

- **Bandwidth settings during SW sub-band changes**
  - Previously, audio filter would not update after switching between SW bands or completing station seek without full mode change
  - Now corrected

- **Settings menu timeout**
  - Corrected issue where Settings menu would remain open indefinitely
  - Now closes automatically after inactivity, just like other temporary modes

---

## Changed

- **Removed 'Units Switch' (UNI) setting**
  - Frequency units (kHz/MHz) now permanently enabled on main screen
  - Removes unnecessary configuration step on/off

- **Streamlined CW sideband selection**
  - 'CW Switch' setting removed from menu to free up memory
  - CW sideband (LSB/USB) selection now handled directly only by long-press on `STEP` button

---

## Optimizations

- **Bandwidth display logic (`showBandwidth`)**
  - Refactored for Flash savings
  - Now calculates direct pointer to `PROGMEM` data, eliminating RAM buffer and data copy loop

- **Long-press handler for Volume controls**
  - Streamlined to save critical Flash memory
  - Logic that provided configurable delay for volume change repeats removed
  - New implementation simpler and more direct, sacrificing adjustable repeat speed for significant reduction in size

- **Minor operator adjustments**
  - Adjusted operators to equivalent arithmetic expressions without branches convenient for optimization by compiler

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.6.0**

---

## Added

- **Dynamic AM Seek Step**
  - Seek operation now uses currently selected manual tuning step (1, 5, 9, or 10 kHz)
  - Gives user direct control over scan speed and precision
  - Replaces previous fixed seek step (1 / 5 kHz)

- **Restored advanced SSB tuning system**
  - For smooth, "analog-like" experience
  - BFO now operates in seamless ±13 kHz fine-tuning window
  - Automatically rolls over into main frequency to eliminate perceptible tuning jumps
  - BFO calibration setting with ±2.5 kHz range available in menu for precise compensation of hardware variations in quartz crystal resonator

---

## Fixed

- **Settings Menu control**
  - Now longer 10-second inactivity timeout for Settings menu to prevent premature closing
  - 3-second timeout for simple modes (Volume, Step, etc.) retained

- **EEPROM Data Integrity**
  - Corrected 16-bit EEPROM reads in `readEepromHeader` and `loadFMFav`
  - Switched to sequential, multi-step process to eliminate undefined behavior from multiple `addr++` operations
  - Prevents potential byte-swapping and data corruption

---

## Changed

- **Font adjusted for better readability**
  - So that it does not strain the eyes

---

## Optimizations

- **Code refactoring for Flash savings (total ~200+ bytes):**
  - Eliminated redundant and unreachable conditions in all main button handlers (`Step`, `Mode`, `Volume`, `Favorites`), saving **68 bytes**
  - Removed unnecessary UI redraw logic from `doSync` and `handleModeButton`, saving **26 bytes**
  - Removed unused `g_prevMode` variable and its EEPROM storage, saving **26 bytes** Flash and 2 bytes RAM
  - Optimized `SettingParamToUI` by flattening logic with "guard clause" pattern, saving **6 bytes**
  - Refactored `handleAgcButton`, `snapToNewStep`, `applyBandConfiguration`, and `processEncoderForCommands`, saving combined **14 bytes**
  - Refactored `doStep()` to use unified processing block with switch-case statement using pointers to mode-specific variables
  - Refactored `syncModeDependentSettings()` to use ternary-based assignment, saving **12 bytes**
  - Refactored `doBandwidth()` to unify AM/FM logic via pointer-based approach, saving **24 bytes**
  - `doSeek()` cached pointer to band element in SW scan loop, saving **8 bytes**

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.7.0**

---

## Added

- **FM Audio Enhancement Profile**
  - Implemented permanent audio enhancement profile for FM band
  - Core improvement, not a menu option, applies automatically
  - Profile actively enables:
    1. **Quiet Tuning:** Aggressive soft-mute configuration using custom `setProperty` values (`0x1300`-`0x1305`) to eliminate static hiss when tuning between stations
    2. **Experimental Noise Blanker:** Undocumented Noise Blanker (properties `0x1900`-`0x1904`) enabled by default to potentially reduce impulse noise
  - **NOTE:** Code also includes configurations for "Warm Sound EQ" (via Hi-Cut filter) and "Forced MONO" operation
    - These features **disabled by default** (commented out) to preserve neutral, stereo-capable audio profile for headphone users and to conserve memory
    - Can be manually enabled in source code for experimentation

---

## Changed

- **Increased Frequency Digit Size and Spacing**
  - Main frequency display now uses **larger, 32-pixel high seven-segment digits** (previously 24px) for dramatically improved readability
  - Digit "blueprints" and rendering logic redesigned to maintain correct proportions at new size
  - Added **2px spacing between each digit** to prevent visual merging, further enhancing clarity
  - Vertical position of frequency display adjusted to keep it centered

---

## Optimizations

- **Major Font System Refactoring (-200+ bytes)**
  - Freed over **200 bytes of Flash memory** by overhauling standard 6x8 font system
  - All unused characters, including entire lowercase set, removed from firmware binary
  - Replaced original linear font map with compact array and lookup table (`_charMap_min` & `_charLookup`)
  - System provides faster character retrieval and ensures any request for unsupported character safely renders blank space
  - All UI text strings converted to UPPERCASE to align with new memory-efficient font map

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.8.0**

---

## Changed

- **Removed Non-Functional 'Force FM Mono' (FMM) Feature**
  - Extensive debugging process conducted to address non-functional 'FMM' setting
  - All known documented (`0x18xx` properties) and undocumented (`0x1100` bit 8) methods to force mono reception "on-the-fly" systematically implemented and tested
  - **Conclusion:** Specific Si4735 chip revision **strictly adheres to datasheet limitation** stating stereo blend properties can only be set during initial power-up/configuration phase
  - Chip consistently ignored all attempts to change properties dynamically
  - **Action Taken:** 'FMM' setting and all associated logic **completely removed** from firmware
  - Receiver now operates in default, reliable automatic stereo mode as intended by hardware

- **Enhanced UI Typography**
  - Compact font regressed to include essential lowercase characters (`k`, `m`, `z`) for standardized display of SI units (e.g., `kHz`, `MHz`)
  - De-emphasis values in menu simplified to just `50` / `75`

---

## Optimizations

- **Migrated from `Wire.h` to `microWire.h` I2C Library**
  - Entire project migrated from standard Arduino `Wire` library to AlexGyver's lightweight `microWire` library
  - Done to overcome critical memory limitations of ATmega328P microcontroller
  - **Memory Savings:** ~**2 KB of Flash** and **64 bytes of SRAM** freed
  - Savings from eliminating internal I2C buffers and using more compact, non-interrupt-driven implementation
  - **Impact:** Provides crucial headroom for application stack, enhancing runtime stability and unlocking potential for future feature additions
  - Migration implemented as seamless "drop-in replacement" with no loss of hardware functionality

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v5.8.1**

`(!) Attention: This is a technical release focused on internal improvements. There are no new user-facing features.`

---

## Optimizations

- **Code Health & Refactoring**
  - Overhauled project internal structure for improved modularity, making future updates easier to develop and maintain
  - Broke down large functions into smaller (SRP) & reusable utilities to reduce code duplication (DRY)
  - Improved code readability in key modules (UI and input handling) through better formatting and clearer comments
  - Refactored input handling subsystem into clean, table-driven dispatcher
  - Centralized all button logic, eliminating redundant handlers (DRY) to improve code readability and simplify future maintenance

------------------------------------------------------------------------------------------------------------
### **MOD_NO_RDS v6.0.0**

---

## Added

- **Universal Favorites System**
  - Previous FM-only feature replaced with powerful memory system for **all modes** (AM, LSB, USB, CW, FM)
  - Each memory slot now saves frequency, modulation, and **precise BFO offset**
  - Allows one-touch recall of perfectly tuned SSB/CW stations
  - Control scheme redesigned for intuitive access:
    - **Long-press `AGC`:** Save current station
    - **Long-press `STEP`:** Open/Close the Favorites menu

---

## Fixed

- **FM Stereo Indicator Logic**
  - Corrected function call to ensure UI is updated safely after internal state change
  - Stereo indicator (`*`) now works reliably

---

## Changed

- **Favorites Menu UI improvements**
  - Screen designed for clarity, displaying **5 stations per page** with clean spacing
  - Perfectly aligned, multi-column layout shows frequency (with BFO) and modulation for each entry
  - Format adapts for AM/SSB and FM
  - Redrawing optimized to **prevent flicker** when scrolling within single page
  - **Auto-exit timer** closes menu after period of inactivity

- **Intuitive Display Wake-up**
  - Display now wakes from auto-timeout state on **any button press**
  - First press only reactivates screen, preventing accidental changes to frequency or settings

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.0.1**

---

## Fixed

- **SSB/CW Mode Activation from Favorites**
  - Fixed bug where selecting SSB/CW favorite while in AM mode would change display indicator but not actual audio
  - Receiver now reliably engages correct mode

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.0.2**

`ATS_EX.ino / Input.h / SI4735_fixed.h`

---

## Added

- **Centralized Configuration Constants**
  - Refactored all magic numbers for timings, thresholds, and algorithm parameters into named constants within `Defines.h`
  - Improves readability and simplifies future tuning

---

## Fixed

- **Independent CW Mode Bandwidth**
  - Bandwidth parameter now fully independent and non-adjustable in CW mode
  - Prevents user LSB/USB bandwidth setting from being altered
  - Ensures correct preservation when switching back from CW

---

## Changed

- **Accelerated SSB/CW Mode Switching**
  - Optimized SSB patch algorithm for linear O(N) complexity
  - Eliminated 62,000 redundant loop iterations per patch load
  - Combined with increased I2C bus speed to 800kHz, significantly reduces activation delay for SSB/CW modes

- **Robust SSB Patch Loading**
  - Implemented per-segment status check during patch download as per AN332 datasheet
  - Guarantees data integrity by detecting I2C transmission errors
  - Prevents potential device instability or lock-ups

- **Streamlined Mode Switching UI**
  - Active command mode (e.g., Volume, Step, BW) now automatically reset when cycling through modulation types (AM/SSB/CW)

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.0.3**

`ats_ex_firmware.ino / Defines.h`

---

## Fixed

- **Critical AM/SW Audio Bug on Cold Start**
  - Fixed bug causing audio to be completely muted or have very low volume on AM/SW bands after powering on (cold start)
  - Issue did not occur during "hot" mode switches (e.g., AM -> FM -> AM)
  - Cause traced to timing-sensitive regression introduced during code refactoring
  - Separation of initialization commands into multiple functions disrupted delicate command sequence required by Si4735 chip DSP after `POWER_UP` cycle
  - Caused DSP to incorrectly activate Soft Mute and fail to apply AVC gain settings
  - Fix restores stable behavior of legacy firmware (1.18) by consolidating all critical audio and gain configuration commands in `configureAMMode` function
  - All parameters sent in rapid sequence immediately after main `setAM` command

---

## Changed

- **More Reliable State Saving Logic**
  - Reworked EEPROM saving mechanism to significantly reduce risk of losing last tuned frequency if device powered off shortly after adjustment
  - Logic now uses shorter, more responsive 15-second idle timeout for saving operational state (frequency, band)
  - Menu setting changes saved instantly upon exit
  - Dual-path approach ensures both predictable behavior for user settings and better data integrity for tuning actions while protecting EEPROM from excessive wear

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.1.0**

`ATS_EX.ino / Globals.h / Defines.h / SI4735_fixed.h`

---

## Added

- **Selectable FM Audio Profile**
  - New "FM Audio Profile" (`FMP`) setting to toggle built-in speaker equalization
  - **ON (Default):** Optimizes audio for internal speaker, providing warmer sound
  - **OFF:** Disables EQ for clean audio output for headphones

- **Selectable AM Noise Blanker**
  - New "AM Noise Blanker" (`ANB`) setting, disabled by default
  - When enabled, activates experimental digital filter using factory-default values from AN332 datasheet
  - Helps(?) reduce impulse noise in AM mode

- **Selectable FM Force Mono**
  - Return back and fixed new "Force Mono" (`FMO`) setting for FM band, disabled by default
  - When enabled, forces receiver into mono mode by comprehensively adjusting all three stereo blend mechanisms (RSSI, SNR, and Multipath)

---

## Changed

- **Optimized FM Audio Profile**
  - Edit FM Noise Blanker with safe values to reduce impulse noise
  - Increased Soft Mute SNR threshold suppression of static hiss between stations
  - All related constants in `Defines.h` verified against datasheet, corrected, and better documented

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.1.1**

`Input.h`

---

## Fixed

- **Display Wake-Up and Manual Power-Off Functionality**
  - Fixed regression from v6.0 that prevented waking display if it timed out while in settings menu
  - Wake-up action now correctly prioritized in all modes
  - Restored ability to manually turn display off with short press of `AGC` button on main screen
  - Feature was unintentionally lost during previous refactoring
  - Code refactored to use new helper function (`setDisplayPower`) eliminating code duplication and clarifying logic

---

## Optimizations

- **Refactored Property Settings**
  - Created new `applyProperties` helper to set Si4735 properties from `PROGMEM` tables
  - Refactored audio configuration functions (`FMAudioConfigure`, `applyAMNoiseBlankerSettings`) to use new declarative method
  - Improves memory savings

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.1.2**

`ATS_EX.ino / Memory.h`

---

## Fixed

- **Stable Frequency Across Mode Switches (AM/SSB)**
  - Fixed critical bug causing frequency to jump to old value when switching from SSB to AM
  - BFO tuning adjustments were not being applied to main frequency before saving
  - Fix ensures internal state is normalized before mode switch
  - Receiver stays on exact tuned frequency

- **I2C Stability**
  - I2C bus speed, previously set to 800kHz for faster mode switching, adjusted to default 500kHz
  - Prevents potential device lock-ups in SSB/CW mode activation in some IC

- **AM Audio on Startup**
  - Fixed muted AM audio on startup by correcting initialization sequence
  - Ensures user settings from EEPROM loaded *before* radio chip is configured

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.2.0**

`ATS_EX.ino / Defines.h / Globals.h / UI.h`

---

## Added

- **Adjustable Squelch for AM**
  - New "Squelch" (`SQL`) setting in menu allowing users to set RSSI threshold from `OFF` to `60`
  - When signal strength drops below set level, audio automatically muted
  - Feature disabled in FM mode
  - Uses Si4735's internal hardware mute, ensuring it works with both speaker and headphones
  - EEPROM memory map updated to correctly save new setting

---

## Changed

- **Signal Polling Logic**
  - Centralized all RSSI retrieval logic into single `getSignalQuality()` function

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.2.1**

`ATS_EX.ino / Defines.h`

---

## Fixed

- **CW Auto-Compensation Logic**
  - Fixed bug causing 1 kHz frequency drift when switching CW sidebands (CW-L/CW-U)
  - Issue caused by conflict between manual BFO adjustments and automatic pitch offset

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.0**

`ATS_EX.ino / Globals.h / Defines.h / UI.h`

---

## Added

- **Selectable CW Pitch**
  - `CWP` menu setting to select comfortable CW sidetone (500-800 Hz)
  - Replaces fixed 500 Hz tone
  - Chosen pitch automatically used for BFO compensation to ensure tuning accuracy

- **Adjustable FM Soft Mute**
  - Two advanced settings: "FSA" (Attenuation) and "FST" (Threshold)
  - Fine-grained control over FM soft mute behavior
  - Decouples soft mute from `FMP` audio profile
  - Allows users to balance quiet tuning with ability to hear weak DX stations

---

## Fixed

- **Preserved SSB Bandwidth on Mode Switch**
  - Fixed bug that incorrectly reset SSB bandwidth to 3.0 kHz when switching from AM or toggling Sync setting
  - Incorrect bounds check corrected
  - User-selected bandwidth now reliably preserved

- **Symmetrical BFO Calibration**
  - Fixed asymmetrical BFO calibration for USB/CW-U modes
  - Offset now correctly inverted based on active sideband
  - Enables accurate compensation for all SSB and CW modes

- **Seamless AM↔SSB/CW frequency transition**
  - Folded SSB fine tune (BFO) into kHz before mode switch to preserve exact tuned carrier
  - Removed 1 kHz rounding and rollback
  - Clamped at band edges in `prepareModeSwitch`

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.1**

`SSD1306_OLED.h / UI.h / Input.h`

---

## Fixed

- **Display Driver Stability**
  - Rewrote `drawDigit` function to fix critical bug causing random crashes
  - Previous use of Variable Length Array (VLA) on stack led to stack overflows during rapid tuning
  - New implementation uses static buffer, eliminating stack corruption and making display driver stable

- **Slower BAND+ / BAND− long-press repeat**
  - Added `BAND_LP_REPEAT_MS` throttle (default ~240 ms) to reduce overshoot when holding button
  - Long-press cycling was too fast on real hardware and often jumped past target band

- **Preserved SSB Fine-Tune (BFO) Across Mode Switches**
  - Fixed bug where fine-tune offset (the ".xx" portion of frequency) was reset to zero after switching from SSB to AM/CW and back
  - Per-band RAM cache now saves last used BFO value when leaving SSB mode
  - Restores precise fractional frequency when returning to LSB/USB
  - Ensures tuning adjustments not lost during session

---

## Changed

- **Enhanced UI Readability**
  - Reworked seven-segment digit geometry and increased inter-digit spacing for much cleaner and more legible frequency display
  - Corrected multiple visual glitches including decimal point placement & label overlap

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.2**

---

## Fixed

- **Saving of manual attenuator (ATT) settings in AM/SSB modes**
  - ATT value now correctly saved when changing subbands and modes (including BFO rollover)
  - Eliminates background noise return and rollback to AUTO

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.3**

`ATS_EX.ino / Defines.h / UI.h`

---

## Added

- **Soft Mute Indicator**
  - New 'SM' indicator appears under volume level to show when DSP soft mute is active
  - RSSI display now fixed-width to prevent jitter

---

## Fixed

- **Improved Unit Display**
  - Frequency units (kHz/MHz) now visually aligned with large digits
  - Hidden on long SSB frequencies to avoid overlapping BFO value

---

## Changed

- **FM audio (AN332) enhancement**
  - Applied multipath blend defaults and corrected Hi-Cut mapping
  - Added brief datasheet notes near defaults
  - Results in cleaner audio under multipath and predictable Hi-Cut behavior tied to MULT

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.4**

`Input.h`

---

## Changed

- **Settings navigation UX**
  - Encoder now scrolls seamlessly across pages
  - Reaching end of page moves to first item of next page, and vice versa (no BAND+ needed inside Settings)
  - Full wrap-around across entire list preserved
  - BAND+/BAND− still work as page shortcuts

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.5**

---

## Fixed

- **Overhauled Encoder Handling for System Stability (Critical)**
  - Resolved multiple critical race conditions related to rotary encoder
  - Eliminates system freezes (**live-locks**) that occurred during rapid tuning
  - Prevents random unpredictable frequency jumps ensuring smooth and reliable manual tuning
  - Guarantees proper atomic access to shared variables between main loop and interrupts preventing data corruption

- **Improved Scan/Seek Responsiveness (Critical)**
  - Fixed race condition in scan stop mechanism that could cause device to ignore user input
  - Now immediately responds to stop commands, ensuring full control during automatic station searching

- **Dual-Path Band Switching (Fast vs Full)**
  - Split `bandSwitch` into Fast (LW/MW/SW) and Full (AM↔FM) paths
  - Eliminates micro-freezes and unnecessary redraws during rapid band changes

- **SSB Frequency Corruption on Mode Switch (Critical)**
  - Fixed critical integer overflow bug that caused frequency corruption (e.g., jumping to `65535 kHz`) when switching from SSB to AM/CW at band lower edge with negative BFO
  - Normalization logic now uses safe 32-bit arithmetic for all calculations to guarantee stability

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.6**

---

## Changed

- **System Robustness with EEPROM Data Validation**
  - Added strict boundary checks in `loadBandState()` for all indices (bandwidth, step) loaded from EEPROM
  - Prevents potential system instability or crashes caused by corrupted data leading to out-of-bounds array access

- **Graceful Failure on EEPROM Wear**
  - Implemented simple wear-detection mechanism
  - If firmware fails to write default settings after reset, it flags EEPROM as 'bad' (`EEPROM WEAR`)
  - Prevents endless reset loops and blocks all future writes
  - Allows receiver to continue operating with default settings

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.7**

`ATS_EX.ino`

---

## Optimizations

- **Major Flash Footprint Reduction & Micro-Optimizations**
  - Reduced total Flash size by **66 bytes** (30684 → 30618) with no change to RAM usage, behavior, or UI
  - Achieved through series of micro-optimizations for compiler

- **Tuning & System Responsiveness**
  - Reworked `checkStopSeeking()` callback to use single critical section, making scan/seek stop commands more reliable and immediate
  - Replaced expensive 32-bit `abs()` calls in `performBfoRolloverWithBandCheck()` and `performFrequencyUpdateCheck()` with direct comparisons and efficient 16-bit math
  - Improves performance during rapid tuning across band edges

- **SSB/CW & FM Path Refinements**
  - Replaced conditional branches for SSB Sync settings with branchless arithmetic (`1-p`, `3*p`), simplifying logic in `configureSSBMode()` and `doSync()` handlers
  - Simplified BFO calculation in `updateBFO()` with clearer signed-type handling while preserving identical behavior for CW pitch offset and sideband calibration
  - Streamlined FM de-emphasis and stereo mode settings by removing redundant comparisons and using direct arithmetic/boolean parameter values

- **Code Housekeeping & Redundancy Removal**
  - Eliminated duplicated code by reusing `setCpuPrescaler()` helper during initial setup
  - Improved `applyProperties()` to use more efficient pointer-based walk over PROGMEM data tables instead of indexed access

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.3.8**

`UI.h`

---

## Optimizations

- **UI Flash Footprint**
  - Reduced total Flash size by **128 bytes** via targeted micro-optimizations in UI subsystem

- **Render Path**
  - Refactored main screen and menu drawing helpers (`showSignalQuality`, `showFrequency`, `fav_pageOf`, `calcSettingPos`) to eliminate redundant logic, function calls, and variables

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.4.0**

`ATS_EX.ino / Defines.h / Input.h / CW_decoder.h`

---

## Added

- **Experimental built-in CW Decoder**
  - Added lightweight CW (Morse code) to text decoder
  - **Activation:** Long-press **MODE** button while in CW mode to toggle decoder view
  - **Hardware:** Connect speaker audio via **2.2 µF capacitor** to pin **A6**. No external DC bias circuit needed
  - Decoder tuned for receiver's audio output (~40-380 mVpp) and decodes A-Z / 0-9

---

## Fixed

- **UI Stability**
  - Reworked main loop to automatically exit CW decoder view when switching to other modes
  - Prevents UI from freezing

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.4.1**

`ATS_EX.ino / Input.h`

---

## Fixed

- **Critical Stability in Mode Switching**
  - Corrected out-of-bounds array access vulnerability during AM to SSB/CW mode changes, preventing potential crashes
  - Fix ensures stable modulation switching and adheres to stricter type safety

---

## Changed

- **Unified User Activity Tracking**
  - Centralized all user activity detection (button presses, encoder turns) into single, reliable mechanism
  - Ensures auto-sleep and menu timeouts behave predictably and consistently across all modes and menus

- **Responsive Wake-from-Sleep**
  - Enhanced user experience when waking device from auto-sleep
  - First button press now both wakes display and executes its intended action
  - Eliminates need for second press

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.0**

`ATS_EX.ino / Defines.h / Globals.h`

---

## Added

- **Selectable Short-Wave AFC for AM**
  - New "SW AFC" (`SWA`) menu setting to auto-center AM stations on SW bands
  - Offers four profiles: OFF (Default), PPM (datasheet defaults), Hz Normal (~1.6kHz window), and Hz Aggressive (~2.0kHz window)
  - EEPROM map updated to persist selected profile

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.1**

`CW_decoder.h`

`(!) Attention: This release is a complete overhaul of the experimental CW Decoder. All changes are confined to this feature.`

---

## Changed

- **Complete CW Decoder Overhaul**
  - Decoder now robust and significantly more sensitive
  - Implemented **adaptive ADC reference** that automatically switches between `INTERNAL` (1.1V) and `AVCC` (5V) to prevent signal saturation
  - New **software AC coupling filter** ensures reliable decoding with simple hardware
  - All signal processing parameters fine-tuned for better weak-signal performance
  - UI upgraded to proper **multi-line viewport** that wraps and clears automatically

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.2**

`CW_decoder.h`

`(!) Note: This release is a complete overhaul of the CW decoder engine for a major boost in accuracy and stability.`

---

## Changed

- **Complete CW Decoder Overhaul**
  - Core logic replaced with sophisticated adaptive engine that dynamically tracks operator speed
  - New real-time WPM (Words Per Minute) calculation with output smoothing provides stable speed reading
  - Addition of timing hysteresis prevents decoding flutter
  - Significantly boosts accuracy and stability across various speeds and noisy conditions

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.3**

`SI4735_fixed.h / patch_ssb_compressed.h / ATS_EX.ino`

---

## Changed

- **New SSB Patch System (Finalized)**
  - Firmware now exclusively uses new highly compressed patch format
  - Completely removed old space-consuming patch arrays
  - Patch loading code (`downloadCompressedPatch`) cleaned up to be simpler and easier to follow
  - Results in more efficient and slightly smaller final firmware

- **Credit:** Credit for the clever patch compression method goes to @den3rats

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.4**

`(!) Attention: This is a technical release focused on a major internal refactoring. All user-facing features and behavior remain unchanged.`

---

## Optimizations

- **Core Code Overhaul**
  - Main `ATS_EX.ino` file split into smaller focused helper functions
  - Improves code readability and simplifies future maintenance without impacting firmware size or performance
  - Refactoring covered:
    - **Tuning & Seek:** Broke down BFO rollover, AM/FM/SSB tuning, band switching, and seek logic into dedicated inline helpers to clarify complex state transitions
    - **Hardware Control:** Modularized low-level functions for SSB patch loading, FM audio profiles, and hardware configuration to isolate hardware-specific logic
    - **State Management:** Extracted core utilities for state sync, user activity tracking, and timed tasks into reusable helpers
    - **User Action Handlers:** Refactored settings menu, favorites management, and parameter handlers (e.g., step, volume) for better clarity and consistency

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.5**

`SI4735_fixed.h`

---

## Fixed

- **Critical seek logic error (`seekStationProgress`)**
  - Seek operation now initiated only once instead of being restarted in loop
  - Now correctly awaited by polling `STCINT` (Seek/Tune Complete) flag
  - https://github.com/pu2clr/SI4735/issues/45

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.5.6**

`UI.h`

`(!) Note: Technical refactor of the Settings and Favorites UI; all user-facing behavior is unchanged.`

---

## Fixed

- **Display accuracy**
  - Settings now show correct labels: `DisplayOff`→OFF, `SQL`→OFF, `Brightness`→1-10
  - Favorites display true tuned frequency with BFO applied
  - Right-aligned frequency remains stable during digit count changes
  - RSSI values <10 now properly spaced

---

## Changed

- **Settings navigation order (row-first L→R)**
  - Selection now moves across row (Left→Right) and then down to next row
  - Implemented by remapping `calcSettingPos()` only; no layout or redraw logic changed

---

## Optimizations

- **Complete UI subsystem overhaul**
  - Split monolithic drawing code into 50+ specialized helper functions for better maintainability
  - No change to user-facing behavior or firmware size increase

- **Rendering performance**
  - Implemented partial screen updates (settings value-only, favorites cursor-only)
  - Reduced PROGMEM reads through early returns and caching
  - Minimized I2C traffic with conditional clears and single-char writes

- **Code organization**
  - Logical function grouping: utilities → brightness → status widgets → frequency display → favorites → settings → orchestrators

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.6.0**

`ATS_EX.ino / Defines.h / Globals.h / UI.h`

---

## Added

- **S-Point Signal Meter**
  - New "S-Point" (`SPT`) display setting
  - Switch from raw RSSI number to classic `S0` to `S9+` meter reading
  - More intuitive for HF/SWL
  - Mapping uses different dBuV thresholds for HF and FM, so S-readings feel correct for band you're on
  - Off by default; if you prefer old RSSI numbers, no change needed

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.7.0**

`ATS_EX.ino / Defines.h / Globals.h / Utils.h / UI.h / RadioControl.h`

`(!) Note: Major Refactor`

---

## Added

- **Per-Modulation Audio Settings (AVC, Soft Mute, AGC)**
  - Addresses common issue of significant volume jumps when switching between AM and SSB/CW
  - Introduces independent memory for key audio settings (`AVC`, `Soft Mute`, `AGC`) for `AM`, `LSB`, and `USB`
  - `CW` mode intelligently inherits settings from active sideband (LSB or USB) ensuring audio consistency without needing separate profile
  - When you adjust `AVC` in `LSB` mode, it is saved specifically for `LSB` and won't affect settings for `AM`
  - Receiver automatically applies correct audio profile when you change modulation

- **Configurable Menu Navigation**
  - Added `NAV` setting (Page 4) to switch between efficient row-first (default) and legacy column-first layout

- **FM Volume Balancing**
  - Addresses long-standing issue of **FM mode being significantly louder** than AM/SSB
  - Firmware now automatically applies software volume offset when in FM mode ensuring **consistent listening level** when switching between bands
  - Offset now fully user-configurable setting, **`FVA` (FM Volume Adjust)**, located on Page 5 of menu

---

## Changed

- **Advanced SSB Patch Compression**
  - Developed by @den3rats
  - More efficient compression scheme for SSB patch data saving additional **60 bytes** of Flash mem
  - Logic for identifying `0x15` commands now encoded directly into `cutoff_nonzero_lengths` array
  - Eliminates need for separate `cmd__0x15_offsets` table

- **Settings Menu Organization**
  - Settings regrouped thematically across five pages for more logical and intuitive navigation
  - All SSB/CW-specific options now consolidated on single page
  - Settings menu now remembers last page you were on
  - When re-entering menu, it opens on that page instead of defaulting to page 1

- **AVC Control System rework**
  - AVC setting now 0-10 scale instead of previous technical 12-90 range
  - Non-linear formula used to map this scale to hardware compensating for chip response
  - Makes adjustment feel much more intuitive
  - Default level set to 10 (max) to match previous version (90) behavior

- **BFO Systems rework**
  - Per-Band BFO Calibration: BFO calibration no longer single global setting
  - Now saved individually for each band, allowing for fine-tuning across frequencies

---

## Optimizations

- **Code Refactoring**
  - All Si4735 radio chip control logic extracted from `ATS_EX.ino` into new `RadioControl.h` file
  - Creates Hardware Abstraction Layer (HAL), making code cleaner, more modular and easier to maintain

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.7.1**

---

## Fixed

- **UI / Font**
  - Restored `+` character to compact font set to ensure correct on-screen display

---

## Optimizations

- **Core Logic Refactoring**
  - Improved bandwidth control logic (`doBandwidth`)
  - Rewrote critical SSB frequency normalization logic (`normalizeSsbBeforeSwitch`) to improve stability when switching modes

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.7.2**

---

## Fixed

- **AVC Zero Value Persistence**
  - Corrected initialization check that incorrectly treated valid AVC setting of '0' as uninitialized state
  - Logic now reliably distinguishes between user-set '0' and erased EEPROM byte (`-1`)
  - Ensures full 0-10 range persists correctly

---

## Changed

- **Mode-Dependent Settings Logic refactor**
  - Implemented robust, data-driven system to manage setting availability based on current radio mode
  - Prevents adjustment of inapplicable settings (like AVC in FM) and visually indicates their inactive state (`---`) in menu
  - Improves clarity and prevents user error

- **Drawing Functions in `SSD1306_OLED.h` refactor**
  - Decomposed `drawDigit` into inline helpers for clarity with 0-byte size change

- **FM De-emphasis Default**
  - Changed default FM de-emphasis from 75µs to 50µs for better compatibility with European and international broadcasting standards
  - Users in Americas can still switch to 75µs via settings menu

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.7.3**

`ATS_EX.ino`

---

## Optimizations

- **S-Meter Subsystem Overhaul refactor**
  - Core `rssiToSLevel` function decomposed
  - Eliminates duplicate code paths for HF and FM modes

- **Flash Savings in S-Meter Logic**
  - Optimized S-meter by removing unnecessary calculation of numeric dB values over `S9`
  - Since UI only ever displays `+` symbol, this change saves critical Flash memory without altering on-screen display or S-point thresholds

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.8.0**

`Memory.h / Defines.h`

---

## Fixed

- **SSB Mode Persistence**
  - Fixed issue where USB/LSB selection was not saved to EEPROM and always defaulted to LSB after reboot
  - Receiver now correctly remembers whether you were using USB or LSB when switching from AM to SSB
  - Added `lastSsbMode` field to EEPROM storage structure to persist sideband preference across power cycles

`RadioControl.h`

- **CW Sideband Switch ATT Reset**
  - Fixed issue where switching between LSB/USB in CW mode would reset ATT (attenuation) to AUTO despite correct visual display
  - Receiver now correctly applies saved ATT setting for new sideband context after CW sideband switch
  - Added `applyAgcSettings()` call in `doCWSwitch()` to restore hardware AGC state after re-tuning

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.8.1**

`RadioControl.h`

---

## Fixed

- **SSB to FM rollover stability (Critical)**
  - Added safety check in `doFrequencyTuneSSB()` to prevent invalid BFO commands when switching to FM
  - Enforced active state reload to fix frequency discontinuities ("phantom frequency") immediately after band switching
  - Removed redundant `ssbGuardFmAfterRollover` logic to save flash memory

- **SW AFC logic & state leakage**
  - Fixed "OFF" setting to properly reset hardware registers instead of doing nothing
  - Forced default profile on non-SW bands to prevent aggressive AFC settings from persisting when switching to MW/LW
  - Fixed safety check in `swAfcRegFromHzK` to return safe defaults instead of maximum window on error

- **AM AGC duplication**
  - Removed redundant AGC apply inside `configureAMMode()`

- **Encoder tuning overflow**
  - Updated `doFrequencyTune()` to use full 32-bit arithmetic for frequency accumulator

`Input.h`

- **Encoder ISR stability**
  - Removed unsafe `interrupts()` calls inside ISR to prevent Stack Overflow during fast rotation
  - Improved seek cancellation responsiveness by detecting any pin change instead of waiting for full step

`GyverOLED.h`

- **Screen clearing boundary**
  - Fixed off-by-one error in `clear()` function that prevented last pixel column (127) from being erased

`Battery.h`

- **Battery indicator instability**
  - Increased filter suppression (1/16) and removed instant jump to eliminate reading jitter during OLED activity

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v6.9.0**

`ATS_EX.ino / Defines.h / Input.h / Globals.h / RadioControl.h / SI4735_fixed.h / SSD1306_OLED.h`

---

## Added

- **Band Map Rework (Seamless Coverage, 32 bands)**
  - Expanded band table from 28 to 32 entries with continuous LW/MW/SW coverage
  - Added dedicated HAM sub-bands: **60m amateur (60H)**, **30m**, **20m**
  - Fine-grained SW segmentation removes gaps and makes band switching more predictable

---

## Fixed

- **Band Defaults Safety**
  - Corrected out-of-range `current_freq` defaults for SW bands to ensure stored last frequency always starts within `[min_freq..max_freq]`
  - Prevents edge-case tuning glitches when entering band whose saved/default frequency was outside its limits

- **RSSI Polling in SSB/CW Modes**
  - Enabled RSSI updates for LSB/USB/CW by polling RSQ (0x43) via library path in `getSignalQuality()`
  - Prevented `RSSI always 0 / invalid` behavior in SSB/CW when RSSI display enabled in Settings

- **UI + Globals correctness**
  - `g_voltagePinConnnected` → `g_voltagePinConnected`
  - `g_savedSsbBfo[28]` → `g_savedSsbBfo[g_bandCount]`
  - `g_bandCount` updated to **32**
  - Small fixes in UI

---

## Changed

- **Input Handling Cleanup refactor**
  - Reworked `simpleEvent()` long-press filtering into `switch(pin)` whitelist (only selected buttons keep long-press / others downgrade to short press)
  - Minor structural cleanup in several button handlers to reduce nesting and make behavior easier to audit

- **Volume/Mute UX improvement**
  - Optimized Volume-Down short press handler to restore mute state without unnecessary chip volume reads
  - Reduced redundant I2C interactions in mute/unmute path, keeping UI response snappy

- **Display-Off / Activity Behavior improvement**
  - Ensured display wake path correctly marks user activity when screen was turned off by timeout
  - Keeps menu timeouts and auto-sleep behavior consistent

- **Stable boot state improvement**
  - Explicit initialization of key runtime globals (flags/timers/counters) to avoid random power-up state

- **Defaults cleanup refactor (no logic/EEPROM changes)**
  - `BD`/`BM` macros for repeated band defaults + `#undef BD/#undef BM`
  - Consolidated identical `defaultModeSettings[]` into single `DEFAULT_MODE_SETTINGS`
  - `initModeSettingsDefaults()` uses it (EEPROM format and sync logic unchanged)

---

## Optimizations

- **Flash Footprint Micro-Savings (Critical 32KB Fit)**
  - Applied small guard clause refactors (early returns) and simplified mode-branching where it reduced generated code
  - Changed several `const` globals to `static constexpr` to eliminate unnecessary data storage
  - Minor reductions in button handler logic helped recover few dozen bytes of Flash headroom in tight builds

- **Safe Stop-Seeking Callback**
  - Refactored `checkStopSeeking()` critical section to preserve/restore `SREG` instead of calling `interrupts()` unconditionally
  - Prevents rare edge cases where seek callback could accidentally re-enable interrupts from nested/atomic context while seek is running

- **SSD1306_OLED.h Display Driver v1.8**
  - Removed unused `setScale()` function and `_scaleX`, `_scaleY`, `_maxY` variables
  - Replaced `constrain()` calls with inline `OLED_CLAMP` macro to reduce function call overhead
  - Unified `beginData()`, `beginCommand()`, `beginOneCommand()` via single `beginTransmMode()` helper
  - Replaced manual buffer clear loop with `memset()` in `prepareLocalBuffer()`
  - Optimized `clear(x,y,x1,y1)` to use single `while` loop instead of nested `for` loops
  - Simplified `getFont()` return logic with ternary operator

------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v7.0.0**

`Memory.h / RadioControl.h / Utils.h / UI.h / ATS_EX.ino / SimpleButton.h / SimpleButton.cpp / Defines.h / Globals.h / SSD1306_OLED.h / CustomFonts.h`

---

## Added

- **Compile-time EEPROM layout guards**
  - Added `static_assert` checks in `Memory.h` to catch EEPROM map/struct packing regressions at compile time (block overlaps, packing changes, band count bounds, EEPROM size limit)
  - Ensures updated EEPROM layout stays consistent when changing `g_bandCount`, structs, or block addresses

---

## Fixed

- **EEPROM Safety (SSB Mode Clamp)**
  - Added safety clamp for `g_lastSsbMode` after loading `ReceiverHeader` from EEPROM (**only LSB/USB allowed**)
  - Protects **AM → SSB** switching from corrupted EEPROM values (like `0xFF`) that could cause invalid mode/UI indexing

- **EEPROM Safety (Brightness + DisplayOff clamps)**
  - Added clamps after loading settings from EEPROM to keep `Brightness` within **0..9** (prevents LUT out-of-bounds) and `DisplayOff` within **0..4** (prevents `T[]` out-of-bounds)
  - Eliminates rare crashes/undefined behavior on erased or corrupted EEPROM (e.g. `0xFF` values)

- **Favorites EEPROM Data Validation**
  - Added clamp for `FavoriteStation.modulation` while loading favorites from EEPROM (**must be 0..4**)
  - Prevents corrupted favorites from showing wrong mode labels and from applying invalid modes during `tuneToSelectedFavorite()`

- **Favorites selection stability (no OOB, no empty pages)**
  - Hardened favorites list maintenance by preventing underflow in `compactFavoritesFrom()` and forcing `g_favoriteSelected = 0` after `loadFavorites()`
  - Added selection sanitization in `showFavorites()` to avoid "blank page" rendering when selection is out of range

- **EEPROM Safety (Favorites save clamp)**
  - Added local clamp in favorites saving to prevent EEPROM out-of-range writes if `g_totalFavorites` is ever corrupted in RAM

- **Seek Result Persistence**
  - Updated `finalizeSeekUpdate()` in `RadioControl.h` to mark state dirty after seek
  - Ensures found station will be saved by normal **save-on-idle** logic (won't be lost after power-off)

- **EEPROM Reset Defaults (AVC = 10 for SSB contexts)**
  - Fixed factory-reset path to initialize **ALL** mode contexts (AM/LSB/USB) via `initModeSettingsDefaults()`
  - Prevents old bug where only current context was initialized and **LSB/USB AVC could stay at 0**, making SSB audio too quiet after EEPROM reset

- **Per-band BFO calibration persistence (EEPROM)**
  - Previously BFO calibration could appear "per-band" in menu, but after reboot it could reset or behave globally because only global settings block was persisted
  - Added `bfoCal` to `BandStatePacked` (**6 bytes per band state**) and now store/load it per band from EEPROM
  - Calibration applied immediately in SSB/CW, including when switching bands (each band keeps its own correct offset)

- **FM volume compensation consistency (EEPROM)**
  - Save user volume (`g_volume`) to EEPROM to avoid FM compensation corrupting persisted volume

---

## Changed

- **OLED I2C speed (fixed, no menu option)**
  - I2C bus speed now fixed via `I2C_BASE_HZ` (default **77 kHz**) and applied by `applyI2CSpeed()` with CPU prescaler compensation
  - SSB patch upload still switches to fast I2C temporarily (500 kHz request) and restores fixed bus speed afterward

- **Expanded band table (36 bands) + updated EEPROM map**
  - Expanded **SW broadcast & amateur** band segmentation to **36 total bands** (LW/MW and FM behavior unchanged)
  - Updated EEPROM layout to store **36 band states** (`36 * 6B = 216B`) and shifted subsequent blocks accordingly
  - Bumped `APP_VERSION` to force EEPROM reset due to layout change

- **SSB Sync handler cleanup (No Behavior Change)**
  - Updated `doSync()` (`ATS_EX.ino`) to use `switch` branching for LSB/USB, while keeping original "no Sync in CW" rule
  - Makes mode-specific logic easier to read and maintain

- **EEPROM band state pack/unpack cleanup (no behavior change)**
  - Simplified band state packing by introducing local `PACK4`/`UNPACK_LO`/`UNPACK_HI` macros in `Memory.h`
  - Replaced hardcoded clamp limits with existing project constants (`g_bwSSBMaxIdx`, `g_maxFilterAM`, `AM_STEPS_COUNT`, `SSB_STEPS_COUNT`, `g_lastStepFM`, etc.)
  - Centralized EEPROM address math into local `uint16_t addr` variables for bands and favorites

- **Settings.h — unified settings storage + access API**
  - Introduced `g_SettingsMeta[]` (**PROGMEM**) as single source of truth for each setting: 3-char name, default value, UI type, callback, "isActive" predicate
  - Introduced `g_SettingsParams[]` (**RAM**) as single mutable settings buffer used by menu and runtime logic
  - Added consistent Settings API (used everywhere instead of direct array access):
    - `getSettingParam()` / `setSettingParam()` — read/write setting value
    - `settingRef()` — reference access for in-place modifiers
  - Added PROGMEM helper accessors: `getSettingName()`, `getSettingType()`, `getSettingDefault()`, `callSettingCallback()`, `isSettingActive()`
  - Added `initSettingsDefaults()` to correctly populate `g_SettingsParams[]` from PROGMEM defaults after EEPROM reset/invalid EEPROM

---

## Optimizations

- **Removed Legacy EEPROM Delay Stub (Flash Savings)**
  - Removed obsolete `resetEepromDelay()` stub (old `g_storeTime` logic, only forced `g_previousFrequency = 0`)
  - Real EEPROM saving behavior stays the same (still based on `g_lastUserActivityTime` + `g_stateIsDirty`)

- **UI Micro-Refactor (Flash Savings, No Behavior Change)**
  - Reworked `stereoIndicatorChar()` and `showStep()` to use `switch`-based mode branching while keeping same UI behavior

- **Button handling: faster polling + Flash savings (no behavior change)**
  - `SimpleButton` caches correct `PINx` register pointer and bit mask (`_pinReg`, `_pinMask`) so `checkEvent()` doesn't re-calculate port/bit every call
  - `pin` extraction (`_PinDebounceState >> 10`) done only when callback actually called (lazy), reducing work in common "no press" path

- **SSD1306_OLED.h — Seven-seg tables (Flash Savings, No Behavior Change)**
  - Replaced template static members (`symbolMasks`, `segs`) with global `static const PROGMEM` tables to avoid template static overhead
  - Flattened segment table into 32-byte array and simplified segment fetch logic using pointer stride (`ptr += 4`)
  - Added compile-time "editing helpers" without affecting binary size: `OLED_SEG_A..OLED_SEG_DOT` bit macros, `OLED_SEG4(x,y,len,h)` macro
  - Build result: **Program size 30596 bytes** (≈ **124 bytes free**), RAM unchanged

- **SSD1306_OLED.h v1.9 — Flash & RAM savings (~36 bytes Flash, ~2 bytes RAM)**
  - Removed `delayMicroseconds(2)` from `endTransm()` — I2C bus timing already sufficient at 77 kHz
  - Simplified segment definition reads using `memcpy_P()`
  - Streamlined `getFont()` — removed redundant `col >= FONT_COLS` check
  - Removed unused `OLED_ONE_DATA_MODE` define
  - Changed `_maxRow` and `_maxX` from instance constants to `static constexpr` (saves RAM per instance)

- **CustomFonts.h — Compact 5-column font storage (~49 bytes Flash)**
  - Font glyphs in `_charMap_min[][]` now store **5 columns** instead of 6 (6th spacing column was always `0x00`)
  - Spacing column generated at runtime in `write()` — supports inversion correctly (`0xFF` when inverted)
  - Updated `getFont()` to work with `FONT_COLS = 5`
  - Total savings: **49 bytes** (49 glyphs × 1 byte each)

- **Encoder interrupts: removed `attachInterrupt()` (Flash savings, no behavior change)**
  - Replaced Arduino `attachInterrupt()` with direct AVR external interrupts **INT0/INT1** (Nano **D2/D3**)
  - Added `ISR(INT0_vect)` / `ISR(INT1_vect)` that call `rotaryEncoder()`
  - Result: ≈ **88 bytes** saved, encoder behavior unchanged

- **Rotary.cpp: faster pin sampling (no behavior change)**
  - Simplified encoder pin read to single shift+mask: `uint8_t pinstate = (PIND >> 2) & 0x03;`
  - Fewer instructions in hot path (ISR-driven polling)

- **SSD1306_OLED.h — Removed Arduino `Print` dependency (Flash savings)**
  - `GyverOLED` no longer inherits from `Print` (avoids pulling `Print.cpp` and related C++ ABI code)
  - Implemented minimal local `print()` overloads used by UI (`char`, `const char*`, `F("...")`, integer types)

- **SSD1306_OLED.h — Removed libc `utoa/ultoa` dependency (extra Flash savings)**
  - Replaced `utoa()/ultoa()`-based number printing with local integer-to-text conversion
  - Avoids pulling `utoa_ncheck.o` and `strrev.o` from avr-libc

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v7.0.1**

`Settings.h / UI.h / Input.h / RadioControl.h / ATS_EX.ino`

---

## Added

- **Column-first navigation tables (PROGMEM)**
  - Added `g_navColFirstOrder[29]` and `g_navColFirstReverse[29]`
  - Macro-generated for maintainability, handles partial last page correctly

---

## Fixed

- **Settings menu layout stability**
  - Menu items now always render in Row-first order regardless of NAV setting
  - NAV affects only cursor movement order, not item positions

- **SSB band switching cascade bug**
  - Removed `bandSwitch()` calls from `bfoFastRollover()` and `bfoPreciseRollover()`
  - Band now determined by frequency table lookup after rollover calculation
  - Eliminates incorrect band jumps (e.g., 75m→60m→49m→41m) when tuning near band edges
  - Fixes direction error where BFO sign was used instead of frequency boundary crossing

---

## Changed

- **NAV setting behavior**
  - `doNavStyle()` no longer triggers menu redraw
  - Cursor path switches between linear (0→1→2→3...) and column-first (0→2→4→1→3→5...)

- **SSB rollover architecture**
  - `bfoFastRollover()` normalizes negative BFO remainder to 0..999 Hz
  - `bfoPreciseRollover()` base kHz when absolute frequency exits band boundaries
  - `doFrequencyTuneSSB()` finds correct band via inline frequency table search

---

## Optimizations

- **Settings API unified via templates**
  - Simplified PROGMEM accessors to `uint8_t` parameter only

- **Removed redundant functions (Flash savings)**
  - Removed `calcSettingPosRowFirst()`, `calcSettingPosColumnFirst()`, `wrapAroundIndex()`
  - Navigation logic consolidated into `getNextSettingIndex()`

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v7.0.2**

`SSD1306_OLED.h / UI.h / Utils.h / Globals.h / RadioControl.h / Memory.h / ATS_EX.ino`

---

## Added

- **`ssbChipRate()` - centralized rate-limited chip update**
  - 16-bit timer (2 bytes RAM saved vs 32-bit, safe for 25ms window)
  - Cache invalidation on band/mode change prevents state desync after external changes
  - Bandwidth updates removed from SSB hot path to avoid I2C bursts during tuning; BW applied only on band change
  - Single I2C entry point for entire SSB tuning path

- **`MIN_SETFREQ_INTERVAL_MS` constant in `Defines.h`**
  - Default: 25ms - minimum time between chip commands in SSB mode
  - Increase to 30-35ms if using high-PPR encoders (>50 PPR)

- **Lightweight number output helpers (UI-side)**
  - `oledPrintU8_2()` - width 2, space padded (for RSSI / counters)
  - `oledPrintU8_3()` - width 3, space padded (for battery %)
  - `oledPrintU8_min()` - minimal width, no padding (for tight counters)
  - `oledPrintDec2()` - 2-digit with leading zero (for `.XX` suffix)
  - Helpers build small char buffer and call `oled.print()` - single I2C transaction per number

- **EEPROM settings validation on boot (`validateLoadedSettings()`)**
  - Added `validateLoadedSettings()` in `Memory.h` after loading EEPROM settings
  - Sanitizes loaded `int8_t` settings to documented hardware ranges (AN332 reserved-bits safe)
  - Prevents erased/corrupted EEPROM values (`0xFF` -> `-1` -> `0xFFFF`) from being sent to `SI4735::setProperty()`
  - Applied to FM soft mute (0x1302/0x1303), AM/SSB soft mute threshold (0x3303), squelch, and mode-dependent settings tables
  - EEPROM layout unchanged (no version bump needed)

- **AN332-compliant Hi-Cut constants in `Defines.h`**
  - Added property address constants: `FM_HICUT_SNR_HIGH_THRESHOLD_PROP` through `FM_HICUT_CUTOFF_PROP`
  - Added AN332 default value constants: `FM_HICUT_SNR_HIGH_DEFAULT`, `FM_HICUT_SNR_LOW_DEFAULT`, `FM_HICUT_ATTACK_DEFAULT`, `FM_HICUT_RELEASE_DEFAULT`, `FM_HICUT_MP_TRIGGER_DEFAULT`, `FM_HICUT_MP_END_DEFAULT`
  - Kept legacy constant names with corrected values for backward compatibility

- **Bounded `waitToSend()` (prevents hard hangs)**
  - Replaced original infinite CTS polling loop with bounded spin loop (`SI4735_CTS_SPINS`)
  - Validates I2C read (`requestFrom(...) == 1`) and ignores invalid `Wire.read()` results
  - Exits on `CTS=1`, `ERR=1`, or when spin limit reached, so MCU cannot hang forever waiting for CTS

- **AN332 reference PDFs bundled into project**
  - Added `PDF` folder with multiple SI47xx AN332 revisions and related documentation for quick offline access

- **Local SI4735 library sources (for stability and easier patching)**
  - Included SI4735 library files directly in project to avoid dependency/version drift until upstream releases official fix

---

## Fixed

- **`drawInverted<T>` template type safety**
  - Replaced generic template with two explicit overloads: `drawInverted(..., const char*)` and `drawInverted(..., const __FlashStringHelper*)`
  - Prevents accidental numeric-type instantiations that could cause undefined behavior

- **SSB tuning lockup on fast encoder rotation**
  - Root cause: `waitToSend()` in PU2CLR library has no timeout - I2C command floods cause infinite CTS wait loop
  - Added `ssbChipRate()` - rate-limits chip commands to once per `MIN_SETFREQ_INTERVAL_MS` (25ms)
  - UI updates immediately on every encoder tick, chip updates deferred
  - Reduces I2C bus load by 60-87% depending on encoder speed

- **RSSI/SNR display (source-driven)**
  - UI clears RSSI only on `UI_SIGNAL_NO_VALUE` (no mode-specific `RSSI_AM_Off` checks in UI)
  - RSSI off-policy handled in `RadioControl` to prevent stale/ghost readings and unnecessary RSQ polling

- **Volume mute/unmute state desync (UI shows value but audio stays muted)**
  - Root cause: mute path used direct `g_si4735.setVolume(0)` which bypassed `applyCompensatedVolume()` cache (`s_last`)
  - `applyCompensatedVolume()` now treats `g_muteVolume` as hardware state and updates `s_last` accordingly
  - `handleVolumeDownShortPress()` no longer calls `setVolume(0)` directly - all volume changes go through `applyCompensatedVolume()`

- **Setting name print safety**
  - `getSettingName()` copies 4 bytes with no `'\0'`
  - UI now guarantees null-termination before `oled.print()` (`nameBuf[5]`, `nameBuf[4]=0`)

- **FM Force Mono behavior (AN332-compliant blend thresholds)**
  - Root cause: `setFmStereoMode(true)` was using `monoVal=0` for `0x1800` (RSSI stereo threshold) and `0x1804` (SNR stereo threshold), which does **not** force mono per AN332 rules
  - Updated `setFmStereoMode()` blend table to apply correct mono-forcing values: RSSI/SNR thresholds: `127` forces mono; Multipath thresholds keep inverted logic: `0` forces mono
  - Ensures `ForceMono` menu setting actually forces mono across RSSI + SNR + Multipath blend engines

- **FM Hi-Cut filter AN332 compliance**
  - Root cause: old code used incorrect legacy constants that did not match AN332 property definitions
  - Per AN332: there is NO separate "enable" property at 0x1A00 - Hi-Cut disabled when `FM_HICUT_CUTOFF_FREQUENCY FREQ[2:0] == 0`
  - Rewrote `applyFmHiCutProfile()` to properly configure all 7 Hi-Cut registers
  - Old disable path only cleared 2 registers (0x1A00, 0x1A06), leaving stale values in 0x1A01-0x1A05

---

## Changed

- **`doFrequencyTuneSSB()` restructured**
  - All math (BFO rollover, band lookup) and UI updates execute without I2C
  - `ssbChipRate()` called as final step with rate limiting
  - DSP bandwidth (`doBandwidth(0)`) applied only on band change (BW does not depend on frequency)
  - Removed inline I2C calls that could flood bus

- **Lightweight number helpers refactored for I2C batching**
  - `oledPrintU8_2()`, `oledPrintU8_3()`, `oledPrintDec2()` now use `char buf[]` + `oled.print(buf)`
  - Reduces I2C transactions from 2-3 per number to 1

- **Lightweight number helpers: `divmod10()` extracted as shared division helper**
  - Replaced 6 inline copies of `q = v / 10; r = v - q * 10` with single `divmod10()` function
  - Returns `DivMod10` struct (two `uint8_t` fields) — maps to register pair on AVR, zero RAM overhead
  - Compiler emits ONE hardware division per call site (no separate `/` and `%` instructions)

- **UI no longer uses `oled.print(number)`**
  - Battery %, RSSI display, settings page counters, favorites counters and frequencies now rendered via lightweight helpers or direct digit writes
  - Consistent rendering behavior across all numeric displays

- **SSB Cutoff filter logic centralized**
  - Added helper `ssbAutoCutoffFromBwIdx(hwBw)` to centralize auto-cutoff rule for common bandwidths
  - Added helper `ssbCutoffForHwBw(hwBw)` to compute final cutoff value (AUTO / manual) in one place
  - `updateSSBCutoffFilter()` now uses `ssbCutoffForHwBw()` as single source of truth

- **`configureSSBMode()` cutoff/bandwidth apply order**
  - SSB mode configuration uses `configureSSBModeBatch()` to apply core DSP parameters
  - Cutoff filter applied right after via `updateSSBCutoffFilter()` to keep final audio profile consistent after reconfig

- **`Defines.h` reorganized with clear section headers**
  - Added structured section dividers (`// ===== SECTION NAME =====`)
  - Grouped constants by functionality: EEPROM, Timing, Hardware Pins, Compile Switches, Audio Profiles
  - Improved inline documentation with AN332 references

- **`applyFmHiCutProfile()` completely rewritten**
  - Now configures all 7 Hi-Cut registers (0x1A00-0x1A06) instead of partial subset
  - Speaker EQ profile uses SNR thresholds of 127 to keep filter always engaged
  - Default/disable profile properly restores all AN332 defaults before disabling
  - Uses symbolic constants from `Defines.h` instead of magic numbers

- **`SMeter` setting scope**
  - `SMeter` now affects **AM/SSB/CW only**
  - FM ignores `SMeter` and always displays raw RSSI

- **`rssiToSLevel()` simplified (HF-only)**
  - Removed FM-specific S-meter mapping logic from `rssiToSLevel()`
  - S-meter conversion now only used for AM-family modes

- **Favorites page rendering cleanup**
  - Favorites page bounds unpacking centralized via `fav_unpackPageBounds(...)`
  - No behavioral changes (same redraw/cursor-update logic, same empty-list behavior)

- **AVC update flow**
  - AVC changes and mode reconfiguration now use single HW entry point (`applyAvcGainHW`) to keep behavior consistent and reduce duplicated code under LTO

---

## Optimizations

- **Removed heavy numeric printing from OLED driver (~180 bytes Flash)**
  - Dropped `printUnsigned(uint32_t)`, `printSigned(int32_t)` and all numeric `print()` overloads
  - Keeps only string printing: `print(char)`, `print(const char*)`, `print(F("..."))`
  - Avoids pulling in 32-bit division/formatting code from avr-libc

- **`sendByte()` simplified for microWire (~20 bytes Flash, 1 byte RAM)**
  - Removed `_writes` counter and `I2C_BATCH` mechanism - unnecessary with bufferless microWire
  - Cleaner code path: `Wire.write(data)` only

- **Text batching via `writeCore()` + `print()` wrappers**
  - Added `writeCore()` - character output without `beginData()`/`endTransm()` calls
  - `print(const char*)` and `print(F("..."))` now wrap entire string in single I2C transaction
  - Reduces START/STOP events per string from N to 1

- **`g_bandIndex` type changed from `int8_t` to `uint8_t` (~20-40 bytes Flash)**
  - Eliminates signed extension instructions after index operations
  - Uses `mul` instead of `muls` for band address calculation
  - Reduces code size across all functions accessing `g_bandList[g_bandIndex]`

- **`bandSwitch()` rewritten without modulo operator (~15-25 bytes Flash)**
  - Replaced `(g_bandIndex + delta + g_bandCount) % g_bandCount` with explicit `if/else`
  - Avoids expensive `%` operation and signed/unsigned mixing on AVR

- **Band configuration functions now accept `Band&` parameter**
  - `configureFMMode(const Band&)`, `configureAMMode(const Band&, uint16_t, uint16_t)`, `configureSSBMode(Band&, uint16_t, uint16_t, bool)`
  - Eliminates repeated `g_bandList[g_bandIndex]` address calculations inside sub-functions

- **`applyBandConfiguration()` caches band reference and fields**
  - Single `Band& band = g_bandList[g_bandIndex]` at function entry
  - Extracts `minFreq`, `maxFreq` into local variables before passing to helpers
  - Reduces redundant `index × 18 + base` recalculations after each `call`

- **`partialUpdate()` loop structure**
  - Moved `data == NULL` check out of per-byte loop
  - Separate tight loops: fill with `0x00` or stream from `*data++`

- **Fast OLED window setter for hot paths**
  - Introduced `setWindowRaw(x0, y0, x1, y1)` - no bounds clamping, minimal I2C commands
  - Hot calls (cursor positioning, fill operations) switched to `setWindowRaw()`

- **Batched remaining small UI outputs to reduce I2C noise**
  - Added small buffers for short fragments that were printed via multiple `oled.write()` calls
  - Fewer I2C START/STOP events → less EMI / "bus noise" impact during redraws

- **Startup redraw path (reduced redundant clears / full redraws)**
  - Removed extra `oled.clear()` from `initOLED()`
  - `showSplashScreen()` no longer clears screen after hold delay
  - `applyInitialConfiguration()` no longer performs `oled.clear()` + `showStatus()`; reads back real tuned frequency and updates only digits area
  - Result: fewer full-screen clears during boot and less I2C traffic

- **`applyCompensatedVolume()` with hardware volume caching**
  - Added `static uint8_t s_last` cache to skip redundant `setVolume()` I2C calls
  - Generated asm uses tail-call to `SI4735::setVolume()` (`rjmp`)

- **Seek bandwidth apply reduced to band changes only**
  - `doSeek()` stores `oldBand` before seek and computes `bandChanged` after SW remap
  - `finalizeSeekUpdate(bool bandChanged)` calls `doBandwidth(0)` only when `bandChanged` is true

- **Settings item positioning (hot path)**
  - Switched to `localIdx` (0..5) + bit ops instead of `% UI_SETTINGS_PER_PAGE`
  - Fewer cycles and avoids pulling extra div/mod helpers

- **Reduced redraw/I2C traffic in Settings**
  - Keep split redraw path: `full row` vs `value-only` update
  - Prevents redundant name redraws during cursor moves / value edits

- **Removed duplicate Favorites page bounds calculation**
  - Moved `fav_getPageBounds(page)` to out-of-line helper (`noinline`) returning packed bounds

- **Deduplicated current `bandType` access**
  - Added `currentBandType()` (`noinline`) as lightweight accessor
  - Reduces repeated `index × sizeof(Band) + base` address calculations

- **Deduplicated AVC hardware apply path**
  - Added `applyAvcGainHW(avcIndex)` (`noinline`) and routed both mode apply path and menu handler through same HW function

- **Reduced band pointer recalculation inside UI hot paths**
  - `showBandTag()` uses single `const Band&` for both `bandType` and `name` access
  - `showStep()` preloads step indices before `switch`

- **Removed FM soft-mute sign-extension overhead**
  - FM soft-mute properties now pass values as `(uint16_t)(uint8_t)getSettingParam(...)` to avoid signed extension

- **Reduced volatile reread in `switchCommand()`**
  - Switched "CMD_NONE?" decision to use known local `newMode` instead of rereading `g_activeCommand`

- **Favorites bounds unpack helper (readability-only, no size impact)**
  - Added `fav_unpackPageBounds(page, start, end)` (`inline`)

------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v7.1.0**

`SI4735_fixed.h / RDS.h (new) / ATS_EX.ino / UI.h / MEMORY.h / Battery.h / Defines.h / Input.h / Globals.h / RadioControl.h / Boot.h (new) / SMeter.h (new) / Favorites.h (new) / SettingsData.h / SettingsLogic.h / microWire.h / microWire.cpp`

---

## Added

- **RDS RadioText display for FM stations (`RDS.h`, `SI4735_fixed.h`)**
  - Decodes RDS Group 2A and 2B packets to display station RadioText (song title, artist, promo messages)
  - Text shown on bottom row and auto-scrolls when longer than 21 characters
  - Clears on frequency change, signal loss (6 s timeout), or when station updates text
  - Toggle on/off with long press MODE while in FM; status indicator **"RS"** shown when active
  - Lightweight implementation (~500 bytes flash) and does not use heavy PU2CLR RDS stack
  - Compile-time switch `ENABLE_RDS_MINI` in `Defines.h` to disable if flash space critical

- **Mini Pong game (`Game.h`)**
  - Simple single-player Pong game playable while listening to FM
  - Paddle control via encoder; long press MODE to start/exit
  - Text-based rendering using existing compact font (no graphics buffer overhead)
  - Compile-time switch `ENABLE_GAME` in `Defines.h`

- **Boot sequence reorganization (`Boot.h`)**
  - Hardware initialization (CPU clock, I2C speed, interrupts, EEPROM reset detection) moved into dedicated module
  - Makes main sketch cleaner and startup logic easier to follow
  - No additional compiled code; implemented as include module

- **S-meter display module (`SMeter.h`)**
  - RSSI→S-point conversion logic moved out of UI code into own module
  - Simplifies tuning thresholds and enables future VHF/UHF scale support

- **Favorites management module (`Favorites.h`)**
  - Favorites features (add/delete/tune/list + EEPROM save/load) consolidated into one module
  - Improves maintainability by keeping all favorites logic together

- **Settings code split (`SettingsData.h`, `SettingsLogic.h`)**
  - Setting metadata (names/defaults/types) separated from handler logic (`do*()` callbacks)
  - Keeps structure cleaner without increasing firmware size

- **SW Link (SWL) — link STEP and BW across all SW sub-bands**
  - New setting `SWL` (0/1) links **AM + SSB** step and bandwidth indices across all SW segments
  - Master SW segment: `SW_MASTER_BAND_INDEX = 2`
  - When enabled:
    - Entering any SW segment applies master STEP/BW to current SW band
    - Changing STEP or BW in any SW segment updates master values
    - Enabling SWL normalizes all SW segments to master
    - Startup normalizes SW segments after EEPROM load
  - No effect on LW/MW/FM bands
  - Partial EEPROM saves also include SW master band to preserve linked state across reboot

---

## Fixed

- **UI freeze during fast encoder tuning (`Input.h`, `RadioControl.h`, `Globals.h`)**
  - Fast rotation could freeze UI while audio continued playing due to encoder step accumulation during band crossings
  - Each crossing triggered full screen redraw (~300 ms) and FM↔AM transitions could trigger 2 s SSB patch load
  - Tuning functions now receive encoder delta as direct parameter (one-way flow: ISR → buffer → handler)
  - Removed unused `applySafeEncoderDeltaAndTune()` and atomic include

- **Occasional stall when tuning quickly (`ATS_EX.ino`)**
  - RSSI polling could run before new frequency committed to chip, causing I2C contention and ~1.5 s pause
  - Polling now waits until pending frequency update completes

- **Battery indicator missing after selecting alternate pin (`Boot.h`, `ATS_EX.ino`)**
  - Battery pin probe previously ran before EEPROM settings load, so it always checked default A2
  - Probe now runs after EEPROM load, so alternate pin selections (e.g., A1) work reliably

- **Band jumping through multiple SW sub-bands (`RadioControl.h`)**
  - Near SW band edges, resolved frequency could fall into gap and skip multiple bands in one step
  - Frequency now clamped to destination band edge before switching bands

- **Random resets on some bootloaders (`ATS_EX.ino`)**
  - Some Optiboot builds leave watchdog enabled; long operations (e.g., SSB patch load) could exceed timeout
  - Watchdog now disabled immediately at startup

- **EEPROM reset sometimes missed on power on (`ATS_EX.ino`)**
  - Added short confirmation delay for reset key combo (ENCODER or AGC held at boot) to handle contact bounce

- **SW AFC settings not applied after band switch (`RadioControl.h`)**
  - Switching between MW and SW now reapplies correct AFC profile reliably

- **I2C bus hang on signal loss or probe failure (`microWire.cpp`)**
  - Replaced all infinite `while(!(TWCR & _BV(TWINT)))` loops with `twiWaitTwintOrReset()` (~40 ms timeout at 16 MHz)
  - On timeout, TWI hardware reset (`TWCR = _BV(TWEN)`) to release bus
  - Prevents permanent firmware hang when SDA/SCL held low, slave stuck, or pull-ups missing

- **`read()` underflow when called more times than `requestFrom()` length (`microWire.cpp`)**
  - `_requested_bytes` is `uint8_t`; decrementing from 0 previously wrapped to 255, causing 255 extra reads with ACK
  - Added guard: `if (_requested_bytes == 0) return 0`

- **SI4735 CTS polling could hang forever or spin for ~3 seconds when chip absent (`SI4735_fixed.h`)**
  - Original library had no timeout; missing CTS could hang indefinitely
  - With microWire, `read()` returns 0 on NACK (not 0xFF like standard Wire), so CTS bit never sets
  - Added `Wire.available()` check after `requestFrom()` to detect NACK and exit immediately
  - Added spin limit (5000 iterations) to prevent infinite loop if CTS never arrives

- **Comment typo in `endTransmission()` (`microWire.cpp`)**
  - Corrected comment: `return 3` was incorrectly documented as returning 2

- **I2C speed drift when changing CPU prescaler (`Boot.h`)**
  - `Wire.setClock()` computes timing from compile-time `F_CPU` and ignores `CLKPR` division
  - `applyI2CSpeed()` now writes `TWBR` directly based on actual CPU clock so SCL stays correct after changing `CPUSpeed`
  - `TWSR` forced to 0 (prescaler 1) to match formula and microWire defaults

---

## Changed

- **Band map expanded for finer SW segmentation (`Globals.h`)**
  - `g_bandCount` increased from 38 to 44
  - Additional SW sub-ranges added to make band navigation more granular
  - LW/MW and FM entries unchanged

- **EEPROM map updated for 44 bands (`Defines.h`)**
  - Band state block resized to `44 × 6` bytes
  - All following EEPROM blocks shifted accordingly
  - Note: this build requires EEPROM reset after update

- **OLED init path cleanup (`ATS_EX.ino`)**
  - Removed redundant `setPower(true)` call after `oled.init()` (init sequence already enables display and charge pump)

- **Encoder pin init deduplicated (`ATS_EX.ino`)**
  - Removed duplicate PD2/PD3 pin setup in `initHardwarePins()` (encoder pins configured by global `Rotary` constructor)

- **External interrupt setup made direct (`ATS_EX.ino`)**
  - `initSi4735()` now writes final values to `EICRA/EIMSK` for INT0/INT1 CHANGE directly (no read-modify-write)

- **AMP control init centralized (`ATS_EX.ino`, `RadioControl.h`)**
  - AMP pin DDR configured once at startup
  - `setAmpState()` now toggles PORT only (no DDR writes at runtime)

- **Favorites UI + persistence moved into module boundary**
  - Favorites list rendering moved from `UI.h` to `Favorites.h`
  - Favorites save/load helpers moved under `Favorites` module
  - Input dispatch remains in `Input.h`

- **Compact font updated for RDS RadioText readability (`CustomFonts.h`)**
  - Added lowercase `a–z` glyph support in compact font map/lookup
  - Improves readability of mixed-case RDS RadioText

- **Dead cross-band helper removed (`RadioControl.h`)**
  - Removed unused `resolveCrossBandFreq()` and `currentBandStep()` (superseded by `tuneResolveCrossBandFreq()`)

- **microWire I2C library upgraded to v2.3**
  - `begin()` uses direct port register init instead of two `pinMode()` calls (≈140 bytes flash saved)
  - Added `twiCommand()` helper for centralized TWI command dispatch with built-in timeout
  - TWI status codes replaced with named enum constants in write path
  - Added SLA+R NACK (0x48) handling (prevents hang when slave does not ACK read request)
  - `beginTransmission()` and `requestFrom()` skip `write()` after failed `start()`
  - Removed dead include `pins_arduino.h`

- **Boot code cleanup and helper split (`Boot.h`)**
  - `initFast()` split into smaller helpers `initTimer0()` and `disableBootloaderUART()`
  - Encoder interrupt init extracted into `initEncoderInterrupts()`
  - EEPROM reset helpers made explicit (`clearEEPROMVersion()` etc.)

---

## Optimizations

- **Favorites header counter logic**
  - `oledPrintFavCounter()` rewritten to use fixed position buffer fill instead of right to left `b[i--]`
  - Avoids heavy pointer/index arithmetic for variable index writes

- **EEPROM save path (less repeated checks)**
  - APP_ID and VERSION stored via single `eeprom_update_word()` instead of two `eeprom_update_byte()`
  - Settings block stored via `eeprom_update_block()` instead of 29 individual `eeprom_update_byte()` calls
  - Removed redundant `CHECK_EEPROM_WEAR()` from `saveBandState()` and inside `saveBands()` and `handleModeSettingsEEPROM(true)`

- **Battery UI update timer uses 16-bit timestamp**
  - `updateAndShowBattery()` uses `uint16_t lastChargeShow16` (wrap safe for 10s interval)
  - Avoids unnecessary 32-bit time arithmetic

- **SI4735_fixed init path kept minimal (project specific)**
  - `setup()` avoids redundant `Wire.begin()` since OLED init already calls it
  - `getDeviceI2CAddress()` does not call `Wire.begin()`

- **SSB/CW BFO update path avoids heavy helper call**
  - `updateBFO()` now sets BFO via `g_si4735.setProperty(SSB_BFO)` instead of calling `SI4735::setSSBBfo()`

- **Favorites add path avoids redundant slot recompute**
  - `addFavorite()` rewritten to walk favorites with pointer and reuse final slot address for insertion

- **Favorites delete compaction (`Favorites.h`)**
  - `compactFavoritesFrom()` rewritten as flat forward byte copy (safe for overlapping regions)

- **Battery percent curve calculation deduplicated**
  - `calculateRawPercent()` refactored to use single noinline helper (`calcBatterySegmentPct()`) for all 4 piecewise linear zones

- **UI reduced duplicate band/mode checks in inversion logic**
  - `showBandTag()` and `showModulation()` avoid extra bandType checks for inversion decisions

- **UI timeout bookkeeping switched to 16-bit timer**
  - `g_lastAdjustmentTime` changed from `uint32_t` to `uint16_t` with 16-bit delta math for timeout checks

- **`applyBandConfiguration()` FM configuration moved out of line**
  - `configureFMMode()` marked noinline

- **SI4735 disable FM debug noise command uses byte `Wire.write()` overload**
  - `disableFmDebug()` uses `Wire.write((uint8_t))` to avoid selecting wider overload under microWire

- **Custom minimal init replaces Arduino core `init()`**
  - `main()` uses project local `initFast()` instead of calling Arduino core `init()`
  - Keeps Timer0 timebase (millis/delay), disables UART, skips non-required peripheral init

- **Settings menu page computation simplified (`Input.h`)**
  - `calculateSettingsPage()` replaced with compact math instead of if-chain
  - Guard: `static_assert(SETTINGS_MAX <= 131)`

- **Frequency update housekeeping simplified (`ATS_EX.ino`)**
  - `handleDelayedFrequencyUpdate()` simplified (encoder delta already coalesced earlier)

- **SEEK post-processing reduced (`SI4735_fixed.h`, `RadioControl.h`)**
  - `executeHardwareSeek()` now uses final frequency produced by seek routine directly avoiding extra post-seek frequency/status fetch

- **Tuning size reduction (`RadioControl.h`)**
  - `doFrequencyTune()` caches current band fields to reduce repeated `g_bandList[g_bandIndex]` address recomputation

- **Favorites EEPROM I/O (`Favorites.h`)**
  - Switched save/load from per-item loops to bulk `eeprom_update_block()` / `eeprom_read_block()`

- **Band state packing (`MEMORY.h`)**
  - `saveBandState()` packs step/bw nibbles using swap based shift to reduce AVR codegen size

- **Button FSM codegen (`SimpleButton.cpp`)**
  - `checkEvent()` uses 8-bit state (4-bit field) to avoid 16-bit compares and dead high byte ops

- **Activity timers (`ATS_EX.ino`, `Utils.h`)**
  - Deduplicated millis()/1000 seconds bookkeeping via small helper shared by `noteUserActivity()` and `markStateAsDirty()`

- **Signal/stereo polling consolidated (`ATS_EX.ino`)**
  - `handleSignalAndStereoUpdates()` uses single `millis()` call reused by `updateFmStereoIndicator()`
  - Early exits ordered from cheapest to most expensive check

- **microWire TWI helpers reduce flash duplication (`microWire.cpp`)**
  - `twiWaitTwintOrReset()` and `twiCommand()` marked noinline so single copy shared by 4 call sites
  - `twiStatus()` marked inline and compiles to single in plus andi pair

- **Band pointer dedup**
  - Added `currentBandPtr()` helper to reduce repeated `g_bandList[g_bandIndex]` address recomputation
  - Migrated multiple hot paths to use `currentBandPtr()`

- **OLED cursor update (`SSD1306_OLED.h`)**
  - `writeCore()` now updates x once using `x += FONT_TOTAL` instead of multiple `x++` sequences

- **ADC start sequence merged (`Utils.h`)**
  - `adcReadAx()` now writes `ADCSRA` once with `ADEN|ADSC` and prescaler

- **microWire int promotion cleanup (`microWire.cpp`)**
  - `beginTransmission(uint8_t)` writes 8-bit address to avoid selecting `write(int)` wrapper

- **SSB mode gate (`Utils.h`)**
  - `isSSB()` reads `g_currentMode` once and uses compact unsigned range check

- **SI4735 fast reset delays dedup (`SI4735_fixed.h`)**
  - Added small `delay10ms` helper used by `resetFastD12()`

- **I2C clock compensation is compile time and division free (`Boot.h`)**
  - `TWBR_TABLE[]` precomputed at compile time from `I2C_BASE_HZ` and `F_CPU`
  - Runtime path reduced to `CLKPR` clamp + `TWSR=0` + `TWBR=table[p]`

- **SI4735 SSB mode property send dedup (`SI4735.cpp`)**
  - `sendSSBModeProperty()` now reuses `sendProperty(SSB_MODE, value)`

- **SI4735 AM bandwidth property send dedup (`SI4735.cpp`)**
  - `setBandwidth(AMCHFLT, AMPLFLT)` now packs bits per AN332 and calls `sendProperty(AM_CHANNEL_FILTER, packed)`

- **SSB cutoff helper dedup under -Os + LTO (`RadioControl.h`)**
  - Removed `always_inline` from `ssbCutoffForHwBw()` and forced single shared body via `__attribute__((noinline))`

- **OLED seven segment local buffer drawing reduces int-promotions (`SSD1306_OLED.h`)**
  - `local_setPixel()` and segment line helpers updated to keep operations 8-bit where possible

- **SI4735 `sendProperty()` uses direct byte packing (`SI4735.cpp` / `SI4735_fixed.h`)**
  - Eliminates temporary `si47x_property` unions and `raw.byteHigh/byteLow` extraction
  - Confirmed in `.lst`: calls `TwoWire::write(unsigned char)`

- **SI4735 `setSSBConfig()` clears raw bytes before bitfield fill**
  - Clears `currentSSBMode.raw[0..1]` before setting `param.*` fields
  - Ensures reserved bits explicitly zeroed per AN332


------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v7.1.1**

`CW_decoder.h / ATS_EX.ino / microWire.h / microWire.cpp / Globals.h / RadioControl.h / SettingsLogic.h / RDS.h / Input.h / UI.h / Defines.h / Utils.h / CustomFonts.h` 

---

## Added

- **S-meter graph (thin signal bar)**
  - Compile-time flag `ENABLE_SIGNAL_BAR` (0/1)
  - Draws a thin RSSI strength bar between the top status line and the large frequency digits
  - Uses a compact table-based 0..49 strength mapping with integer interpolation (no floats)
  - Automatically clears the bar when RSSI is unavailable (`255`)
  - Controlled via the existing `SPT` setting (0..3: `RSSI`, `SPT`, `RSSI+BAR`, `SPT+BAR`)
  - Settings UI strings updated to match the new 4-state `SPT` (no EEPROM layout change)

- **Splash credits (scrolling “loading line” replacement)**
  - Compile-time flag `ENABLE_SPLASH_CREDITS_SCROLL` (0/1)
  - Timing knobs:
    - `SPLASH_CREDITS_STEP_MS`
    - `SPLASH_CREDITS_TOTAL_MS`
  - Text macro:
    - `APP_SPLASH_CREDITS_TEXT`
  - Reuses the same 21-column window scroller as RDS, avoiding duplicate draw logic
  
---

## Fixed

- **Compact font: underscore `_` support**
  - Added the `_` glyph to `_charMap_min`
  - Mapped ASCII 95 (`'_'`) in `_charLookup`
  - Fixes rendering of strings like `MOD_NO_RDS` (underscore no longer disappears)

- **CW decoder build order / configuration declaration**
  - `cwAdcInit_Internal1V1_A6()` referenced `AdcConfig::SETTLE_MS` before `AdcConfig` was defined
  - Moved all config structs above any code that references them

- **Goertzel math correctness (operator precedence)**
  - Expression `(int32_t)coeff * state->Q1 >> 14` relied on implicit precedence and could shift unexpectedly
  - Made the shift explicit: `((int32_t)coeff * st->Q1) >> Q14_SHIFT`

- **CW timing and decode path consistency / stability**
  - Unified dot/dash classification between decoding and adaptation
    - Previously: `classifyMarkEvent` used an additive threshold (`dotLength + 2`), while `adaptDotTiming` used a multiplicative threshold (`dotLength * 3/2`)
    - This could classify the same mark as a dot for output but a dash for speed adaptation
    - Introduced a single `isDotMark()` helper (`dotLength * 3/2`) and passed the result into adaptation
  - Improved WPM smoothing to reduce jitter
    - Replaced equal-weight smoothing `(old + new) >> 1`
    - With weighted smoothing `(old * 3 + new * 1) / 4`
  - Removed redundant / duplicated guards
    - Removed redundant `resetMorseAccumulator()` in `cwViewEnter` (already handled via `CWDecoder_begin -> initializeDecoderState`)
    - Removed duplicate `length == 0` check in the flush path (early return in `flushCurrentSymbol` is sufficient)

- **Favorites FM-to-FM tuning click (unnecessary full reconfiguration)**
  - Selecting an FM favorite while already in FM (and staying within the same FM band) no longer runs the full `applyBandConfiguration()` sequence
  - Avoids redundant amp mute/unmute and FM re-init, which could produce an audible click
  - FM preset switching now performs a simple `setFrequency()` retune, while all cross-band / cross-modulation cases keep the original full reconfiguration path
  
---

## Changed

- **Decoder state and adaptation refactor**
  - `initializeDecoderState()` now reuses `resetStateOnTransition()` to avoid duplicating state zeroing (`runLength`, `currentState`, `gateCounter`, `symbolTimeout`)
  - `adaptDotTiming()` now accepts a pre-computed `isDot` flag so timing adaptation always uses the same classification as the decoder
  - Added `decDotLength()` helper for symmetry with `incDotLength()`, replacing the inline `if (dotLength > DOT_MIN) dotLength--` in the adaptation branch

- **Display power handling refactor (flash reduction)**
  - Split inline `setDisplayPower(bool on)` into two out-of-line helpers:
    - `displayPowerOn()` and `displayPowerOff()` (both `noinline`)
  - `setDisplayPower(bool on)` is now a thin dispatcher
  - Removes duplicated “display ON” sequences previously emitted from multiple call sites (`handleAgcShortPress()`, `wakeUpDisplayIfNeeded()`)

- **UI signal quality rendering cleanup (less duplication)**
  - `showSignalQuality()` now sets the cursor once and delegates rendering to a small helper
  - Reduces repeated cursor setup and keeps RSSI/SPT formatting rules in one place

- **`loop()` register pressure reduction (prologue optimization)**
  - Moved heavy computation out of `loop()` by marking critical functions as `noinline`:
    - `handlePeriodicTasks()`
    - `handleSignalAndStereoUpdates(uint32_t, uint16_t)`
    - `handleDelayedFrequencyUpdate()`
    - `doFrequencyTune(int16_t)`
    - `doFrequencyTuneSSB(int16_t)`
  - Removed `always_inline` from BFO rollover helpers:
    - `bfoPreciseRollover()`
    - `performBfoRolloverWithBandCheck()`
  - **Result:** `loop()` prologue reduced from 18 callee-saved registers (`r2–r17`, `r28–r29`) to just 3 (`r17`, `r28`, `r29`)
  - Saves ~60 cycles per iteration (prologue + epilogue overhead)
  - Trade-off: +140 bytes flash for separate function frames

---

## Optimizations

- **Code compaction (CW + core + I2C + SI4735 + UI)**
  - **CW decoder loops and lookups**
    - `calculateGoertzelPower()` and `fillBufferAndValidate()` now use `ptr/end` iteration instead of indexed `g_acBuffer[i]`
    - `lookupMorseCharacter()` reads PROGMEM using `*ptr++` instead of `pgm_read_byte(&g_morseLookup[i])`
    - Introduced a shared `thresholdFromDot(num, den)` helper for both mark and gap thresholds
  - **CW decoder helpers**
    - Added a shared `clampU8()` for WPM bounds
    - Extracted `clampRunLength()` to centralize the overflow guard and keep `sampleAndAnalyzeSignal()` smaller
  - **Core + I2C + SSB**
    - Encoder atomic read (`getAndResetEncoderCount()`): simplified from `counter -= value` to `counter = 0` under `cli()`
    - microWire error-path deduplication: `TwoWire::failAddressNack()` and `TwoWire::failDataNack()` are reused from `start()`, `write()`, and `read()`
    - SSB fast rollover math (`bfoFastRollover()`): removed a redundant `div -> mul -> div` chain
    - Mode context caching during band reconfiguration
      - `applyBandConfiguration()` now computes `ModeContext` once and passes it into AM and SSB configuration helpers
      - Avoids repeated `getModeContext()` calls in the AM-family configuration path
    - FM step handling clean-up
      - Removed undefined behavior from FM step selection by avoiding `int16_t*` casts over the `g_tabStepFM` byte table
      - FM step values are now explicitly zero-extended when converted to `uint16_t`, preventing unintended sign-extension and making the logic robust if the table ever changes
  - **SI4735 property writes**
    - Added `siSetProperty(uint16_t prop_addr, uint16_t prop_val)` (`noinline`) bound to the global `g_si4735`
    - Replaced direct `g_si4735.setProperty(...)` call sites with `siSetProperty(...)` (including `applyProperties()`)
  - **SWLink**
    - Forced `swLinkCopySwParams()` out-of-line (`noinline`) to reduce duplicate codegen
  - **UI**
    - Favorites FM formatting: `oledPrintFreqFM()` rewritten to use fixed buffer indices instead of `b[i++]`
    - `INT1` ISR aliased to `INT0` (both use the same `rotaryEncoder()` handler)
    - `refreshCommandIndicators()` forced out-of-line (`noinline`) to keep a single shared refresh chain
    - Favorites menu micro-optimizations
      - Cached `g_favoriteSelected` once in the cursor update loop to avoid reloading it for each row
      - Cached `fav.modulation`, `fav.frequency`, and `fav.bfo` into locals before call-heavy reconfiguration code to reduce repeated address recomputation when registers get clobbered
  - **Text scrolling**
    - Reused a shared 21-column window renderer for both RDS mini and splash credits to avoid duplicate draw logic
  - **RDS decode loop compaction**
    - Replaced 4 × inline `rdsS()` calls in `rdsDecode()` with a single `rdsDecodeN()` noinline helper
    - Added `rdsBlockCDPtr()` accessor to `SI4735_fixed` for direct pointer access to contiguous CH/CL/DH/DL bytes (`raw[8..11]`)
    - Group 2B path reuses Group 2A call site via compiler tail merge (`rjmp`)
  - **millis() consolidation in periodic tasks**
    - `handlePeriodicTasks()` now computes `now` and `now_s` once and passes them to sub-functions
    - `rdsMiniTask()` accepts `uint32_t now32` parameter instead of calling `millis()` internally
    - `handleSignalAndStereoUpdates()`, `handleCommandTimeout()`, `handleSettingsSave()` accept precomputed timestamps
    - `amRssiPollingAllowed()` and `shouldSaveStateOnIdle()` share a single `millis()/1000` division
    - Eliminates 2 redundant `millis()` calls and 1 redundant 32-bit division per loop iteration
	
	
------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v7.1.2**

`SI4735_fixed.h / RadioControl.h / RDS.h`

---

## Fixed

- **FM seek frequency desynchronization (rare)**
  - After a hardware seek completed, the UI could display a stale intermediate frequency (e.g. 97.7 MHz) while the chip was actually tuned to the found station (e.g. 102.5 MHz)
  - **Root cause:** when seek was interrupted by timeout or stop-button, `seekStationProgressGetFrequency()` returned the old `currentWorkFrequency` cached *before* the `showFunc()` callback and its 100 ms delay 
	during which the chip could have already found and locked onto a station
  - **Fix:** after `getStatus(0, 1)` (cancel), the actual chip frequency is now read from the cancel response (`READFREQH`/`READFREQL`) instead of returning the stale cached value
  - Seek helper `readStatusFreq()` extracted to eliminate three identical frequency-read sequences

- **Redundant `setFrequency()` after hardware seek**
  - `finalizeSeekUpdate()` previously called `g_si4735.setFrequency(g_currentFrequency)` unconditionally after every seek — even though the chip was already tuned by the hardware seek itself
  - This redundant FM_TUNE_FREQ / AM_TUNE_FREQ command could cause a race condition with the just-completed seek, contributing to the frequency desynchronization described above
  - `setFrequency()` is now issued only when the band index actually changed (SW sub-band remap), where re-tuning is required

- **Unnecessary `fmAlign10k()` after FM seek**
  - `doSeek()` applied `fmAlign10k()` (floor-round to 10 kHz grid) to the FM seek result
  - This was redundant because `setSeekFmSpacing(10)` already guarantees the chip returns a frequency on the correct 10 kHz grid
  - In edge cases where the chip returned a frequency not perfectly aligned, `fmAlign10k()` could round it *away* from the actual tuned frequency, worsening the desync
  - Removed the `fmAlign10k()` call and the now-unused helper function

- **RDS text persisting after seek/tune**
  - After seek or manual tuning to a new station, the previous station's RDS RadioText and clock remained on screen until the 6-second stale timeout expired
  - Added `s_freq` tracking — RDS content is now cleared and redrawn instantly when `g_currentFrequency` changes

- **RDS clock overwriting scrolling RadioText**
  - Clock display ("HH:MM") was drawn over the rightmost 5 characters of the 21-column RadioText area, corrupting the scrolling text
  - RT window is now limited to 16 characters (left zone), with clock pinned to the remaining 5 characters (right zone) — both on the same OLED row but never overlapping

---

## Changed

- **`seekStationProgressGetFrequency()` cleanup**
  - Three identical `READFREQH`/`READFREQL` read-and-pack sequences replaced by a single private `readStatusFreq()` helper
  - Three separate abort conditions (ERR, stop-button, timeout) consolidated into one `if` block, eliminating duplicated cancel logic

- **RDS clock placeholder when waiting for time**
  - Previously the clock zone was blank until a Group 4A packet arrived
  - Now shows `--:--` as soon as any RDS data is received (`s_ok != 0`), indicating the receiver is locked and waiting for time broadcast

- **RDS rendering optimization (Single-pass I2C burst)**
  - `rdsRedraw()` now builds the entire 21-character row (RT + Clock) in a local buffer using inline `rdsFillRT()` and `rdsFillClock()` helpers, sending it to the display via a single `oled.print()` call
  - Eliminates 16 repetitive individual `oled.write()` calls per cycle, drastically reducing I2C bus overhead and freeing CPU cycles for the main `loop()`

- **RDS internal refactoring**
  - Extracted shared helpers to eliminate repeated patterns:
    - `rdsHasData()` / `rdsHasValidClock()` — state query helpers (replace raw comparisons)
    - `rdsElapsed(now, since)` — 16-bit millisecond delta (replaces inline casts)
    - `rdsClearContent()` — clears RT + clock + `s_ok` without touching HW state
    - `rdsClearAndRedraw()` / `rdsResetAndRedraw()` — combined state + draw helpers
    - `fmtTime5()` — formats "HH:MM" into 5-char buffer (reuses `fmtDigit2`)
  - Cached `g_si4735.rdsBlockCDPtr()` once in `rdsDecodeGroup2Chars()` instead of calling twice
  - `rdsExtractCT()` minute calculation simplified from three statements to one expression
  - `rdsDeactivateHw()` / `rdsActivateHw()` refactored to use early-return patterns

- **OLED rendering math optimization (`SSD1306_OLED.h`)**
  - Changed coordinate parameters (`x`, `y`) from 16-bit `int` to 8-bit `uint8_t` across all core drawing methods (`setCursor`, `setCursorXY`, `setWindow`, `clear`, `drawDigit`).
  - Since the 128x64 display coordinates never exceed 255, using 16-bit integers previously forced the AVR compiler to perform redundant 16-bit arithmetic and continuously load zeroed high-bytes into registers. 

- **Prevented redundant I2C method inlining (`microWire.cpp`)**
  - Added `__attribute__((noinline))` to the `TwoWire::requestFrom` method overloads.
  - Under aggressive LTO (`-flto`) and size optimization (`-Os`), the compiler was unfolding the I2C transaction setup logic directly into caller functions. Forcing no-inline deduplicates this logic.
  
------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v7.1.3**

`RDS.h`, `SI4735_fixed.h`

---

## Added

- **RDS Local Time Offset (`RDS.h`)**
  - Added `rdsApplyLocalOffset()` — converts UTC from Group 4A to local time using station-transmitted offset (DL bits 5:0)
  - Pure 8-bit math, no div/mod; handles half-hour zones (e.g. UTC+5:30) and midnight wraparound
  - Enabled by default; comment out call in `rdsExtractCT()` to save ~30 bytes flash and display raw UTC

- **BLER-gated Clock Time (`RDS.h`, `SI4735_fixed.h`)**
  - CT (Group 4A) now accepted only when blocks C+D have BLER ≤ 1 (at most 1–2 bit corrected errors)
  - Block B is skipped — its group-type field is already validated by `rdsIsGroup4A()`
  - CT arrives once per minute — a single corrupted group means wrong clock for up to 60s, unlike RT which self-corrects on the next segment repeat within seconds
  - Hardware pre-filter (`FM_RDS_CONFIG = 0xAA01`) rejects uncorrectable blocks (level 3) before they reach FIFO; software BLER gate provides additional protection for time-critical CT data
  - Added `rdsGetBLER()` accessor in `SI4735_fixed.h` — reads raw[12] from FM_RDS_STATUS response

## Fixed

- **`rdsStoreChars` signed char comparison (`RDS.h`)**
  - Changed `char c = *data++` to `uint8_t raw = *data++` — AVR `char` is signed, so bytes 0x80–0xFF were treated as negative and incorrectly replaced with spaces
  - Practical impact minimal (most FM stations transmit ASCII 0x20–0x7F, and the OLED font only covers ASCII), but the comparison is now technically correct

- **`rdsFillRT` pointer arithmetic (`RDS.h`)**
  - Changed `s_rt[s_scrl]` pointer with separate index to `s_rt[s_scrl + i]` — avoids computing a pointer past array bounds when scroll position exceeds text length
  
------------------------------------------------------------------------------------------------------------


### **MOD_NO_RDS v7.1.4**

`microWire.cpp`, `RadioControl.h`

---

## Fixed

- **I2C NACK cascade and bus hang**
	https://github.com/diqezit/ats20_ats_ex/issues/62
	
  - `write()` now returns `0` immediately if a NACK flag is already set, preventing cascading ~30ms TWINT timeouts on subsequent bytes
  - `write(const uint8_t* buffer, size_t size)` breaks on first failed byte and returns actual bytes sent instead of blindly consuming the buffer
  - `failAddressNack()` and `failDataNack()` now immediately issue a STOP condition to release the I2C bus, preventing TWI hardware stalls

- **Favorites recall stuck in AM after SSB station**
	https://github.com/diqezit/ats20_ats_ex/issues/63

  - `configureAMMode()` did not clear `g_ssbLoaded`, unlike `configureFMMode()`
  - Stale true flag caused `favoriteNeedsFullReset()` to skip `loadSSBPatch()` on next SSB recall UI showed LSB/USB while chip stayed in AM
  - Fixed by clearing `g_ssbLoaded = false` on entry, matching the existing pattern in `configureFMMode()` and `performModeCycle()`
  
------------------------------------------------------------------------------------------------------------

### **MOD_NO_RDS v7.1.5**

`Memory.h / RadioControl.h / ATS_EX.ino / Globals.h / SSD1306_OLED.h / Rotary.cpp / SI4735.cpp / SMeter.h / SettingsData.h / Favorites.h / Input.h`

---

## Fixed

- **applyBrightness() missing on factory reset**
  - Reset path never applied compiled default brightness, stayed at OLED default until reboot
  - Moved call out of `readAllReceiverInformation()` (both branches) into `setup()` after `loadReceiverConfig()`, runs once unconditionally

- **Rate-limit swallowing final BFO in SSB mode**
  - `ssbChipRate()` had 40ms rate-limit that could leave chip tuned to previous BFO when encoder stops mid-window
  - UI showed correct frequency/BFO while receiver stayed on previous values until next tuning event
  - Added deferred `ssbChipRate()` call in `handlePeriodicTasks()` for SSB modes

- **False "SAVED" confirmation on full/duplicate favorite**
  - `addFavorite()` silently failed if the list was full (20/20) or the station already existed, but `showSavedConfirmation()` was called unconditionally
  - `addFavorite()` now returns `bool` (true on success), `handleAgcLongDone()` only shows the confirmation screen if the station was actually added

---

## Changed

- **saveLastFreq() helper**
  - Duplicate `g_lastSavedFrequency = g_currentFrequency` in `bandSwitch()` / `saveAllReceiverInformation()` merged into one noinline function
  - Declared in `Globals.h`, defined in `ATS_EX.ino` next to `getAndResetEncoderCount()`

- **Brightness call unified**
  - Single call in `setup()`, removed from valid EEPROM branch in `readAllReceiverInformation()`

- **Removed `setI2CStandardMode()` call in `ssbPatchFinalize()`**
  - `Wire.setClock(100000)` immediately overwritten by `applyI2CSpeed()` which writes TWBR based on CPU prescaler

- **Settings validation table-driven**
  - 8 separate if-blocks for settings clamping replaced with single PROGMEM table + loop
  - `g_clampTable[]` stores `{index, max, default}` for CPUSpeed, Brightness, DisplayOff, FmSmAtt, FmSmThr, SoftMuteThr, SQL, SMeter
  - `validateLoadedSettings()` now only validates mode-specific settings (AGC/SoftMute/AVC)

- **Rotary encoder table moved to PROGMEM**
  - `ttable` was allocated in SRAM (.data) and duplicated in FLASH
  - Now stored in PROGMEM, read via `pgm_read_byte` in `Rotary::process()`
- **RSQ response size type optimized**
  - `int sizeResponse` changed to `uint8_t` in `getCurrentReceivedSignalQuality()`
  - Removes 16-bit arithmetic in response loop, explicit `(int)` cast added for `Wire.requestFrom` to resolve ambiguous overload

---

## Optimizations

- **EEPROM settings read** `eeprom_read_block()` replaces per byte loop
- **Dead `g_previousFrequency` write removed** overwritten before use
- **FM step index cast** `(uint8_t)` cast on `stepIdxFM` removes sign extend, matches existing AM/SSB pattern
- **saveLastFreq() dedup** one shared helper instead of two inline copies
- **Removed dead zero-gap check in S-meter interpolation**
- **`main()` entry point attributes** `__attribute__((OS_main, used, noreturn))` added to `main()`. 
  - Eliminates standard prologue/epilogue (push/pop Y-pointer and `ret`) reducing register pressure
- **`loadBandState()` clamping refactored** Unpacked EEPROM values are now clamped in local registers before being committed to the `Band` struct. 
  - Prevents GCC `-Os` from emitting redundant `movw` instructions to reload the `Band*` pointer (Z) before every field write

- **GyverOLED core refactoring ** 
  - `writeCore()` and `partialUpdate()` marked as `__attribute__((noinline))` to deduplicate their guards and window-setup code from `print()`, `write()`, `clearBox()`, and `drawDigit()` (-48 bytes FLASH)
  - 8-bit shifts (`y >> 3`) forced via `BIT8_LSR3` macro to prevent GCC `-Os` from promoting them to 16-bit `asr/ror` loops
  - Segment mask iteration (`renderSegmentsToBuffer`) changed to rotate the mask (`m >>= 1`) instead of shifting `1 << b` every loop
  - `writeCore()` font pointer lookup hoisted to run once per character instead of once per column
  
- **Removed redundant per-line `clearBox()` from `fav_drawLine()`**
  - The full-page clear in `fav_drawPageOrEmpty()` already covers all list rows (y=8..63), so clearing each row again before drawing was redundant on full redraws

- **PROGMEM packed reads**
  - `showChargeOnDisplay()` removed dead suffix ternary for 100% battery display `oledPrintU8_3suf` ignores suffix at v==100 anyway
  - `renderSegmentsToBuffer()` in `GyverOLED` single `pgm_read_dword` for segment blueprints instead of 4×`pgm_read_byte`
  
- **SSB settings toggle dedup**: Extracted `toggleSettingAndSyncSSB()` helper for `doSync()` and `doSSBAVC()`

------------------------------------------------------------------------------------------------------------
