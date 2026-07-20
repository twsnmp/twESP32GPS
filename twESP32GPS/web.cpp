#include "web.h"
#include "config.h"
#include "dashboard.h"  // Embedded Raw String dashboard HTML
#include <ArduinoJson.h>
#include <WiFiManager.h>

// =============================================================================
//  Constructor / Destructor
// =============================================================================

WebManager::WebManager(GPSManager& gps, NTPServer& ntp)
  : _server(nullptr), _gps(gps), _ntp(ntp) {}

WebManager::~WebManager() {
  if (_server) {
    _server->stop();
    delete _server;
    _server = nullptr;
  }
}

// =============================================================================
//  begin()
// =============================================================================

void WebManager::begin(uint16_t port) {
  // Allocate WebServer with the requested port
  if (_server) {
    _server->stop();
    delete _server;
  }
  _server = new WebServer(port);

  // Route: /  → serve Svelte dashboard from Flash memory
  _server->on("/", HTTP_GET, [this]() { handleRoot(); });

  // Route: /favicon.ico → return 204 No Content to avoid browser 404 console errors
  _server->on("/favicon.ico", HTTP_GET, [this]() {
    _server->sendHeader("Cache-Control", "public, max-age=604800");
    _server->send(204, "image/x-icon", "");
  });

  // Route: /api/gps → JSON API (no cache)
  _server->on("/api/gps", HTTP_GET, [this]() { handleApiGps(); });

  // Route: /api/reset-wifi → clear saved WiFi credentials and restart
  _server->on("/api/reset-wifi", HTTP_POST, [this]() { handleApiResetWifi(); });

  // Route: catch-all → 404
  _server->onNotFound([this]() { handleNotFound(); });

  _server->begin();
  Serial.printf("[Web] HTTP server started on port %d\n", port);
}

// =============================================================================
//  handleClient()
// =============================================================================

void WebManager::handleClient() {
  if (_server) _server->handleClient();
}

// =============================================================================
//  handleRoot() — serve HTML from Flash (PROGMEM)
// =============================================================================

void WebManager::handleRoot() {
  _server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server->sendHeader("Pragma", "no-cache");
  _server->sendHeader("Expires", "0");
  _server->send_P(200, "text/html", DASHBOARD_HTML);
}

// =============================================================================
//  handleApiGps() — real-time JSON API
// =============================================================================

void WebManager::handleApiGps() {
  GPSState state = _gps.getState();

  // ArduinoJson v7: JsonDocument is the main type (no StaticJsonDocument)
  JsonDocument doc;

  doc["hasFix"]        = state.hasFix;
  doc["time"]          = state.time;
  doc["date"]          = state.date;
  doc["latitude"]      = serialized(String(state.latitude,  6));
  doc["longitude"]     = serialized(String(state.longitude, 6));
  doc["altitude"]      = serialized(String(state.altitude,  1));
  doc["speedKmh"]      = serialized(String(state.speedKmh,  2));
  doc["numSatellites"] = state.numSatellites;
  doc["numSatsInView"] = state.satelliteCount;
  doc["stratum"]       = state.stratum;
  doc["uptime"]        = state.uptimeSeconds;
  doc["ntpClients"]    = (uint32_t)_ntp.clientCount();

  JsonArray sats = doc["satellites"].to<JsonArray>();
  for (int i = 0; i < state.satelliteCount; i++) {
    const SatelliteDetail& d = state.satellites[i];
    JsonObject sat = sats.add<JsonObject>();
    sat["prn"]       = d.prn;
    sat["system"]    = d.system;
    sat["elevation"] = d.elevation;
    sat["azimuth"]   = d.azimuth;
    sat["snr"]       = d.snr;
  }

  String json;
  serializeJson(doc, json);

  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->sendHeader("Cache-Control", "no-cache, no-store");
  _server->send(200, "application/json", json);
}

// =============================================================================
//  handleApiResetWifi() — clear WiFiManager credentials and restart
// =============================================================================

void WebManager::handleApiResetWifi() {
  Serial.println("[Web] /api/reset-wifi called — clearing WiFi settings and restarting.");

  _server->sendHeader("Access-Control-Allow-Origin", "*");
  _server->sendHeader("Cache-Control", "no-cache, no-store");
  _server->send(200, "application/json",
                "{\"status\":\"ok\",\"message\":\"WiFi settings cleared. Rebooting...\"}");

  // Brief delay to allow the HTTP response to be sent before restart
  delay(500);

  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

// =============================================================================
//  handleNotFound()
// =============================================================================

void WebManager::handleNotFound() {
  _server->send(404, "text/plain", "Not Found");
}
