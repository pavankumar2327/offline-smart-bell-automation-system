#include "profile_manager.h"

#include "logger.h"
#include "storage_manager.h"

void ProfileManager::begin(StorageManager* storage, Logger* logger) {
  storage_ = storage;
  logger_ = logger;
}

bool ProfileManager::getProfiles(JsonDocument& out) {
  return storage_ && storage_->readJson("/profiles.json", out);
}

bool ProfileManager::saveProfiles(JsonDocument& doc) {
  bool ok = storage_ && storage_->writeJson("/profiles.json", doc);
  if (ok && logger_) logger_->info("profile", "Profiles updated");
  return ok;
}

String ProfileManager::activeProfileId() {
  JsonDocument doc;
  if (!getProfiles(doc)) return "regular";
  return doc["activeProfileId"] | "regular";
}

bool ProfileManager::activate(const String& id) {
  JsonDocument doc;
  if (!getProfiles(doc) || !profileExists(id)) return false;
  doc["activeProfileId"] = id;
  bool ok = saveProfiles(doc);
  if (ok && logger_) logger_->info("profile", "Activated " + id);
  return ok;
}

bool ProfileManager::profileExists(const String& id) {
  JsonDocument doc;
  if (!getProfiles(doc)) return false;
  for (JsonObject p : doc["profiles"].as<JsonArray>()) {
    if (id == p["id"].as<String>()) return true;
  }
  return false;
}

bool ProfileManager::getSchedule(const String& profileId, JsonDocument& out) {
  JsonDocument all;
  if (!storage_ || !storage_->readJson("/schedules.json", all)) return false;
  JsonArray source = all["profiles"][profileId].as<JsonArray>();
  out["entries"].to<JsonArray>();
  if (!source.isNull()) out["entries"].set(source);
  return true;
}

bool ProfileManager::saveSchedule(const String& profileId, JsonArray entries) {
  JsonDocument all;
  if (!storage_ || !storage_->readJson("/schedules.json", all)) return false;
  all["profiles"][profileId].set(entries);
  bool ok = storage_->writeJson("/schedules.json", all);
  if (ok && logger_) logger_->info("schedule", "Schedule saved for " + profileId);
  return ok;
}

String ProfileManager::activeProfileName() {
  JsonDocument doc;
  if (!getProfiles(doc)) return activeProfileId();
  String id = doc["activeProfileId"] | "regular";
  for (JsonObject p : doc["profiles"].as<JsonArray>()) {
    if (id == p["id"].as<String>()) return p["name"] | id;
  }
  return id;
}

