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

# **ATS-20+ EX Firmware - The Complete User Guide**

This manual provides a comprehensive overview of the ATS-20+ EX firmware's features and controls

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
1.  **Enter/Exit:** Short-press **`BAND-`**.
2.  **Navigation:** **Rotate the encoder** to select items. Short-press **`BAND+`** to change pages.
3.  **Editing:**
    1.  Select an item, then short-press the **encoder button** to enter "Edit Mode".
    2.  **Rotate the encoder** to change the value.
    3.  Short-press the **encoder button** again to confirm.
4.  **Save & Exit:** Exit the menu by pressing `BAND-` or by waiting for the auto-exit timeout. Changes are saved to EEPROM automatically.

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
