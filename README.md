### **ATS-20+ Firmware Modifications (diqezit's Fork)**

<p align="center">
  <img src="https://img.shields.io/badge/Firmware-ATS--20+-blueviolet?style=for-the-badge&logo=github">
  <img src="https://img.shields.io/badge/status-active-success?style=for-the-badge">
  <img src="https://img.shields.io/github/languages/top/diqezit/ats20_ats_ex?style=for-the-badge">
  <img src="https://img.shields.io/github/last-commit/diqezit/ats20_ats_ex?style=for-the-badge">
</p>
<p align="center">
  <a href="https://translate.google.com/translate?sl=en&tl=ru&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Russian-blue?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=es&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Spanish-blue?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=de&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-German-lightgrey?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=fr&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-French-blue?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=it&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Italian-green?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=pl&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Polish-red?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=ja&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Japanese-purple?style=for-the-badge"></a>
  <a href="https://translate.google.com/translate?sl=en&tl=zh-CN&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex"><img src="https://img.shields.io/badge/Translate_to-Chinese-red?style=for-the-badge"></a>
</p>

This repository is a fork of the original `goshante/ats20_ats_ex` firmware, dedicated to custom modifications, bug fixes, and new features. All discussion regarding these new versions should take place here.

Your feedback and suggestions are welcome!
**Please**, put a star on the repository to raise the firmware higher up on the global list. **Thanks :)**

You can find the original project here: [goshante/ats20_ats_ex](https://github.com/goshante/ats20_ats_ex)

---

### **Key Modifications**

This fork introduces two main branches of improvements over the original firmware:

1.  **Audio Pop/Click Elimination (Hardware & Software Mod):** A modification that completely removes pops and clicks when switching modes. It requires a minor physical change to the receiver circuit.
2.  **`MOD_NO_RDS` Firmware Series:** An alternative firmware branch where the RDS feature was removed to free up program space (from first modifications). This enabled the addition of new functionality, most notably a **unified Favorites system for all bands (AM, FM, SSB, CW)**, along with dozens of other fixes and improvements in code.

**Important:** This audio improvement only affects the speaker output, as the headphone jack is connected before the amplifier.

`I strongly recommend to connect the voltage divider as close to the pin as possible - use for this a two - 10kOm resistors.
In order to eliminate possible interference with ADC readout distortion`

![Diagram showing how to connect the pins](https://github.com/user-attachments/assets/681c515e-bbb1-4213-845b-1d2d178f52b2)

**Font, moving elements on the screen have been changed since version 5.0 - if you want to use the old mapping - use version 4.11 without long term support.**

[GET 4.11](https://github.com/diqezit/ats20_ats_ex/releases/tag/v4.11)

### `V4.11`

<img width="1280" height="741" alt="image" src="https://github.com/user-attachments/assets/fcd30ee4-013e-4de6-a1a5-330c730b1e02" />


### `v5.x`

<img width="1280" height="766" alt="image" src="https://github.com/user-attachments/assets/1bf3ad8d-35a8-442b-b2ad-37518ec571f8" />

<img width="1280" height="737" alt="image" src="https://github.com/user-attachments/assets/551c7c44-bc77-4109-ab67-aa5f2a6ed839" />

------------------------------------------------------------------------------------------------------------


### **Firmware Installation**

This guide explains how to upload firmware to your ATS-20(+) receiver

#### **Step-by-Step**

1.  **Download the necessary files:**
    *   **Firmware File:** [**ATS\_EX.ino.with\_bootloader.hex**](https://github.com/diqezit/ats20_ats_ex/blob/mod_no_rds/ATS_EX/ATS_EX.ino.with_bootloader.hex) (Click the link, then find the "Download raw file" button).
    *   **Programming Software:** [**xLoader**](https://github.com/binaryupdates/xLoader) (Download the zip file and extract it).

2.  **Install Drivers (if needed):**
    *   Most ATS-20(+) receivers use a CH340 chip for USB communication. 
		If your computer doesn't recognize the receiver when you plug it in, you may need to install the driver. 
		This is typically a one-time setup.
    *   Download and install the driver from a reliable source, such as [SparkFun](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all).

3.  **Connect the Receiver:**
    *   **Connect your ATS-20+ receiver directly to your PC**
    *   Power on receiver.

4.  **Find the COM Port:**
    *   Open **Device Manager** in Windows.
    *   Look under the "Ports (COM & LPT)" section.
    *   Note the COM port number assigned to your receiver (e.g., `COM3`, `COM4`). 
		If no port appears, check your driver installation or USB cable.

5.  **Configure and Upload:**
    *   Launch `xLoader.exe`.
    *   Set parameters exactly as shown in the image below:
        *   **Hex file:** Click the `...` button and select the `ATS_EX.ino.with_bootloader.hex` file you downloaded.
        *   **Device:** Select `Uno(ATmega328)`.
        *   **COM Port:** Select the port number you found in Device Manager.
        *   **Baud rate:** Set to `57600`.


    <img width="257" height="262" alt="image" src="https://github.com/user-attachments/assets/285a943a-7b0f-4f39-b580-3757103eafb4" />

6.  **Start the Upload:**
    *   Click the **Upload** button.
    *   Wait for the process to complete. xLoader will display a message like "30xxx bytes uploaded" at the bottom of the window.
    *   Your receiver should restart automatically with the new firmware and reset. 
	*   Installation is complete!

------------------------------------------------------------------------------------------------------------

# **ATS-20+ EX Firmware - The Complete User Guide**

This manual provides a comprehensive overview of the ATS-20+ EX firmware's features and controls.

## **Table of Contents**

1.  **Core Concepts: Understanding the Interface**
    *   [The "Active Command" Paradigm](#1-core-concepts-understanding-the-interface)
2.  **Section 1: Main Screen Operations**
    *   [Power Management & Screen Control](#11-power-management--screen-control)
    *   [Frequency Tuning & Step Control](#12-frequency-tuning--step-control)
    *   [Volume Control & Mute](#13-volume-control--mute)
    *   [Band Navigation](#14-band-navigation)
    *   [Mode Switching (AM/SSB/CW)](#15-mode-switching-amssbcw)
3.  **Section 2: Advanced Reception Controls**
    *   [SSB Mode Operation](#21-ssb-mode-operation)
    *   [CW (Morse Code) Mode Operation](#22-cw-morse-code-mode-operation)
    *   [Bandwidth (BW) Filter Adjustment](#23-bandwidth-bw-filter-adjustment)
4.  **Section 3: System Functions**
    *   [Station Scanning](#31-station-scanning)
    *   [Favorites Management](#32-favorites-management)
    *   [The Settings Menu: A Deep Dive](#33-the-settings-menu-a-deep-dive)
5.  **Section 4: Maintenance & Support**
    *   [Factory Reset (EEPROM Reset)](#41-factory-reset-eeprom-reset)
6.  **Quick Reference Button Chart**
    *   [Main Screen Operations Chart](#quick-reference-button-chart)

---

### **1. Core Concepts: Understanding the Interface**

The firmware's UI is based on the **"Active Command"** paradigm. This model allows the encoder to serve multiple purposes without complex menus.

*   **Default State:** By default the **encoder knob** controls the **frequency**.
*   **Active Command:** A short press on `VOL+` `STEP` `BW` or `BAND+` makes that function the "Active Command". The corresponding UI element becomes inverted and the **encoder knob temporarily controls that function**.

**Example: Changing Volume**
1.  Short-press `VOL+`. The volume value becomes inverted.
2.  Rotate the encoder to set the desired level.
3.  Short-press the encoder button to exit immediately or simply wait 3 seconds for it to time out. The encoder will then revert to controlling the frequency.

---

### **Section 1: Main Screen Operations**

#### **1.1. Power Management & Screen Control**

*   **Screen Off Mode:** A short press on the **`AGC`** button toggles the screen's power. The receiver continues to operate with minimal power drain significantly extending battery life.
*   **Wake Screen:** If the screen is off (either manually or by timeout) press **any button** to wake it. The first press only reactivates the screen and does not trigger any other action.

#### **1.2. Frequency Tuning & Step Control**

*   **Tuning Step Selection:**
    1.  Short-press **`STEP`**. The "STEP" value on the display will be highlighted.
    2.  **Rotate the encoder** to select the desired step size.
    3.  Short-press the **encoder button** to confirm and exit or wait for the timeout.

#### **1.3. Volume Control & Mute**

*   **Quick Adjustment:** Short-press **`VOL+`** then **rotate the encoder**.
*   **Continuous Adjustment:** **Press and hold `VOL+`** or **`VOL-`**.
*   **Mute:** Short-press **`VOL-`** to toggle mute. The volume level is replaced by " M".

#### **1.4. Band Navigation**

*   **Quick Jump:** Short-press **`BAND+`** then **rotate the encoder** to jump between pre-defined bands.
*   **Seamless Tuning:** Tune past the edge of the current band to automatically switch to the adjacent one. Note: This applies between AM/SW bands; tuning past the SW band edge will not roll over into the FM band.

#### **1.5. Mode Switching (AM/SSB/CW)**

*   **Short-press `MODE`** to cycle through reception modes: `AM` → `LSB`/`USB` → `CW` → `AM`. This function is disabled on FM bands.

---

### **Section 2: Advanced Reception Controls**

#### **2.1. SSB Mode Operation**

1.  **Enter SSB Mode:** Use the `MODE` button until `LSB` or `USB` is displayed.
2.  **Sideband Selection:** **Press and hold the `BW` button** for 1-2 seconds to switch between `LSB` and `USB`.
3.  **Two-Stage Tuning:** SSB requires a two-stage process for clear audio:
    *   **Coarse Tune (Frequency):** Select a **large step** (e.g. `1 kHz` `5 kHz`). Tune the main frequency until you hear intelligible but still high or low-pitched audio.
    *   **Fine-Tune (Clarifier):** Select a **small step** (e.g. `10 Hz` `50 Hz`). Now tuning only adjusts the audio pitch to make voices sound natural without changing the main frequency.
4.  **Sync Feature:** While in SSB mode **press and hold the `MODE` button** to toggle the Sync feature. An `S` indicator will appear when active. This engages the DSP's Automatic Frequency Control (AFC) to compensate for signal drift reducing fading and distortion.

#### **2.2. CW (Morse Code) Mode Operation**

1.  **Enter CW Mode:** Use the `MODE` button to select `CW`.
2.  **CW Sideband:** **Press and hold `BW`** to switch the reception sideband (`L` for LSB `U` for USB). This is a tool to eliminate nearby interference. The receiver will automatically adjust the frequency to keep the audible tone consistent.
3.  **Tuning:** The receiver's CW pitch is fixed at ~500 Hz. Tune the main frequency until the Morse code signal's tone is clear and distinct. Tip: Use a narrow BW filter (e.g. 0.5 kHz) in CW mode to isolate the signal.

#### **2.3. Bandwidth (BW) Filter Adjustment**

1.  Short-press the **`BW`** button.
2.  **Rotate the encoder** to change the filter width.
    *   **Narrower (e.g. 1.8 kHz):** Rejects adjacent channel interference improving clarity in crowded bands.
    *   **Wider (e.g. 4.0 kHz):** Provides better audio fidelity on strong clear signals.

---

### **Section 3: System Functions**

#### **3.1. Station Scanning**

*   **Enable First:** In the Settings Menu (`BAND-`) set the `SCN` item to `On`.
*   **Start/Stop Scan:** In AM or FM mode short-press the **encoder button** to start scanning. Press any button to stop.

#### **3.2. Favorites Management**

This feature saves a complete station profile: frequency mode (AM/LSB/USB/CW/FM) and the precise BFO offset for SSB/CW signals. This allows for one-press recall of a perfectly tuned station.

*   **To Save a Station:**
    1.  Tune to any station and adjust it for optimal reception (including fine-tuning the pitch in SSB/CW).
    2.  **Press and hold `AGC`**. A "SAVED" confirmation will appear on the screen.
    3.  **Note:** The system prevents saving duplicate entries (same frequency and mode).

*   **To Access and Use the Favorites List:**
    1.  **Press and hold `STEP`** to open the Favorites menu.
    2.  **Navigate:** **Rotate the encoder** to scroll through your saved stations.
    3.  **Tune:** Highlight the desired station and **short-press the encoder button**. The receiver will instantly reconfigure itself to match all the favorite's saved parameters.
    4.  **Delete:** To remove a station highlight it in the list and press the **`BW`** button.
    5.  **Exit:** To close the menu without tuning **short-press `STEP`**. The menu also closes automatically after 10 seconds of inactivity.

#### **3.3. The Settings Menu: A Deep Dive**

To enter and exit the settings menu perform a **short press** on the **`BAND-`** button. The menu is organized into pages.

*   **Navigation:** Rotate the encoder to select an item. Short-press **`BAND+`** to switch between pages.
*   **Editing:** Select an item then short-press the encoder to enter "Edit Mode" (a `>` appears next to the value). Rotate the encoder to change the value. Press the encoder again to confirm.
*   **Save & Exit:** The menu closes automatically after 10 seconds of inactivity or you can exit manually by pressing `BAND-`. All changes are saved to EEPROM upon exit.

##### **Page 1: General & Audio**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `ATT` | **Attenuator.** Manages gain. **`AUT`:** Standard **Automatic Gain Control (AGC)**. **Manual values:** Disables AGC and sets a **fixed attenuation level**. Use this to prevent overload from strong local stations. | Selection | `AUT`, `1`..`37` |
| `SCN` | **Scan Switch.** Determines the encoder button's function. **`On`:** Quick **Seek**. **`Off`:** Quick **Step** adjustment. **Note:** In SSB/CW the encoder press **always** activates `STEP` mode. | Switch | `On` / `Off` |
| `AVC` | **AVC Max Gain.** Sets the maximum gain for the Automatic Volume Control system in AM/SSB. Higher values increase volume for weak stations. **Important:** Only active when `ATT` is `AUT`. | Number | `12`..`90` |
| `SMA` | **Soft Mute Attenuation.** Defines the amount of audio attenuation on weak signals. `0` disables it. **Note:** Not used in FM mode. | Number | `0`..`32` |
| `SMT` | **Soft Mute Threshold.** Sets the **minimum SNR** below which soft mute activates. Helps eliminate static hiss. **Note:** Not used in FM mode. | Number | `0`..`63` |
| `DE` | **FM De-Emphasis.** Matches the regional broadcast standard. **`50`:** Europe/Asia. **`75`:** North America. **Note:** Only active in FM mode. | Selection | `50` / `75` |

##### **Page 2: SSB & Display**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `BFO` | **BFO Calibration.** Calibrates the oscillator for SSB/CW. If all SSB stations sound off-pitch use this to correct the global offset. Each step is 100 Hz. | Number | `-25`..`+25` |
| `SSM` | **SSB Soft Mute.** Enables an alternative soft muting algorithm specifically adapted for SSB mode. | Switch | `On` / `Off` |
| `SVC` | **SSB Volume Control.** Activates a separate automatic volume control system that works only in LSB USB and CW modes. | Switch | `On` / `Off` |
| `COF` | **SSB Cutoff Filter.** Manages a high-pass filter. **`AUT`:** Filter chosen based on current BW. **`1`, `2`:** Manual selection for interference rejection. | Selection | `AUT`, `1`, `2` |
| `SYN` | **SSB Sync.** Activates **DSP AFC (Automatic Frequency Control)** to compensate for frequency drift. **Note:** Not active in CW mode. | Switch | `On` / `Off` |
| `SCR` | **Screen Brightness.** Adjusts the OLED display brightness from `1` (min) to `10` (max). | Number | `1`..`10` |

##### **Page 3: Hardware & Miscellaneous**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `CAP` | **Antenna Capacitor.** Controls a capacitor at the antenna input. **`AUT`:** Engaged for FM only. **`On`:** Forced on for all bands. May improve reception on some antennas. | Switch | `AUT` / `On` |
| `CPU` | **CPU Speed.** **`16MHz`:** Max performance. **`8MHz`:** Low-power mode increases battery life with slightly slower UI response. | Switch | `16MHz` / `8MHz` |
| `BAP` | **Battery Pin.** Selects the analog port (`A1` or `A2`) for measuring battery voltage. Must match your board's hardware layout. | Selection | `A1` / `A2` |
| `SWU` | **SW Units.** Changes the frequency format for Shortwave (SW) bands. **`kHz`:** e.g. `7100 kHz`. **`MHz`:** e.g. `7.10 MHz`. | Selection | `kHz` / `MHz` |
| `RSI` | **Disable AM RSSI.** On some hardware RSSI polling in AM can cause audible clicks. This is a troubleshooting option. **`On`:** Disables RSSI updates but the signal meter will not work in AM. **`Off`:** Standard mode. | Switch | `On` / `Off` |
| `DIS` | **Display Off Timer.** Automatically turns off the screen after a period of inactivity. **`Off`:** Always on. **`10m`..`60m`:** Time in minutes. | Selection | `Off`, `10m`..`60m` |

##### **Page 4: Audio Test**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `FMP` | **FM Audio Profile.** Toggles a built-in equalizer. **`On`:** (Default) Applies an EQ profile optimized for the internal speaker. **`Off`:** Bypasses EQ for a flat audio output ideal for headphones. | Switch | `On` / `Off` |
| `ANB` | **AM Noise Blanker.** Activates an experimental digital filter to reduce impulse noise (e.g. from car ignitions) in AM modes. Disabled by default. | Switch | `On` / `Off` |
| `FMO`| **Force FM Mono.** Overrides automatic stereo reception and forces the output to mono. Useful for improving clarity on weak noisy FM stations. | Switch | `On` / `Off` |

---

### **Section 4: Maintenance & Support**

#### **4.1. Factory Reset (EEPROM Reset)**

This procedure resets all settings band states and clears all saved favorites.

1.  Power the receiver **off**.
2.  **Press and hold the encoder button**.
3.  While holding the button **power the receiver on**.
4.  The screen may display "EEPROM RST". The reset is complete.

**Alternative Method:** If the encoder button is faulty you can perform a reset by **holding the `AGC` button** during power-on instead.

---

### **Quick Reference Button Chart**

(Applies when on the main listening screen)

| Button | Short Press (Tap) | Long Press (Hold 1-2 sec) |
| :--- | :--- | :--- |
| **`MODE`** | Cycle Mode (`AM`→`SSB`→`CW`) | Toggle Sync *(SSB only)* |
| **`STEP`** | Activate Step selection | Open/Close Favorites Menu |
| **`BW`** | Activate Bandwidth selection | Switch Sideband *(SSB/CW only)* |
| **`BAND+`** | Activate Band selection | Cycle bands up continuously |
| **`BAND-`** | Open/Close Settings Menu | Cycle bands down continuously |
| **`VOL+`** | Activate Volume control | Increase volume continuously |
| **`VOL-`** | Toggle Mute | Decrease volume continuously |
| **`AGC`** | Toggle Screen Power On/Off | Save Current Station to Favorites |
| **Encoder** | Activate Step OR Start Scan (depends on `SCN` setting) | (No function) |

