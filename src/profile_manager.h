#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager;
class Logger;

class ProfileManager {
 public:
  void begin(StorageManager* storage, Logger* logger);
  bool getProfiles(JsonDocument& out);
  bool saveProfiles(JsonDocument& doc);
  String activeProfileId();
  bool activate(const String& id);
  bool profileExists(const String& id);
  bool getSchedule(const String& profileId, JsonDocument& out);
  bool saveSchedule(const String& profileId, JsonArray entries);
  String activeProfileName();

 private:
  StorageManager* storage_ = nullptr;
  Logger* logger_ = nullptr;
};

