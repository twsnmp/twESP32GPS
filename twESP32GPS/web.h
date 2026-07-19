#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "gps.h"
#include "ntp.h"

// =============================================================================
//  Web Server — serves the Svelte dashboard (gzip) and /api/gps JSON endpoint
// =============================================================================

class WebManager {
public:
  WebManager(GPSManager& gps, NTPServer& ntp);
  ~WebManager();

  void begin(uint16_t port = 80);
  void handleClient();     // Call from loop()

private:
  WebServer*  _server;     // Heap-allocated to avoid non-copyable issue
  GPSManager& _gps;
  NTPServer&  _ntp;

  void handleRoot();
  void handleApiGps();
  void handleNotFound();
};
