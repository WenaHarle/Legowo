// Legowo - RGB_LED
// Three sliders (ch 11-13, 0-255) set the RGB LED, switch ch 10 turns it on/off.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

const int PIN_R = 25, PIN_G = 26, PIN_B = 27;

void applyColor() {
  bool on = Legowo.read(10) > 0;
  analogWrite(PIN_R, on ? (int)Legowo.read(11) : 0);
  analogWrite(PIN_G, on ? (int)Legowo.read(12) : 0);
  analogWrite(PIN_B, on ? (int)Legowo.read(13) : 0);
}

void setup() {
  Serial.begin(115200);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
}

void loop() {
  Legowo.run();
}

LEGOWO_RECEIVE(10) { applyColor(); }
LEGOWO_RECEIVE(11) { applyColor(); }
LEGOWO_RECEIVE(12) { applyColor(); }
LEGOWO_RECEIVE(13) { applyColor(); }
