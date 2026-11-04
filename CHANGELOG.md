# Changelog

## 1.0.0

Rilis pertama library Legowo.

- `Legowo.begin(ssid, password, deviceKey)` dan `Legowo.run()` non-blocking dengan reconnect Wi-Fi/MQTT
  (backoff eksponensial) dan pesan Serial yang jelas.
- ESP32: MQTT over WebSocket + TLS ke `lmqtt.madewena.com:443` dengan verifikasi sertifikat.
- `LEGOWO_WRITE(channel)`, `onReceive`, `bindSwitch`, `bindSlider`, `bindSensor`, dan timer `every()`.
- OTA bawaan (ESP32): unduhan HTTPS terautentikasi, verifikasi ukuran/SHA-256/tanda tangan,
  anti-rollback, dan rollback otomatis jika firmware baru tidak sehat.
- ESP8266 eksperimental (MQTT TLS 8883, tanpa OTA).
