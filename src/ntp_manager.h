#pragma once

#include <Arduino.h>

class RTCManager;
class Logger;

class NTPManager {
 public:
  void begin(RTCManager* rtc, Logger* logger);
  void loop(bool wifiConnected);
  bool syncNow();
  bool synced() const;
  String status() const;
  String lastSyncSource() const;
  uint32_t lastSyncEpoch() const;
  void noteBrowserSync(uint32_t epoch);

 private:
  RTCManager* rtc_ = nullptr;
  Logger* logger_ = nullptr;
  bool synced_ = false;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastSyncEpoch_ = 0;
  String lastSource_ = "RTC";
};

