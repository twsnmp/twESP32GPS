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

  // --- Debug log: print timing info for each NTP request ---
  {
    GPSState dbg      = _gps.getState();
    uint64_t dbgPps   = GPSManager::ppsMicros;
    uint32_t dbgIntvl = GPSManager::ppsIntervalUs;
    uint32_t dbgCnt   = GPSManager::ppsCount;
    uint64_t dbgNow   = (uint64_t)micros();
    uint32_t dbgElapsed = (uint32_t)(dbgNow - dbgPps);
    int32_t  dbgDrift   = (dbgIntvl > 0) ? (int32_t)dbgIntvl - 1000000 : 0;
    Serial.printf(
      "[NTP] #%lu  NMEA=%s %s  gpsUnixSec=%lu"
      "  pps#%lu interval=%lu us (drift%+ld us)"
      "  elapsed=%lu us  recv=%lu.%06lu\n",
      (unsigned long)_clientCount,
      dbg.date[0] ? dbg.date : "(no date)",
      dbg.time[0] ? dbg.time : "(no time)",
      (unsigned long)recvSec,
      (unsigned long)dbgCnt,
      (unsigned long)dbgIntvl,
      (long)dbgDrift,
      (unsigned long)dbgElapsed,
      (unsigned long)recvSec,
      (unsigned long)recvUsec
    );
  }

  // --- Parse client version number from Byte 0 ---
  uint8_t liVnMode = req[0];
  uint8_t vn       = (liVnMode >> 3) & 0x07; // Version Number

  // --- Build NTP response packet (48 bytes) ---
  uint8_t resp[48];
  memset(resp, 0, sizeof(resp));

  GPSState state = _gps.getState();

  // Byte 0: LI | VN | Mode
  // LI = 0 (no warning) when GPS-locked, LI = 3 (alarm/unsynchronised) otherwise.
  // NTP clients will discard stratum-16 or LI=3 servers.
  uint8_t li = (state.stratum == 1) ? 0 : 3;
  resp[0] = (uint8_t)((li << 6) | (vn << 3) | 4);

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

bool NTPServer::getCurrentTime(uint32_t& outSec, uint32_t& outUsec) const {
  GPSState state = _gps.getState();

  // If there's no GPS fix or we haven't synced a valid epoch timestamp yet,
  // fall back to low-accuracy ESP32 system time (based on millis()).
  if (!state.hasFix || GPSManager::ppsValidUnixSec < 1700000000UL) {
    uint64_t now_ms = millis();
    outSec  = (uint32_t)(now_ms / 1000);
    outUsec = (uint32_t)((now_ms % 1000) * 1000);
    return false;
  }

  // Lock-free sequence read to ensure consistency between ppsValidUnixSec and ppsMicros
  uint32_t count1 = GPSManager::ppsCount;
  uint32_t sec = GPSManager::ppsValidUnixSec;
  uint64_t ppsTs = GPSManager::ppsMicros;
  if (count1 != GPSManager::ppsCount) {
    sec = GPSManager::ppsValidUnixSec;
    ppsTs = GPSManager::ppsMicros;
  }

  uint64_t nowUs = (uint64_t)micros();
  uint32_t elapsedUs = (uint32_t)(nowUs - ppsTs);

  if (!GPSManager::ppsFlag) {
    elapsedUs = 0;
  }

  // Clock-drift correction using the measured PPS interval.
  uint32_t interval = GPSManager::ppsIntervalUs;
  if (interval > 500000UL && interval < 1500000UL && interval != 1000000UL) {
    elapsedUs = (uint32_t)(((uint64_t)elapsedUs * 1000000ULL) / (uint64_t)interval);
  }

  // Wrap elapsedUs if it rolled past 1 second (e.g. if we missed a PPS edge)
  if (elapsedUs >= 1000000UL) {
    sec += elapsedUs / 1000000UL;
    elapsedUs = elapsedUs % 1000000UL;
  }

  outSec  = sec;
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
