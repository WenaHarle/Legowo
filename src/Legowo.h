/*
 * Legowo - library IoT untuk ESP32 (dan ESP8266) yang terhubung ke server Legowo.
 *
 *   #define LEGOWO_FW_VERSION "1.0.0"
 *   #include <Legowo.h>
 *
 *   void setup() {
 *     Legowo.begin("SSID", "PASSWORD", "DEVICE_KEY");   // key disalin dari dashboard
 *   }
 *   void loop() {
 *     Legowo.run();                                     // non-blocking
 *   }
 *   LEGOWO_RECEIVE(10) {                                // terima nilai dari dashboard
 *     digitalWrite(2, param.asInt());
 *   }
 *
 * Nomor channel bebas (0 s/d LEGOWO_MAX_CHANNELS-1). Peran channel TIDAK ditentukan oleh nomornya,
 * melainkan oleh cara Anda memakainya: Legowo.write(ch, nilai) MENGIRIM data ke dashboard,
 * sedangkan LEGOWO_RECEIVE(ch){...} (atau onReceive/bindSwitch/bindSlider) MENERIMA nilai dari
 * dashboard. (LEGOWO_WRITE masih didukung sebagai nama lama untuk LEGOWO_RECEIVE.)
 * Library otomatis memberi tahu server channel mana yang berperan kontrol.
 * Koneksi Wi-Fi/MQTT, TLS, reconnect, dan OTA ditangani library.
 */
#pragma once

#include <Arduino.h>
#include "LegowoConfig.h"

// ---------------------------------------------------------------------------- Nilai channel
class LegowoParam {
 public:
  explicit LegowoParam(float v = 0) : _v(v) {}
  int asInt() const { return (int)lroundf(_v); }
  float asFloat() const { return _v; }
  double asDouble() const { return (double)_v; }
  bool asBool() const { return _v > 0.5f || _v < -0.5f; }
  String asString() const { return String(_v); }

 private:
  float _v;
};

typedef void (*LegowoHandler)(LegowoParam param);

enum LegowoSensorMode {
  DIGITAL_NORMAL,    // digitalRead (0/1)
  DIGITAL_INVERTED,  // !digitalRead, pin INPUT_PULLUP (tombol ke GND)
  ANALOG_RAW,        // analogRead mentah
  ANALOG_PERCENT     // analogRead dipetakan 0-100
};

// ---------------------------------------------------------------------------- Timer ringan
// Pengganti delay(): fungsi dipanggil berkala tanpa memblokir loop().
class LegowoTimer {
 public:
  typedef void (*Callback)();
  int setInterval(unsigned long ms, Callback cb) { return _add(ms, cb, true); }
  int setTimeout(unsigned long ms, Callback cb) { return _add(ms, cb, false); }
  void cancel(int id) {
    if (id >= 0 && id < LEGOWO_MAX_TIMERS) _slots[id].active = false;
  }
  void run();

 private:
  struct Slot {
    Callback cb;
    unsigned long interval;
    unsigned long last;
    bool repeat;
    bool active;
  };
  Slot _slots[LEGOWO_MAX_TIMERS] = {};
  int _add(unsigned long ms, Callback cb, bool repeat);
};

// ---------------------------------------------------------------------------- Pengaturan build
// Diisi dari #define di sketch (lewat begin() inline di bawah), karena #define sketch
// tidak terlihat oleh file .cpp library.
struct LegowoSettings {
  const char* fwVersion;
  const char* board;
  const char* mqttHost;
  uint16_t mqttWsPort;
  const char* mqttWsPath;
  const char* mqttTlsHost;
  uint16_t mqttTlsPort;
  const char* topicPrefix;
  const char* otaHost;
  const char* otaPubKey;
  const char* caCert;
};

class LegowoClass {
 public:
  // Mulai koneksi Wi-Fi dan MQTT (tidak memblokir). deviceKey: "<device_id>:<secret>" dari dashboard.
  bool begin(const char* ssid, const char* password, const char* deviceKey) {
    static const LegowoSettings s = {
      LEGOWO_FW_VERSION, LEGOWO_BOARD, LEGOWO_MQTT_HOST, LEGOWO_MQTT_WS_PORT, LEGOWO_MQTT_WS_PATH,
      LEGOWO_MQTT_TLS_HOST, LEGOWO_MQTT_TLS_PORT, LEGOWO_TOPIC_PREFIX, LEGOWO_OTA_HOST,
      LEGOWO_OTA_PUBKEY, LEGOWO_CA_CERT,
    };
    return _begin(ssid, password, deviceKey, s);
  }

  // Panggil sesering mungkin di loop(). Jangan pakai delay() panjang.
  void run();

  // Kirim nilai ke dashboard (dikirim berkelompok setiap interval).
  void write(int channel, float value);
  void write(int channel, int value) { write(channel, (float)value); }
  void write(int channel, bool value) { write(channel, value ? 1.0f : 0.0f); }
  void virtualWrite(int channel, float value) { write(channel, value); }

  // Nilai terakhir channel (dari dashboard atau dari write()).
  float read(int channel) const;

  // Handler saat dashboard mengirim nilai ke channel ini.
  void onReceive(int channel, LegowoHandler handler);

  // Binding tanpa kode: dashboard -> pin, atau pin -> dashboard.
  void bindSwitch(int channel, int pin, bool activeLow = false);
  void bindSlider(int channel, int pin, int minVal = 0, int maxVal = 255);
  void bindSensor(int channel, int pin, LegowoSensorMode mode = DIGITAL_NORMAL);

  // Jalankan fungsi berkala tanpa delay(). Mengembalikan id timer.
  int every(unsigned long ms, LegowoTimer::Callback cb) { return timer.setInterval(ms, cb); }

  void setInterval(unsigned long ms);  // interval kirim telemetri & baca sensor (default 2000 ms)
  void setDebug(bool enable) { _debug = enable; }
  void syncAll();                       // kirim ulang semua channel yang pernah ditulis

  // OTA aktif secara default. Matikan bila perangkat tidak boleh diperbarui dari server.
  void enableOTA() { _otaEnabled = true; }
  void disableOTA() { _otaEnabled = false; }
  bool otaEnabled() const { return _otaEnabled; }

  bool connected() const;
  bool wifiConnected() const;
  const char* deviceId() const { return _deviceId; }
  const char* firmwareVersion() const { return _s ? _s->fwVersion : ""; }

  LegowoTimer timer;

  // ------------------------------------------------------------------ internal
  static void registerMacroHandler(int channel, LegowoHandler handler);
  void _onMessage(uint8_t kind, const char* data, size_t len);
  void _onConnected();
  void _onDisconnected();
  void _publish(const char* suffix, const char* payload, int qos);
  void _otaReport(const char* id, const char* version, const char* status, int progress, const char* message);
  void _logf(const char* fmt, ...);
  void _errorf(const char* fmt, ...);

  enum { MSG_COMMAND = 1, MSG_OTA = 2 };

 private:
  bool _begin(const char* ssid, const char* password, const char* deviceKey, const LegowoSettings& s);
  void _handleWiFi(unsigned long now);
  void _handleMqtt(unsigned long now);
  void _flushTelemetry();
  void _sendStatus();
  bool _isSliderChannel(int channel) const;
  void _applyCommand(int channel, float value);
  void _pollSensors();

  const LegowoSettings* _s = nullptr;
  const char* _ssid = nullptr;
  const char* _password = nullptr;
  char _deviceId[37] = {0};
  char _secret[65] = {0};
  bool _started = false;
  bool _debug = false;
  bool _otaEnabled = true;

  float _values[LEGOWO_MAX_CHANNELS] = {};
  bool _dirty[LEGOWO_MAX_CHANNELS] = {};
  bool _written[LEGOWO_MAX_CHANNELS] = {};
  LegowoHandler _handlers[LEGOWO_MAX_CHANNELS] = {};

  struct SwitchBinding { int8_t pin; bool activeLow; bool active; };
  struct SliderBinding { int8_t channel; int8_t pin; int minVal; int maxVal; };
  struct SensorBinding { int8_t channel; int8_t pin; LegowoSensorMode mode; };
  SwitchBinding _switches[LEGOWO_MAX_CHANNELS];
  SliderBinding _sliders[LEGOWO_MAX_BINDINGS];
  SensorBinding _sensors[LEGOWO_MAX_BINDINGS];
  uint8_t _numSliders = 0;
  uint8_t _numSensors = 0;

  unsigned long _interval = 2000;
  unsigned long _lastFlush = 0;
  unsigned long _lastPoll = 0;
  unsigned long _lastStatus = 0;
  bool _flushSoon = false;

  // Wi-Fi
  bool _wifiUp = false;
  unsigned long _wifiAttemptAt = 0;
  unsigned long _wifiBackoff = 10000;
  bool _timeRequested = false;
  unsigned long _timeRequestedAt = 0;

  // MQTT
  bool _mqttStarted = false;
  unsigned long _mqttRetryAt = 0;
  unsigned long _mqttBackoff = 2000;
  volatile bool _mqttUp = false;
  bool _greeted = false;
};

extern LegowoClass Legowo;

// Daftarkan handler untuk MENERIMA nilai dari dashboard di satu channel:
//   LEGOWO_RECEIVE(10) { digitalWrite(LED, param.asInt()); }
// (Untuk MENGIRIM ke dashboard pakai Legowo.write(ch, nilai).)
struct LegowoRegistrar {
  LegowoRegistrar(int channel, LegowoHandler h) { LegowoClass::registerMacroHandler(channel, h); }
};
#define LEGOWO_RECEIVE(ch)                                         \
  static void _legowo_rx_##ch(LegowoParam param);                  \
  static LegowoRegistrar _legowo_reg_##ch((ch), _legowo_rx_##ch);  \
  static void _legowo_rx_##ch(LegowoParam param)

// Nama lama (gaya Blynk) — tetap didukung demi kompatibilitas, tapi LEGOWO_RECEIVE
// lebih jelas arahnya (write = kirim ke dashboard, receive = terima dari dashboard).
#define LEGOWO_WRITE(ch) LEGOWO_RECEIVE(ch)
