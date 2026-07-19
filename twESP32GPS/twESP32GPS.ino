/**
 * twESP32GPS — Main Sketch
 *
 * Hardware: Seeed Studio XIAO ESP32-S3 + VK2828U7G5 GPS
 *
 * Features:
 *   - Stratum 1 NTP Server via UDP port 123 (1PPS disciplined)
 *   - REST JSON API at /api/gps
 *   - Svelte web dashboard served from LittleFS (gzipped)
 *   - WiFiManager Captive Portal for WiFi configuration
 *
 * Wiring:
 *   GPS G (GND) → GND
 *   GPS V (VCC) → 3V3
 *   GPS T (TX)  → D7 (GPIO 44)
 *   GPS R (RX)  → D6 (GPIO 43)
 *   GPS B (PPS) → D1 (GPIO  2)
 *   GPS E (EN)  → 3V3
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "gps.h"
#include "ntp.h"
#include "web.h"

// =============================================================================
//  Global instances
// =============================================================================

// NOTE: Serial2 is pre-defined by the ESP32 Arduino Core (UART2).
// We pass it directly to GPSManager instead of declaring a new HardwareSerial.
static GPSManager  gpsManager;
static NTPServer   ntpServer(gpsManager);
static WebManager  webManager(gpsManager, ntpServer);

// =============================================================================
//  setup()
// =============================================================================

void setup() {
  // Debug serial
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== twESP32GPS ===");

  // --- GPS Serial + 1PPS interrupt ---
  gpsManager.begin(Serial2, GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN, PPS_PIN);

  // --- WiFiManager (Captive Portal) ---
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3-min portal timeout
  wifiManager.setConnectTimeout(20);

  // Callback: show IP when connected
  wifiManager.setAPCallback([](WiFiManager* mgr) {
    Serial.printf("[WiFi] Config AP started: SSID='%s'\n", WIFI_AP_NAME);
    Serial.println("[WiFi] Connect to AP and open http://192.168.4.1 to configure WiFi.");
  });

  wifiManager.setSaveConfigCallback([]() {
    Serial.println("[WiFi] Configuration saved. Connecting...");
  });

  // Auto-connect (blocks until connected or portal times out)
  if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASS)) {
    Serial.println("[WiFi] Failed to connect within timeout. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  // --- NTP Server ---
  ntpServer.begin(NTP_PORT);

  // --- Web Server ---
  webManager.begin(HTTP_PORT);

  Serial.printf("[Ready] NTP: %s:%d | Web: http://%s/\n",
                WiFi.localIP().toString().c_str(), NTP_PORT,
                WiFi.localIP().toString().c_str());
}

// =============================================================================
//  loop()
// =============================================================================

void loop() {
  // Feed GPS serial data
  gpsManager.update();

  // Handle incoming NTP UDP requests
  ntpServer.handleRequests();

  // Handle incoming HTTP requests
  webManager.handleClient();
}
