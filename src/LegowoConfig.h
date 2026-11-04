// Konfigurasi default Legowo. Semua bisa ditimpa dengan #define SEBELUM #include <Legowo.h>.
#pragma once

// Versi firmware sketch Anda. Dilaporkan ke server untuk OTA (format MAJOR.MINOR.PATCH).
#ifndef LEGOWO_FW_VERSION
#define LEGOWO_FW_VERSION "0.0.0"
#endif

// Broker MQTT. ESP32 terhubung lewat MQTT over WebSocket + TLS (wss://HOST:443/mqtt),
// sehingga bisa lewat Cloudflare Tunnel tanpa membuka port di router.
#ifndef LEGOWO_MQTT_HOST
#define LEGOWO_MQTT_HOST "lmqtt.madewena.com"
#endif
#ifndef LEGOWO_MQTT_WS_PORT
#define LEGOWO_MQTT_WS_PORT 443
#endif
#ifndef LEGOWO_MQTT_WS_PATH
#define LEGOWO_MQTT_WS_PATH "/mqtt"
#endif

// ESP8266 belum mendukung MQTT over WebSocket di library ini dan memakai MQTT TLS (8883).
// Server harus menyediakan listener 8883 (lihat README bagian ESP8266).
#ifndef LEGOWO_MQTT_TLS_HOST
#define LEGOWO_MQTT_TLS_HOST "lmqtts.madewena.com"
#endif
#ifndef LEGOWO_MQTT_TLS_PORT
#define LEGOWO_MQTT_TLS_PORT 8883
#endif

#ifndef LEGOWO_TOPIC_PREFIX
#define LEGOWO_TOPIC_PREFIX "legowo"
#endif

// Host unduhan firmware OTA. Device menolak URL OTA yang tidak mengarah ke host ini.
#ifndef LEGOWO_OTA_HOST
#define LEGOWO_OTA_HOST "ldev.madewena.com"
#endif

// Kunci publik ECDSA P-256 (PEM) untuk verifikasi tanda tangan firmware.
// Jika diisi, firmware tanpa tanda tangan valid ditolak. Buat dengan scripts/ota-sign.sh header.
#ifndef LEGOWO_OTA_PUBKEY
#define LEGOWO_OTA_PUBKEY ""
#endif

// Root CA untuk verifikasi TLS. Default: bundle root CA Cloudflare/Let's Encrypt.
#ifndef LEGOWO_CA_CERT
#include "LegowoCerts.h"
#define LEGOWO_CA_CERT LEGOWO_CA_BUNDLE
#endif

// Nama board yang dilaporkan ke server (dipakai untuk mencocokkan firmware OTA).
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
