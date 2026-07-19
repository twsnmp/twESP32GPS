#include "gps.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
//  Static / ISR variables
// =============================================================================

volatile uint64_t GPSManager::ppsMicros = 0;
volatile bool     GPSManager::ppsFlag   = false;

void IRAM_ATTR GPSManager::onPPS() {
  GPSManager::ppsMicros = (uint64_t)micros();
  GPSManager::ppsFlag   = true;
}

// =============================================================================
//  Constructor
// =============================================================================

GPSManager::GPSManager() {
  memset(&_state, 0, sizeof(_state));
  _state.stratum = 16;
  _gsvSessionCount = 0;
}

// =============================================================================
//  begin()
// =============================================================================

void GPSManager::begin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin, int ppsPin) {
  _serial = &serial;
  serial.begin(baud, SERIAL_8N1, rxPin, txPin);

  // 1PPS hardware interrupt
  pinMode(ppsPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(ppsPin), GPSManager::onPPS, RISING);

  Serial.println("[GPS] Initialized. Waiting for NMEA data...");
}

// =============================================================================
//  update() — call every loop()
// =============================================================================

void GPSManager::update() {
  if (!_serial) return;

  static char lineBuf[128];
  static int  lineLen = 0;

  while (_serial->available()) {
    char c = (char)_serial->read();

    // Feed raw byte to TinyGPS++ (handles RMC, GGA, etc.)
    _gps.encode(c);

    // Also buffer full NMEA sentences for custom GSV parsing
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 6 && lineBuf[0] == '$') {
        parseNMEASentence(lineBuf);
      }
      lineLen = 0;
    } else {
      if (lineLen < (int)sizeof(lineBuf) - 1) {
        lineBuf[lineLen++] = c;
      }
    }
  }

  // Sync TinyGPS++ data into _state
  if (_gps.location.isValid()) {
    _state.hasFix       = true;
    _state.latitude     = _gps.location.lat();
    _state.longitude    = _gps.location.lng();
    _state.stratum      = 1;
  } else {
    _state.hasFix   = false;
    _state.stratum  = 16;
  }

  if (_gps.altitude.isValid()) {
    _state.altitude = _gps.altitude.meters();
  }
  if (_gps.speed.isValid()) {
    _state.speedKmh = _gps.speed.kmph();
  }
  if (_gps.satellites.isValid()) {
    _state.numSatellites = (int)_gps.satellites.value();
  }

  // Update time string from TinyGPS++
  if (_gps.time.isValid()) {
    snprintf(_state.time, sizeof(_state.time), "%02d:%02d:%02d",
             _gps.time.hour(), _gps.time.minute(), _gps.time.second());
  }
  if (_gps.date.isValid()) {
    snprintf(_state.date, sizeof(_state.date), "%04d-%02d-%02d",
             _gps.date.year(), _gps.date.month(), _gps.date.day());
  }

  _state.uptimeSeconds = millis() / 1000;
}

// =============================================================================
//  getState() — returns a copy for JSON serialization
// =============================================================================

GPSState GPSManager::getState() const {
  return _state;
}

// =============================================================================
//  NMEA sentence dispatcher
// =============================================================================

void GPSManager::parseNMEASentence(const char* sentence) {
  // Validate checksum
  // sentence format: $TTSSS,...*HH
  const char* star = strchr(sentence, '*');
  if (!star) return;

  uint8_t computed = 0;
  for (const char* p = sentence + 1; p < star; p++) {
    computed ^= (uint8_t)(*p);
  }
  uint8_t given = (uint8_t)strtol(star + 1, nullptr, 16);
  if (computed != given) return;

  // Dispatch
  // Talker: characters 1-2, sentence type: characters 3-5
  // e.g. $GPGSV → talker=GP, type=GSV
  //      $GNGSV → talker=GN, type=GSV
  if (strstr(sentence, "GSV,") != nullptr) {
    parseGSV(sentence);
  }
}

// =============================================================================
//  GSV Custom Parser
//  $TALKERGSV,totalMsg,msgNum,totalSats,PRN,El,Az,SNR,...*CS
// =============================================================================

// Helper function to extract fields from a comma-separated NMEA sentence,
// handling consecutive commas correctly (unlike strtok which groups them).
static char* getNextField(char** s) {
  if (s == nullptr || *s == nullptr) return nullptr;
  char* start = *s;
  char* comma = strchr(start, ',');
  if (comma != nullptr) {
    *comma = '\0';
    *s = comma + 1;
  } else {
    *s = nullptr;
  }
  return start;
}

void GPSManager::parseGSV(const char* sentence) {
  // Copy sentence (we'll modify it in-place using getNextField)
  char buf[160];
  strncpy(buf, sentence, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // Strip checksum
  char* star = strchr(buf, '*');
  if (star) *star = '\0';

  char* p = buf;

  // Tokenize the sentence type (e.g., "$GPGSV")
  char* tok = getNextField(&p);
  if (!tok) return;

  // Extract talker from sentence ID (e.g. "$GPGSV" → "GP")
  char talker[4] = {0};
  if (tok[0] == '$' && strlen(tok) >= 6) {
    talker[0] = tok[1];
    talker[1] = tok[2];
    talker[2] = '\0';
  }

  const char* system = systemFromTalker(talker);

  // totalMessages
  tok = getNextField(&p);
  if (!tok) return;
  int totalMsg = atoi(tok);

  // msgNumber
  tok = getNextField(&p);
  if (!tok) return;
  int msgNum = atoi(tok);

  // totalSatsInView
  tok = getNextField(&p);
  if (!tok) return;
  int totalSats = atoi(tok);
  (void)totalSats; // Suppress unused variable warning

  // Find or create session for this system
  GSVSession* session = nullptr;
  for (int i = 0; i < _gsvSessionCount; i++) {
    if (strcmp(_gsvSessions[i].system, system) == 0) {
      session = &_gsvSessions[i];
      break;
    }
  }
  if (!session) {
    if (_gsvSessionCount < 6) {
      session = &_gsvSessions[_gsvSessionCount++];
      strncpy(session->system, system, sizeof(session->system) - 1);
      session->count = 0;
    } else {
      return; // No more room
    }
  }

  // First message — reset buffer
  if (msgNum == 1) {
    session->totalMessages    = totalMsg;
    session->receivedMessages = 0;
    session->count            = 0;
  }
  session->receivedMessages++;

  // Parse up to 4 satellite entries per GSV message
  for (int i = 0; i < 4; i++) {
    tok = getNextField(&p);
    if (!tok) break; // End of sentence
    if (strlen(tok) == 0) {
      // Empty PRN field. We must advance past the other 3 fields of this slot if present.
      getNextField(&p); // El
      getNextField(&p); // Az
      getNextField(&p); // SNR
      continue;
    }
    int prn = atoi(tok);

    tok = getNextField(&p);
    if (!tok) break;
    int el = (strlen(tok) > 0) ? atoi(tok) : 0;

    tok = getNextField(&p);
    if (!tok) break;
    int az = (strlen(tok) > 0) ? atoi(tok) : 0;

    tok = getNextField(&p);
    int snr = 0;
    if (tok && strlen(tok) > 0) snr = atoi(tok);

    if (prn <= 0) continue;

    // Classify QZSS / SBAS PRN overrides
    const char* actualSys = classifyPRN(system, prn);

    if (session->count < 48) {
      SatelliteDetail& d = session->buf[session->count++];
      d.prn       = prn;
      d.elevation = el;
      d.azimuth   = az;
      d.snr       = snr;
      strncpy(d.system, actualSys, sizeof(d.system) - 1);
    }
  }

  // When all GSV messages for this session have arrived, merge into _state
  if (session->receivedMessages == session->totalMessages) {
    mergeSatellites();
  }
}

// =============================================================================
//  mergeSatellites() — flatten all sessions into _state.satellites[]
// =============================================================================

void GPSManager::mergeSatellites() {
  int total = 0;
  for (int s = 0; s < _gsvSessionCount; s++) {
    GSVSession& sess = _gsvSessions[s];
    for (int i = 0; i < sess.count && total < MAX_SATELLITES; i++) {
      _state.satellites[total++] = sess.buf[i];
    }
  }
  _state.satelliteCount = total;
}

// =============================================================================
//  Helpers
// =============================================================================

const char* GPSManager::systemFromTalker(const char* talker) {
  if (strcmp(talker, "GP") == 0) return "GPS";
  if (strcmp(talker, "GL") == 0) return "GLONASS";
  if (strcmp(talker, "GA") == 0) return "Galileo";
  if (strcmp(talker, "GB") == 0 || strcmp(talker, "BD") == 0) return "BeiDou";
  if (strcmp(talker, "QZ") == 0) return "QZSS";
  if (strcmp(talker, "GN") == 0) return "GPS"; // Multi-constellation, treat as GPS base
  return "GPS";
}

const char* GPSManager::classifyPRN(const char* baseSys, int prn) {
  // QZSS: PRN 193-202 (reported under GP talker)
  if (prn >= 193 && prn <= 202) return "QZSS";
  // SBAS standard range 120-158, or SBAS compat 33-64
  if ((prn >= 120 && prn <= 158) || (prn >= 33 && prn <= 64)) return "SBAS";
  return baseSys;
}
