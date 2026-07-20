#include "gps.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// ESP-IDF GPIO/interrupt APIs for high-priority PPS ISR
#include "driver/gpio.h"
#include "esp_intr_alloc.h"

// =============================================================================
//  Static / ISR variables
// =============================================================================

volatile uint64_t GPSManager::ppsMicros    = 0;
volatile bool     GPSManager::ppsFlag      = false;
volatile uint32_t GPSManager::ppsIntervalUs = 0;
volatile uint32_t GPSManager::ppsCount     = 0;
volatile uint32_t GPSManager::ppsValidUnixSec = 0;

// High-priority ISR registered via gpio_isr_handler_add.
// Runs at ESP_INTR_FLAG_LEVEL3, above WiFi stack, minimising capture latency.
void IRAM_ATTR GPSManager::onPPS(void* /*arg*/) {
  uint64_t now = (uint64_t)micros();

  if (GPSManager::ppsMicros > 0) {
    uint32_t interval = (uint32_t)(now - GPSManager::ppsMicros);

    // ------------------------------------------------------------------
    // Debounce: ignore any trigger within 500 ms of the last accepted one.
    //
    // GPS PPS pulse width is typically 50-100 ms.  Without debounce the
    // ISR fires TWICE per second: once on the rising edge (correct) and
    // once on the falling edge (~80 ms later).  The falling-edge capture
    // overwrites ppsMicros and shifts every elapsed computation by ~80 ms,
    // producing a persistent ~-80 ms NTP offset.
    //
    // A 500 ms gate passes exactly one capture per GPS second while still
    // allowing a missed-PPS recovery on the next cycle.
    // ------------------------------------------------------------------
    if (interval < 500000UL) {
      return;  // Too close to previous capture — falling-edge noise, ignore
    }

    // Valid inter-PPS interval (should be ~1,000,000 us for GPS PPS)
    if (interval < 1500000UL) {
      GPSManager::ppsIntervalUs = interval;
    }
  }

  GPSManager::ppsMicros = now;
  GPSManager::ppsFlag   = true;
  GPSManager::ppsCount++;
  GPSManager::ppsValidUnixSec++;
}


// =============================================================================
//  Constructor
// =============================================================================

GPSManager::GPSManager() {
  memset(&_state, 0, sizeof(_state));
  _state.stratum = 16;
  _gsvSessionCount = 0;
  _lastSyncedNmeaSec = 0;
}

// =============================================================================
//  convertToUnixSec()
// =============================================================================

uint32_t GPSManager::convertToUnixSec(int year, int month, int day, int hour, int minute, int second) {
  int y = year - 1900;
  int m = month - 1;
  static const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  long days = (y - 70) * 365 + (y - 69) / 4 - (y - 1) / 100 + (y + 299) / 400;
  for (int i = 0; i < m; i++) {
    days += daysInMonth[i];
    if (i == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
      days += 1;
    }
  }
  days += day - 1;
  return (uint32_t)(days * 86400L + hour * 3600L + minute * 60L + second);
}

// =============================================================================
//  begin()
// =============================================================================

void GPSManager::begin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin, int ppsPin) {
  _serial = &serial;
  serial.begin(baud, SERIAL_8N1, rxPin, txPin);

  // -------------------------------------------------------------------------
  // 1PPS high-priority interrupt via ESP-IDF GPIO API
  //
  // Arduino's attachInterrupt() runs at a medium priority that can be
  // preempted by the WiFi stack, causing ISR capture latency of 50-100 ms.
  //
  // ESP_INTR_FLAG_LEVEL3 sits above the WiFi interrupt level on ESP32,
  // reducing capture latency to single-digit microseconds.
  // ESP_INTR_FLAG_IRAM ensures the handler stays in IRAM and is never
  // swapped out during flash operations.
  // -------------------------------------------------------------------------
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << ppsPin);
  io_conf.mode         = GPIO_MODE_INPUT;
  io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type    = GPIO_INTR_POSEDGE;
  gpio_config(&io_conf);

  // gpio_install_isr_service may already have been called by the Arduino
  // framework. ESP_ERR_INVALID_STATE is the "already installed" code — safe
  // to ignore.
  esp_err_t err = gpio_install_isr_service(
      ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[GPS] gpio_install_isr_service failed: 0x%x\n", err);
  }

  err = gpio_isr_handler_add((gpio_num_t)ppsPin, GPSManager::onPPS, nullptr);
  if (err != ESP_OK) {
    Serial.printf("[GPS] gpio_isr_handler_add failed: 0x%x\n", err);
  } else {
    Serial.printf("[GPS] PPS ISR registered on GPIO %d (LEVEL3/IRAM)\n", ppsPin);
  }

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

  if (_gps.time.isValid()) {
    snprintf(_state.time, sizeof(_state.time), "%02d:%02d:%02d",
             _gps.time.hour(), _gps.time.minute(), _gps.time.second());
  }
  if (_gps.date.isValid()) {
    snprintf(_state.date, sizeof(_state.date), "%04d-%02d-%02d",
             _gps.date.year(), _gps.date.month(), _gps.date.day());
  }

  // Decoupled Time-Discipline Synchronization Logic:
  // If 1PPS has not fired yet, update ppsValidUnixSec directly.
  // Once 1PPS is active, update ppsValidUnixSec only within the safe age window (300ms - 700ms)
  // after the last 1PPS pulse to prevent serial NMEA latency step-errors.
  if (_gps.time.isValid() && _gps.date.isValid()) {
    uint32_t nmeaUnixSec = convertToUnixSec(
      _gps.date.year(), _gps.date.month(), _gps.date.day(),
      _gps.time.hour(), _gps.time.minute(), _gps.time.second()
    );
    if (nmeaUnixSec > 0) {
      if (!GPSManager::ppsFlag) {
        GPSManager::ppsValidUnixSec = nmeaUnixSec;
      } else {
        uint32_t lastPps = (uint32_t)GPSManager::ppsMicros;
        uint32_t now = micros();
        uint32_t elapsedMs = (now - lastPps) / 1000;
        if (elapsedMs >= 300 && elapsedMs <= 700) {
          if (_lastSyncedNmeaSec != nmeaUnixSec) {
            GPSManager::ppsValidUnixSec = nmeaUnixSec;
            _lastSyncedNmeaSec = nmeaUnixSec;
          }
        }
      }
    }
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
