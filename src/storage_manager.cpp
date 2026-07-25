#include "storage_manager.h"

#include <LittleFS.h>

namespace {
const char* DEFAULT_SETTINGS = R"json({
  "deviceName":"Smart Bell Controller",
  "timezone":"IST",
  "gmtOffsetSec":19800,
  "daylightOffsetSec":0,
  "autoNtp":true,
  "autoBrowserSync":true,
  "browserSyncThresholdSec":30,
  "watchdogEnabled":true,
  "defaultBellDuration":3000,
  "defaultRepeatCount":1,
  "defaultRepeatInterval":1000,
  "wifi":{"staSsid":"","staPassword":"","apSsid":"SmartBell-Setup","apPassword":"smartbell123"}
})json";

const char* DEFAULT_PROFILES = R"json({
  "activeProfileId":"regular",
  "profiles":[
    {"id":"regular","name":"Regular","locked":false},
    {"id":"exam","name":"Exam","locked":false},
    {"id":"half-day","name":"Half-Day","locked":false},
    {"id":"weekend","name":"Weekend","locked":false},
    {"id":"custom","name":"Custom","locked":false}
  ]
})json";

const char* DEFAULT_SCHEDULES = R"json({
  "profiles":{
    "regular":[
      {"time":"09:00","duration":3000,"repeatCount":2,"repeatInterval":1000,"enabled":true,"label":"Morning Bell"},
      {"time":"12:30","duration":3000,"repeatCount":1,"repeatInterval":1000,"enabled":true,"label":"Lunch Bell"},
      {"time":"16:00","duration":3000,"repeatCount":2,"repeatInterval":1000,"enabled":true,"label":"Closing Bell"}
    ],
    "exam":[],
    "half-day":[],
    "weekend":[],
    "custom":[]
  }
})json";

const char* DEFAULT_HOLIDAYS = R"json({
  "single":[],
  "weekly":[0],
  "ranges":[]
})json";

const char* DEFAULT_USERS = R"json({
  "users":[
    {"username":"admin","passwordHash":"240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9","role":"admin"}
  ]
})json";

const char* DEFAULT_LOGS = R"json({"logs":[]})json";
}

bool StorageManager::begin() {
  if (LittleFS.begin(true)) return true;
  return LittleFS.begin(true);
}

bool StorageManager::ensureDefaults() {
  bool ok = true;
  ok &= ensureFile("/settings.json", DEFAULT_SETTINGS);
  ok &= ensureFile("/profiles.json", DEFAULT_PROFILES);
  ok &= ensureFile("/schedules.json", DEFAULT_SCHEDULES);
  ok &= ensureFile("/holidays.json", DEFAULT_HOLIDAYS);
  ok &= ensureFile("/users.json", DEFAULT_USERS);
  ok &= ensureFile("/logs.json", DEFAULT_LOGS);
  return ok;
}

bool StorageManager::exists(const char* path) { return LittleFS.exists(path); }

bool StorageManager::readJson(const char* path, JsonDocument& doc) {
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  return !err;
}

bool StorageManager::writeJson(const char* path, JsonDocument& doc) {
  String tmp = String(path) + ".tmp";
  File file = LittleFS.open(tmp, "w");
  if (!file) return false;
  if (serializeJson(doc, file) == 0) {
    file.close();
    LittleFS.remove(tmp);
    return false;
  }
  file.flush();
  file.close();
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

size_t StorageManager::usedBytes() const { return LittleFS.usedBytes(); }
size_t StorageManager::totalBytes() const { return LittleFS.totalBytes(); }

String StorageManager::readText(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) return "";
  String out = file.readString();
  file.close();
  return out;
}

bool StorageManager::ensureFile(const char* path, const char* defaultJson) {
  if (LittleFS.exists(path) && isValidJsonFile(path)) return true;
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  file.print(defaultJson);
  file.close();
  return true;
}

bool StorageManager::isValidJsonFile(const char* path) {
  JsonDocument doc;
  return readJson(path, doc);
}
