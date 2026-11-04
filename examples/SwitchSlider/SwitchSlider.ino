// Legowo - SwitchSlider
// Tanpa menulis handler: binding langsung channel dashboard <-> pin.
//   ch 0  <- tombol BOOT (GPIO0)        ch 10 -> relay (GPIO16)
//   ch 1  <- potensiometer (GPIO34, %)  ch 13 -> LED PWM (GPIO2, slider 0-255)
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

void setup() {
  Serial.begin(115200);
  Legowo.bindSwitch(10, 16);                    // relay
  Legowo.bindSlider(13, 2, 0, 255);             // kecerahan LED
  Legowo.bindSensor(0, 0, DIGITAL_INVERTED);    // tombol (INPUT_PULLUP)
  Legowo.bindSensor(1, 34, ANALOG_PERCENT);     // potensiometer 0-100%
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
}

void loop() {
  Legowo.run();
}
