// Legowo - DHT_Sensor
// Suhu (ch 0) dan kelembapan (ch 1) dari DHT22, relay (ch 10) dari dashboard.
// Butuh library "DHT sensor library" (Adafruit) dari Library Manager.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>
#include <DHT.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

DHT dht(4, DHT22);                      // data DHT di GPIO4

void bacaDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) return;     // sensor belum siap / kabel lepas
  Legowo.write(0, t);
  Legowo.write(1, h);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Legowo.bindSwitch(10, 16);            // relay di GPIO16
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(5000, bacaDHT);
}

void loop() {
  Legowo.run();
}
