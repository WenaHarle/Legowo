// Legowo - Basic
// Send a sensor value to channel 0 and control an LED from the dashboard (channel 10).
// Device key: Legowo dashboard -> Devices -> Add device -> copy the key.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

const int LED_PIN = 2;

void sendSensor() {
  Legowo.write(0, analogRead(34));      // channel 0 -> dashboard
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(2000, sendSensor);       // every 2 seconds, without delay()
}

void loop() {
  Legowo.run();
}

LEGOWO_RECEIVE(10) {                    // dashboard switch -> channel 10
  digitalWrite(LED_PIN, param.asInt());
}
