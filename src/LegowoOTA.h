// Internal: klien OTA Legowo. Tidak perlu di-include dari sketch.
#pragma once
#include <Arduino.h>

struct LegowoSettings;

namespace legowo_ota {
// Dipanggil sekali dari Legowo.begin(): cek apakah firmware ini hasil OTA yang belum divalidasi.
void setup(const LegowoSettings* s, const char* deviceId, const char* secret);
// Dipanggil setelah MQTT terhubung: tandai firmware sehat / laporkan hasil OTA sebelumnya.
void onConnected();
// Pesan dari {prefix}/devices/{id}/ota
void request(const char* json, size_t len);
// Dipanggil dari Legowo.run(): unduh bertahap, watchdog rollback.
void loop();
bool busy();
}  // namespace legowo_ota
