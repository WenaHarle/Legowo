/*
 * Legowo Basic Example
 * 
 * Simple LED control and sensor reading
 * 
 * Get your DEVICE_KEY from Legowo Dashboard:
 * 1. Go to Dashboard -> Devices
 * 2. Create a new device or select existing one  
 * 3. Copy the "Device Key" (64 character hex string)
 */

#include "Legowo.h"

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
// CHANNEL MAPPING
// =============================================================================

// Channels 0-9: Device -> Dashboard (send sensor data UP to cloud)
#define CH_TEMPERATURE    0    // Sensor data: ESP sends temperature TO dashboard
#define CH_BUTTON_STATE   1    // Sensor data: ESP sends button state TO dashboard

// Channels 10+: Dashboard -> Device (receive commands FROM cloud)
#define CH_LED            10   // Control: Dashboard sends LED command TO ESP

// =============================================================================
// HARDWARE
// =============================================================================

#define LED_PIN       2
#define BUTTON_PIN    0

// =============================================================================
// GLOBALS
// =============================================================================

Legowo legowo(DEVICE_KEY);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(LED_PIN, LOW);
  
  legowo.setDebug(true);
  legowo.setInterval(5000);  // Send data every 5 seconds
  
  // Register handler: called when dashboard SENDS data to channel 10
  legowo.onReceive(CH_LED, [](LegowoParam param) {
    int value = param.asInt();
    digitalWrite(LED_PIN, value);
    Serial.print("[LED] Received from dashboard: ");
    Serial.println(value ? "ON" : "OFF");
  });
  
  legowo.begin(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT);
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  legowo.run();
  
  // Send sensor data TO dashboard (upload telemetry)
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000) {
    lastRead = millis();
    
    // Simulated temperature (replace with real sensor)
    float temp = 20.0 + random(100) / 10.0;
    legowo.write(CH_TEMPERATURE, temp);  // SEND temperature TO dashboard
    
    // Button state
    int button = !digitalRead(BUTTON_PIN);
    legowo.write(CH_BUTTON_STATE, button);  // SEND button state TO dashboard
    
    Serial.print("Sent to dashboard -> Temp: ");
    Serial.print(temp);
    Serial.print("C, Button: ");
    Serial.println(button);
    
    // Example: Read cached LED value (last command received from dashboard)
    float ledState = legowo.read(CH_LED);
    Serial.print("LED state (from dashboard): ");
    Serial.println(ledState);
  }
  
  delay(10);
}
