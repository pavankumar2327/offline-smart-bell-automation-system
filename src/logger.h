#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager;

class Logger {
 public:
  void begin(StorageManager* storage);
  void info(const String& category, const String& message);
  void warn(const String& category, const String& message);
  void error(const String& category, const String& message);
  bool getLogs(JsonDocument& out);
  bool clear();

 private:
  void append(const String& level, const String& category, const String& message);
  StorageManager* storage_ = nullptr;
};

