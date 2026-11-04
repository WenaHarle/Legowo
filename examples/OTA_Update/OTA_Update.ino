// Legowo - OTA_Update
// OTA sudah aktif bawaan. Langkah update dari dashboard:
//  1. Naikkan LEGOWO_FW_VERSION (mis. "1.0.1"), Sketch > Export Compiled Binary.
//  2. Admin upload .bin di lapp-admin > Firmware (board sesuai, mis. esp32), lalu Rilis/Rollout.
//  3. Pengguna menekan "Update" di halaman perangkat. Perangkat mengunduh, memverifikasi
//     SHA-256 (dan tanda tangan jika LEGOWO_OTA_PUBKEY diisi), restart, lalu melapor.
//     Jika firmware baru tidak bisa terhubung ke server dalam 5 menit, otomatis kembali ke versi lama.
// Partition scheme harus punya 2 slot OTA (default Arduino IDE sudah cukup).
#define LEGOWO_FW_VERSION "1.0.0"
// #define LEGOWO_OTA_PUBKEY "-----BEGIN PUBLIC KEY-----\n...\n-----END PUBLIC KEY-----\n"
#include <Legowo.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

void setup() {
  Serial.begin(115200);
  Legowo.setDebug(true);                // tampilkan progres OTA di Serial Monitor
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  // Legowo.disableOTA();               // matikan bila perangkat tidak boleh di-update dari server
}

void loop() {
  Legowo.run();
}
