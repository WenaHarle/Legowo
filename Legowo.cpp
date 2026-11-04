/*
 * Legowo.cpp - Implementation
 */

#include "Legowo.h"

// =============================================================================
// STATIC MEMBERS
// =============================================================================

Legowo* Legowo::_instance = nullptr;
LegowoWriteHandler _legowoWriteHandlers[LEGOWO_MAX_CHANNELS] = {nullptr};

// =============================================================================
// CONSTRUCTOR
// =============================================================================

Legowo::Legowo(const char* deviceKey)
  : _deviceKey(deviceKey)
  , _mqttClient(_wifiClient)
  , _numSensorBindings(0)
  , _numSliderBindings(0)
  , _lastSensorRead(0)
  , _lastTelemetry(0)
  , _lastStatus(0)
  , _lastReconnect(0)
  , _telemetryInterval(LEGOWO_TELEMETRY_INTERVAL)
  , _debug(false)
{
  _instance = this;

  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    _channels[i] = 0;
    _channelDirty[i] = false;
    _handlers[i] = nullptr;
    _switchPins[i] = -1;
  }

  for (int i = 0; i < LEGOWO_MAX_BINDINGS; i++) {
    _sensorBindings[i].active = false;
    _sliderBindings[i].active = false;
  }

  // Build topics using device_key
  snprintf(_topicCommand, sizeof(_topicCommand), "device/%s/command", _deviceKey);
  snprintf(_topicStatus, sizeof(_topicStatus), "device/%s/status", _deviceKey);
}

// =============================================================================
// BEGIN
// =============================================================================

void Legowo::begin(const char* ssid, const char* password, const char* mqttServer, int mqttPort) {
  _ssid = ssid;
  _password = password;
  _mqttServer = mqttServer;
  _mqttPort = mqttPort;

  Serial.println();
  Serial.println("================================================");
  Serial.println("         Legowo IoT Platform");
  Serial.println("================================================");
  Serial.println();

  // Copy macro-registered handlers
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    if (_legowoWriteHandlers[i] != nullptr) {
      _handlers[i] = _legowoWriteHandlers[i];
      if (_debug) {
        Serial.print("  Handler registered for channel ");
        Serial.println(i);
      }
    }
  }

  _setupWiFi();
  _setupMQTT();
}

// =============================================================================
// RUN (call in loop)
// =============================================================================

void Legowo::run() {
  unsigned long now = millis();

  // Maintain WiFi
  if (WiFi.status() != WL_CONNECTED) {
    _setupWiFi();
  }

  // Maintain MQTT
  if (!_mqttClient.connected()) {
    if (now - _lastReconnect > LEGOWO_RECONNECT_INTERVAL) {
      _lastReconnect = now;
      _reconnectMQTT();
    }
  } else {
    _mqttClient.loop();
  }

  // Auto-poll bound sensors
  if (now - _lastSensorRead > _telemetryInterval) {
    _lastSensorRead = now;
    _pollSensors();
  }

  // Send telemetry
  if (now - _lastTelemetry > _telemetryInterval) {
    _lastTelemetry = now;
    _sendTelemetry();
  }

  // Send status heartbeat
  if (now - _lastStatus > LEGOWO_STATUS_INTERVAL) {
    _lastStatus = now;
    _sendStatus("online");
  }
}

// =============================================================================
// SIMPLE BINDING API
// =============================================================================

void Legowo::bindSwitch(int channel, int pin) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  _switchPins[channel] = pin;

  if (_debug) {
    Serial.print("[Bind] Switch ch");
    Serial.print(channel);
    Serial.print(" -> pin ");
    Serial.println(pin);
  }
}

void Legowo::bindSlider(int channel, int pin, int minVal, int maxVal) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  if (_numSliderBindings >= LEGOWO_MAX_BINDINGS) return;

  pinMode(pin, OUTPUT);
  analogWrite(pin, 0);

  LegowoSliderBinding& b = _sliderBindings[_numSliderBindings++];
  b.channel = channel;
  b.pin = pin;
  b.minVal = minVal;
  b.maxVal = maxVal;
  b.active = true;

  if (_debug) {
    Serial.print("[Bind] Slider ch");
    Serial.print(channel);
    Serial.print(" -> pin ");
    Serial.print(pin);
    Serial.print(" (");
    Serial.print(minVal);
    Serial.print("..");
    Serial.print(maxVal);
    Serial.println(")");
  }
}

void Legowo::bindSensor(int channel, int pin, LegowoSensorMode mode) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  if (_numSensorBindings >= LEGOWO_MAX_BINDINGS) return;

  // Configure pin mode based on sensor type
  if (mode == DIGITAL_NORMAL) {
    pinMode(pin, INPUT);
  } else if (mode == DIGITAL_INVERTED) {
    pinMode(pin, INPUT_PULLUP);
  }
  // Analog pins do not need pinMode on most boards

  LegowoSensorBinding& b = _sensorBindings[_numSensorBindings++];
  b.channel = channel;
  b.pin = pin;
  b.mode = mode;
  b.active = true;

  if (_debug) {
    Serial.print("[Bind] Sensor ch");
    Serial.print(channel);
    Serial.print(" <- pin ");
    Serial.print(pin);
    Serial.print(" mode ");
    Serial.println((int)mode);
  }
}

// =============================================================================
// INTERNAL: APPLY BINDINGS WHEN COMMAND ARRIVES
// =============================================================================

void Legowo::_applySwitch(int channel, float value) {
  int pin = _switchPins[channel];
  if (pin < 0) return;
  digitalWrite(pin, value > 0 ? HIGH : LOW);
}

void Legowo::_applySlider(int channel, float value) {
  for (int i = 0; i < _numSliderBindings; i++) {
    LegowoSliderBinding& b = _sliderBindings[i];
    if (!b.active || b.channel != channel) continue;

    int v = (int)value;
    if (v < b.minVal) v = b.minVal;
    if (v > b.maxVal) v = b.maxVal;

    int pwm = map(v, b.minVal, b.maxVal, 0, 255);
    analogWrite(b.pin, pwm);
  }
}

// =============================================================================
// INTERNAL: AUTO-POLL BOUND SENSORS
// =============================================================================

int Legowo::_readSensorPin(const LegowoSensorBinding& b) {
  switch (b.mode) {
    case DIGITAL_NORMAL:
      return digitalRead(b.pin);

    case DIGITAL_INVERTED:
      return !digitalRead(b.pin);

    case ANALOG_RAW:
      return analogRead(b.pin);

    case ANALOG_PERCENT: {
      int raw = analogRead(b.pin);
      #ifdef ESP32
        return map(raw, 0, 4095, 0, 100);
      #else
        return map(raw, 0, 1023, 0, 100);
      #endif
    }
  }
  return 0;
}

void Legowo::_pollSensors() {
  for (int i = 0; i < _numSensorBindings; i++) {
    LegowoSensorBinding& b = _sensorBindings[i];
    if (!b.active) continue;

    int value = _readSensorPin(b);
    write(b.channel, value);
  }
}

// =============================================================================
// WRITE TO CHANNEL (Send data from device to dashboard)
// This uploads telemetry/sensor data to the cloud
// =============================================================================

void Legowo::write(int channel, float value) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  _channels[channel] = value;
  _channelDirty[channel] = true;
}

void Legowo::write(int channel, int value) {
  write(channel, (float)value);
}

void Legowo::write(int channel, bool value) {
  write(channel, value ? 1.0f : 0.0f);
}

// =============================================================================
// READ CHANNEL (Get cached value from local memory)
// Returns last value received from dashboard or written locally
// =============================================================================

float Legowo::read(int channel) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return 0;
  return _channels[channel];
}

// =============================================================================
// REGISTER HANDLER (Callback when dashboard sends command to this channel)
// =============================================================================

void Legowo::onReceive(int channel, LegowoWriteHandler handler) {
  if (channel < 0 || channel >= LEGOWO_MAX_CHANNELS) return;
  _handlers[channel] = handler;
}

// =============================================================================
// CONNECTION STATUS
// =============================================================================

bool Legowo::connected() {
  return _mqttClient.connected();
}

bool Legowo::wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// =============================================================================
// SYNC ALL
// =============================================================================

void Legowo::syncAll() {
  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    _channelDirty[i] = true;
  }
  _sendTelemetry();
}

// =============================================================================
// SET INTERVAL
// =============================================================================

void Legowo::setInterval(unsigned long ms) {
  _telemetryInterval = ms;
}

// =============================================================================
// DEBUG
// =============================================================================

void Legowo::setDebug(bool enable) {
  _debug = enable;
}

// =============================================================================
// WIFI SETUP
// =============================================================================

void Legowo::_setupWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.print(_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED");
  }
}

// =============================================================================
// MQTT SETUP
// =============================================================================

void Legowo::_setupMQTT() {
  _mqttClient.setServer(_mqttServer, _mqttPort);
  _mqttClient.setCallback(_mqttCallback);
  _mqttClient.setBufferSize(LEGOWO_MQTT_BUFFER_SIZE);
}

void Legowo::_reconnectMQTT() {
  Serial.print("[MQTT] Connecting...");

  String clientId = "legowo-";
  clientId += String(random(0xffff), HEX);

  // LWT message for offline detection
  char lwtPayload[] = "{\"status\":\"offline\",\"online\":false}";

  if (_mqttClient.connect(clientId.c_str(), NULL, NULL,
                          _topicStatus, 0, false, lwtPayload)) {
    Serial.println(" OK");
    _mqttClient.subscribe(_topicCommand);
    Serial.print("[MQTT] Subscribed: ");
    Serial.println(_topicCommand);
    _sendStatus("online");
  } else {
    Serial.print(" FAILED (");
    Serial.print(_mqttClient.state());
    Serial.println(")");
  }
}

// =============================================================================
// MQTT CALLBACK (static)
// =============================================================================

void Legowo::_mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (_instance) {
    _instance->_handleMessage(topic, payload, length);
  }
}

// =============================================================================
// HANDLE INCOMING MESSAGE (Command from dashboard)
// Called when dashboard sends a command to control the device
// =============================================================================

void Legowo::_handleMessage(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    if (_debug) Serial.println("[MQTT] JSON parse error");
    return;
  }

  if (_debug) {
    Serial.print("[MQTT] Command: ");
    serializeJson(doc, Serial);
    Serial.println();
  }

  // Handle channel commands: {"channel": N, "value": X} or {"0": X, "1": Y, ...}
  if (doc.containsKey("channel") && doc.containsKey("value")) {
    int channel = doc["channel"].as<int>();
    float value = doc["value"].as<float>();

    if (channel >= 0 && channel < LEGOWO_MAX_CHANNELS) {
      _channels[channel] = value;

      // Auto-apply bindings (switch/slider)
      _applySwitch(channel, value);
      _applySlider(channel, value);

      // Then call user handler if any
      if (_handlers[channel] != nullptr) {
        LegowoParam param(value);
        _handlers[channel](param);
      }
      if (_debug) {
        Serial.print("[MQTT] Ch");
        Serial.print(channel);
        Serial.print(" = ");
        Serial.println(value);
      }
    }
  } else {
    // Handle {"0": X, "1": Y, ...} format
    for (JsonPair kv : doc.as<JsonObject>()) {
      const char* key = kv.key().c_str();

      bool isNumber = true;
      for (int i = 0; key[i] != '\0'; i++) {
        if (key[i] < '0' || key[i] > '9') {
          isNumber = false;
          break;
        }
      }

      if (isNumber) {
        int channel = atoi(key);
        float value = kv.value().as<float>();

        if (channel >= 0 && channel < LEGOWO_MAX_CHANNELS) {
          _channels[channel] = value;

          _applySwitch(channel, value);
          _applySlider(channel, value);

          if (_handlers[channel] != nullptr) {
            LegowoParam param(value);
            _handlers[channel](param);
          }
        }
      }
    }
  }
}

// =============================================================================
// SEND TELEMETRY (Upload dirty channels to dashboard)
// This sends data FROM device TO cloud/dashboard
// =============================================================================

void Legowo::_sendTelemetry() {
  if (!_mqttClient.connected()) return;

  for (int i = 0; i < LEGOWO_MAX_CHANNELS; i++) {
    if (_channelDirty[i]) {
      char topic[128];
      snprintf(topic, sizeof(topic), "device/%s/data/%d", _deviceKey, i);

      JsonDocument doc;
      doc["value"] = _channels[i];
      doc["ts"] = millis();

      char payload[128];
      serializeJson(doc, payload);

      _mqttClient.publish(topic, payload);
      _channelDirty[i] = false;

      if (_debug) {
        Serial.print("[MQTT] Sent ch");
        Serial.print(i);
        Serial.print(" = ");
        Serial.println(_channels[i]);
      }
    }
  }
}

// =============================================================================
// SEND STATUS
// =============================================================================

void Legowo::_sendStatus(const char* status) {
  if (!_mqttClient.connected()) return;

  JsonDocument doc;
  doc["status"] = status;
  doc["online"] = true;
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["heap"] = ESP.getFreeHeap();

  char payload[256];
  serializeJson(doc, payload);

  _mqttClient.publish(_topicStatus, payload);

  if (_debug) {
    Serial.println("[MQTT] Status: online");
  }
}
