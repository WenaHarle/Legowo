// Legowo default configuration. Everything can be overridden with a #define BEFORE #include <Legowo.h>.
#pragma once

// Your sketch's firmware version. Reported to the server for OTA (MAJOR.MINOR.PATCH format).
#ifndef LEGOWO_FW_VERSION
#define LEGOWO_FW_VERSION "0.0.0"
#endif

// MQTT broker. The ESP32 connects via MQTT over WebSocket + TLS (wss://HOST:443/mqtt),
// so it can go through a Cloudflare Tunnel without opening a port on the router.
#ifndef LEGOWO_MQTT_HOST
#define LEGOWO_MQTT_HOST "lmqtt.madewena.com"
#endif
#ifndef LEGOWO_MQTT_WS_PORT
#define LEGOWO_MQTT_WS_PORT 443
#endif
#ifndef LEGOWO_MQTT_WS_PATH
#define LEGOWO_MQTT_WS_PATH "/mqtt"
#endif

// The ESP8266 does not yet support MQTT over WebSocket in this library and uses MQTT TLS (8883).
// The server must provide an 8883 listener (see the ESP8266 section of the README).
#ifndef LEGOWO_MQTT_TLS_HOST
#define LEGOWO_MQTT_TLS_HOST "lmqtts.madewena.com"
#endif
#ifndef LEGOWO_MQTT_TLS_PORT
#define LEGOWO_MQTT_TLS_PORT 8883
#endif

#ifndef LEGOWO_TOPIC_PREFIX
#define LEGOWO_TOPIC_PREFIX "legowo"
#endif

// OTA firmware download host. The device rejects OTA URLs that do not point to this host.
#ifndef LEGOWO_OTA_HOST
#define LEGOWO_OTA_HOST "ldev.madewena.com"
#endif

// ECDSA P-256 public key (PEM) for firmware signature verification.
// If set, firmware without a valid signature is rejected. Create one with the scripts/ota-sign.sh header.
#ifndef LEGOWO_OTA_PUBKEY
#define LEGOWO_OTA_PUBKEY ""
#endif

// Root CA for TLS verification. Default: the Cloudflare/Let's Encrypt root CA bundle.
#ifndef LEGOWO_CA_CERT
#include "LegowoCerts.h"
#define LEGOWO_CA_CERT LEGOWO_CA_BUNDLE
#endif

// Board name reported to the server (used to match OTA firmware).
#ifndef LEGOWO_BOARD
#if defined(ESP8266)
#define LEGOWO_BOARD "esp8266"
#elif defined(CONFIG_IDF_TARGET)
#define LEGOWO_BOARD CONFIG_IDF_TARGET
#else
#define LEGOWO_BOARD "esp32"
#endif
#endif

#define LEGOWO_MAX_CHANNELS 32
#define LEGOWO_MAX_BINDINGS 16
#define LEGOWO_MAX_TIMERS 8
#define LEGOWO_STATUS_INTERVAL_MS 30000UL
