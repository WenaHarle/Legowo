// Legowo - Basic
// Kirim nilai sensor ke channel 0 dan kendalikan LED dari dashboard (channel 10).
// Device key: dashboard Legowo -> Perangkat -> Tambah perangkat -> salin key.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

const int LED_PIN = 2;

void kirimSensor() {
  Legowo.write(0, analogRead(34));      // channel 0 -> dashboard
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(2000, kirimSensor);      // tiap 2 detik, tanpa delay()
}

void loop() {
  Legowo.run();
}

LEGOWO_WRITE(10) {                      // switch di dashboard -> channel 10
  digitalWrite(LED_PIN, param.asInt());
}
