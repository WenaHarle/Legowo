// Legowo - SwitchSlider
// Without writing any handler: bind dashboard channels directly <-> pins.
//   ch 0  <- BOOT button (GPIO0)        ch 10 -> relay (GPIO16)
//   ch 1  <- potentiometer (GPIO34, %)  ch 13 -> PWM LED (GPIO2, slider 0-255)
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

void setup() {
  Serial.begin(115200);
  Legowo.bindSwitch(10, 16);                    // relay
  Legowo.bindSlider(13, 2, 0, 255);             // LED brightness
  Legowo.bindSensor(0, 0, DIGITAL_INVERTED);    // button (INPUT_PULLUP)
  Legowo.bindSensor(1, 34, ANALOG_PERCENT);     // potentiometer 0-100%
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
}

void loop() {
  Legowo.run();
}
