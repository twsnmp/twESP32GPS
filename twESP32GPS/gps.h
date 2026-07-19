#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"  // for MAX_SATELLITES

// =============================================================================
//  GPS State Structures (mirrors twgps GPSState / SatelliteDetail)
// =============================================================================

struct SatelliteDetail {
  int  prn;
  int  elevation;
  int  azimuth;
  int  snr;
  char system[12]; // "GPS","GLONASS","Galileo","BeiDou","QZSS","SBAS"
};

struct GPSState {
  bool             hasFix;
  char             time[16];       // "HH:MM:SS"
  char             date[16];       // "YYYY-MM-DD"
  double           latitude;
  double           longitude;
  double           altitude;
  double           speedKmh;
  int              numSatellites;  // tracked (from GGA)
  int              satelliteCount; // in view (from GSV)
  SatelliteDetail  satellites[MAX_SATELLITES];
  uint8_t          stratum;       // 1 = Fix, 16 = No Fix
  uint32_t         uptimeSeconds;
  uint32_t         ntpClients;
};

// =============================================================================
//  GPS Manager
// =============================================================================

class GPSManager {
public:
  GPSManager();

  void begin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin, int ppsPin);
  void update();           // Call from loop() — feeds GPS serial into TinyGPS++

  // 1PPS ISR — must be declared IRAM_ATTR, called via attachInterrupt
  static void IRAM_ATTR onPPS();

  // Thread-safe state access (disables interrupts briefly)
  GPSState getState() const;

  // For NTP: returns micros() captured at last 1PPS rising edge
  static volatile uint64_t ppsMicros;
  static volatile bool     ppsFlag;

private:
  TinyGPSPlus  _gps;
  HardwareSerial* _serial = nullptr;

  // Internal mutable state (updated from ISR-safe context)
  GPSState     _state;

  // Satellite tracking map (system x prn -> detail), rebuilt per GSV set
  // We use a flat array in _state.satellites for JSON output
  // GSV parsing state
  struct GSVSession {
    char     system[12];
    int      totalMessages;
    int      receivedMessages;
    SatelliteDetail buf[48];
    int      count;
  } _gsvSessions[6]; // GPS, GLONASS, Galileo, BeiDou, QZSS, SBAS
  int _gsvSessionCount = 0;

  void parseNMEASentence(const char* sentence);
  void parseGSV(const char* sentence);
  void parseRMC(const char* sentence);
  void parseGGA(const char* sentence);
  void mergeSatellites();

  // Identify which constellation system a GSV talker ID belongs to
  static const char* systemFromTalker(const char* talker);
  // Classify PRN override for QZSS / SBAS
  static const char* classifyPRN(const char* baseSys, int prn);
};
