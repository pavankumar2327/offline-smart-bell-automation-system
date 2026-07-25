#pragma once

#include <Arduino.h>

class Logger;

class WatchdogManager {
 public:
  void begin(Logger* logger, bool enabled);
  void loop();
  void markScheduler();
  void markWeb();
  void markWifi();
  String status() const;

 private:
  Logger* logger_ = nullptr;
  bool enabled_ = false;
  uint32_t schedulerMs_ = 0;
  uint32_t webMs_ = 0;
  uint32_t wifiMs_ = 0;
};

