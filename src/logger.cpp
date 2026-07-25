#include "logger.h"

#include "app_config.h"
#include "storage_manager.h"

void Logger::begin(StorageManager* storage) { storage_ = storage; }

void Logger::info(const String& category, const String& message) { append("info", category, message); }
void Logger::warn(const String& category, const String& message) { append("warn", category, message); }
void Logger::error(const String& category, const String& message) { append("error", category, message); }

bool Logger::getLogs(JsonDocument& out) {
  if (!storage_) return false;
  return storage_->readJson("/logs.json", out);
}

bool Logger::clear() {
  if (!storage_) return false;
  JsonDocument doc;
  doc["logs"].to<JsonArray>();
  return storage_->writeJson("/logs.json", doc);
}

void Logger::append(const String& level, const String& category, const String& message) {
  if (!storage_) return;
  JsonDocument doc;
  if (!storage_->readJson("/logs.json", doc)) doc["logs"].to<JsonArray>();
  JsonArray logs = doc["logs"].as<JsonArray>();
  while (logs.size() >= AppConfig::MAX_LOGS) logs.remove(0);
  JsonObject row = logs.add<JsonObject>();
  row["ms"] = millis();
  row["level"] = level;
  row["category"] = category;
  row["message"] = message;
  storage_->writeJson("/logs.json", doc);
}

