# Changelog

## 1.2.0

- **BREAKING: `LEGOWO_WRITE(ch)` removed.** Use `LEGOWO_RECEIVE(ch)` instead (same behavior, clearer
  name). Sketches still using the old macro will not compile — rename `LEGOWO_WRITE` to
  `LEGOWO_RECEIVE`. The alias existed only in 1.1.3; this drops the duplicate.
- **Documentation and comments translated to English** (the public-facing default), with the
  Indonesian README kept at [`README.id.md`](README.id.md). Source code comments, example sketch
  comments, Serial Monitor messages, and the Arduino Library Manager description are now English.

## 1.1.3

- New macro **`LEGOWO_RECEIVE(ch)`** to receive a value from the dashboard — a clearer name for the
  direction (`Legowo.write` = send to the dashboard, `LEGOWO_RECEIVE` = receive from the dashboard).
  All examples and the README now use this name. **`LEGOWO_WRITE(ch)` is still supported** as an alias
  (compatible; no breaking changes to existing code).

## 1.1.2

- The status message now includes the device's **IP address** (in addition to the existing RSSI), so
  the dashboard can show the IP and Wi-Fi signal strength in the device status section.

## 1.1.1

- **Public DNS fallback** (8.8.8.8 and 1.1.1.1) is set up automatically after Wi-Fi connects. The
  network's DNS stays primary; the fallback is used if the primary fails. Fixes the case where a device
  gets an IP but fails to resolve host names (`getaddrinfo() returns 202`) on hotspots/networks with
  broken DNS, so NTP and the MQTT broker stay reachable.
- New **SendReceive** example: sends dummy data (counter, wave, random) to the dashboard and prints the
  values received from the dashboard on the Serial Monitor, with no hardware needed.

## 1.1.0

- **A channel's role is no longer decided by its number.** Channel numbers are free (0–31); use
  `Legowo.write(ch, ...)` for sensors and `LEGOWO_WRITE(ch)`/`onReceive`/`bindSwitch`/`bindSlider` for
  controls. The library declares the list of control channels to the server via the status message, so
  the server stores each channel's history by its role without depending on the number (previously
  0–9 = sensor, 10+ = control).
- Documentation and examples updated to match. No API changes; existing sketches keep working.

## 1.0.0

First release of the Legowo library.

- `Legowo.begin(ssid, password, deviceKey)` and non-blocking `Legowo.run()` with Wi-Fi/MQTT reconnect
  (exponential backoff) and clear Serial messages.
- ESP32: MQTT over WebSocket + TLS to `lmqtt.madewena.com:443` with certificate verification.
- `LEGOWO_WRITE(channel)`, `onReceive`, `bindSwitch`, `bindSlider`, `bindSensor`, and the `every()` timer.
- Built-in OTA (ESP32): authenticated HTTPS download, size/SHA-256/signature verification,
  anti-rollback, and automatic rollback if the new firmware is unhealthy.
- Experimental ESP8266 (MQTT TLS 8883, no OTA).
