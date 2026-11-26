#include <Arduino.h>
#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProDimSwitch.h>

// =====================================================================
// 1. ENTER YOUR WI-FI & SINRIC PRO CREDENTIALS HERE
// =====================================================================
#define WIFI_SSID       "Carolynternet"
#define WIFI_PASS       "FRee5Gwifi09916036290!!!"
#define APP_KEY         "44b8324c-2d04-4e55-b720-f201ed219b5f"
#define APP_SECRET      "d4d28f49-1e98-4525-9b58-a466d02bd052-06e8c931-3ad6-424a-acc9-c67b6022fae1"
#define FAN_DEVICE_ID   "6accc1801a22f0aaee65a267"

// =====================================================================
// 2. ESP32-S3 N16R8 SAFE GPIO PIN DEFINITIONS
// =====================================================================
#define APP_BTN0    4   // Stop Button (0%)
#define APP_BTN1    5   // Low Speed Button (33%)
#define APP_BTN2    6   // Normal Speed Button (66%)
#define APP_BTN3    7   // High Speed Button (100%)

#define APP_OE      15  // ENABLE (TXS0108E Logic Shifter)
#define APP_RELAY1  16  // Relay 1 (Low Speed - 33%)
#define APP_RELAY2  17  // Relay 2 (Normal Speed - 66%)
#define APP_RELAY3  18  // Relay 3 (High Speed - 100%)

// =====================================================================
// 3. RELAY TRIGGER LOGIC (Active-LOW for SunFounder 4-Channel Module)
// =====================================================================
// Most optocoupler relay modules (like SunFounder 4-Relay) turn ON on LOW (0V).
// If using an Active-HIGH relay module, swap LOW and HIGH below.
#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

int currentPowerLevel = 0;
bool cloudConnected = false;
bool sinricStarted = false;

// Helper to print to BOTH USB-C ports (UART0 and Native USB) on ESP32-S3
void logPrintln(const String &msg) {
  Serial.println(msg);
  Serial0.println(msg);
}

// Normalize user percentage input to 0, 33, 66, or 100 (Page 15 of Lab Manual)
int update_power(int power_level) {
  if (power_level <= 0)  return 0;
  if (power_level <= 33) return 33;
  if (power_level <= 66) return 66;
  return 100;
}

// Drive the physical relay pins based on the fan state
void apphw_set_state(int level) {
  currentPowerLevel = update_power(level);

  // Turn all relays OFF first
  digitalWrite(APP_RELAY1, RELAY_OFF);
  digitalWrite(APP_RELAY2, RELAY_OFF);
  digitalWrite(APP_RELAY3, RELAY_OFF);

  // Switch ON the corresponding speed relay
  if (currentPowerLevel == 33) {
    digitalWrite(APP_RELAY1, RELAY_ON);
    logPrintln("[HW] Fan Speed: LOW (33%) -> RELAY 1 (GPIO 16) ON");
  } 
  else if (currentPowerLevel == 66) {
    digitalWrite(APP_RELAY2, RELAY_ON);
    logPrintln("[HW] Fan Speed: NORMAL (66%) -> RELAY 2 (GPIO 17) ON");
  } 
  else if (currentPowerLevel == 100) {
    digitalWrite(APP_RELAY3, RELAY_ON);
    logPrintln("[HW] Fan Speed: HIGH (100%) -> RELAY 3 (GPIO 18) ON");
  } 
  else {
    logPrintln("[HW] Fan Speed: OFF (0%) -> ALL RELAYS OFF");
  }
}

// Handle Alexa "Turn On / Turn Off" directives
bool onPowerState(const String &deviceId, bool &state) {
  logPrintln(String("[ALEXA] PowerState Directive: ") + (state ? "ON" : "OFF"));
  if (!state) {
    apphw_set_state(0);
  } else if (currentPowerLevel == 0) {
    apphw_set_state(33); // Start at Low speed (33%) when turned ON
  }
  return true;
}

// Handle Alexa "SetPowerLevel" directives (e.g. "Alexa, set Smart Fan to 50%")
bool onPowerLevel(const String &deviceId, int &powerLevel) {
  logPrintln(String("[ALEXA] SetPowerLevel Directive: ") + String(powerLevel) + "%");
  apphw_set_state(powerLevel);
  powerLevel = currentPowerLevel;
  return true;
}

// Handle Alexa "AdjustPowerLevel" directives (e.g. "Alexa, increase Smart Fan by 10%")
bool onAdjustPowerLevel(const String &deviceId, int &levelDelta) {
  int newLevel = constrain(currentPowerLevel + levelDelta, 0, 100);
  logPrintln(String("[ALEXA] AdjustPowerLevel Delta: ") + String(levelDelta) + "% -> " + String(newLevel) + "%");
  apphw_set_state(newLevel);
  levelDelta = currentPowerLevel;
  return true;
}

// Publish state to Cloud when a physical button is pressed on the breadboard
void appcloud_publish(int val) {
  apphw_set_state(val);

  if (WiFi.status() == WL_CONNECTED && cloudConnected) {
    SinricProDimSwitch &myFan = SinricPro[FAN_DEVICE_ID];
    myFan.sendPowerStateEvent(currentPowerLevel > 0);
    if (currentPowerLevel > 0) {
      myFan.sendPowerLevelEvent(currentPowerLevel);
    }
    logPrintln(String("[CLOUD] Published reported powerlevel: ") + String(currentPowerLevel) + "%");
  } else {
    logPrintln(String("[LOCAL] Relay updated to ") + String(currentPowerLevel) + "% (Cloud offline)");
  }
}

// Poll the 4 breadboard push buttons
void checkButtons() {
  static unsigned long lastPressTime = 0;
  if (millis() - lastPressTime < 250) return; // 250ms debounce

  if (digitalRead(APP_BTN0) == LOW && currentPowerLevel != 0) {
    lastPressTime = millis();
    logPrintln("\r\n[BTN] Stop Button (GPIO 4) Pressed!");
    appcloud_publish(0);
  } 
  else if (digitalRead(APP_BTN1) == LOW && currentPowerLevel != 33) {
    lastPressTime = millis();
    logPrintln("\r\n[BTN] Low Speed Button (GPIO 5) Pressed!");
    appcloud_publish(33);
  } 
  else if (digitalRead(APP_BTN2) == LOW && currentPowerLevel != 66) {
    lastPressTime = millis();
    logPrintln("\r\n[BTN] Normal Speed Button (GPIO 6) Pressed!");
    appcloud_publish(66);
  } 
  else if (digitalRead(APP_BTN3) == LOW && currentPowerLevel != 100) {
    lastPressTime = millis();
    logPrintln("\r\n[BTN] High Speed Button (GPIO 7) Pressed!");
    appcloud_publish(100);
  }
}

void startSinricPro() {
  if (sinricStarted) return;
  sinricStarted = true;

  SinricProDimSwitch &myFan = SinricPro[FAN_DEVICE_ID];
  myFan.onPowerState(onPowerState);
  myFan.onPowerLevel(onPowerLevel);
  myFan.onAdjustPowerLevel(onAdjustPowerLevel);

  SinricPro.onConnected([]() {
    cloudConnected = true;
    logPrintln("[CLOUD] >>> Connected to Sinric Pro & Alexa Ready! <<<");
  });
  SinricPro.onDisconnected([]() {
    cloudConnected = false;
    logPrintln("[CLOUD] Disconnected from Sinric Pro!");
  });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(1500);

  logPrintln("\r\n==========================================");
  logPrintln("   ESP32-S3 Voice-Controlled Smart Fan");
  logPrintln("==========================================");

  // Initialize Button GPIOs with internal pull-up resistors
  pinMode(APP_BTN0, INPUT_PULLUP);
  pinMode(APP_BTN1, INPUT_PULLUP);
  pinMode(APP_BTN2, INPUT_PULLUP);
  pinMode(APP_BTN3, INPUT_PULLUP);

  // Initialize Level Shifter Enable & Relay GPIOs as outputs
  pinMode(APP_OE, OUTPUT);
  pinMode(APP_RELAY1, OUTPUT);
  pinMode(APP_RELAY2, OUTPUT);
  pinMode(APP_RELAY3, OUTPUT);

  // Enable TXS0108E Port B outputs (set OE high)
  digitalWrite(APP_OE, HIGH);
  apphw_set_state(0);

  // Scan 2.4GHz Wi-Fi networks so we can verify the exact SSID
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  // Set TX power to 8.5dBm (fixes ESP32-S3 N16R8 Wi-Fi connection stability)
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  logPrintln("[WIFI] Scanning nearby 2.4GHz Wi-Fi networks...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    logPrintln(String("  Found 2.4GHz Wi-Fi: \"") + WiFi.SSID(i) + "\" (" + String(WiFi.RSSI(i)) + " dBm)");
  }

  logPrintln(String("\r\n[WIFI] Connecting to \"") + WIFI_SSID + "\"...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
    checkButtons(); // Buttons work immediately even while Wi-Fi is connecting!
    Serial.print(".");
    Serial0.print(".");
    delay(400);
  }

  if (WiFi.status() == WL_CONNECTED) {
    logPrintln(String("\r\n[WIFI] Connected! IP: ") + WiFi.localIP().toString());
    startSinricPro();
  } else {
    logPrintln("\r\n[WIFI] Still connecting in background (check 2.4GHz Wi-Fi list above)...");
  }
}

void loop() {
  checkButtons();

  if (WiFi.status() == WL_CONNECTED) {
    if (!sinricStarted) {
      logPrintln(String("\r\n[WIFI] Connected! IP: ") + WiFi.localIP().toString());
      startSinricPro();
    }
    SinricPro.handle();
  }

  // Print a live heartbeat every 5 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= 5000) {
    lastStatus = millis();
    logPrintln(String("[STATUS] WiFi: ") + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Connecting...") +
               " | Cloud: " + (cloudConnected ? "ONLINE" : "Waiting...") +
               " | Fan PowerLevel: " + String(currentPowerLevel) + "%");
  }
}
