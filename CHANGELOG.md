# Changelog

## 1.1.1

- **Fallback DNS publik** (8.8.8.8 dan 1.1.1.1) dipasang otomatis setelah Wi-Fi tersambung. DNS dari
  jaringan tetap jadi utama; fallback dipakai bila yang utama gagal. Memperbaiki kasus perangkat dapat
  IP tapi gagal me-resolve nama host (`getaddrinfo() returns 202`) di hotspot/jaringan dengan DNS
  bermasalah, sehingga NTP dan broker MQTT tetap bisa dijangkau.

## 1.1.0

- **Peran channel tidak lagi ditentukan oleh nomornya.** Nomor channel bebas (0–31); gunakan
  `Legowo.write(ch, ...)` untuk sensor dan `LEGOWO_WRITE(ch)`/`onReceive`/`bindSwitch`/`bindSlider`
  untuk kontrol. Library mendeklarasikan daftar channel kontrol ke server lewat pesan status, jadi
  server menyimpan histori tiap channel sesuai perannya tanpa bergantung pada nomor (sebelumnya
  0–9 = sensor, 10+ = kontrol).
- Dokumentasi dan contoh diperbarui mengikuti perubahan ini. Tidak ada perubahan API; sketch lama
  tetap jalan.

## 1.0.0

Rilis pertama library Legowo.

- `Legowo.begin(ssid, password, deviceKey)` dan `Legowo.run()` non-blocking dengan reconnect Wi-Fi/MQTT
  (backoff eksponensial) dan pesan Serial yang jelas.
- ESP32: MQTT over WebSocket + TLS ke `lmqtt.madewena.com:443` dengan verifikasi sertifikat.
- `LEGOWO_WRITE(channel)`, `onReceive`, `bindSwitch`, `bindSlider`, `bindSensor`, dan timer `every()`.
- OTA bawaan (ESP32): unduhan HTTPS terautentikasi, verifikasi ukuran/SHA-256/tanda tangan,
  anti-rollback, dan rollback otomatis jika firmware baru tidak sehat.
- ESP8266 eksperimental (MQTT TLS 8883, tanpa OTA).
