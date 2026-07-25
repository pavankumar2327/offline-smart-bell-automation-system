#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class ProfileManager;
class HolidayManager;
class RelayController;
class RTCManager;
class Logger;

class Scheduler {
 public:
  void begin(ProfileManager* profiles, HolidayManager* holidays, RelayController* relay, RTCManager* rtc, Logger* logger);
  void loop();
  bool validateEntries(JsonArray entries, String& error);
  bool nextBell(JsonDocument& out);
  String lastSchedulerStatus() const;

 private:
  bool shouldFire(JsonObject entry, const String& nowHhmm);
  ProfileManager* profiles_ = nullptr;
  HolidayManager* holidays_ = nullptr;
  RelayController* relay_ = nullptr;
  RTCManager* rtc_ = nullptr;
  Logger* logger_ = nullptr;
  uint32_t lastCheckMs_ = 0;
  uint32_t lastDebugMs_ = 0;
  String lastFiredDate_;
  String lastFiredMinute_;
  String status_ = "idle";
};
