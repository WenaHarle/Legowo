# Legowo Arduino Library

A Blynk-like Arduino library for the Legowo IoT platform.

## Installation

### Arduino IDE
1. Download this library folder
2. Copy to `Documents/Arduino/libraries/`
3. Restart Arduino IDE

### PlatformIO
Add to `platformio.ini`:
```ini
lib_deps =
    https://github.com/your-repo/legowo-firmware
```

## Quick Start

```cpp
#include "Legowo.h"

#define WIFI_SSID     "your-wifi"
#define WIFI_PASSWORD "your-password"
#define MQTT_SERVER   "your-server-ip"
#define DEVICE_KEY    "device-key-from-dashboard"

Legowo legowo(DEVICE_KEY);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Register handler: called when dashboard SENDS data to channel 10
  legowo.onReceive(10, [](LegowoParam param) {
    digitalWrite(LED_PIN, param.asBool());
  });
  
  legowo.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER);
}

void loop() {
  legowo.run();
  
  // Send sensor data TO dashboard
  legowo.write(0, temperature);  // Upload to cloud
  legowo.write(1, humidity);
  
  // Read cached value (last received from dashboard)
  float ledState = legowo.read(10);
}
```

## API Reference

### Constructor
```cpp
Legowo legowo(deviceKey);
```

### Setup
```cpp
legowo.begin(ssid, password, mqttServer, mqttPort);  // Port defaults to 1883
legowo.setDebug(true);      // Enable serial debug output
legowo.setInterval(5000);   // Set telemetry interval (ms)
```

### Main Loop
```cpp
legowo.run();  // Call this in loop() - handles WiFi, MQTT, callbacks
```

### Send Data TO Dashboard (Upload)
```cpp
legowo.write(channel, value);  // float, int, or bool

// Examples:
legowo.write(0, 25.5);         // Send temperature to channel 0
legowo.write(1, true);         // Send boolean to channel 1
legowo.write(2, 1024);         // Send integer to channel 2
```

### Read Cached Value
```cpp
float value = legowo.read(channel);  // Get last value from local cache
```

### Receive Data FROM Dashboard (Callback)
```cpp
// Using lambda (recommended)
legowo.onReceive(10, [](LegowoParam param) {
  int value = param.asInt();
  bool state = param.asBool();
  float fval = param.asFloat();
  digitalWrite(LED, state);
});

// Or using LEGOWO_WRITE macro (legacy, for Blynk users)
LEGOWO_WRITE(10) {
  digitalWrite(LED, param.asBool());
}
```

### Connection Status
```cpp
if (legowo.connected()) { ... }      // MQTT connected
if (legowo.wifiConnected()) { ... }  // WiFi connected
```

### Sync All Channels
```cpp
legowo.syncAll();  // Force send all channel values
```

## API Summary

| Method | Direction | Description |
|--------|-----------|-------------|
| `legowo.write(ch, val)` | Device → Dashboard | Send sensor data to cloud |
| `legowo.read(ch)` | Local Cache | Read last value from memory |
| `legowo.onReceive(ch, fn)` | Dashboard → Device | Handler when receiving command |

## Channel Mapping Guide

| Channel | Usage | Widget |
|---------|-------|--------|
| 0-5 | Sensor data | Gauge, Chart, Single Value |
| 6-9 | Control feedback | Switch, Slider (two-way sync) |
| 10-19 | Relays/outputs | Switch |
| 20-29 | Reserved | Custom |

## Dashboard Widget Setup

### For Reading Sensors:
1. Create **Gauge**, **Chart**, or **Single Value** widget
2. Select the channel your device writes to
3. Device sends: `legowo.write(0, temperature)`

### For Control (Switch):
1. Create **Switch** widget
2. Select channel (e.g., 10)
3. Dashboard sends command when user toggles switch
4. Handle with:
```cpp
legowo.onReceive(10, [](LegowoParam param) {
  digitalWrite(RELAY, param.asBool());
});
```
5. For feedback: `legowo.write(10, relayState)` in loop

### For Control (Slider):
1. Create **Slider** widget  
2. Select channel (e.g., 11)
3. Set Min/Max (e.g., 0-255 for PWM)
4. Handle with:
```cpp
legowo.onReceive(11, [](LegowoParam param) {
  analogWrite(PWM_PIN, param.asInt());
});
```

## Two-Way Sync

For control widgets to stay in sync with device state:

```cpp
bool ledState = false;

void setup() {
  // Handle commands from dashboard
  legowo.onReceive(10, [](LegowoParam param) {
    ledState = param.asBool();
    digitalWrite(LED_PIN, ledState);
  });
  
  legowo.begin(WIFI_SSID, WIFI_PASS, MQTT_SERVER);
}

void loop() {
  legowo.run();
  
  // Send state back so widget stays synced
  legowo.write(10, ledState);
}
```

## Examples

- `examples/minimal/` - Simplest possible example
- `examples/relay_control/` - 4-channel relay controller
- `examples/sensor_station/` - Multi-sensor weather station
- `widget_test/` - Full widget test with LED control

## Troubleshooting

### WiFi won't connect
- Check SSID and password
- Ensure 2.4GHz network (ESP doesn't support 5GHz)

### MQTT won't connect
- Verify server IP is reachable
- Check device ID and token match dashboard
- Ensure EMQX is running on port 1883

### Commands not received
- Verify channel numbers match between dashboard and code
- Check `LEGOWO_WRITE()` macro is defined before `setup()`
- Enable debug: `legowo.setDebug(true)`

### Values not updating
- Call `legowo.run()` in every `loop()` iteration
- Check `legowo.write()` is being called with correct channel

## License

MIT License - Free to use in personal and commercial projects.
