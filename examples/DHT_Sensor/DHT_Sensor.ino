/*
 * Legowo DHT Sensor Example
 * 
 * Read DHT11/DHT22 temperature & humidity sensor
 * Control relay from dashboard
 * 
 * Required Libraries:
 * - DHT sensor library by Adafruit
 * - Adafruit Unified Sensor
 * 
 * Get your DEVICE_KEY from Legowo Dashboard:
 * 1. Go to Dashboard -> Devices
 * 2. Create a new device or select existing one
 * 3. Copy the "Device Key" (64 character hex string)
 */

#include "Legowo.h"
#include <DHT.h>

// =============================================================================
// CONFIGURATION - UPDATE THESE!
// =============================================================================

#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASS     "YourWiFiPassword"
#define MQTT_SERVER   "<your_legowo_server_ip_or_domain>"  // e.g., "192.168.1.100" or "legowo.example.com"
#define MQTT_PORT     1883

// Get this from Legowo Dashboard -> Device Detail -> Device Key
#define DEVICE_KEY    "<paste_your_64_char_device_key_here>"

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

#define DHT_PIN       4
#define DHT_TYPE      DHT22    // DHT11 or DHT22
#define RELAY_PIN     16

// =============================================================================
// CHANNEL MAPPING
// =============================================================================

// Sensors (ESP -> Dashboard)
#define CH_TEMPERATURE    0
#define CH_HUMIDITY       1
#define CH_HEAT_INDEX     2

// Controls (Dashboard -> ESP)
#define CH_RELAY          10
#define CH_AUTO_MODE      11
#define CH_TEMP_THRESHOLD 12

// =============================================================================
// GLOBALS
// =============================================================================

Legowo legowo(DEVICE_KEY);
DHT dht(DHT_PIN, DHT_TYPE);

bool autoMode = false;
float tempThreshold = 28.0;

// =============================================================================
// CHANNEL HANDLERS
// =============================================================================

// Manual relay control from dashboard
LEGOWO_WRITE(CH_RELAY) {
  if (!autoMode) {
    int value = param.asInt();
    digitalWrite(RELAY_PIN, value);
    Serial.print("[Relay] Manual: ");
    Serial.println(value ? "ON" : "OFF");
  } else {
    Serial.println("[Relay] Ignored - Auto mode active");
  }
}

// Auto mode toggle
LEGOWO_WRITE(CH_AUTO_MODE) {
  autoMode = param.asBool();
  Serial.print("[Auto Mode] ");
  Serial.println(autoMode ? "ENABLED" : "DISABLED");
}

// Temperature threshold for auto mode
LEGOWO_WRITE(CH_TEMP_THRESHOLD) {
  tempThreshold = param.asFloat();
  Serial.print("[Threshold] ");
  Serial.print(tempThreshold);
  Serial.println("C");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("=== Legowo DHT Sensor ===");
  
  // Initialize hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  dht.begin();
  
  // Configure Legowo
  legowo.setDebug(true);
  legowo.setInterval(10000);  // Send every 10 seconds
  legowo.begin(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT);
  
  Serial.println("Setup complete!");
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  legowo.run();
  
  // Read DHT sensor
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 10000) {
    lastRead = millis();
    
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("[DHT] Read failed!");
      return;
    }
    
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    
    // Send to dashboard
    legowo.write(CH_TEMPERATURE, temperature);
    legowo.write(CH_HUMIDITY, humidity);
    legowo.write(CH_HEAT_INDEX, heatIndex);
    
    Serial.println("------------------------------");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Heat Index: ");
    Serial.print(heatIndex);
    Serial.println(" C");
    
    // Auto mode: turn on relay if temp exceeds threshold
    if (autoMode) {
      bool shouldBeOn = temperature > tempThreshold;
      digitalWrite(RELAY_PIN, shouldBeOn);
      Serial.print("Auto Relay: ");
      Serial.println(shouldBeOn ? "ON" : "OFF");
    }
  }
  
  delay(10);
}
