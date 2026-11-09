// Legowo - built-in OTA client (ESP32). ESP8266: not yet supported, OTA commands are reported as failed.
#include "LegowoOTA.h"

#include <ArduinoJson.h>

#include "Legowo.h"

#if defined(ESP32)
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include "esp_ota_ops.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#endif

namespace legowo_ota {

#if defined(ESP32)
namespace {

enum State { IDLE, PENDING, DOWNLOADING };

struct Request {
  char id[40];
  char version[20];
  char url[220];
  char sha256[68];
  uint32_t size;
  char sig[220];
};

const LegowoSettings* s_settings = nullptr;
char s_auth[120];  // "Device <id>:<secret>"
State s_state = IDLE;
Request s_req;

WiFiClientSecure* s_tls = nullptr;
HTTPClient* s_http = nullptr;
esp_ota_handle_t s_handle = 0;
const esp_partition_t* s_target = nullptr;
mbedtls_md_context_t s_md;
bool s_mdActive = false;
uint32_t s_written = 0;
int s_lastPct = -1;
unsigned long s_lastData = 0;
uint8_t* s_buf = nullptr;
const size_t BUF_SIZE = 4096;

// Validation after reboot
bool s_verifyPending = false;
bool s_reportRolledBack = false;
unsigned long s_bootAt = 0;
char s_pendId[40] = {0};
char s_pendVer[20] = {0};

const unsigned long VERIFY_TIMEOUT_MS = 5UL * 60UL * 1000UL;
const uint32_t MAX_BOOTS = 3;

bool parseSemver(const char* v, int out[3]) {
  return v && sscanf(v, "%d.%d.%d", &out[0], &out[1], &out[2]) == 3;
}

// >0 if a is newer than b
int compareSemver(const char* a, const char* b) {
  int x[3], y[3];
  if (!parseSemver(a, x) || !parseSemver(b, y)) return 1;  // unknown old version: allow
  for (int i = 0; i < 3; i++) {
    if (x[i] != y[i]) return x[i] > y[i] ? 1 : -1;
  }
  return 0;
}

void report(const char* status, int progress, const char* message) {
  Legowo._otaReport(s_req.id, s_req.version, status, progress, message);
}

void cleanup() {
  if (s_http) {
    s_http->end();
    delete s_http;
    s_http = nullptr;
  }
  if (s_tls) {
    delete s_tls;
    s_tls = nullptr;
  }
  if (s_mdActive) {
    mbedtls_md_free(&s_md);
    s_mdActive = false;
  }
  if (s_buf) {
    free(s_buf);
    s_buf = nullptr;
  }
}

void fail(const char* message) {
  if (s_handle) {
    esp_ota_abort(s_handle);
    s_handle = 0;
  }
  cleanup();
  Legowo._errorf("OTA: %s", message);
  report("failed", -1, message);
  s_state = IDLE;
}

void clearPending() {
  Preferences p;
  p.begin("legowo-ota", false);
  p.clear();
  p.end();
}

void rollback(const char* reason) {
  Preferences p;
  p.begin("legowo-ota", true);
  String prev = p.getString("prev", "");
  p.end();
  const esp_partition_t* part = prev.length()
    ? esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, prev.c_str())
    : nullptr;
  if (!part || esp_ota_set_boot_partition(part) != ESP_OK) {
    Legowo._errorf("OTA: rollback failed (%s), the old slot was not found.", reason);
    clearPending();
    s_verifyPending = false;
    return;
  }
  Legowo._errorf("OTA: new firmware %s. Reverting to the previous firmware...", reason);
  delay(300);
  ESP.restart();
}

bool hexEquals(const uint8_t* hash, const char* hex) {
  static const char* digits = "0123456789abcdef";
  if (strlen(hex) != 64) return false;
  for (int i = 0; i < 32; i++) {
    char hi = tolower((unsigned char)hex[i * 2]);
    char lo = tolower((unsigned char)hex[i * 2 + 1]);
    if (hi != digits[hash[i] >> 4] || lo != digits[hash[i] & 0xF]) return false;
  }
  return true;
}

bool verifySignature(const uint8_t* hash, const char* sigB64, const char* pubKeyPem) {
  unsigned char sig[160];
  size_t sigLen = 0;
  if (mbedtls_base64_decode(sig, sizeof(sig), &sigLen, (const unsigned char*)sigB64, strlen(sigB64)) != 0) return false;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)pubKeyPem, strlen(pubKeyPem) + 1) == 0
    && mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

void startDownload() {
  char prefix[96];
  snprintf(prefix, sizeof(prefix), "https://%s/", s_settings->otaHost);
  if (strncmp(s_req.url, prefix, strlen(prefix)) != 0) return fail("Firmware URL is not from the Legowo server, rejected.");

  s_target = esp_ota_get_next_update_partition(nullptr);
  if (!s_target) return fail("Partition scheme has no OTA slot. Pick a scheme with OTA in Tools > Partition Scheme.");
  if (s_req.size == 0 || s_req.size > s_target->size) return fail("Firmware size exceeds the OTA slot.");

  s_buf = (uint8_t*)malloc(BUF_SIZE);
  s_tls = new WiFiClientSecure();
  s_http = new HTTPClient();
  if (!s_buf || !s_tls || !s_http) return fail("Not enough memory for OTA.");
  s_tls->setCACert(s_settings->caCert);
  s_http->setTimeout(15000);
  s_http->setReuse(false);
  if (!s_http->begin(*s_tls, s_req.url)) return fail("Cannot open the firmware URL.");
  s_http->addHeader("Authorization", s_auth);

  Legowo._logf("OTA: downloading firmware %s (%lu bytes)...", s_req.version, (unsigned long)s_req.size);
  int code = s_http->GET();
  if (code != 200) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Server refused the download (HTTP %d).", code);
    return fail(msg);
  }
  int len = s_http->getSize();
  if (len >= 0 && (uint32_t)len != s_req.size) return fail("File size from the server does not match.");

  if (esp_ota_begin(s_target, s_req.size, &s_handle) != ESP_OK) {
    s_handle = 0;
    return fail("Cannot prepare the OTA slot.");
  }
  mbedtls_md_init(&s_md);
  s_mdActive = true;
  mbedtls_md_setup(&s_md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&s_md);
  s_written = 0;
  s_lastPct = -1;
  s_lastData = millis();
  s_state = DOWNLOADING;
  report("downloading", 0, nullptr);
}

void finish() {
  uint8_t hash[32];
  mbedtls_md_finish(&s_md, hash);
  if (!hexEquals(hash, s_req.sha256)) return fail("Firmware SHA-256 does not match, firmware rejected.");

  const char* pub = s_settings->otaPubKey;
  if (pub && *pub) {
    if (!s_req.sig[0]) return fail("Firmware is unsigned, rejected.");
    if (!verifySignature(hash, s_req.sig, pub)) return fail("Invalid firmware signature, rejected.");
  }
  esp_err_t err = esp_ota_end(s_handle);
  s_handle = 0;
  if (err != ESP_OK) return fail("Firmware image is corrupt or not for this chip.");
  if (esp_ota_set_boot_partition(s_target) != ESP_OK) return fail("Cannot activate the new firmware slot.");

  const esp_partition_t* running = esp_ota_get_running_partition();
  Preferences p;
  p.begin("legowo-ota", false);
  p.putString("ver", s_req.version);
  p.putString("id", s_req.id);
  p.putString("prev", running ? running->label : "");
  p.putUInt("boots", 0);
  p.end();

  cleanup();
  Legowo._logf("OTA: firmware %s installed, restarting...", s_req.version);
  report("downloading", 100, "Installing firmware, device restarting");
  delay(1200);  // give the report time to be sent
  ESP.restart();
}

void download() {
  // Core 2.x: WiFiClient*, core 3.x: NetworkClient*.
  auto* stream = s_http->getStreamPtr();
  unsigned long start = millis();
  // Process incrementally (max ~30 ms per run() call) so the loop does not stall.
  while (s_written < s_req.size && millis() - start < 30) {
    size_t avail = stream->available();
    if (!avail) break;
    size_t want = min(min(avail, BUF_SIZE), (size_t)(s_req.size - s_written));
    int n = stream->readBytes(s_buf, want);
    if (n <= 0) break;
    if (esp_ota_write(s_handle, s_buf, n) != ESP_OK) return fail("Failed to write to flash.");
    mbedtls_md_update(&s_md, s_buf, n);
    s_written += n;
    s_lastData = millis();
  }
  int pct = (int)((uint64_t)s_written * 100 / s_req.size);
  if (pct / 10 != s_lastPct / 10) {
    s_lastPct = pct;
    report("downloading", pct, nullptr);
  }
  if (s_written >= s_req.size) return finish();
  if (!stream->connected() && !stream->available()) return fail("Connection dropped during download.");
  if (millis() - s_lastData > 20000) return fail("Download stalled (timeout).");
}

}  // namespace

void setup(const LegowoSettings* s, const char* deviceId, const char* secret) {
  s_settings = s;
  snprintf(s_auth, sizeof(s_auth), "Device %s:%s", deviceId, secret);

  if (!esp_ota_get_next_update_partition(nullptr)) {
    Legowo._logf("WARNING: partition scheme without an OTA slot; updates from the server cannot be installed. "
                 "Pick Tools > Partition Scheme > \"Default 4MB with spiffs\" or \"Minimal SPIFFS (OTA)\".");
  }

  Preferences p;
  p.begin("legowo-ota", false);
  String ver = p.getString("ver", "");
  if (ver.length()) {
    strlcpy(s_pendVer, ver.c_str(), sizeof(s_pendVer));
    strlcpy(s_pendId, p.getString("id", "").c_str(), sizeof(s_pendId));
    if (ver == s->fwVersion) {
      uint32_t boots = p.getUInt("boots", 0) + 1;
      p.putUInt("boots", boots);
      p.end();
      if (boots > MAX_BOOTS) return rollback("repeated restarts");
      s_verifyPending = true;
      s_bootAt = millis();
      Legowo._logf("OTA: firmware %s just installed, waiting to connect to the server for validation.", s_pendVer);
      return;
    }
    // Running version is not the new version: the old firmware is active again (rollback).
    s_reportRolledBack = true;
  }
  p.end();
}

void onConnected() {
  if (s_verifyPending) {
    s_verifyPending = false;
    esp_ota_mark_app_valid_cancel_rollback();
    clearPending();
    Legowo._logf("OTA: firmware %s is healthy and kept.", s_pendVer);
    Legowo._otaReport(s_pendId, s_pendVer, "success", 100, nullptr);
  }
  if (s_reportRolledBack) {
    s_reportRolledBack = false;
    clearPending();
    Legowo._otaReport(s_pendId, s_pendVer, "rolled_back", -1, "New firmware failed to run, reverted to the previous version");
  }
}

void request(const char* json, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return;
  if (s_state != IDLE) {
    Legowo._otaReport(doc["id"] | "", doc["version"] | "", "failed", -1, "Another update is already in progress");
    return;
  }
  memset(&s_req, 0, sizeof(s_req));
  strlcpy(s_req.id, doc["id"] | "", sizeof(s_req.id));
  strlcpy(s_req.version, doc["version"] | "", sizeof(s_req.version));
  strlcpy(s_req.url, doc["url"] | "", sizeof(s_req.url));
  strlcpy(s_req.sha256, doc["sha256"] | "", sizeof(s_req.sha256));
  strlcpy(s_req.sig, doc["sig"] | "", sizeof(s_req.sig));
  s_req.size = doc["size"] | 0;

  if (!s_req.url[0] || strlen(s_req.sha256) != 64 || !s_req.size) {
    report("failed", -1, "Incomplete OTA command");
    return;
  }
  // Anti-rollback: only a newer version.
  if (compareSemver(s_req.version, s_settings->fwVersion) <= 0) {
    report("failed", -1, "Firmware version is not newer than the installed one");
    return;
  }
  Legowo._logf("OTA: update %s -> %s accepted.", s_settings->fwVersion, s_req.version);
  s_state = PENDING;
}

void loop() {
  if (s_verifyPending && millis() - s_bootAt > VERIFY_TIMEOUT_MS) rollback("did not connect to the server within 5 minutes");
  if (s_state == PENDING) startDownload();
  else if (s_state == DOWNLOADING) download();
}

bool busy() { return s_state != IDLE; }

#else  // ESP8266 ---------------------------------------------------------------

void setup(const LegowoSettings*, const char*, const char*) {}
void onConnected() {}
void request(const char* json, size_t len) {
  JsonDocument doc;
  deserializeJson(doc, json, len);
  Legowo._otaReport(doc["id"] | "", doc["version"] | "", "failed", -1, "OTA is not yet supported on ESP8266");
}
void loop() {}
bool busy() { return false; }

#endif

}  // namespace legowo_ota
