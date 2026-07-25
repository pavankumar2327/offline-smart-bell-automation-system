#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager;
class Logger;
class RTCManager;

class HolidayManager {
 public:
  void begin(StorageManager* storage, Logger* logger);
  bool isHoliday(RTCManager& rtc, String* reason = nullptr);
  bool getAll(JsonDocument& out);
  bool save(JsonDocument& doc);

 private:
  StorageManager* storage_ = nullptr;
  Logger* logger_ = nullptr;
};

