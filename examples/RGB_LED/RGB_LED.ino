// Legowo - RGB_LED
// Tiga slider (ch 11-13, 0-255) mengatur LED RGB, switch ch 10 menyalakan/mematikan.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

const int PIN_R = 25, PIN_G = 26, PIN_B = 27;

void terapkanWarna() {
  bool nyala = Legowo.read(10) > 0;
  analogWrite(PIN_R, nyala ? (int)Legowo.read(11) : 0);
  analogWrite(PIN_G, nyala ? (int)Legowo.read(12) : 0);
  analogWrite(PIN_B, nyala ? (int)Legowo.read(13) : 0);
}

void setup() {
  Serial.begin(115200);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
}

void loop() {
  Legowo.run();
}

LEGOWO_RECEIVE(10) { terapkanWarna(); }
LEGOWO_RECEIVE(11) { terapkanWarna(); }
LEGOWO_RECEIVE(12) { terapkanWarna(); }
LEGOWO_RECEIVE(13) { terapkanWarna(); }
