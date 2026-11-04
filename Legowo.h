/*
 * Legowo.h - IoT Platform Library for ESP32/ESP8266
 *
 * Simple Blynk-like interface for Legowo dashboard
 *
 * ============================================================================
 * QUICK START (cara paling mudah)
 * ============================================================================
 *
 *   #include "Legowo.h"
 *
 *   Legowo legowo("your_device_key");
 *
 *   void setup() {
 *     legowo.begin("WiFiName", "WiFiPass", "mqtt.server.com");
 *
 *     // Otomatis kontrol pin lewat dashboard:
 *     legowo.bindSwitch(10, RELAY_PIN);          // Switch ch 10 -> Relay
 *     legowo.bindSlider(13, LED_PIN, 0, 255);    // Slider ch 13 -> LED PWM
 *
 *     // Otomatis baca pin & kirim ke dashboard:
 *     legowo.bindSensor(0, BUTTON_PIN, DIGITAL_INVERTED);
 *     legowo.bindSensor(1, POT_PIN, ANALOG_PERCENT);
 *   }
 *
 *   void loop() {
 *     legowo.run();   // satu baris ini cukup, semua otomatis!
 *   }
 *
 * ============================================================================
 * ADVANCED API (kalau butuh logika kustom)
 * ============================================================================
 *
 *   legowo.write(0, sensorValue);              // Kirim data ke dashboard
 *   legowo.read(10);                           // Baca nilai cache lokal
 *   legowo.onReceive(10, [](LegowoParam p){     // Handler custom
 *     digitalWrite(LED_PIN, p.asInt());
 *   });
 *
 * API Summary:
 *   - legowo.bindSwitch(ch, pin)            : Auto-handle switch widget -> pin
 *   - legowo.bindSlider(ch, pin, min, max)  : Auto-handle slider widget -> PWM
 *   - legowo.bindSensor(ch, pin, mode)      : Auto-read pin -> kirim ke dashboard
 *   - legowo.write(ch, val)                 : SEND data TO dashboard
 *   - legowo.read(ch)                       : READ cached value (local memory)
 *   - legowo.onReceive(ch, fn)              : HANDLER custom saat dashboard kirim data
 */

#ifndef LEGOWO_H
#define LEGOWO_H

#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <PubSubClient.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define LEGOWO_MAX_CHANNELS 30
#define LEGOWO_MAX_BINDINGS 16
#define LEGOWO_MQTT_BUFFER_SIZE 512
#define LEGOWO_TELEMETRY_INTERVAL 5000    // 5 seconds default
#define LEGOWO_STATUS_INTERVAL 30000      // 30 seconds heartbeat
#define LEGOWO_RECONNECT_INTERVAL 5000

// =============================================================================
// SENSOR BINDING MODES
// =============================================================================

enum LegowoSensorMode {
  DIGITAL_NORMAL,      // digitalRead langsung (0/1)
  DIGITAL_INVERTED,    // digitalRead dibalik (untuk INPUT_PULLUP)
  ANALOG_RAW,          // analogRead nilai mentah (0-4095 ESP32 / 0-1023 ESP8266)
  ANALOG_PERCENT       // analogRead dipetakan ke 0-100%
};

// =============================================================================
// CHANNEL VALUE CLASS (like Blynk's param)
// =============================================================================

class LegowoParam {
public:
  LegowoParam(float val) : _value(val) {}

  int asInt() const { return (int)_value; }
  float asFloat() const { return _value; }
  double asDouble() const { return (double)_value; }
  bool asBool() const { return _value > 0; }
  String asString() const { return String(_value); }

  operator int() const { return asInt(); }
  operator float() const { return asFloat(); }
  operator bool() const { return asBool(); }

private:
  float _value;
};

// =============================================================================
// CHANNEL HANDLER TYPE
// =============================================================================

typedef void (*LegowoWriteHandler)(LegowoParam param);

// =============================================================================
// INTERNAL BINDING STRUCTS
// =============================================================================

struct LegowoSensorBinding {
  int channel;
  int pin;
  LegowoSensorMode mode;
  bool active;
};

struct LegowoSliderBinding {
  int channel;
  int pin;
  int minVal;
  int maxVal;
  bool active;
};

// =============================================================================
// MAIN LEGOWOCORE CLASS
// =============================================================================

class Legowo {
public:
  // Constructor - pass DEVICE_KEY from dashboard (64 char hex string)
  Legowo(const char* deviceKey);

  // Setup - connect to WiFi and MQTT
  void begin(const char* ssid, const char* password, const char* mqttServer, int mqttPort = 1883);

  // Main loop - call this in loop()
  void run();

  // ---------------------------------------------------------------------------
  // SIMPLE BINDING API (recommended for beginners)
  // ---------------------------------------------------------------------------

  // Bind switch widget (ON/OFF) to a digital output pin.
  // Pin is set OUTPUT automatically. Dashboard switch directly controls pin.
  void bindSwitch(int channel, int pin);

  // Bind slider widget to a PWM pin with auto value mapping.
  // Pin is set OUTPUT automatically. Slider value (minVal..maxVal) is mapped
  // to PWM 0..255.
  void bindSlider(int channel, int pin, int minVal = 0, int maxVal = 255);

  // Bind a sensor pin to a channel. Pin is read automatically every interval
  // and sent to dashboard.
  //   DIGITAL_NORMAL    : pinMode INPUT,        sends digitalRead()
  //   DIGITAL_INVERTED  : pinMode INPUT_PULLUP, sends !digitalRead()
  //   ANALOG_RAW        : sends analogRead() raw value
  //   ANALOG_PERCENT    : sends analogRead() mapped to 0-100
  void bindSensor(int channel, int pin, LegowoSensorMode mode = DIGITAL_NORMAL);

  // ---------------------------------------------------------------------------
  // ADVANCED API
  // ---------------------------------------------------------------------------

  // Send value to channel (upload to dashboard)
  // Use this to send sensor data from device to cloud
  void write(int channel, float value);
  void write(int channel, int value);
  void write(int channel, bool value);

  // Alias for write() - more intuitive naming
  void virtualWrite(int channel, float value) { write(channel, value); }
  void virtualWrite(int channel, int value) { write(channel, value); }
  void virtualWrite(int channel, bool value) { write(channel, value); }

  // Read cached channel value (last value received from dashboard or written locally)
  // Note: This reads from local memory, not from server
  float read(int channel);

  // Get channel value (alias for read)
  float getValue(int channel) { return read(channel); }

  // Register callback when dashboard SENDS data to this channel
  // This is called when device RECEIVES a command from dashboard
  void onReceive(int channel, LegowoWriteHandler handler);

  // Alias for backward compatibility
  void onWrite(int channel, LegowoWriteHandler handler) { onReceive(channel, handler); }

  // Connection status
  bool connected();
  bool wifiConnected();

  // Manual sync - send all channels
  void syncAll();

  // Set telemetry interval (ms) - also controls sensor binding poll rate
  void setInterval(unsigned long ms);

  // Debug mode
  void setDebug(bool enable);

private:
  const char* _deviceKey;
  const char* _ssid;
  const char* _password;
  const char* _mqttServer;
  int _mqttPort;

  WiFiClient _wifiClient;
  PubSubClient _mqttClient;

  float _channels[LEGOWO_MAX_CHANNELS];
  bool _channelDirty[LEGOWO_MAX_CHANNELS];
  LegowoWriteHandler _handlers[LEGOWO_MAX_CHANNELS];

  // Bindings
  LegowoSensorBinding _sensorBindings[LEGOWO_MAX_BINDINGS];
  LegowoSliderBinding _sliderBindings[LEGOWO_MAX_BINDINGS];
  int _switchPins[LEGOWO_MAX_CHANNELS];   // -1 = no switch bound
  int _numSensorBindings;
  int _numSliderBindings;
  unsigned long _lastSensorRead;

  char _topicCommand[128];
  char _topicStatus[128];

  unsigned long _lastTelemetry;
  unsigned long _lastStatus;
  unsigned long _lastReconnect;
  unsigned long _telemetryInterval;

  bool _debug;

  void _setupWiFi();
  void _setupMQTT();
  void _reconnectMQTT();
  void _sendTelemetry();
  void _sendStatus(const char* status);
  void _handleMessage(char* topic, byte* payload, unsigned int length);
  void _pollSensors();
  void _applySwitch(int channel, float value);
  void _applySlider(int channel, float value);
  int _readSensorPin(const LegowoSensorBinding& b);

  static Legowo* _instance;
  static void _mqttCallback(char* topic, byte* payload, unsigned int length);
};

// =============================================================================
// MACRO FOR EASY CHANNEL HANDLERS (Legacy - kept for backward compatibility)
// Prefer using legowo.onReceive(channel, handler) instead
// =============================================================================

#define LEGOWO_WRITE(channel) \
  void _legowoWriteHandler##channel(LegowoParam param); \
  struct _LegowoWriteRegistrar##channel { \
    _LegowoWriteRegistrar##channel() { \
      _legowoWriteHandlers[channel] = _legowoWriteHandler##channel; \
    } \
  }; \
  static _LegowoWriteRegistrar##channel _legowoReg##channel; \
  void _legowoWriteHandler##channel(LegowoParam param)

// Alias macro - more intuitive naming
#define LEGOWO_RECEIVE(channel) LEGOWO_WRITE(channel)

// Global handler array for macro registration
extern LegowoWriteHandler _legowoWriteHandlers[LEGOWO_MAX_CHANNELS];

#endif // LEGOWO_H
