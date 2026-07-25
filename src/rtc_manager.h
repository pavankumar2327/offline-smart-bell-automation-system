#pragma once

#include <Arduino.h>
#include <RTClib.h>

class Logger;

class RTCManager {
 public:
  void begin(Logger* logger);
  bool available() const;
  DateTime now();
  bool setEpoch(uint32_t epoch, const String& source);
  uint32_t epoch();
  uint32_t utcEpoch();
  String isoTime();
  String hhmm();
  String dateYmd();
  uint8_t dayOfWeek();
  String status() const;

 private:
  RTC_DS3231 rtc_;
  Logger* logger_ = nullptr;
  bool available_ = false;
};
