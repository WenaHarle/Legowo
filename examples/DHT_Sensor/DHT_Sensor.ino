// Legowo - DHT_Sensor
// Temperature (ch 0) and humidity (ch 1) from a DHT22, relay (ch 10) from the dashboard.
// Needs the "DHT sensor library" (Adafruit) from the Library Manager.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>
#include <DHT.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

DHT dht(4, DHT22);                      // DHT data on GPIO4

void readDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) return;     // sensor not ready / wire disconnected
  Legowo.write(0, t);
  Legowo.write(1, h);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Legowo.bindSwitch(10, 16);            // relay on GPIO16
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(5000, readDHT);
}

void loop() {
  Legowo.run();
}
