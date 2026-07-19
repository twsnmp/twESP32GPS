#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "gps.h"

// =============================================================================
//  NTP Server
//  Implements RFC 4330 / RFC 5905 Stratum 1 (GPS reference clock)
//  Mirrors the logic from twgps ntp.go
// =============================================================================

class NTPServer {
public:
  explicit NTPServer(GPSManager& gps);

  void begin(uint16_t port = 123);
  void handleRequests();   // Call from loop()

  uint32_t clientCount() const { return _clientCount; }

private:
  GPSManager& _gps;
  WiFiUDP     _udp;
  uint16_t    _port;
  uint32_t    _clientCount = 0;

  // Build and send NTP response
  void processPacket(const uint8_t* req, size_t len);

  // Convert Unix timestamp + microseconds offset → 64-bit NTP timestamp
  static uint64_t toNTPTimestamp(uint32_t unixSec, uint32_t usFraction);

  // Get current GPS-based Unix time using 1PPS discipline
  // Returns true if GPS fix is valid
  bool getCurrentTime(uint32_t& outSec, uint32_t& outUsec) const;
};
