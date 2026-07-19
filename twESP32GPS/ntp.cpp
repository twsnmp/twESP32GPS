#include "ntp.h"
#include "config.h"
#include <string.h>

// NTP epoch is January 1, 1900.
// Unix epoch is January 1, 1970.
// Difference = 70 years = 2208988800 seconds
static const uint32_t NTP_EPOCH_OFFSET = 2208988800UL;

// =============================================================================
//  Constructor
// =============================================================================

NTPServer::NTPServer(GPSManager& gps) : _gps(gps), _port(123) {}

// =============================================================================
//  begin()
// =============================================================================

void NTPServer::begin(uint16_t port) {
  _port = port;
  _udp.begin(port);
  Serial.printf("[NTP] Listening on UDP port %d\n", port);
}

// =============================================================================
//  handleRequests() — call from loop()
// =============================================================================

void NTPServer::handleRequests() {
  int packetSize = _udp.parsePacket();
  if (packetSize < 48) return; // NTP packets are exactly 48 bytes

  uint8_t req[48];
  int len = _udp.read(req, sizeof(req));
  if (len < 48) return;

  processPacket(req, (size_t)len);
}

// =============================================================================
//  processPacket()
// =============================================================================

void NTPServer::processPacket(const uint8_t* req, size_t /*len*/) {
  _clientCount++;

  // --- Receive timestamp (as early as possible after receiving packet) ---
  uint32_t recvSec  = 0;
  uint32_t recvUsec = 0;
  getCurrentTime(recvSec, recvUsec);

  // --- Parse client version number from Byte 0 ---
  uint8_t liVnMode = req[0];
  uint8_t vn       = (liVnMode >> 3) & 0x07; // Version Number

  // --- Build NTP response packet (48 bytes) ---
  uint8_t resp[48];
  memset(resp, 0, sizeof(resp));

  GPSState state = _gps.getState();

  // Byte 0: LI=0 (no warning), VN=client version, Mode=4 (Server)
  resp[0] = (uint8_t)((0 << 6) | (vn << 3) | 4);

  // Byte 1: Stratum
  resp[1] = state.stratum; // 1 = GPS primary, 16 = unsynchronised

  // Byte 2: Poll interval (log2 seconds, minimum = 4 → 16s)
  resp[2] = 4;

  // Byte 3: Precision (log2 seconds, -20 ≈ 1 microsecond)
  resp[3] = (uint8_t)(-20);

  // Bytes 4–7: Root Delay (0 for Stratum 1 GPS)
  // Bytes 8–11: Root Dispersion (0 for Stratum 1 GPS)
  // (already zeroed by memset)

  // Bytes 12–15: Reference Identifier
  if (state.stratum == 1) {
    memcpy(resp + 12, "GPS ", 4);
  } else {
    memcpy(resp + 12, "LOCL", 4);
  }

  // --- Timestamps ---
  // Reference Timestamp (bytes 16–23): time of last GPS sync = 1PPS trigger
  uint32_t refSec  = 0;
  uint32_t refUsec = 0;
  getCurrentTime(refSec, refUsec);
  uint64_t refNTP = toNTPTimestamp(refSec, refUsec);
  resp[16] = (uint8_t)(refNTP >> 56);
  resp[17] = (uint8_t)(refNTP >> 48);
  resp[18] = (uint8_t)(refNTP >> 40);
  resp[19] = (uint8_t)(refNTP >> 32);
  resp[20] = (uint8_t)(refNTP >> 24);
  resp[21] = (uint8_t)(refNTP >> 16);
  resp[22] = (uint8_t)(refNTP >> 8);
  resp[23] = (uint8_t)(refNTP);

  // Originate Timestamp (bytes 24–31): copy of client's Transmit Timestamp
  memcpy(resp + 24, req + 40, 8);

  // Receive Timestamp (bytes 32–39): when we received the packet
  uint64_t recvNTP = toNTPTimestamp(recvSec, recvUsec);
  resp[32] = (uint8_t)(recvNTP >> 56);
  resp[33] = (uint8_t)(recvNTP >> 48);
  resp[34] = (uint8_t)(recvNTP >> 40);
  resp[35] = (uint8_t)(recvNTP >> 32);
  resp[36] = (uint8_t)(recvNTP >> 24);
  resp[37] = (uint8_t)(recvNTP >> 16);
  resp[38] = (uint8_t)(recvNTP >> 8);
  resp[39] = (uint8_t)(recvNTP);

  // Transmit Timestamp (bytes 40–47): as late as possible before sending
  uint32_t txSec  = 0;
  uint32_t txUsec = 0;
  getCurrentTime(txSec, txUsec);
  uint64_t txNTP = toNTPTimestamp(txSec, txUsec);
  resp[40] = (uint8_t)(txNTP >> 56);
  resp[41] = (uint8_t)(txNTP >> 48);
  resp[42] = (uint8_t)(txNTP >> 40);
  resp[43] = (uint8_t)(txNTP >> 32);
  resp[44] = (uint8_t)(txNTP >> 24);
  resp[45] = (uint8_t)(txNTP >> 16);
  resp[46] = (uint8_t)(txNTP >> 8);
  resp[47] = (uint8_t)(txNTP);

  // Send response to requester
  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write(resp, 48);
  _udp.endPacket();
}

// =============================================================================
//  getCurrentTime()
//  Uses GPS NMEA UTC + 1PPS microsecond offset for sub-millisecond precision
// =============================================================================

bool NTPServer::getCurrentTime(uint32_t& outSec, uint32_t& outUsec) const {
  GPSState state = _gps.getState();

  if (!state.hasFix || state.date[0] == '\0' || state.time[0] == '\0') {
    // Fall back to ESP32 system time (millis-based, low accuracy)
    uint64_t now_ms = millis();
    outSec  = (uint32_t)(now_ms / 1000);
    outUsec = (uint32_t)((now_ms % 1000) * 1000);
    return false;
  }

  // Parse GPS UTC date "YYYY-MM-DD" and time "HH:MM:SS"
  int year, month, day, hour, minute, second;
  sscanf(state.date, "%d-%d-%d", &year, &month, &day);
  sscanf(state.time, "%d:%d:%d", &hour, &minute, &second);

  // Convert GPS UTC to Unix timestamp (simple formula, no leap-second correction)
  // Days since Unix epoch (1970-01-01)
  // Use mktime-style calculation
  int y = year - 1900;
  int m = month - 1;
  // Days in each month (non-leap)
  static const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  long days = (y - 70) * 365 + (y - 69) / 4 - (y - 1) / 100 + (y + 299) / 400;
  for (int i = 0; i < m; i++) {
    days += daysInMonth[i];
    // Leap year correction for Feb
    if (i == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
      days += 1;
    }
  }
  days += day - 1;

  uint32_t gpsUnixSec = (uint32_t)(days * 86400L + hour * 3600L + minute * 60L + second);

  // Add 1PPS sub-second offset: elapsed micros since last PPS edge
  uint64_t ppsTs   = GPSManager::ppsMicros;
  uint64_t nowUs   = (uint64_t)micros();
  uint32_t elapsedUs = (uint32_t)(nowUs - ppsTs);

  // If ppsFlag hasn't been triggered yet (startup), skip offset
  if (!GPSManager::ppsFlag) {
    elapsedUs = 0;
  }

  // If elapsed > 1.5 seconds something is wrong — clamp
  if (elapsedUs > 1500000) {
    elapsedUs = 0;
  }

  outSec  = gpsUnixSec;
  outUsec = elapsedUs;

  return true;
}

// =============================================================================
//  toNTPTimestamp()
//  Converts Unix seconds + microsecond offset → 64-bit NTP timestamp
//  NTP timestamp = seconds since 1900-01-01 (32 bits) | fraction (32 bits)
// =============================================================================

uint64_t NTPServer::toNTPTimestamp(uint32_t unixSec, uint32_t usFraction) {
  uint64_t ntpSec   = (uint64_t)unixSec + NTP_EPOCH_OFFSET;
  // Fraction: usFraction / 1,000,000 × 2^32
  uint64_t frac     = ((uint64_t)usFraction << 32) / 1000000ULL;
  return (ntpSec << 32) | frac;
}
