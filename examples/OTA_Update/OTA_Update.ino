// Legowo - OTA_Update
// OTA is enabled by default. Steps to update from the dashboard:
//  1. Bump LEGOWO_FW_VERSION (e.g. "1.0.1"), Sketch > Export Compiled Binary.
//  2. Web app: device > Firmware tab > My firmware > Upload firmware. Pick the file
//     YourSketch.ino.bin (not .bootloader/.partitions/.merged), check "Install now".
//     (Or install official firmware from an admin via the "Update" button.)
//  3. The device downloads, verifies the SHA-256 (and the signature if LEGOWO_OTA_PUBKEY
//     is set), restarts, then reports back.
//     If the new firmware cannot connect to the server within 5 minutes, it rolls back automatically.
// The partition scheme must have 2 OTA slots (the Arduino IDE default is enough).
#define LEGOWO_FW_VERSION "1.0.0"
// #define LEGOWO_OTA_PUBKEY "-----BEGIN PUBLIC KEY-----\n...\n-----END PUBLIC KEY-----\n"
#include <Legowo.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

void setup() {
  Serial.begin(115200);
  Legowo.setDebug(true);                // show OTA progress in the Serial Monitor
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  // Legowo.disableOTA();               // disable if the device must not be updated from the server
}

void loop() {
  Legowo.run();
}
