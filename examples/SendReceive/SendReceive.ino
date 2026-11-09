// Legowo - SendReceive
// A complete example without any physical sensor/actuator:
//   - SENDS dummy data to the dashboard (channels 0, 1, 2)
//   - RECEIVES values from the dashboard and prints them on the ESP32 Serial Monitor (channels 10, 11)
//
// Set up on the Legowo dashboard:
//   - A "Value"/"Chart" widget on channel 0 (counter), 1 (wave), 2 (random)
//   - A "Slider" widget on channel 10 and a "Switch" on channel 11
// Device key: dashboard -> Devices -> Add device -> copy the key.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>
#include <math.h>

const char* WIFI_SSID  = "YOUR_WIFI_NAME";
const char* WIFI_PASS  = "YOUR_WIFI_PASSWORD";
const char* DEVICE_KEY = "PASTE_DEVICE_KEY_FROM_DASHBOARD";

// ----------------------------------------------------------------- Send dummy data
// Called periodically by Legowo.every() below, without delay().
void sendDummy() {
  static uint32_t counter = 0;

  // Channel 0: a counter that keeps rising.
  Legowo.write(0, (float)counter++);

  // Channel 1: a smooth sine wave 0..100 (like a fake temperature sensor).
  float wave = 50.0f + 50.0f * sinf(millis() / 2000.0f);
  Legowo.write(1, wave);

  // Channel 2: a random value 0..100.
  int rnd = random(0, 101);
  Legowo.write(2, (float)rnd);

  Serial.printf("[send] ch0=%lu ch1=%.1f ch2=%d -> dashboard\n", (unsigned long)(counter - 1), wave, rnd);
}

void setup() {
  Serial.begin(115200);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(2000, sendDummy);    // send dummy data every 2 seconds
}

void loop() {
  Legowo.run();                     // required; do not use long delay() calls
}

// ----------------------------------------------------------------- Receive values
// Values from the dashboard are printed on the Serial Monitor (no hardware needed).

LEGOWO_RECEIVE(10) {               // dashboard slider (e.g. 0..255) -> channel 10
  Serial.printf("[recv] slider channel 10 = %d\n", param.asInt());
}

LEGOWO_RECEIVE(11) {               // dashboard switch (ON/OFF) -> channel 11
  Serial.printf("[recv] switch channel 11 = %s\n", param.asBool() ? "ON" : "OFF");
}
