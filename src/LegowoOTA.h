// Internal: Legowo OTA client. No need to include this from a sketch.
#pragma once
#include <Arduino.h>

struct LegowoSettings;

namespace legowo_ota {
// Called once from Legowo.begin(): check whether this firmware is an OTA build not yet validated.
void setup(const LegowoSettings* s, const char* deviceId, const char* secret);
// Called after MQTT connects: mark the firmware healthy / report the previous OTA result.
void onConnected();
// Message from {prefix}/devices/{id}/ota
void request(const char* json, size_t len);
// Called from Legowo.run(): incremental download, rollback watchdog.
void loop();
bool busy();
}  // namespace legowo_ota
