/*
 * Legowo RGB LED & Multi-Slider Example
 * 
 * Control RGB LED with 3 sliders from dashboard
 * 
 * Dashboard Widget Setup:
 * - Channel 10: Switch widget (RGB On/Off)
 * - Channel 11: Slider widget (Red 0-255)
 * - Channel 12: Slider widget (Green 0-255)
 * - Channel 13: Slider widget (Blue 0-255)
 * - Channel 14: Slider widget (Master Brightness 0-100)
 * 
 * Get your DEVICE_KEY from Legowo Dashboard
 */

#include "Legowo.h"

// =============================================================================
// CONFIGURATION - UPDATE THESE!
// =============================================================================

#define WIFI_SSID     "YourWiFiSSID"
#define WIFI_PASS     "YourWiFiPassword"
#define MQTT_SERVER   "<your_legowo_server_ip_or_domain>"  // e.g., "192.168.1.100" or "legowo.example.com"
#define MQTT_PORT     1883

#define DEVICE_KEY    "<paste_your_64_char_device_key_here>"

// =============================================================================
// HARDWARE PINS (Common Cathode RGB LED)
// =============================================================================

#define RED_PIN       25
#define GREEN_PIN     26
#define BLUE_PIN      27

// =============================================================================
// CHANNEL MAPPING
// =============================================================================

#define CH_RGB_ONOFF      10   // Switch: RGB On/Off
#define CH_RED            11   // Slider: Red (0-255)
#define CH_GREEN          12   // Slider: Green (0-255)
#define CH_BLUE           13   // Slider: Blue (0-255)
#define CH_BRIGHTNESS     14   // Slider: Master brightness (0-100)

// =============================================================================
// GLOBALS
// =============================================================================

Legowo legowo(DEVICE_KEY);

bool rgbEnabled = false;
int redValue = 255;
int greenValue = 0;
int blueValue = 0;
int brightness = 100;

// =============================================================================
// HELPER FUNCTION
// =============================================================================

void updateRGB() {
  if (rgbEnabled) {
    // Apply brightness to each channel
    float brightnessFactor = brightness / 100.0;
    analogWrite(RED_PIN, (int)(redValue * brightnessFactor));
    analogWrite(GREEN_PIN, (int)(greenValue * brightnessFactor));
    analogWrite(BLUE_PIN, (int)(blueValue * brightnessFactor));
  } else {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  }
}

// =============================================================================
// CHANNEL HANDLERS
// =============================================================================

LEGOWO_WRITE(CH_RGB_ONOFF) {
  rgbEnabled = param.asBool();
  updateRGB();
  Serial.print("[RGB] ");
  Serial.println(rgbEnabled ? "ON" : "OFF");
}

LEGOWO_WRITE(CH_RED) {
  redValue = constrain(param.asInt(), 0, 255);
  updateRGB();
  Serial.print("[RGB] Red: ");
  Serial.println(redValue);
}

LEGOWO_WRITE(CH_GREEN) {
  greenValue = constrain(param.asInt(), 0, 255);
  updateRGB();
  Serial.print("[RGB] Green: ");
  Serial.println(greenValue);
}

LEGOWO_WRITE(CH_BLUE) {
  blueValue = constrain(param.asInt(), 0, 255);
  updateRGB();
  Serial.print("[RGB] Blue: ");
  Serial.println(blueValue);
}

LEGOWO_WRITE(CH_BRIGHTNESS) {
  brightness = constrain(param.asInt(), 0, 100);
  updateRGB();
  Serial.print("[RGB] Brightness: ");
  Serial.print(brightness);
  Serial.println("%");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Legowo RGB LED Control ===\n");
  
  // Initialize PWM pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Start with LED off
  updateRGB();
  
  legowo.setDebug(true);
  legowo.begin(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT);
  
  Serial.println("\nDashboard Setup:");
  Serial.println("  CH 10: Switch -> RGB On/Off");
  Serial.println("  CH 11: Slider (0-255) -> Red");
  Serial.println("  CH 12: Slider (0-255) -> Green");
  Serial.println("  CH 13: Slider (0-255) -> Blue");
  Serial.println("  CH 14: Slider (0-100) -> Brightness");
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  legowo.run();
  delay(10);
}
