# BCA152: Voice-Controlled Smart Fan (ESP32-S3 & Alexa)

An IoT-enabled Voice-Controlled Smart Fan breadboard prototype developed for **BCA152** (based on Chapter 12 of *Internet of Things with ESP32* by Packt Publishing). 

This project transforms a conventional 3-speed desk/standing fan into an intelligent smart appliance controllable via physical push buttons, the **Amazon Alexa** mobile app/Echo device, and real-time cloud synchronization.

---

* **4 Discrete Fan Operating Modes:**
  * **STOP / OFF (0%):** All speed relays de-energized.
  * **LOW Speed (33%):** Relay 1 (K1) energized.
  * **NORMAL Speed (66%):** Relay 2 (K2) energized.
  * **HIGH Speed (100%):** Relay 3 (K3) energized.
* **Speed Normalization Engine:** Automatically normalizes arbitrary user voice percentages to the defined discrete speed tiers ($0\%, 33\%, 66\%, 100\%$) matching lab specification.
* **Physical Tactile Push Buttons:** 4 momentary breadboard buttons with software debounce (250 ms) and ESP32 internal pull-up resistors (`INPUT_PULLUP`).
* **Bidirectional Cloud Synchronization:**
  * Physical button presses immediately actuate the relays and publish the updated state to the cloud.
  * Alexa voice commands instantly actuate the relays and provide acoustic/visual feedback.
* **100% Free Cloud Architecture (No AWS Credit Card Needed):** Utilizes **Sinric Pro** IoT WebSocket bridge linked directly to the official **Amazon Alexa Smart Home Skill**.
* **ESP32-S3 Safe Pin Mapping:** Specifically designed for the **ESP32-S3 N16R8** development board, carefully avoiding the internally reserved Octal PSRAM and SPI Flash pins (`GPIO 26–37`).

---
Hardware Requirements

| Qty | Component | Specification / Description | Purpose in Lab |
| :---: | :--- | :--- | :--- |
| **1** | **ESP32-S3 DevKit** | ESP32-S3 N16R8 (16MB Flash, 8MB Octal PSRAM) | Central IoT controller & Wi-Fi client |
| **1** | **4-Channel Relay Module** | SunFounder 5V DC Relay Module with Optocouplers | Acts as the fan speed switches (Active-LOW) |
| **4** | **Tactile Push Buttons** | 4-pin momentary tactile switches | Speed buttons: Stop, Low, Normal, High |
| **1** | **Logic Level Converter** | TXS0108E 3.3V-to-5V Bi-directional Shifter | Steps up ESP32 3.3V GPIO to 5V relay inputs |
| **1** | **Breadboard Power Supply** | MB102 3.3V/5V Power Module + DC Adapter/USB | Supplies stable 5V DC to the relay coils |
| **1** | **Solderless Breadboard** | 830 tie-point full-size breadboard | Component prototyping |
| **30+** | **Jumper Wires** | Male-to-Male (M-M) and Male-to-Female (M-F) | Circuit connections |
| **1** | **USB-C Cable** | Data-capable USB-C cable | Flashing and real-time serial monitoring |

---

Pin Configuration & Wiring

### 1. ESP32-S3 Pin Mapping Table

| Function | ESP32-S3 GPIO | Connected Component | Circuit Note |
| :--- | :---: | :--- | :--- |
| **Button 0 (Stop / 0%)** | `GPIO 4` | Push Button 0 | Connects diagonally between GPIO 4 and GND |
| **Button 1 (Low / 33%)** | `GPIO 5` | Push Button 1 | Connects diagonally between GPIO 5 and GND |
| **Button 2 (Normal / 66%)** | `GPIO 6` | Push Button 2 | Connects diagonally between GPIO 6 and GND |
| **Button 3 (High / 100%)** | `GPIO 7` | Push Button 3 | Connects diagonally between GPIO 7 and GND |
| **Level Shifter OE** | `GPIO 15` | TXS0108E `OE` pin | Output Enable (driven `HIGH`) |
| **Relay 1 (Low Speed)** | `GPIO 16` | Level Shifter A1 $\rightarrow$ Relay `IN1` | Triggers Relay K1 (Active-LOW) |
| **Relay 2 (Normal Speed)** | `GPIO 17` | Level Shifter A2 $\rightarrow$ Relay `IN2` | Triggers Relay K2 (Active-LOW) |
| **Relay 3 (High Speed)** | `GPIO 18` | Level Shifter A3 $\rightarrow$ Relay `IN3` | Triggers Relay K3 (Active-LOW) |

---

4-Channel Relay Header Pinout

```text
[JD-VCC / VCC Jumper] ────► Keep Green Jumper Cap ON (bridges JD-VCC and VCC)

6-Pin Header:
  ├── Pin 1 (Bottom, near logo) : GND   ──► Connect to Breadboard Negative Rail (-)
  ├── Pin 2                    : IN1   ──► Connect to GPIO 16 (or Level Shifter B1)
  ├── Pin 3                    : IN2   ──► Connect to GPIO 17 (or Level Shifter B2)
  ├── Pin 4                    : IN3   ──► Connect to GPIO 18 (or Level Shifter B3)
  ├── Pin 5                    : IN4   ──► Empty (unused)
  └── Pin 6 (Top, near jumper)  : VCC   ──► Connect to Breadboard Positive Rail (+ 5V)
```

---

### 3. Common Ground Rule (Crucial)
```text
  [Breadboard Positive Rail (+)] ────────► 5V (MB102 / 5V DC source)
  
  [Breadboard Negative Rail (-)] ┬───────► MB102 GND
                                 ├───────► ESP32-S3 GND
                                 ├───────► SunFounder Relay GND
                                 └───────► All 4 Button GND legs
```

---

## 💻 Software Architecture & Code

The project is structured for **Visual Studio Code** with the **PlatformIO** extension.

### Directory Structure
```text
BCA152-Voice-Controlled-SmartFan/
├── .gitignore
├── platformio.ini
├── README.md
└── src/
    └── main.cpp
```

### `platformio.ini`
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
monitor_rts = 0
monitor_dtr = 0

build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

lib_deps =
    sinricpro/SinricPro @ ^3.1.1
    bblanchon/ArduinoJson @ ^7.0.4
    links2004/WebSockets @ ^2.4.1
```

---

## ☁️ Cloud & Alexa Integration (Sinric Pro)

1. **Sinric Pro Account:** Create a free account at [https://portal.sinric.pro](https://portal.sinric.pro).
2. **Device Creation:**
   * Create a device of type **DimSwitch** (or custom template with `Power` and `Power Level` capabilities) named **`Smart Fan`**.
   * Copy your **App Key**, **App Secret**, and **Device ID** into `src/main.cpp`.
3. **Amazon Alexa Skill:**
   * In the **Amazon Alexa App**, navigate to **More** $\rightarrow$ **Skills & Games**.
   * Search for **Sinric Pro**, enable the skill, and log in with your Sinric Pro account.
   * Run device discovery to find **`Smart Fan`**.

---

## 🧪 Live Verification & Testing Results

### Test 1: Physical Push Button Actuation $\rightarrow$ Cloud Sync

```text
[STATUS] WiFi: 192.168.1.121 | Cloud: ONLINE | Fan PowerLevel: 0%

[BTN] Low Speed Button (GPIO 5) Pressed!
[HW] Fan Speed: LOW (33%) -> RELAY 1 (GPIO 16) ON
[CLOUD] Published reported powerlevel: 33%

[BTN] Normal Speed Button (GPIO 6) Pressed!
[HW] Fan Speed: NORMAL (66%) -> RELAY 2 (GPIO 17) ON
[CLOUD] Published reported powerlevel: 66%

[BTN] High Speed Button (GPIO 7) Pressed!
[HW] Fan Speed: HIGH (100%) -> RELAY 3 (GPIO 18) ON
[CLOUD] Published reported powerlevel: 100%

[BTN] Stop Button (GPIO 4) Pressed!
[HW] Fan Speed: OFF (0%) -> ALL RELAYS OFF
[CLOUD] Published reported powerlevel: 0%
```

---

### Test 2: Alexa Voice Directives $\rightarrow$ Hardware Actuation

1. **"Alexa, set Smart Fan to 20 percent"**
   ```text
   [ALEXA] SetPowerLevel Directive: 20%
   [HW] Fan Speed: LOW (33%) -> RELAY 1 (GPIO 16) ON
   [STATUS] WiFi: 192.168.1.121 | Cloud: ONLINE | Fan PowerLevel: 33%
   ```
2. **"Alexa, set Smart Fan to 50 percent"**
   ```text
   [ALEXA] SetPowerLevel Directive: 50%
   [HW] Fan Speed: NORMAL (66%) -> RELAY 2 (GPIO 17) ON
   ```
3. **"Alexa, turn off Smart Fan"**
   ```text
   [ALEXA] PowerState Directive: OFF
   [HW] Fan Speed: OFF (0%) -> ALL RELAYS OFF
   [STATUS] WiFi: 192.168.1.121 | Cloud: ONLINE | Fan PowerLevel: 0%
   ```

---

## 🚀 How to Run the Software

1. Clone this repository:
   ```bash
   git clone https://github.com/mccdz1/BCA152-Voice-Controlled-SmartFan.git
   ```
2. Open the cloned folder in **VS Code**.
3. Install the **PlatformIO IDE** extension.
4. Plug the **ESP32-S3** board into your computer via USB-C.
5. Click the **Upload (`→`)** button on the bottom status bar.
6. Click the **Serial Monitor (`🔌`)** button to view live telemetry at `115200` baud.

---

## 👤 Author
* **Michael Cadiz**
* Course: BCA152 - Internet of Things
