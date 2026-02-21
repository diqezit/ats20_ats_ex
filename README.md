# ATS-20 / ATS-20+ firmware (ATS_EX fork) — Favorites/Presets (station memory list)

**diqezit’s fork of `goshante/ats20_ats_ex`** for **ATS-20 / ATS-20+ receivers** based on **ATmega328P + SI4732/SI4735** (OLED UI, AM/FM/SSB/CW).

**Main feature:** a unified **Favorites / Presets / Station Memory / Bookmarks** list (**20 entries**) shared across **AM / FM / SSB / CW**, storing a full station snapshot: **frequency + mode + BFO** (so SSB/CW recall is accurate).

**Also included:** `MOD_NO_RDS` (flash-saving build for ATmega328P), **RDS MINI RadioText**, **audio pop/click elimination (speaker path)**, plus many bug fixes and UI improvements.

<p align="center">
  <img src="https://img.shields.io/badge/Firmware-ATS--20%2B-blueviolet?style=for-the-badge&logo=github" alt="Firmware ATS-20+">
  <img src="https://img.shields.io/badge/status-active-success?style=for-the-badge" alt="Status: active">
  <img src="https://img.shields.io/github/languages/top/diqezit/ats20_ats_ex?style=for-the-badge" alt="Top language">
  <a href="https://github.com/diqezit/ats20_ats_ex/commits">
    <img src="https://img.shields.io/github/last-commit/diqezit/ats20_ats_ex?style=for-the-badge&label=Last%20commit" alt="Last commit">
  </a>
</p>

<details>
  <summary><div align="center"><b>Translate this page (Google Translate)</b></div></summary>
  <br>
  <div align="center">

[![Translate to Russian](https://img.shields.io/badge/Translate_to-Russian-3B82F6?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=ru&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Ukrainian](https://img.shields.io/badge/Translate_to-Ukrainian-84CC16?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=uk&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Polish](https://img.shields.io/badge/Translate_to-Polish-F43F5E?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=pl&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to German](https://img.shields.io/badge/Translate_to-German-DC2626?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=de&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to French](https://img.shields.io/badge/Translate_to-French-6366F1?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=fr&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Spanish](https://img.shields.io/badge/Translate_to-Spanish-EAB308?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=es&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Portuguese](https://img.shields.io/badge/Translate_to-Portuguese-10B981?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=pt&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Italian](https://img.shields.io/badge/Translate_to-Italian-22C55E?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=it&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Turkish](https://img.shields.io/badge/Translate_to-Turkish-0EA5E9?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=tr&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Arabic](https://img.shields.io/badge/Translate_to-Arabic-F97316?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=ar&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Hindi](https://img.shields.io/badge/Translate_to-Hindi-D946EF?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=hi&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Chinese](https://img.shields.io/badge/Translate_to-Chinese-EF4444?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=zh-CN&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Japanese](https://img.shields.io/badge/Translate_to-Japanese-8B5CF6?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=ja&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)
[![Translate to Korean](https://img.shields.io/badge/Translate_to-Korean-22C55E?style=for-the-badge&logo=googletranslate&logoColor=white)](https://translate.google.com/translate?sl=en&tl=ko&u=https%3A%2F%2Fgithub.com%2Fdiqezit%2Fats20_ats_ex)

  </div>
</details>

---

## Key Modifications

This fork introduces three main branches of improvements over the original firmware:

1. **Audio Pop/Click Elimination (Hardware & Software Mod):** a modification that removes pops/clicks when switching modes (requires a small physical change to the receiver circuit).
2. **`MOD_NO_RDS` firmware series:** the original full RDS was replaced with a lightweight **RDS MINI** decoder to stay within ATmega328P flash limits. This freed program space for new functionality — most notably a **unified Favorites system for all bands (AM, FM, SSB, CW)** — plus many other fixes and improvements.
3. **RDS MINI RadioText decoder:** starting from the **v7.1.x** series this fork includes a minimal RDS implementation (~500 bytes Flash). It decodes **Group 2A/2B RadioText** on FM and shows it as scrolling text on OLED row 6. Toggle at runtime via **long-press `MODE` in FM mode** (shows `RS` hint when active).

**Important:** the audio improvement affects only the **speaker output**, because the headphone jack is connected before the amplifier.

---

### **Changelog**

Full changelog is available here: [Changelog.md](https://github.com/diqezit/ats20_ats_ex/blob/mod_no_rds/Changelog.md)

`I strongly recommend to connect the voltage divider as close to the pin as possible - use for this a two - 10kOm resistors.
In order to eliminate possible interference with ADC readout distortion`

![Diagram showing how to connect the pins](https://github.com/user-attachments/assets/681c515e-bbb1-4213-845b-1d2d178f52b2)

**Font, moving elements on the screen have been changed since version 5.0 - if you want to use the old mapping - use version 4.11 without long term support.**

[GET 4.11](https://github.com/diqezit/ats20_ats_ex/releases/tag/v4.11)

### `V4.11`
_General interface view in version 4.11_
<p align="center">
  <img alt="Interface v4.11" src="https://github.com/user-attachments/assets/fcd30ee4-013e-4de6-a1a5-330c730b1e02" />
</p>

### `v5.x`
_Key interface changes in the 5.x versions_
<table align="center">
  <tr>
    <td align="center" width="49%">
      Main Screen
      <br>
      <img alt="Main Screen v5.x" src="https://github.com/user-attachments/assets/1bf3ad8d-35a8-442b-b2ad-37518ec571f8" />
    </td>
    <td align="center" width="49%">
      Settings Screen
      <br>
      <img alt="Info Screen v5.x" src="https://github.com/user-attachments/assets/551c7c44-bc77-4109-ab67-aa5f2a6ed839" />
    </td>
  </tr>
</table>

### `v6.x`
_Screen examples in the 6.x versions_
<table align="center">
  <tr>
    <td align="center" width="49%">
      SSB Mode
      <br>
      <img alt="SSB Mode" src="https://github.com/user-attachments/assets/de5d2098-3a76-4f87-af9b-e5157c0be423" />
    </td>
    <td align="center" width="49%">
      Favorites List
      <br>
      <img alt="Favorites List" src="https://github.com/user-attachments/assets/c2a5d7f5-47ec-4043-b5f8-89131e22f18f" />
    </td>
  </tr>
  <tr>
    <td align="center" width="49%">
      Full Screen Layout
      <br>
      <img alt="Full Screen Layout" src="https://github.com/user-attachments/assets/e1282227-e65f-4015-aee0-134ea80bbc51" />
    </td>
    <td align="center" width="49%">
      Settings Menu
      <br>
      <img alt="Settings Menu" src="https://github.com/user-attachments/assets/1256b31e-2c32-475e-ba54-332c2ea47e93" />
    </td>
  </tr>
</table>

---

### **Firmware Installation**

This guide explains how to upload the firmware to your ATS-20(+) receiver using a Windows PC and the xLoader software

#### **Step 1: Download the Necessary Files**

You will need two things: the programming software and the correct firmware file

1.  **Programming Software:**
    *   Download [**xLoader**](https://github.com/binaryupdates/xLoader) (Click the green "Code" button, then "Download ZIP". Extract the files to a folder on your computer)

2.  **Firmware File (.hex):**
    *   For flashing via USB, you only need the file **without** the bootloader
    *   **Click here to download:** [**ATS\_EX.ino.eightanaloginputs.hex**](https://github.com/diqezit/ats20_ats_ex/blob/mod_no_rds/ATS_EX/ATS_EX.ino.eightanaloginputs.hex) (Click the link, then find the "Download raw file" button)

> **📌 Which File Should I Use?**
> *   The `ATS_EX.ino.eightanaloginputs.hex` file is the correct one for updating your receiver's firmware over a standard USB connection using xLoader
> *   The `ATS_EX.ino.with_bootloader.hex` file is an advanced file used only for programming with an external ISP programmer (like a USBasp). **Do not use this file with xLoader for a standard USB update**
>   *   [Link for advanced users: **With\_bootloader**](https://github.com/diqezit/ats20_ats_ex/blob/mod_no_rds/ATS_EX/ATS_EX.ino.with_bootloader.hex)

#### **Step 2: Install Drivers (If Needed)**

Most ATS-20(+) receivers use a CH340 chip for USB communication. If your computer does not recognize the receiver when you plug it in, you must install the driver. This is a one-time setup

*   Download and install the CH340 driver from a reliable source, such as [SparkFun's Guide](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all)

#### **Step 3: Connect the Receiver**

*   Connect your ATS-20+ receiver directly to a USB port on your PC
*   Power on the receiver

#### **Step 4: Find the COM Port**

*   Open **Device Manager** in Windows (you can search for it in the Start Menu)
*   Expand the **"Ports (COM & LPT)"** section
*   Note the COM port number assigned to your receiver (e.g., `COM3`, `COM4`). If no port appears, verify your driver installation and USB cable

#### **Step 5: Configure xLoader and Upload**

1.  Launch `xLoader.exe` from the folder where you extracted it
2.  Configure the settings precisely as follows:
    *   **Hex file:** Click the `...` button and select the `ATS_EX.ino.eightanaloginputs.hex` file you downloaded
    *   **Device:** Select `Duemilanove/Nano(ATmega328)`. This is correct for most ATS-20+ boards, which use an old bootloader
    *   **COM Port:** Select the port number you found in Device Manager
    *   **Baud rate:** Set to `57600`

    Your settings should look like this:
    <p align="center">
      <img width="257" alt="xLoader Correct Settings" src="https://github.com/user-attachments/assets/58aa3266-afcc-4910-84b4-2796d906ab66" />
    </p>

#### **Step 6: Start the Upload**

1.  Click the **Upload** button
2.  The receiver's lights may flash during the process. Wait for it to complete
3.  xLoader will display a message like "30xxx bytes uploaded" at the bottom of the window when finished
4.  Your receiver should restart automatically with the new firmware. On the first boot after an update it may show **"MEM RESET"** (or **"MEM WEAR"** if EEPROM is worn) - this is normal.

**Installation is complete!**

---
> **Troubleshooting Tips**
> *   **Upload Fails or Times Out:** Double-check that you have selected the correct COM port and that no other software (like a serial monitor) is using it. Try a different USB cable or port
> *   **Still Fails:** Some boards require you to press and release the receiver's **RESET** button just a moment before you click the "Upload" button in xLoader
> *   **Using an Arduino Uno Board Profile:** If your receiver has a newer bootloader (like an official Arduino Uno), you may need to select `Uno(ATmega328)` as the Device and `115200` as the Baud rate. However, `57600` is correct for the vast majority of ATS-20+

---

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
    *   [RDS RadioText Display (FM)](#24-rds-radiotext-display-fm)
4.  **Section 3: System Functions**
    *   [Station Scanning](#31-station-scanning)
    *   [Favorites Management](#32-favorites-management)
    *   [The Settings Menu: A Deep Dive](#33-the-settings-menu-a-deep-dive)
5.  **Section 4: Maintenance & Support**
    *   [Factory Reset (EEPROM Reset)](#41-factory-reset-eeprom-reset)
6.  **Section 5: Experimental Features**
    *   [CW (Morse Code) Decoder](#51-cw-morse-code-decoder-experimental)
7.  **Quick Reference Button Chart**
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

This section describes what the buttons do on the main listening screen.  
The UI is based on the **Active Command** concept: short-press a button to select what the encoder controls (STEP/BW/BAND/VOL), then rotate the encoder to change it.

---

#### **1.1. Power Management & Screen Control**

* **Manual screen toggle (instant):**
  * Short-press **`AGC`** to toggle the OLED power **On/Off**.
  * The radio keeps working normally while the screen is off (audio stays on).
  * When the screen is off, the firmware also changes CPU speed for better battery life.

* **Auto screen-off timer:**
  * Set `DIS` in Settings to enable automatic display timeout.
  * When auto-off triggers, the screen is turned off and CPU goes into a low-power mode.
  * To wake the screen: press any button or rotate the encoder.
  * **Wake rule:** the first wake press does not always trigger a “real action” (to avoid accidental toggles). `AGC` is the “safe” wake button.

---

#### **1.2. Frequency Tuning & Step Control**

* **Default behavior (no active command):**
  * Rotate the encoder to tune frequency.
  * In AM/FM tuning uses the current step grid (1k/5k/9k/10k… or FM 5/10/100).
  * In SSB/CW tuning is fine-tuning via BFO logic (smooth rollover at band edges).

* **Step selection (Active Command):**
  1. Short-press **`STEP`** → the STEP label becomes highlighted (active command).
  2. Rotate the encoder to change the step size.
  3. Short-press the encoder button to exit immediately or wait for timeout (about 3 seconds).
  
* **Notes:**
  * AM and FM steps are sent to the chip (hardware step).
  * SSB/CW “step” controls BFO tuning resolution (the chip always uses 1 kHz base step internally).

---

#### **1.3. Volume Control & Mute**

* **Quick adjustment (Active Command):**
  * Short-press **`VOL+`** → volume becomes active (highlighted).
  * Rotate the encoder to change volume.

* **Continuous adjustment:**
  * Press and hold **`VOL+`** or **`VOL-`** for repeated changes.

* **Mute:**
  * Short-press **`VOL-`** to toggle mute.
  * When muted, the volume field shows **` M`** (not `0`).

* **FM volume compensation:**
  * `FVA` reduces FM volume in software so switching AM↔FM feels consistent.

---

#### **1.4. Band Navigation**

* **Band selection (Active Command):**
  * Short-press **`BAND+`** → band becomes active (highlighted).
  * Rotate the encoder to jump between band slots.

* **Continuous band cycling:**
  * Press and hold **`BAND+`** or **`BAND-`** to cycle bands repeatedly.

* **Seamless tuning across edges:**
  * If you tune past the min/max of the current band, firmware can switch to the adjacent band and continue tuning.
  * When crossing between AM/SW and FM, the frequency is clamped to the FM band edge (to keep tuning valid).

* **Band coverage / memory:**
  * Firmware uses **44 band slots** (LW, MW, many SW segments, plus FM).
  * SW is split into segments so each slot can store its own memory (freq/step/BW/BFO calibration), while still allowing smooth tuning and scanning.

---

#### **1.5. Mode Switching (AM/SSB/CW)**

* **Short-press `MODE`** to cycle modes:
  * `AM` → `LSB/USB` → `CW` → `AM`
* Mode switching is disabled on FM bands (FM always stays FM).
* Long-press `MODE` actions depend on mode:
  * **LSB/USB:** Sync toggle
  * **FM:** RDS MINI toggle (`RS` hint)
  * **CW:** CW decoder view only if `ENABLE_CW_DECODER 1`

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
3.  **Tuning:** The receiver's CW pitch is adjustable via `CWP` setting (500-800 Hz). Tune the main frequency until the Morse code signal's tone matches the selected pitch. Tip: Use a narrow BW filter (e.g. 0.5 kHz) in CW mode to isolate the signal.

#### **2.3. Bandwidth (BW) Filter Adjustment**

1.  Short-press the **`BW`** button.
2.  **Rotate the encoder** to change the filter width.
    *   **Narrower (e.g. 1.8 kHz):** Rejects adjacent channel interference improving clarity in crowded bands.
    *   **Wider (e.g. 4.0 kHz):** Provides better audio fidelity on strong clear signals.

#### **2.4. RDS RadioText Display (FM)**

The receiver includes a lightweight RDS decoder that displays RadioText (RT) from FM broadcast stations directly on the OLED screen.

*   **How to Enable:**
    1.  Tune to an FM station.
    2.  **Press and hold the `MODE` button** for 1-2 seconds. An **`RS`** hint will appear on the screen confirming RDS MINI UI is active.
    3.  To disable, **press and hold `MODE`** again. The `RS` indicator and the text line will disappear.

*   **What It Shows:**
    *   The bottom line of the display (row 6) shows the station's RadioText message. This is typically the current song title, artist name, or station slogan.
    *   If the text is longer than 21 characters it will scroll automatically.

*   **Important Notes:**
    *   RDS is available **only in FM mode**. The decoder is automatically disabled when switching to AM/SSB/CW and re-enabled when returning to FM (if it was previously toggled on).
    *   Not all FM stations transmit RadioText data. If no text appears after several seconds on a strong station, that station may not be broadcasting RT.
    *   The RDS state (on/off) is **not saved to EEPROM** — it resets to off on every power cycle.
    *   The decoder processes only **Group 2A and 2B** (RadioText). It does not display station name (PS), clock, or program type.
    *   RDS decoding requires a reasonably strong FM signal. On weak or noisy signals the text may appear garbled or not appear at all. The decoder will clear stale text after 6 seconds of signal loss.

*   **Compile-Time Control:** RDS MINI can be disabled entirely by setting `ENABLE_RDS_MINI 0` in `Defines.h`. This saves approximately 500 bytes of Flash. When disabled, the long-press MODE action in FM mode does nothing.

---

### **Section 3: System Functions**

#### **3.1. Station Scanning**

Scanning uses the SI4735 hardware seek. It is available only when the encoder button is configured for scanning.

*   **Enable first:** In the Settings Menu set `SCN` to **On**.
    *   `SCN = On`  → encoder short press starts scan *(AM/FM only)*
    *   `SCN = Off` → encoder short press opens **STEP** command *(classic behavior)*

*   **Start scan:** On **AM** or **FM**, short-press the **encoder button**.
    *   Scan direction follows your last tuning direction (up/down).

*   **Stop scan:** While scanning is running:
    *   **press the encoder button**, or
    *   **rotate the encoder** (any movement stops the scan instantly)

*   **Where it scans:**
    *   **LW/MW/FM:** within the current band limits
    *   **SW:** scan uses the full SW search range and then maps the found frequency back to the correct SW segment, so band label/limits stay correct

*   **Notes:**
    *   Scan is **blocked in SSB/CW** (encoder short press opens STEP instead).
    *   FM results are aligned to the 10 kHz grid to match expected spacing.
    *   AM seek spacing is normalized to supported hardware steps (1/5/9/10 kHz).

---

#### **3.2. Favorites Management**

Favorites store a complete station snapshot: **frequency + mode (AM/LSB/USB/CW/FM) + BFO** (for SSB/CW).  
Maximum: **20 favorites** total (shared across all bands).

*   **Save current station:**
    1. Tune and adjust the station (including BFO fine tune in SSB/CW).
    2. **Press and hold `AGC`**.
    3. A **"SAVED"** message is shown.
    * Duplicate entries (same frequency + same mode) are not saved.

*   **Open Favorites menu:**
    * **Press and hold `STEP`** (from the main screen).

*   **Navigate:**
    * Rotate the encoder to move through the list (3 items per page).

*   **Tune to selected favorite:**
    * **Short-press the encoder button** → receiver reconfigures and tunes to the saved station.

*   **Delete favorite:**
    * **Short-press `BW`** in the Favorites menu (deletes the selected entry).

*   **Exit Favorites:**
    * **Short-press `STEP`** to exit without tuning.
    * Auto-exit after ~10 seconds of inactivity.

*   **EEPROM writes:**
    * Favorites are written to EEPROM **only on exit** and only if something changed (reduces EEPROM wear).

---

### **3.3. The Settings Menu: A Deep Dive**

The Settings Menu is the main place to configure how the receiver behaves (audio, RF, FM tweaks, display, hardware options). It is designed to be fast: one button enters/exits, and the encoder does both navigation and editing.

**How to Access:**
- **Enter / Exit:** Short press **`BAND-`**
- **Auto exit:** If there is no activity for ~10 seconds, the menu closes automatically
- **Saving:** Changes are written to EEPROM **when you exit** the menu (manual save is not required)

---

#### **Menu Layout**
- **5 pages**, **6 items per page** (**30 settings total**)
- Each page is a **2×3 grid**
- The header shows the current page: `SETTINGS N|5`

---

#### **Controls (Navigation vs Editing)**
The menu has two modes: **Navigate** and **Edit**.

1) **Navigate (default when you enter)**
- Rotate the encoder to move between items  
- The movement order depends on `NAV`:
  - `ROW`: left → right, then next row (reading order)
  - `COL`: top → bottom, then next column

2) **Edit**
- **Short press the encoder button** to toggle Edit mode for the selected item
- In Edit mode a `>` marker appears next to the value
- Rotate the encoder to change the value
- Short press encoder again to stop editing (stays in the menu)

3) **Switch Pages**
- Short press **`BAND+`** to cycle pages (1 → 2 → 3 → 4 → 5 → 1)

---

#### **Important Notes**
- **Inactive items show `---`:** If a setting does not apply to the current mode/band (example: FM-only options while listening to AM), the value is replaced with `---`.
- **Mode-dependent memory:** `ATT`, `AVC`, and `SMA` are stored separately for **AM**, **LSB**, and **USB**. When you switch mode, these values automatically change to the stored values for that mode.
- **Per-band memory:** Some items (like `BFO` calibration) are stored per band slot, so different SW segments can have their own calibration.

> **Tip:** Change one setting at a time and listen for the result. If you get lost, a factory reset always restores defaults.
> 
---

#### **Page 1: Core Audio & RF** *(Essential audio and reception controls)*

| Name | Detailed Description | Recommended Values | Mode Availability | Type | Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `ATT` | **Attenuator/AGC (Automatic Gain Control)**<br>Controls how the receiver handles signal strength. Think of it as "sunglasses" for the radio.<br>• **`AUT`**: The radio automatically adjusts its sensitivity. This is the best setting for 99% of situations.<br>• **Manual values (`1`..`37` for AM, `1`..`26` for FM)**: Disables automatic control and applies a fixed amount of signal reduction (attenuation).<br>**When to use manual values:** Only if a very powerful local station sounds distorted or "crackly". Start with a value of `5` and increase if needed.<br>**Mode-dependent:** Saved separately for AM/LSB/USB. | **Default:** `AUT`<br>**Strong locals:** `5-15`<br>**Weak signals:** `AUT` | All modes | Selection | `AUT`, `1`..`37` |
| `AVC` | **Automatic Volume Control**<br>Levels the volume between weak and strong stations. Think of it as a TV's "night mode" that boosts quiet dialogue.<br>• **`0`**: Weakest effect. Preserves the natural volume difference between stations. Best for high-fidelity listening.<br>• **`10`**: Strongest effect. Makes very quiet, distant stations sound almost as loud as strong local ones. Best for DXing.<br>**Interaction with `ATT`:** This works with `ATT`. If you apply manual attenuation (`ATT` > 0), you'll need a higher `AVC` value to compensate.<br>**Mode-dependent:** Saved separately for AM/LSB/USB. | **AM Broadcast:** `2-4`<br>**SSB/DXing:** `8-10`<br>**Hi-Fi Audio:** `0-2` | AM/SSB/CW only<br>**"---" in FM** | Number | `0`..`10` |
| `SQL` | **Squelch Level**<br>Silences the receiver until a signal exceeds this strength (AM mode only).<br>• `0`: Always open (hear everything including noise)<br>• `60`: Only very strong signals break through<br>**Use case:** Set to 15-25 to eliminate noise between stations while tuning. | **Default:** `0`<br>**Quiet tuning:** `15-25` | All modes | Number | `0`..`60` |
| `SMA` | **AM Soft Mute Attenuation**<br>How much to reduce volume on weak/noisy signals (in dB).<br>• `0`: No muting (hear all noise)<br>• `32`: Maximum muting (very quiet on weak signals)<br>**Tip:** Start with 10-16 for pleasant listening without losing weak stations.<br>**Mode-dependent:** Separate for AM/LSB/USB | **Default:** `0`<br>**Recommended:** `10-16` | AM/SSB/CW only<br>**"---" in FM** | Number | `0`..`32` |
| `SMT` | **AM Soft Mute SNR Threshold**<br>Signal quality level that triggers soft mute.<br>• Lower values (0-20): Only mute very noisy signals<br>• Higher values (40-63): Mute anything below excellent quality<br>**Balance:** Use 15-25 with SMA=10-16 for good results. | **Default:** `0`<br>**Balanced:** `15-25` | AM/SSB/CW only<br>**"---" in FM** | Number | `0`..`63` |
| `ANB` | **AM Noise Blanker**<br>Digital filter that removes clicking/popping interference.<br>**Helps with:** Power line noise, electric fences, car ignitions<br>**Note:** May slightly affect audio quality - turn off for music listening. | **Default:** `Off`<br>**Urban areas:** `On` | AM/SSB/CW only<br>**"---" in FM** | Switch | `On` / `Off` |

---

#### **Page 2: SSB & CW** *(Single Sideband and Morse Code settings)*

| Name | Detailed Description | Recommended Values | Mode Availability | Type | Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `BFO` | **Beat Frequency Oscillator Calibration**<br>Corrects frequency drift caused by component aging.<br>• Each step = 100 Hz correction<br>• Positive values: Increase displayed frequency<br>• Negative values: Decrease displayed frequency<br>**Usage:** If SSB stations sound too high/low pitched on a specific band, adjust this.<br>**Important:** Saved per band slot. Not used in FM mode. | **Default:** `0`<br>**Typical drift:** `±5` | AM/SSB/CW only<br>**"---" in FM** | Number | `-25`..`+25` |
| `SSM` | **SSB Soft Mute Mode**<br>Reduces noise between SSB/CW transmissions.<br>• `RSS`: mutes based on signal strength<br>• `SNR`: mutes based on signal-to-noise ratio | **Default:** `SNR`<br>**DXing:** `RSS` | SSB/CW only<br>**"---" in AM/FM** | Switch | `RSS` / `SNR` |
| `SVC` | **SSB AVC**<br>Stabilizes audio level on fading SSB/CW signals.<br>**When ON:** reduces fading effects but may pump on noise<br>**When OFF:** natural signal dynamics, better for strong signals | **Default:** `On`<br>**Weak signals:** `On`<br>**Local QSOs:** `Off` | SSB/CW only<br>**"---" in AM/FM** | Switch | `On` / `Off` |
| `COF` | **SSB Audio Cutoff Filter**<br>Removes low-frequency rumble and noise.<br>• `AUT`: auto match to bandwidth<br>• `1`: mild filtering<br>• `2`: aggressive filtering | **Default:** `AUT`<br>**Noisy bands:** `2` | SSB/CW only<br>**"---" in AM/FM** | Selection | `AUT`, `1`, `2` |
| `SYN` | **SSB Sync (AFC)**<br>Helps track drifting SSB signals.<br>**Pros:** less retuning<br>**Cons:** can lock wrong signal/noise | **Default:** `Off`<br>**Broadcast:** `On` | SSB/CW menu item<br>**Note:** works only in LSB/USB. In CW it is forced OFF. | Switch | `On` / `Off` |
| `CWP` | **CW Pitch**<br>Audio tone frequency for Morse code signals. | **Default:** `700`<br>**Common:** `600-700` | AM/SSB/CW only<br>**"---" in FM** | Selection | `500`, `600`, `700`, `800` Hz |

---

#### **Page 3: FM & Advanced Audio** *(FM reception and audio processing)*

| Name | Detailed Description | Recommended Values | Mode Availability | Type | Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `DE ` | **FM De-Emphasis**<br>Must match your region's broadcast standard for proper audio.<br>• `75 µs`: North & South America, South Korea<br>• `50 µs`: Europe, Asia, Africa, Australia<br>**Wrong setting:** Audio will sound too bright or too dull. | **USA:** `75`<br>**Europe:** `50` | FM only<br>**"---" in AM/SSB/CW** | Selection | `50` / `75` µs |
| `FMP` | **FM Audio Profile**<br>Tailors frequency response for your listening setup.<br>• `On`: Reduces treble for internal speaker (warmer sound)<br>• `Off`: Flat response for headphones/external speakers<br>**Try both:** The difference is quite noticeable. | **Speaker:** `On`<br>**Headphones:** `Off` | FM only<br>**"---" in AM/SSB/CW** | Switch | `On` / `Off` |
| `FMO` | **Force Mono**<br>Disables stereo decoding on FM.<br>**Benefits:** 10-20dB noise reduction on weak stations<br>**Drawback:** Loses stereo separation<br>**Use when:** Station is barely receivable in stereo. | **Default:** `Off`<br>**Weak signals:** `On` | FM only<br>**"---" in AM/SSB/CW** | Switch | `On` / `Off` |
| `FSA` | **FM Soft Mute Attenuation**<br>Volume reduction on weak FM signals (in dB).<br>**Higher values:** Quieter background when tuning<br>**Lower values:** Hear weak stations better<br>**Sweet spot:** 15-20 for most users. | **Default:** `22`<br>**DXing:** `0-10`<br>**Casual:** `15-22` | FM only<br>**"---" in AM/SSB/CW** | Number | `0`..`31` |
| `FST` | **FM Soft Mute Threshold**<br>Sets the SNR threshold that triggers FM soft mute.<br>Higher values = more aggressive muting. | **Default:** `0` | FM only<br>**"---" in AM/SSB/CW** | Number | `0`..`15` |
| `SWA` | **Shortwave AFC** *(AM mode, SW bands only)*<br>Helps track drifting AM broadcast stations.<br>• `OFF`: No correction (best for amateur radio)<br>• `PPM`: Standard correction window<br>• `Hz1`: ±1600Hz pull range (normal)<br>• `Hz2`: ±2000Hz pull range (aggressive)<br>**Warning:** Turn OFF for SSB/CW listening! | **Default:** `0` (OFF)<br>**Broadcast:** `2` (Hz1)<br>**SSB/CW:** `0` (OFF) | AM/SSB/CW only<br>**"---" in FM** | Selection | `0`..`3` (0=OFF, 1=PPM, 2=Hz1, 3=Hz2) |

---

#### **Page 4: Display & UI** *(User interface and display settings)*

| Name | Detailed Description | Recommended Values | Mode Availability | Type | Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `SCR` | **Screen Brightness**<br>OLED display brightness control.<br>• `1-3`: Night/dark room use<br>• `4-6`: Indoor daylight<br>• `7-10`: Bright conditions<br>**Battery tip:** Lower brightness significantly extends battery life. | **Indoor:** `4-6`<br>**Night:** `2-3`<br>**Daylight:** `7-9` | All modes | Number | `1`..`10` |
| `SPT` | **Signal Meter Display (4 modes)**<br>• `RSS`: raw RSSI number (0–99)<br>• `SPT`: S-meter (S0..S9+)<br>• `R+B`: RSSI + thin bar graph *(requires `ENABLE_SIGNAL_BAR 1`)*<br>• `S+B`: S-meter + thin bar graph *(requires `ENABLE_SIGNAL_BAR 1`)*<br>**Note:** If `ENABLE_SIGNAL_BAR 0`, bar modes behave like their non-bar variants. | **Default:** `RSS` | All modes | Selection | `RSS`, `SPT`, `R+B`, `S+B` |
| `SWU` | **Shortwave Units**<br>Frequency display format for SW bands.<br>• `kHz`: 14205 kHz (traditional)<br>• `MHz`: 14.205 MHz (modern)<br>**Preference:** Purely cosmetic - choose what you're used to. | **Traditional:** `kHz`<br>**Modern:** `MHz` | AM/SSB/CW only<br>**"---" in FM** | Switch | `kHz` / `MHz` |
| `DIS` | **Display Auto-Off Timer**<br>Saves battery by turning off screen after inactivity.<br>**Radio keeps playing** with screen off!<br>**Wake up:** Press any button<br>**Good practice:** Use 10-15 minutes for battery savings. | **Default:** `OFF`<br>**Battery save:** `10m` or `15m` | All modes | Selection | `OFF`, `10m`, `15m`, `30m`, `60m` |
| `RSI` | **AM RSSI polling** *(click/noise workaround)*<br>• `OFF`: disable AM-family RSSI updates (reduces periodic clicks)<br>• `ON`: enable AM-family RSSI updates | **Default:** `OFF` | AM/SSB/CW only<br>**"---" in FM** | Switch | `ON` / `OFF` |
| `NAV` | **Menu Navigation Pattern**<br>How encoder moves through settings.<br>• `ROW`: Left→Right then down (like reading)<br>• `COL`: Top→Bottom in columns (traditional)<br>**Try both:** ROW is usually faster for accessing items. | **Default:** `ROW`<br>**Traditional:** `COL` | All modes | Switch | `ROW` / `COL` |

---

#### **Page 5: Hardware Configuration** *(Hardware-specific adjustments)*

| Name | Detailed Description | Recommended Values | Mode Availability | Type | Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `CAP` | **Antenna Tuning Capacitor**<br>Internal capacitor for antenna matching.<br>• `On`: Better for random wire, whip antennas<br>• `Off`: Better for 50Ω antennas, external tuners<br>**Test both:** See which gives stronger signals with your antenna. | **Wire antenna:** `On`<br>**50Ω antenna:** `Off` | All modes | Switch | `On` / `Off` |
| `CPU` | **Processor Speed**<br>Trade-off between performance and battery life.<br>• `100%` (16MHz): Snappy UI response<br>• `50%` (8MHz): ~30% longer battery, slightly slower UI<br>**Note:** Radio performance is identical at both speeds. | **AC power:** `100%`<br>**Battery:** `50%` | All modes | Selection | `100%` / `50%` |
| `BAP` | **Battery Monitor Pin**<br>Selects which analog pin reads battery voltage.<br>**Default firmware mapping:** A2<br>**Alternative:** A1<br>**How to test:** Battery % should show realistic values. | **Default:** `A2`<br>**Alternative:** `A1` | All modes | Selection | `A2` / `A1` |
| `SCN` | **Encoder Button Function**<br>What happens when you short-press the encoder.<br>• `On`: Start frequency scanning *(AM/FM only)*<br>• `Off`: Open step size menu (classic)<br>**Note:** Scan is blocked in SSB/CW. | **Recommended:** `On` | All modes | Switch | `On` / `Off` |
| `FVA` | **FM Volume Compensation**<br>Reduces FM volume to match AM/SSB levels.<br>**Problem:** FM broadcasts are often much louder<br>**Solution:** Set to 5-10 to balance volume across modes<br>**Fine-tune:** Adjust until mode switches don't require volume changes. | **Default:** `0`<br>**Typical:** `5-10` | All modes | Number | `0`..`15` |
| `SWL` | **SW Link**<br>Links **STEP** and **BW** settings across all SW band segments.<br><br>**What it does:**<br>• Shortwave (SW) is split into many segments (band slots). Normally each segment can store its own STEP/BW values.<br>• When `SWL` is **On**, all SW segments share the same STEP/BW values (they are kept in sync automatically).<br><br>**How it works:**<br>• Any STEP/BW change you make while you are on an SW segment becomes the “master” value and is copied to all other SW segments.<br>• This happens immediately after changing STEP/BW (no need to save/reboot).<br><br>**Why it is useful:**<br>• No surprise filter/step changes when you move between SW broadcast and ham segments.<br>• Keeps tuning behavior consistent across the entire 1.7–30 MHz range.<br><br>**Notes:**<br>• Affects only SW segments (LW/MW and FM are not linked).<br>• Frequency memories per segment stay independent — only STEP/BW are linked. | **Default:** `Off` | All modes | Switch | `On` / `Off` |

---

> **🔧 Quick Setup Guide for New Users:**
> 1. **First time:** Leave most settings at defaults
> 2. **Set your region:** Adjust `DE` (FM De-emphasis) for your location
> 3. **Optimize display:** Set `SCR` (brightness) for your environment
> 4. **For weak signals:** Enable `AVC=5`, `SMA=12`, `SMT=20`
> 5. **For headphones:** Set `FMP=Off`
> 6. **Save battery:** Set `CPU=50%` and `DIS=15m`

> **📝 Settings Memory:**
> - **Global settings:** Saved once for entire radio
> - **Mode-dependent:** ATT, AVC, SMA saved separately per mode
> - **Band-specific:** BFO calibration saved per band slot (not used in FM)
> - **Auto-save:** Changes saved to EEPROM when exiting menu

---

> **📌 Key Technical Notes:**
> *   **Mode-Dependent Settings:** `ATT`, `AVC`, and `SMA` values are stored separately for AM, LSB, and USB modes. The displayed value changes automatically when you switch modes.
> *   **Band-Specific BFO:** The `BFO` calibration is saved per band slot (not used in FM). This allows precise drift compensation per frequency segment.
> *   **Soft Mute Systems:** AM and FM use completely independent soft mute implementations with different parameter ranges (`SMA`/`SMT` for AM, `FSA`/`FST` for FM).
> *   **SW AFC:** The `SWA` setting only activates on Shortwave bands (1.7-30 MHz) while in AM mode. It helps track drifting broadcast stations but should be disabled for SSB/CW.
> *   **Navigation Styles:** The `NAV` setting affects only the Settings menu navigation, not the main screen or Favorites list.
> *   **Mode Indicators:** Settings showing "---" are not functional in the current mode, helping you focus on relevant parameters only.
> *   **RDS MINI:** The RDS RadioText feature is controlled via long-press MODE in FM mode, not through the Settings menu. Its state resets on power cycle. It costs approximately 500 bytes of Flash and can be disabled at compile time with `ENABLE_RDS_MINI 0`.

---

### **Section 4: Maintenance & Support**

#### **4.1. Factory Reset (EEPROM Reset)**

This procedure resets all settings, band states, and clears all saved favorites to factory defaults.

1.  Power the receiver **off**.
2.  **Press and hold the encoder button**.
3.  While holding the button, **power the receiver on**.
4.  The screen will display **"MEM RESET"** on first boot after reset (or **"MEM WEAR"** if EEPROM is worn).

**Alternative Method:** If the encoder button is faulty, you can perform a reset by **holding the `AGC` button** during power-on instead.

---

<details>
<summary><b>Click to expand: Section 5 (Experimental Features)</b></summary>

#### **5.1. CW (Morse Code) Decoder** *(Experimental)*

> **⚠️ IMPORTANT:** This feature requires hardware modification and disabling Favorites (`ENABLE_FAVORITES 0`) and RDS MINI (`ENABLE_RDS_MINI 0`) due to memory constraints (requires ~30KB flash).

The firmware includes an experimental CW decoder using a fixed-point Goertzel algorithm for real-time Morse code to text conversion directly on the OLED display.

**Technical Implementation:**
- **Algorithm:** Narrowband Goertzel detector (N=128 samples)
- **Reference:** Internal 1.1V ADC reference for improved weak signal detection
- **Adaptive timing:** Automatic WPM tracking (5-60 WPM range)
- **Display:** 6 lines of decoded text with automatic scrolling
- **Input:** Analog pin A6 with AC coupling

**Required Hardware Modification:**

Connect speaker output to A6 with proper biasing:
- **AC Coupling:** 2.2 µF capacitor in series with audio signal
- **Bias Network:**
  - R1: 390 kΩ from +3.48V to A6 (470 kΩ also acceptable, provides ~0.5V bias)
  - R2: 75 kΩ from A6 to GND
  - This creates optimal DC bias for the 1.1V internal reference

**Circuit Diagram:**
![CW Decoder Circuit](https://github.com/user-attachments/assets/860369a8-2c26-4638-a11c-4a60ebe6d625)

**Operation Guide:**

1. **Enable in firmware:** Set `ENABLE_CW_DECODER 1`, `ENABLE_FAVORITES 0`, and `ENABLE_RDS_MINI 0` in Defines.h before compiling
2. **Setup:**
   - Switch to CW mode
   - Set `CWP` (CW Pitch) in settings to match expected tone (600-700 Hz recommended)
3. **Activate decoder:** Long-press `MODE` button to enter decoder view
4. **Display shows:**
   - Header: "CW DECODER" with live WPM reading
   - Main area: Decoded text (auto-scrolls after 6 lines)
5. **Exit:** Press any button to return to normal display

**Performance Characteristics:**
- **Optimal reception:** 600 Hz tone at 25-30 WPM
- **Pitch options:** 500, 600, 700, 800 Hz (selectable via `CWP` setting)
- **Adaptive timing:** Automatically adjusts to speed variations
- **Best results with:**
  - Stable signals (S5 or stronger)
  - Narrow bandwidth filter (0.5-1.0 kHz)
  - Minimal QRM/QRN

**Known Limitations:**
- Requires clean audio signal (affected by noise and fading)
- May struggle with hand-sent code with irregular timing
- Cannot decode overlapping signals

**Alternative Solution:**
For professional-grade decoding performance:
- **Android App:** [Morse Expert by VE3NEA](https://ve3nea.github.io/MorseExpert/)
- Provides superior algorithm, adjustable filters, and logging capabilities

**Test Signal:**
For decoder testing at 600Hz, 30WPM try: `A A A A A / T T T T T / E E E E E / N A N A N A / PARIS PARIS PARIS`

**Additional Technical Details:**

**Signal Processing Chain:**
- **ADC Sampling:** Continuous sampling at ~71.4 Hz block rate (14ms per block)
- **DC Offset Tracking:** Adaptive high-pass filter with 256-sample time constant
- **Goertzel Bins:** Pre-computed coefficients for bins 0-32 (Q14 fixed-point)
- **Signal Detection:** Dual-filter envelope tracking:
  - Fast envelope: Immediate response for tone detection
  - Slow noise floor: 64-sample time constant for baseline estimation
  - Hysteresis margin: Dynamic threshold with 3dB separation

**Morse Timing Engine:**
- **Dot length:** Adaptive from 2-6 blocks (28-84ms)
- **Dash/Dot ratio:** ITU standard 3:1 with tolerance for hand-keying
- **Letter gap:** 1.5× dot length
- **Word gap:** 4.5× dot length
- **Symbol timeout:** 140ms auto-flush for incomplete characters

**Decoder Features:**
- **Character set:** Full alphabet (A-Z), numbers (0-9), slash (/)
- **Pattern matching:** 5-bit packed lookup table for efficient memory use
- **Adaptive speed tracking:**
  - Smoothing factor: 7/8 old + 1/8 new (prevents jitter)
  - WPM display updates every 5 decoded symbols
  - Speed range: 5-60 WPM with automatic adjustment

**Memory Optimization:**
- **Buffer size:** 128 samples (int16_t) for Goertzel processing
- **Lookup table:** 36 bytes for Morse patterns (compressed format)
- **State machine:** 24 bytes total for decoder state
- **Display buffer:** Direct write to OLED (no frame buffer)

**Debug Mode:** (When `DEBUG_CW 1`)
- Serial output at 9600 baud showing:
  - ADC range and DC offset
  - Detected dots/dashes with timing
  - Envelope and noise floor levels
  - Real-time WPM calculations

**Hardware Notes:**
- **Why A6:** This pin has no digital functions, dedicated ADC input
- **Voltage divider values:** Creates ~0.55V DC bias (center of 1.1V range)
- **Capacitor value:** 2.2µF provides -3dB at ~7Hz (blocks DC, passes audio)
- **Input impedance:** ~465kΩ (set by bias network)

</details>

---

### **Quick Reference Button Chart**

(Applies when on the main listening screen)

| Button | Short Press (Tap) | Long Press (Hold 1-2 sec) |
| :--- | :--- | :--- |
| **`MODE`** | Cycle Mode (`AM`→`SSB`→`CW`) | Toggle Sync *(LSB/USB)* / CW Decoder *(CW, only if `ENABLE_CW_DECODER 1`)* / Toggle RDS *(FM)* |
| **`STEP`** | Activate Step selection | Open/Close Favorites Menu |
| **`BW`** | Activate Bandwidth selection | Switch Sideband *(SSB/CW only)* |
| **`BAND+`** | Activate Band selection | Cycle bands up continuously |
| **`BAND-`** | Open/Close Settings Menu | Cycle bands down continuously |
| **`VOL+`** | Activate Volume control | Increase volume continuously |
| **`VOL-`** | Toggle Mute | Decrease volume continuously |
| **`AGC`** | Toggle Screen Power On/Off | Save Current Station to Favorites |
| **`Encoder`** | Activate Step OR Start Scan (depends on `SCN`; scan not available in SSB/CW) | (No function) |

---

If you encounter difficulties with the new versions, you can temporarily roll back and download the version you like best using:

```
https://github.com/diqezit/ats20_ats_ex/archive/<commit_hash>.zip
```

Where **`<commit_hash>`** is copied from the list of commits at:
[https://github.com/diqezit/ats20\_ats\_ex/commits/mod\_no\_rds/](https://github.com/diqezit/ats20_ats_ex/commits/mod_no_rds/)

This way you will get the firmware archive of the desired version.

Thank you.
