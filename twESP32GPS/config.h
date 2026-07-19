#pragma once

// =============================================================================
//  twESP32GPS — Configuration
// =============================================================================

// --- GPS Serial (HardwareSerial 2) ---
#define GPS_BAUD      9600
#define GPS_RX_PIN    44    // D7 on XIAO ESP32-S3
#define GPS_TX_PIN    43    // D6 on XIAO ESP32-S3

// --- 1PPS Interrupt Pin ---
#define PPS_PIN       2     // D1 on XIAO ESP32-S3 (RISING edge = exact 1-second boundary)

// --- NTP Server ---
#define NTP_PORT      123

// --- Web Server ---
#define HTTP_PORT     80

// --- WiFiManager AP (shown when WiFi is not yet configured) ---
#define WIFI_AP_NAME  "twESP32GPS-Setup"
#define WIFI_AP_PASS  ""          // Empty = open AP

// --- GPS data ---
#define MAX_SATELLITES 48         // Max satellite entries to store

// --- Timing ---
#define WIFI_TIMEOUT_MS  180000   // 3 min to configure WiFi before AP fallback loop
