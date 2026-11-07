// Legowo - implementasi inti (Wi-Fi, MQTT, channel, binding, timer).
#include "Legowo.h"

#include <ArduinoJson.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include "LegowoOTA.h"

#if defined(ESP32)
#include <WiFi.h>
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecureBearSSL.h>
#else
#error "Library Legowo hanya mendukung ESP32 dan ESP8266."
#endif

#define LEGOWO_LIB_VERSION "1.1.0"

LegowoClass Legowo;

// Diisi oleh LEGOWO_WRITE() saat inisialisasi statis (array POD -> sudah nol sebelum dipakai).
static LegowoHandler s_macroHandlers[LEGOWO_MAX_CHANNELS];

static char s_topicCmd[96];
static char s_topicOta[96];
static char s_topicStatus[96];

// ============================================================================ Timer
int LegowoTimer::_add(unsigned long ms, Callback cb, bool repeat) {
  if (!cb || ms == 0) return -1;
  for (int i = 0; i < LEGOWO_MAX_TIMERS; i++) {
    if (!_slots[i].active) {
      _slots[i] = {cb, ms, millis(), repeat, true};
      return i;
    }
  }
  return -1;
}

void LegowoTimer::run() {
  unsigned long now = millis();
  for (int i = 0; i < LEGOWO_MAX_TIMERS; i++) {
    Slot& s = _slots[i];
    if (!s.active || now - s.last < s.interval) continue;
    s.last = now;
    if (!s.repeat) s.active = false;
    s.cb();
  }
}

// ============================================================================ Log
void LegowoClass::_logf(const char* fmt, ...) {
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print("[Legowo] ");
  Serial.println(buf);
}

void LegowoClass::_errorf(const char* fmt, ...) {
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print("[Legowo] GAGAL: ");
  Serial.println(buf);
}

void LegowoClass::registerMacroHandler(int channel, LegowoHandler handler) {
  if (channel >= 0 && channel < LEGOWO_MAX_CHANNELS) s_macroHandlers[channel] = handler;
}

static bool timeValid() { return time(nullptr) > 1700000000; }

static uint64_t epochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

// ============================================================================ Transport MQTT
#if defined(ESP32)
namespace {
struct QMsg {
  uint8_t kind;
  uint16_t len;
  char data[640];
};
QueueHandle_t s_queue = nullptr;
esp_mqtt_client_handle_t s_client = nullptr;
volatile bool s_evConnected = false;
volatile bool s_evDisconnected = false;
volatile int s_lastError = 0;  // 1 ditolak (key), 2 sertifikat, 3 jaringan/TLS
char s_uri[160];

void mqttEvent(void*, esp_event_base_t, int32_t id, void* data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
      s_lastError = 0;
      s_evConnected = true;
      break;
    case MQTT_EVENT_DISCONNECTED:
      s_evDisconnected = true;
      break;
    case MQTT_EVENT_DATA: {
      // Pesan Legowo kecil; fragmen (pesan besar) diabaikan.
      if (ev->current_data_offset != 0 || ev->data_len != ev->total_data_len) break;
      if (ev->data_len <= 0 || ev->data_len >= (int)sizeof(QMsg::data)) break;
      QMsg m;
      m.kind = 0;
      if (ev->topic_len == (int)strlen(s_topicCmd) && strncmp(ev->topic, s_topicCmd, ev->topic_len) == 0) {
        m.kind = LegowoClass::MSG_COMMAND;
      } else if (ev->topic_len == (int)strlen(s_topicOta) && strncmp(ev->topic, s_topicOta, ev->topic_len) == 0) {
        m.kind = LegowoClass::MSG_OTA;
      }
      if (!m.kind) break;
      m.len = ev->data_len;
      memcpy(m.data, ev->data, ev->data_len);
      m.data[m.len] = 0;
      // Handler user dijalankan di loop(), bukan di task MQTT.
      xQueueSend(s_queue, &m, 0);
      break;
    }
    case MQTT_EVENT_ERROR:
      if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        s_lastError = 1;
      } else if (ev->error_handle->esp_tls_cert_verify_flags != 0) {
        s_lastError = 2;
      } else {
        s_lastError = 3;
      }
      break;
    default:
      break;
  }
}
}  // namespace
#elif defined(ESP8266)
namespace {
BearSSL::WiFiClientSecure* s_tls = nullptr;
BearSSL::X509List* s_trust = nullptr;
PubSubClient* s_mqtt = nullptr;

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  uint8_t kind = 0;
  if (strcmp(topic, s_topicCmd) == 0) kind = LegowoClass::MSG_COMMAND;
  else if (strcmp(topic, s_topicOta) == 0) kind = LegowoClass::MSG_OTA;
  if (kind) Legowo._onMessage(kind, (const char*)payload, len);
}
}  // namespace
#endif

// ============================================================================ begin
bool LegowoClass::_begin(const char* ssid, const char* password, const char* deviceKey, const LegowoSettings& s) {
  _s = &s;
  _ssid = ssid;
  _password = password;

  Serial.println();
  _logf("Legowo %s | firmware %s | board %s", LEGOWO_LIB_VERSION, s.fwVersion, s.board);

  // Device key: "<device_id>:<secret>" (36 + 1 + 64 karakter)
  const char* colon = deviceKey ? strchr(deviceKey, ':') : nullptr;
  bool ok = colon && (colon - deviceKey) == 36 && strlen(colon + 1) == 64;
  for (int i = 0; ok && i < 64; i++) ok = isxdigit((unsigned char)colon[1 + i]);
  if (!ok) {
    _errorf("Format DEVICE_KEY salah. Salin ulang device key dari dashboard Legowo (format <id>:<64 karakter>).");
    return false;
  }
  memcpy(_deviceId, deviceKey, 36);
  _deviceId[36] = 0;
  memcpy(_secret, colon + 1, 64);
  _secret[64] = 0;

  snprintf(s_topicCmd, sizeof(s_topicCmd), "%s/devices/%s/command", s.topicPrefix, _deviceId);
  snprintf(s_topicOta, sizeof(s_topicOta), "%s/devices/%s/ota", s.topicPrefix, _deviceId);
  snprintf(s_topicStatus, sizeof(s_topicStatus), "%s/devices/%s/status", s.topicPrefix, _deviceId);

  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    if (!_handlers[i] && s_macroHandlers[i]) _handlers[i] = s_macroHandlers[i];
  }

#if defined(ESP32)
  if (!s_queue) s_queue = xQueueCreate(6, sizeof(QMsg));
#endif

  legowo_ota::setup(_s, _deviceId, _secret);

  if (!ssid || !*ssid) {
    _errorf("Nama Wi-Fi (SSID) kosong.");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  _wifiAttemptAt = millis();
  _logf("Menghubungkan ke Wi-Fi \"%s\"...", ssid);
  _started = true;
  return true;
}

// ============================================================================ run
void LegowoClass::run() {
  if (!_started) return;
  unsigned long now = millis();
  _handleWiFi(now);
  _handleMqtt(now);

  if (now - _lastPoll >= _interval) {
    _lastPoll = now;
    _pollSensors();
  }
  if (_mqttUp && (now - _lastFlush >= _interval || (_flushSoon && now - _lastFlush >= 200))) {
    _lastFlush = now;
    _flushSoon = false;
    _flushTelemetry();
  }
  if (_mqttUp && now - _lastStatus >= LEGOWO_STATUS_INTERVAL_MS) {
    _sendStatus();
  }
  legowo_ota::loop();
  timer.run();
}

void LegowoClass::_handleWiFi(unsigned long now) {
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    if (!_wifiUp) {
      _wifiUp = true;
      _wifiBackoff = 10000;
      _logf("Wi-Fi terhubung, IP %s, sinyal %d dBm", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      if (!_timeRequested) {
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        _timeRequested = true;
        _timeRequestedAt = now;
      }
    }
    return;
  }
  if (_wifiUp) {
    _wifiUp = false;
    _logf("Wi-Fi terputus, mencoba menyambung lagi...");
    _wifiAttemptAt = now;
  }
  if (now - _wifiAttemptAt < _wifiBackoff) return;
  if (st == WL_NO_SSID_AVAIL) {
    _errorf("Wi-Fi \"%s\" tidak ditemukan. Periksa nama Wi-Fi dan pastikan jaringan 2.4 GHz.", _ssid);
  } else if (st == WL_CONNECT_FAILED) {
    _errorf("Tidak bisa masuk ke Wi-Fi \"%s\". Periksa password Wi-Fi.", _ssid);
  } else {
    _logf("Belum terhubung ke Wi-Fi \"%s\", mencoba lagi...", _ssid);
  }
  WiFi.disconnect();
  WiFi.begin(_ssid, _password);
  _wifiAttemptAt = now;
  _wifiBackoff = min(_wifiBackoff * 2, 60000UL);
}

void LegowoClass::_handleMqtt(unsigned long now) {
#if defined(ESP32)
  if (s_evConnected) {
    s_evConnected = false;
    esp_mqtt_client_subscribe(s_client, s_topicCmd, 1);
    esp_mqtt_client_subscribe(s_client, s_topicOta, 1);
    _mqttUp = true;
    _mqttBackoff = 2000;
    _onConnected();
  }
  if (s_evDisconnected) {
    s_evDisconnected = false;
    _onDisconnected();
    _mqttRetryAt = now + _mqttBackoff;
  }
  QMsg m;
  while (s_queue && xQueueReceive(s_queue, &m, 0) == pdTRUE) _onMessage(m.kind, m.data, m.len);

  if (!_wifiUp) return;
  // TLS butuh jam yang benar; tunggu NTP maksimal 15 detik.
  if (!timeValid() && now - _timeRequestedAt < 15000) return;

  if (!_mqttStarted) {
    snprintf(s_uri, sizeof(s_uri), "wss://%s:%u%s", _s->mqttHost, (unsigned)_s->mqttWsPort, _s->mqttWsPath);
    esp_mqtt_client_config_t cfg = {};
#if ESP_IDF_VERSION_MAJOR >= 5
    cfg.broker.address.uri = s_uri;
    cfg.broker.verification.certificate = _s->caCert;
    cfg.credentials.username = _deviceId;
    cfg.credentials.client_id = _deviceId;
    cfg.credentials.authentication.password = _secret;
    cfg.session.last_will.topic = s_topicStatus;
    cfg.session.last_will.msg = "{\"online\":false}";
    cfg.session.last_will.qos = 1;
    cfg.session.keepalive = 30;
    cfg.network.disable_auto_reconnect = true;
    cfg.network.timeout_ms = 15000;
    cfg.buffer.size = 1024;
    cfg.task.stack_size = 6144;
#else
    cfg.uri = s_uri;
    cfg.cert_pem = _s->caCert;
    cfg.username = _deviceId;
    cfg.client_id = _deviceId;
    cfg.password = _secret;
    cfg.lwt_topic = s_topicStatus;
    cfg.lwt_msg = "{\"online\":false}";
    cfg.lwt_qos = 1;
    cfg.keepalive = 30;
    cfg.disable_auto_reconnect = true;
    cfg.network_timeout_ms = 15000;
    cfg.buffer_size = 1024;
    cfg.task_stack = 6144;
#endif
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
      _errorf("Tidak bisa membuat klien MQTT (memori tidak cukup?).");
      _mqttRetryAt = now + 10000;
      return;
    }
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqttEvent, nullptr);
    _logf("Menghubungkan ke server %s ...", _s->mqttHost);
    esp_mqtt_client_start(s_client);
    _mqttStarted = true;
    _mqttRetryAt = 0;
    return;
  }
  if (!_mqttUp && _mqttRetryAt && (long)(now - _mqttRetryAt) >= 0) {
    _mqttRetryAt = 0;
    _logf("Menghubungkan ulang ke server...");
    esp_mqtt_client_reconnect(s_client);
  }
#elif defined(ESP8266)
  if (s_mqtt) s_mqtt->loop();
  if (_mqttUp && s_mqtt && !s_mqtt->connected()) {
    _onDisconnected();
    _mqttRetryAt = now + _mqttBackoff;
  }
  if (!_wifiUp || _mqttUp) return;
  if (!timeValid() && now - _timeRequestedAt < 15000) return;
  if (_mqttRetryAt && (long)(now - _mqttRetryAt) < 0) return;

  if (!s_mqtt) {
    s_tls = new BearSSL::WiFiClientSecure();
    s_trust = new BearSSL::X509List(_s->caCert);
    s_tls->setTrustAnchors(s_trust);
    s_mqtt = new PubSubClient(*s_tls);
    s_mqtt->setServer(_s->mqttTlsHost, _s->mqttTlsPort);
    s_mqtt->setBufferSize(1024);
    s_mqtt->setKeepAlive(30);
    s_mqtt->setCallback(mqttCallback);
  }
  if (timeValid()) s_tls->setX509Time(time(nullptr));
  _logf("Menghubungkan ke server %s:%u ...", _s->mqttTlsHost, (unsigned)_s->mqttTlsPort);
  if (s_mqtt->connect(_deviceId, _deviceId, _secret, s_topicStatus, 1, false, "{\"online\":false}")) {
    s_mqtt->subscribe(s_topicCmd, 1);
    s_mqtt->subscribe(s_topicOta, 1);
    _mqttUp = true;
    _mqttBackoff = 2000;
    _onConnected();
  } else {
    int st = s_mqtt->state();
    if (st == MQTT_CONNECT_BAD_CREDENTIALS || st == MQTT_CONNECT_UNAUTHORIZED) {
      _errorf("Device key ditolak server (salah atau sudah dicabut). Salin key baru dari dashboard.");
      _mqttBackoff = 60000;
    } else if (s_tls->getLastSSLError() != 0) {
      _errorf("Sertifikat server tidak valid atau jam perangkat salah (kode TLS %d).", s_tls->getLastSSLError());
    } else {
      _errorf("Tidak bisa terhubung ke server (kode %d). Mencoba lagi %lu detik lagi.", st, _mqttBackoff / 1000);
    }
    _mqttRetryAt = now + _mqttBackoff;
    _mqttBackoff = min(_mqttBackoff * 2, 60000UL);
  }
#endif
}

void LegowoClass::_onConnected() {
  _logf("Terhubung ke server Legowo. Device ID %s", _deviceId);
  _greeted = true;
  _sendStatus();
  // Kirim ulang semua nilai terakhir agar dashboard langsung sinkron.
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) _dirty[i] = _dirty[i] || _written[i];
  _flushSoon = true;
  legowo_ota::onConnected();
}

void LegowoClass::_onDisconnected() {
  if (_mqttUp) _logf("Koneksi ke server terputus.");
  _mqttUp = false;
#if defined(ESP32)
  int err = s_lastError;
  s_lastError = 0;
  if (err == 1) {
    _errorf("Device key ditolak server (salah atau sudah dicabut). Salin key baru dari dashboard.");
    _mqttBackoff = 60000;
    return;
  }
  if (err == 2) {
    _errorf("Sertifikat server tidak valid. Periksa jam perangkat atau LEGOWO_CA_CERT.");
  } else if (err == 3) {
    _errorf("Tidak bisa terhubung ke %s. Periksa koneksi internet.", _s->mqttHost);
  }
  _logf("Mencoba lagi dalam %lu detik.", _mqttBackoff / 1000);
  _mqttBackoff = min(_mqttBackoff * 2, 60000UL);
#endif
}

bool LegowoClass::connected() const { return _mqttUp; }
bool LegowoClass::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }

// ============================================================================ Publish
void LegowoClass::_publish(const char* suffix, const char* payload, int qos) {
  if (!_mqttUp) return;
  char topic[112];
  snprintf(topic, sizeof(topic), "%s/devices/%s/%s", _s->topicPrefix, _deviceId, suffix);
#if defined(ESP32)
  esp_mqtt_client_publish(s_client, topic, payload, 0, qos, 0);
#elif defined(ESP8266)
  (void)qos;  // PubSubClient hanya QoS 0 untuk publish
  s_mqtt->publish(topic, payload);
#endif
}

void LegowoClass::_sendStatus() {
  _lastStatus = millis();
  JsonDocument doc;
  doc["online"] = true;
  doc["fw"] = _s->fwVersion;
  doc["board"] = _s->board;
#if defined(ESP32)
  doc["chip"] = ESP.getChipModel();
#else
  doc["chip"] = "ESP8266";
#endif
  doc["rssi"] = WiFi.RSSI();
  doc["heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;
  // Deklarasikan channel yang berperan kontrol: punya handler (LEGOWO_WRITE/onReceive) atau
  // binding switch/slider. Server memakai ini untuk menyimpan histori channel kontrol saat
  // nilainya berubah, dan sensor sebagai rata-rata per jendela. Nomor channel tidak menentukan peran.
  JsonArray control = doc["control"].to<JsonArray>();
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    if (_handlers[i] || _switches[i].active || _isSliderChannel(i)) control.add(i);
  }
  char buf[512];
  serializeJson(doc, buf, sizeof(buf));
  _publish("status", buf, 1);
}

bool LegowoClass::_isSliderChannel(int ch) const {
  for (int i = 0; i < _numSliders; i++) if (_sliders[i].channel == ch) return true;
  return false;
}

void LegowoClass::_flushTelemetry() {
  JsonDocument doc;
  int n = 0;
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    if (!_dirty[i]) continue;
    doc[String(i)] = _values[i];
    _dirty[i] = false;
    n++;
  }
  if (!n) return;
  if (timeValid()) doc["ts"] = epochMs();
  char buf[768];
  serializeJson(doc, buf, sizeof(buf));
  _publish("telemetry", buf, 0);
  if (_debug) _logf("Kirim telemetri: %s", buf);
}

void LegowoClass::_otaReport(const char* id, const char* version, const char* status, int progress, const char* message) {
  JsonDocument doc;
  if (id && *id) doc["id"] = id;
  if (version && *version) doc["version"] = version;
  doc["status"] = status;
  if (progress >= 0) doc["progress"] = progress;
  if (message && *message) doc["message"] = message;
  char buf[320];
  serializeJson(doc, buf, sizeof(buf));
  _publish("ota/status", buf, 1);
}

// ============================================================================ Channel
void LegowoClass::write(int channel, float value) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  _values[channel] = value;
  _dirty[channel] = true;
  _written[channel] = true;
}

float LegowoClass::read(int channel) const {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return 0;
  return _values[channel];
}

void LegowoClass::onReceive(int channel, LegowoHandler handler) {
  if (channel >= 0 && channel < LEGOWO_MAX_CHANNELS) _handlers[channel] = handler;
}

void LegowoClass::setInterval(unsigned long ms) { _interval = ms < 200 ? 200 : ms; }

void LegowoClass::syncAll() {
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) _dirty[i] = _written[i];
  _flushSoon = true;
}

void LegowoClass::_onMessage(uint8_t kind, const char* data, size_t len) {
  if (kind == MSG_OTA) {
    if (!_otaEnabled) {
      _logf("Perintah OTA diabaikan karena OTA dimatikan (disableOTA).");
      _otaReport(nullptr, nullptr, "failed", -1, "OTA dimatikan di perangkat");
      return;
    }
    legowo_ota::request(data, len);
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    if (_debug) _logf("Perintah tidak dikenali (bukan JSON).");
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (obj["ch"].is<int>()) {
    _applyCommand(obj["ch"].as<int>(), obj["v"].as<float>());
  } else if (obj["channel"].is<int>()) {
    _applyCommand(obj["channel"].as<int>(), obj["value"].as<float>());
  } else {
    for (JsonPair kv : obj) {
      const char* k = kv.key().c_str();
      if (!isdigit((unsigned char)k[0])) continue;
      _applyCommand(atoi(k), kv.value().as<float>());
    }
  }
}

void LegowoClass::_applyCommand(int ch, float v) {
  if (ch < 0 || ch >= LEGOWO_MAX_CHANNELS) return;
  _values[ch] = v;
  _written[ch] = true;
  if (_switches[ch].active) {
    bool on = v > 0.5f || v < -0.5f;
    digitalWrite(_switches[ch].pin, (on != _switches[ch].activeLow) ? HIGH : LOW);
  }
  for (int i = 0; i < _numSliders; i++) {
    SliderBinding& b = _sliders[i];
    if (b.channel != ch) continue;
    int x = constrain((int)lroundf(v), b.minVal, b.maxVal);
    analogWrite(b.pin, b.maxVal == b.minVal ? 0 : map(x, b.minVal, b.maxVal, 0, 255));
  }
  if (_debug) _logf("Dashboard -> channel %d = %s", ch, String(v).c_str());
  if (_handlers[ch]) _handlers[ch](LegowoParam(v));
  // Kirim balik nilai sebagai konfirmasi ke dashboard.
  _dirty[ch] = true;
  _flushSoon = true;
}

// ============================================================================ Binding
void LegowoClass::bindSwitch(int channel, int pin, bool activeLow) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, activeLow ? HIGH : LOW);
  _switches[channel] = {(int8_t)pin, activeLow, true};
}

void LegowoClass::bindSlider(int channel, int pin, int minVal, int maxVal) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS || _numSliders >= LEGOWO_MAX_BINDINGS) return;
  pinMode(pin, OUTPUT);
#if defined(ESP8266)
  analogWriteRange(255);
#endif
  analogWrite(pin, 0);
  _sliders[_numSliders++] = {(int8_t)channel, (int8_t)pin, minVal, maxVal};
}

void LegowoClass::bindSensor(int channel, int pin, LegowoSensorMode mode) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS || _numSensors >= LEGOWO_MAX_BINDINGS) return;
  if (mode == DIGITAL_NORMAL) pinMode(pin, INPUT);
  else if (mode == DIGITAL_INVERTED) pinMode(pin, INPUT_PULLUP);
  _sensors[_numSensors++] = {(int8_t)channel, (int8_t)pin, mode};
}

void LegowoClass::_pollSensors() {
  for (int i = 0; i < _numSensors; i++) {
    const SensorBinding& b = _sensors[i];
    float v = 0;
    switch (b.mode) {
      case DIGITAL_NORMAL: v = digitalRead(b.pin); break;
      case DIGITAL_INVERTED: v = !digitalRead(b.pin); break;
      case ANALOG_RAW: v = analogRead(b.pin); break;
      case ANALOG_PERCENT: {
#if defined(ESP32)
        const float maxRaw = 4095.0f;
#else
        const float maxRaw = 1023.0f;
#endif
        v = roundf(analogRead(b.pin) * 100.0f / maxRaw);
        break;
      }
    }
    // Hanya kirim jika berubah, supaya tidak membanjiri server.
    if (!_written[b.channel] || _values[b.channel] != v) write(b.channel, v);
  }
}
