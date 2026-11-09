<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/legowo-logo.svg">
    <img src="docs/legowo-logo-light.svg" alt="Legowo" width="260">
  </picture>
</p>

# Legowo for Arduino

**Language:** **English** · [Indonesia](README.id.md)

Arduino library that connects an **ESP32** to the **Legowo** IoT platform: send sensor data to the
dashboard, control pins from the dashboard, and receive over-the-air (OTA) firmware updates from the
server.

```cpp
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>

void setup() {
  Legowo.begin("WIFI_NAME", "WIFI_PASSWORD", "DEVICE_KEY");   // key from the dashboard
}

void loop() {
  Legowo.run();                                               // no delay()
}

LEGOWO_RECEIVE(10) {                                          // receive a value from the dashboard -> channel 10
  digitalWrite(2, param.asInt());
}
```

Wi-Fi, MQTT over WebSocket + TLS (certificate verified), automatic reconnect, and OTA are all handled
by the library. The default server `lmqtt.madewena.com` is built in; there is nothing to configure.

## What the app looks like

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/dashboard-dark.png">
  <img src="docs/screenshots/dashboard.png" alt="Legowo dashboard with value, gauge, switch, status, slider, chart, and table widgets">
</picture>

A dashboard with every widget type: **value** (temperature, light), **gauge** (humidity), **switch**
(light, channel 10), device **status**, **slider** (PWM, channel 11), history **chart**, and a
**table** of the latest values. Values update in real time; widgets are arranged freely in *Edit* mode.

<table>
  <tr>
    <td width="62%"><img src="docs/screenshots/new-device.png" alt="Add device: device key and ready-to-copy Arduino code"></td>
    <td rowspan="2"><img src="docs/screenshots/mobile.png" alt="Dashboard on a phone"></td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/device-data.png" alt="Device detail: chart and stored data"></td>
  </tr>
  <tr>
    <td>Top: <b>Add device</b> immediately gives you ready-to-copy Arduino code. Bottom: the device
    <b>Data</b> tab (chart and stored values).</td>
    <td>Phone view.</td>
  </tr>
</table>

<sub>Images use demo data; the device key shown is no longer valid.</sub>

## Get started in 5 minutes

1. **Install the ESP32 board** (once). Arduino IDE → *File → Preferences → Additional boards manager URLs*, add:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   Then *Tools → Board → Boards Manager* → search **esp32** (Espressif Systems) → *Install*.
2. **Install the Legowo library** (once):
   1. Download `Legowo-x.y.z.zip` from [Releases](https://github.com/WenaHarle/Legowo/releases/latest)
      (do not extract it).
   2. Arduino IDE → *Sketch → Include Library → Add .ZIP Library* → pick that ZIP file.
   3. Install its dependency: *Tools → Manage Libraries* → search **ArduinoJson** (Benoit Blanchon) → *Install*.

   > Legowo is in the process of being listed in the Arduino Library Manager. Once listed, just
   > *Tools → Manage Libraries* → search **Legowo** → *Install* (ArduinoJson is installed automatically).
3. **Create a device** in the Legowo dashboard → *Devices → Add device*. Copy the code it shows (the
   device key is displayed only once).
4. **Paste** the code into the Arduino IDE and fill in your Wi-Fi name and password (2.4 GHz).
5. Select your board (e.g. *ESP32 Dev Module*) and port, then **Upload**. Open the *Serial Monitor* (115200):
   ```
   [Legowo] Wi-Fi connected, IP 192.168.1.23, signal -54 dBm
   [Legowo] Connected to the Legowo server. Device ID 3f2a...
   ```

Full examples are under *File → Examples → Legowo*: `Basic`, `SwitchSlider`, `DHT_Sensor`, `RGB_LED`, `OTA_Update`, `SendReceive`.

## Channels

Channel numbers are **free** (0 to 31). A channel's role is **not** decided by its number, but by how
you use it:

| Usage | Direction | Role | Example |
|---|---|---|---|
| `Legowo.write(ch, value)` / `bindSensor` | device → dashboard | sensor | `Legowo.write(3, temperature);` |
| `LEGOWO_RECEIVE(ch){...}` / `onReceive` / `bindSwitch` / `bindSlider` | dashboard → device | control | `LEGOWO_RECEIVE(3) { ... }` |

The library automatically tells the server which channels act as controls (via the status message), so
the server knows how to store each channel's history without depending on its number. A value received
from the dashboard is automatically echoed back as a confirmation, so switch/slider widgets reflect the
real state on the device.

### Live and stored

Every value you send appears **live** on the dashboard immediately. For chart history, the server stores
at most one point per channel per 10 seconds (average, min, and max), or per the interval chosen in the
device's **Settings** tab (10 seconds to 5 minutes). Control channels are stored whenever their value
changes. So `setInterval()` controls how fast the dashboard moves, not how much data is stored. The
server limits each device to 20 messages/second.

## API

| Function | Description |
|---|---|
| `Legowo.begin(ssid, pass, deviceKey)` | Start Wi-Fi + server connection (non-blocking). |
| `Legowo.run()` | Call continuously in `loop()`. |
| `Legowo.write(ch, value)` | Send a value to the dashboard (batched per interval). |
| `Legowo.read(ch)` | Last value of a channel. |
| `LEGOWO_RECEIVE(ch) { ... }` | Handler for when the dashboard sends a value; `param.asInt()`, `asFloat()`, `asBool()`. (`LEGOWO_WRITE` is the old name, still supported.) |
| `Legowo.onReceive(ch, fn)` | Same as `LEGOWO_RECEIVE`, registered at runtime. |
| `Legowo.bindSwitch(ch, pin, activeLow=false)` | A dashboard switch drives the pin directly. |
| `Legowo.bindSlider(ch, pin, min, max)` | A dashboard slider → PWM pin. |
| `Legowo.bindSensor(ch, pin, mode)` | Read a pin automatically: `DIGITAL_NORMAL`, `DIGITAL_INVERTED`, `ANALOG_RAW`, `ANALOG_PERCENT`. |
| `Legowo.every(ms, fn)` | Run a function periodically without `delay()` (`Legowo.timer` for `setTimeout`/`cancel`). |
| `Legowo.setInterval(ms)` | Telemetry send + sensor read interval (default 2000 ms, minimum 200 ms). See *Live and stored*. |
| `Legowo.setDebug(true)` | More detailed logs in the Serial Monitor. |
| `Legowo.connected()` / `wifiConnected()` | Connection status. |
| `Legowo.disableOTA()` / `enableOTA()` | OTA is **enabled by default**. |

## Firmware update (OTA)

OTA is included and points at the Legowo server (commands via `lmqtt.madewena.com`, downloads via
`https://ldev.madewena.com`). The first upload is still over USB; after that the sketch can be updated
without a cable.

**Installing your own sketch**

1. Bump `LEGOWO_FW_VERSION` in the sketch (e.g. `"1.0.0"` → `"1.0.1"`).
2. Arduino IDE: *Sketch → Export Compiled Binary*. Open the sketch's `build/...` folder and take
   **`YourSketch.ino.bin`**. Do not use `.bootloader.bin`, `.partitions.bin`, or `.merged.bin`; the
   server rejects those because the device cannot boot if it installs them.
3. In the web app, open the device → **Firmware** tab → **My firmware** → **Upload firmware**. Fill in
   the same version as `LEGOWO_FW_VERSION`, pick the file, and check *Install on this device now*.

This firmware is visible only to you and can only be installed on your own devices. Make sure the new
sketch keeps using Legowo with OTA enabled; otherwise the next update has to go over USB again.

**Official firmware from an admin**

An admin can upload official firmware for one board (e.g. `esp32`), then release it or run a staged
rollout. Newer official firmware appears as an *Update* button on the **Firmware** tab.

**What happens on the device**

1. The device receives the OTA command, downloads over HTTPS with the device key, then checks the
   **size, SHA-256, and signature** (if `LEGOWO_OTA_PUBKEY` is set).
2. The firmware is written to the second OTA slot, then the device restarts.
3. The new firmware is considered healthy once it reconnects to the server. If it does not connect
   within 5 minutes or keeps restarting, the device **automatically rolls back to the previous
   firmware** and reports `rolled_back`.

Safety rules: only a **newer** version is installed (anti-rollback), and only URLs from
`LEGOWO_OTA_HOST` are accepted.

**Signing (optional).** To make the device install only firmware you have signed, create an ECDSA
P-256 key pair once on your computer (needs `openssl`):

```bash
openssl ecparam -name prime256v1 -genkey -noout -out ota_private.pem   # SECRET, never share
openssl ec -in ota_private.pem -pubout -out ota_public.pem
```

Copy the contents of `ota_public.pem` into the sketch as `LEGOWO_OTA_PUBKEY` (each line ending in `\n`,
see the `OTA_Update` example). For each `.bin`, create a signature and paste it in the *Signature* field
when uploading:

```bash
openssl dgst -sha256 -sign ota_private.pem YourSketch.ino.bin | base64 | tr -d '\n'
```

Once firmware with `LEGOWO_OTA_PUBKEY` is installed, firmware without a matching signature is rejected
by the device, including official firmware from an admin.

**The partition scheme** must have two OTA slots. The Arduino IDE default for ESP32 (*Default 4MB with
spiffs*) is enough. If you use a scheme without OTA (e.g. *Huge APP*), the Serial Monitor shows a warning.

## Advanced settings

Write the `#define`s **before** `#include <Legowo.h>`:

| Define | Default |
|---|---|
| `LEGOWO_FW_VERSION` | `"0.0.0"` |
| `LEGOWO_MQTT_HOST` | `"lmqtt.madewena.com"` (WSS port 443, path `/mqtt`) |
| `LEGOWO_OTA_HOST` | `"ldev.madewena.com"` |
| `LEGOWO_OTA_PUBKEY` | `""` (empty = no signature verification) |
| `LEGOWO_TOPIC_PREFIX` | `"legowo"` |
| `LEGOWO_CA_CERT` | Let's Encrypt / Google Trust Services / Sectigo / SSL.com root CA bundle |
| `LEGOWO_BOARD` | automatic from the chip (`esp32`, `esp32s3`, `esp32c3`, ...) |

## Serial Monitor messages

| Message | What to do |
|---|---|
| `Invalid DEVICE_KEY format` | Re-copy the device key from the dashboard (format `<id>:<64 characters>`). |
| `Wi-Fi "X" not found` | Check the Wi-Fi name; the ESP32 only supports 2.4 GHz. |
| `Cannot join Wi-Fi` | Check the Wi-Fi password. |
| `Cannot connect to ...` / `getaddrinfo() returns 202` | The device got an IP but the network DNS failed to resolve the host name. Since v1.1.1 the library automatically uses a public DNS fallback; if it still fails, the network is blocking outbound DNS — try another Wi-Fi/hotspot. |
| `Device key rejected by server` | The key is wrong, was rotated, or was revoked. Create a new key in the device's *Settings* tab. |
| `Invalid server certificate` | Make sure the clock can sync (internet to NTP) and that `LEGOWO_CA_CERT` is unchanged. |
| `partition scheme without OTA slot` | Pick a partition scheme with OTA in the *Tools* menu. |

## ESP8266

Experimentally supported for telemetry and control, via MQTT TLS port 8883 to `LEGOWO_MQTT_TLS_HOST`.
The Legowo server at `madewena.com` currently only exposes MQTT over WebSocket (Cloudflare Tunnel), so
the ESP8266 can only be used once an 8883 listener is enabled on the server. OTA is not yet supported on
the ESP8266.

## License

[MIT](LICENSE) © 2026 Made Wena Harilegawa
