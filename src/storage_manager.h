#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager {
 public:
  bool begin();
  bool ensureDefaults();
  bool readJson(const char* path, JsonDocument& doc);
  bool writeJson(const char* path, JsonDocument& doc);
  bool exists(const char* path);
  size_t usedBytes() const;
  size_t totalBytes() const;
  String readText(const char* path);

 private:
  bool ensureFile(const char* path, const char* defaultJson);
  bool isValidJsonFile(const char* path);
};

