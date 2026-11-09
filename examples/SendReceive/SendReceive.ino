// Legowo - SendReceive
// Contoh lengkap tanpa sensor/aktuator fisik:
//   - MENGIRIM data dummy ke dashboard (channel 0, 1, 2)
//   - MENERIMA nilai dari dashboard dan menampilkannya di Serial Monitor ESP32 (channel 10, 11)
//
// Siapkan di dashboard Legowo:
//   - Widget "Nilai"/"Grafik" di channel 0 (penghitung), 1 (gelombang), 2 (acak)
//   - Widget "Slider" di channel 10 dan "Switch" di channel 11
// Device key: dashboard -> Perangkat -> Tambah perangkat -> salin key.
#define LEGOWO_FW_VERSION "1.0.0"
#include <Legowo.h>
#include <math.h>

const char* WIFI_SSID  = "NAMA_WIFI";
const char* WIFI_PASS  = "PASSWORD_WIFI";
const char* DEVICE_KEY = "TEMPEL_DEVICE_KEY_DARI_DASHBOARD";

// ----------------------------------------------------------------- Kirim dummy
// Dipanggil berkala oleh Legowo.every() di bawah, tanpa delay().
void kirimDummy() {
  static uint32_t penghitung = 0;

  // Channel 0: penghitung yang terus naik.
  Legowo.write(0, (float)penghitung++);

  // Channel 1: gelombang sinus halus 0..100 (seperti sensor suhu palsu).
  float gelombang = 50.0f + 50.0f * sinf(millis() / 2000.0f);
  Legowo.write(1, gelombang);

  // Channel 2: nilai acak 0..100.
  int acak = random(0, 101);
  Legowo.write(2, (float)acak);

  Serial.printf("[kirim] ch0=%lu ch1=%.1f ch2=%d -> dashboard\n", (unsigned long)(penghitung - 1), gelombang, acak);
}

void setup() {
  Serial.begin(115200);
  Legowo.begin(WIFI_SSID, WIFI_PASS, DEVICE_KEY);
  Legowo.every(2000, kirimDummy);   // kirim data dummy tiap 2 detik
}

void loop() {
  Legowo.run();                     // wajib; jangan pakai delay() panjang
}

// ----------------------------------------------------------------- Terima nilai
// Nilai dari dashboard ditampilkan di Serial Monitor (tidak butuh perangkat keras).

LEGOWO_RECEIVE(10) {               // Slider dashboard (mis. 0..255) -> channel 10
  Serial.printf("[terima] slider channel 10 = %d\n", param.asInt());
}

LEGOWO_RECEIVE(11) {               // Switch dashboard (ON/OFF) -> channel 11
  Serial.printf("[terima] switch channel 11 = %s\n", param.asBool() ? "ON" : "OFF");
}
