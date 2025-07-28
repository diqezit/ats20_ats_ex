### **ATS-20+ Firmware Modifications (diqezit's Fork)**

This repository is a fork of the original `goshante/ats20_ats_ex` firmware, dedicated to custom modifications, bug fixes, and new features. All discussion regarding these new versions should take place here.

Your feedback and suggestions are welcome!
**Please**, put star to raise the firmware higher up on the global list, **thanks :)**

You can find the original project here: [goshante/ats20_ats_ex](https://github.com/goshante/ats20_ats_ex)

---

### **Key Modifications**

This fork introduces two main branches of improvements over the original firmware:

1.  **Audio Pop/Click Elimination (Hardware & Software Mod):** A modification that completely removes pops and clicks when switching modes. It requires a minor physical change to the receiver's circuit.
2.  **`MOD_NO_RDS` Firmware Series:** An alternative firmware branch where the RDS feature was removed to free up space for new functionality, including an FM Favorites menu, along with dozens of other fixes and improvements.

**Important:** Pops`s issue improvement only affects the speaker output, as the headphone jack is connected before the amplifier.

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
    *   [Mode Switching](#15-mode-switching)
3.  **Section 2: Advanced Reception Controls**
    *   [SSB Mode Operation](#21-ssb-mode-operation)
    *   [CW (Morse Code) Mode Operation](#22-cw-morse-code-mode-operation)
    *   [Bandwidth (BW) Filter Adjustment](#23-bandwidth-bw-filter-adjustment)
    *   [Sync Feature (Synchronous Detector)](#24-sync-feature-synchronous-detector)
4.  **Section 3: System Functions**
    *   [Station Scanning](#31-station-scanning)
    *   [The Settings Menu: A Deep Dive](#32-the-settings-menu-a-deep-dive)
    *   [FM Favorites Management](#33-fm-favorites-management)
5.  **Section 4: Maintenance & Support**
    *   [Factory Reset (EEPROM Reset)](#41-factory-reset-eeprom-reset)
6.  **Quick Reference Button Chart**
    *   [Main Screen Operations Chart](#quick-reference-button-chart)

---

### **1. Core Concepts: Understanding the Interface**

The firmware's UI is based on the **"Active Command"** paradigm.

*   **Default State:** By default, the **encoder knob** controls the **frequency**.
*   **Active Command:** A short press on `VOL+`, `STEP`, `BW`, or `BAND+` makes that function the "Active Command". The corresponding UI element becomes inverted, and the **encoder knob temporarily controls that function** (e.g., volume, step size).

To exit an Active Command, either wait for the automatic timeout or short-press the **encoder button** to cancel immediately.

---

### **Section 1: Main Screen Operations**

#### **1.1. Power Management & Screen Control**

*   **Screen Off Mode:** A short press on the **`AGC`** button toggles the screen's power. The receiver continues to operate, significantly reducing battery consumption.

#### **1.2. Frequency Tuning & Step Control**

*   **Tuning Step Selection:**
    1.  Short-press **`STEP`**. The "STEP" value on the display will be highlighted.
    2.  **Rotate the encoder** to select the desired step size.
    3.  Short-press the **encoder button** to confirm.

#### **1.3. Volume Control & Mute**

*   **Quick Adjustment:** Short-press **`VOL+`**, then **rotate the encoder**.
*   **Continuous Adjustment:** **Press and hold `VOL+`** or **`VOL-`**.
*   **Mute:** Short-press **`VOL-`** to toggle mute. The volume level is replaced by " M".

#### **1.4. Band Navigation**

*   **Quick Jump:** Short-press **`BAND+`**, then **rotate the encoder** to jump between pre-defined bands.
*   **Seamless Tuning:** Tune past the edge of the current band to automatically switch to the adjacent one.

#### **1.5. Mode Switching**

*   **Short-press `MODE`** to cycle through reception modes: `AM` → `LSB`/`USB` → `CW` → `AM`.

---

### **Section 2: Advanced Reception Controls**

#### **2.1. SSB Mode Operation**

1.  **Enter SSB Mode:** Use the `MODE` button until `LSB` or `USB` is displayed.
2.  **Sideband Selection:** **Press and hold the `STEP` button** for 1-2 seconds to switch between `LSB` and `USB`.
3.  **Fine-Tuning (BFO vs. VFO):**
    *   Select a **small step** (e.g., `10 Hz`, `50 Hz`) to adjust the **BFO**. This changes the audio pitch for voice clarification.
    *   Select a **large step** (e.g., `1 kHz`) to adjust the **VFO** (main frequency).

#### **2.2. CW (Morse Code) Mode Operation**

1.  **Enter CW Mode:** Use the `MODE` button to select `CW`.
2.  **CW Sideband:** **Press and hold `STEP`** to switch the reception sideband. This can help isolate a signal from interference. An `L` or `U` indicator will appear.
3.  **Tuning:** The receiver's CW pitch is fixed at ~500 Hz. Tune the main frequency until the signal's tone matches this pitch.

#### **2.3. Bandwidth (BW) Filter Adjustment**

1.  Short-press the **`BW`** button.
2.  **Rotate the encoder** to change the filter width.
    *   **Narrower (e.g., 1.8 kHz):** Reduces or eliminates adjacent channel interference.
    *   **Wider (e.g., 4.0 kHz):** Provides better audio fidelity on strong, clear signals.

#### **2.4. Sync Feature (Synchronous Detector)**

*   While in SSB mode, **press and hold the `MODE` button** to toggle the Sync feature. An `S` indicator will appear when active.

---

### **Section 3: System Functions**

#### **3.1. Station Scanning**

*   **Enable First:** In the Settings Menu (`BAND-`), set the `SCN` item to `On`.
*   **Start/Stop Scan:** In AM or FM mode, short-press the **encoder button** to start scanning. Press any button to stop.

#### **3.2. The Settings Menu: A Deep Dive**

To enter and exit the settings menu, perform a **short press** on the **`BAND-`** button. The menu is organized into three pages.

*   **Navigation:** Rotate the encoder to select an item. Short-press **`BAND+`** to switch between pages.
*   **Editing:** Select an item, then short-press the encoder to enter "Edit Mode" (a `>` will appear). Rotate the encoder to change the value. Press the encoder again to confirm.
*   **Save & Exit:** The menu will close automatically after 10 seconds of inactivity, or you can exit manually by pressing `BAND-`. All changes are saved to EEPROM.

##### **Page 1: General & Audio**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `ATT` | **Attenuator.** Manages the receiver's front-end gain. **`AUT`:** Standard **Automatic Gain Control (AGC)** mode. **Manual values:** Disables AGC and sets a **fixed attenuation level**. Higher values mean stronger signal reduction. Max level is `37` for AM/SSB and `26` for FM. | Selection | `AUT`, `1`..`37` |
| `SCN` | **Scan Switch.** Determines the function of a short encoder press in AM/FM modes. **`On`:** Starts a **Seek** for the next station. **`Off`:** Activates **Step selection mode** (`STEP`). **Note:** In SSB/CW, an encoder press **always** activates `STEP` mode. | Switch | `On` / `Off` |
| `AVC` | **AVC Max Gain.** Defines the aggressiveness of the volume leveling system for AM/SSB. **Important:** This setting is only active when `ATT` is in `AUT` mode. | Number | `12`..`90` |
| `SMA` | **Soft Mute Attenuation.** Defines *how much* the audio will be attenuated when the signal is weak. `0` disables this feature. **Note:** Has no effect in FM mode. | Number | `0`..`32` |
| `SMT` | **Soft Mute Threshold.** Sets the **minimum SNR** below which soft mute is activated. Helps eliminate hiss between stations. **Note:** Has no effect in FM mode. | Number | `0`..`63` |
| `DE` | **FM De-Emphasis.** Selects the de-emphasis time constant for FM radio. **`50u`:** Europe/Asia. **`75u`:** North America. **Note:** Only active in FM mode. | Selection | `50u` / `75u` |

##### **Page 2: SSB & Display**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `BFO` | **BFO Calibration.** Precisely calibrates the Beat Frequency Oscillator for correct SSB/CW demodulation. Each step changes the offset by 100 Hz (range: -2.5 kHz to +2.5 kHz). | Number | `-25`..`+25` |
| `SSM` | **SSB Soft Mute.** Enables an alternative soft muting algorithm specifically adapted for SSB mode. | Switch | `On` / `Off` |
| `SVC` | **SSB Volume Control.** Activates a separate automatic volume control system that works only in LSB, USB, and CW modes. | Switch | `On` / `Off` |
| `COF` | **SSB Cutoff Filter.** Manages an additional high-pass filter. **`AUT`:** Filter is chosen based on the current bandwidth (BW). **`1` and `2`:** Manual selection for precise interference rejection. | Selection | `AUT`, `1`, `2` |
| `SYN` | **SSB Sync.** Activates the **DSP AFC (Automatic Frequency Control)** function. Helps to "lock on" to an SSB signal by compensating for frequency drift. **Note:** Not active in CW mode. | Switch | `On` / `Off` |
| `SCR` | **Screen Brightness.** Adjusts the brightness of the OLED display. `1` is minimum, `10` is maximum. | Number | `1`..`10` |

##### **Page 3: Hardware & Miscellaneous**

| Name | Detailed Description | Type | Range |
| :--- | :--- | :--- | :--- |
| `CAP` | **Antenna Capacitor.** Controls an extra capacitor at the antenna input. **`AUT`:** Engaged for the FM band only. **`On`:** Forced on for all bands. May improve SW/MW reception. | Switch | `AUT` / `On` |
| `CPU` | **CPU Speed.** Changes the microcontroller's clock speed. **`16MHz`:** Max performance. **`8MHz`:** Low-power mode, increases battery life. | Switch | `16MHz` / `8MHz` |
| `BAP` | **Battery Pin.** Selects the analog port (`A1` or `A2`) for measuring battery voltage. Must match your board's hardware layout for a correct reading. | Selection | `A1` / `A2` |
| `SWU` | **SW Units.** Changes the frequency display format for Shortwave (SW) bands. **`kHz`:** e.g., `7100 kHz`. **`MHz`:** e.g., `7.10 MHz`. | Selection | `kHz` / `MHz` |
| `RSI` | **Disable RSSI in AM.** On some units, RSSI polling in AM can cause audible clicks. **`On`:** Disables RSSI updates in AM to prevent this, but the signal meter will not update. **`Off`:** Standard mode. | Switch | `On` / `Off` |
| `DIS` | **Display Off Timer.** Automatically turns off the screen after inactivity to save battery. **`Off`:** Always on. **`10m`..`60m`:** Time in minutes until display turns off. | Selection | `Off`, `10m`..`60m` |

#### **3.3. FM Favorites Management**

*   **Save Station:** Tune to an FM station, then **press and hold `MODE`**.
*   **Access List:** In FM mode, short-press **`MODE`**.
*   **Navigate & Tune:** **Rotate the encoder** to select, then **short-press the encoder button** to tune.
*   **Delete Station:** In the list, highlight a station and press **`BW`**.

---

### **Section 4: Maintenance & Support**

#### **4.1. Factory Reset**

This procedure resets all parameters to their firmware defaults.

1.  Power the receiver **off**.
2.  **Press and hold the encoder button**.
3.  While holding the button, **power the receiver on**.
4.  The screen will display "EEPROM RST". The reset is complete.

If you encounter any persistent issues or suspect a bug after a reset, please open an issue on our GitHub page:
**[https://github.com/diqezit/ats20_ats_ex/issues](https://github.com/diqezit/ats20_ats_ex/issues)**

---

### **Quick Reference Button Chart**

*(Applies when on the main listening screen)*

| Button | Short Press (Tap) | Long Press (Hold 1-2 sec) |
| :--- | :--- | :--- |
| **`MODE`** | Cycle Mode (`AM`→`SSB`→`CW`) | **SSB:** Toggle Sync. **FM:** Save Favorite. |
| **`STEP`** | Activate Step selection | **SSB/CW:** Switch sideband (LSB↔USB) |
| **`BW`** | Activate Bandwidth selection | (No function) |
| **`BAND+`** | Activate Band selection | (No function) |
| **`BAND-`** | Open/Close Settings Menu | (No function) |
| **`VOL+`** | Activate Volume control | Increase volume continuously |
| **`VOL-`** | Toggle Mute | Decrease volume continuously |
| **`AGC`** | Toggle Screen Power On/Off | (No function) |
| **Encoder** | Activate Step OR Start Scan (depends on `SCN` setting) | (No function) |

------------------------------------------------------------------------------------------------------------

Thank you all for your participation and feedback
