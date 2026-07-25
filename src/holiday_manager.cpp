#include "holiday_manager.h"

#include "logger.h"
#include "rtc_manager.h"
#include "storage_manager.h"

void HolidayManager::begin(StorageManager* storage, Logger* logger) {
  storage_ = storage;
  logger_ = logger;
}

bool HolidayManager::isHoliday(RTCManager& rtc, String* reason) {
  JsonDocument doc;
  if (!storage_ || !storage_->readJson("/holidays.json", doc)) return false;
  const String today = rtc.dateYmd();
  const uint8_t dow = rtc.dayOfWeek();

  for (JsonObject h : doc["single"].as<JsonArray>()) {
    String date = h["date"] | "";
    bool repeatYearly = h["repeatYearly"] | false;
    bool exactMatch = today == date;
    bool yearlyMatch = repeatYearly && date.length() == 10 && today.substring(5) == date.substring(5);
    if (exactMatch || yearlyMatch) {
      if (reason) *reason = h["label"] | "Single holiday";
      return true;
    }
  }
  for (JsonVariant v : doc["weekly"].as<JsonArray>()) {
    if (dow == v.as<uint8_t>()) {
      if (reason) *reason = "Weekly holiday";
      return true;
    }
  }
  for (JsonObject r : doc["ranges"].as<JsonArray>()) {
    String start = r["start"] | "";
    String end = r["end"] | "";
    if (start <= today && today <= end) {
      if (reason) *reason = r["label"] | "Vacation";
      return true;
    }
  }
  return false;
}

bool HolidayManager::getAll(JsonDocument& out) {
  return storage_ && storage_->readJson("/holidays.json", out);
}

bool HolidayManager::save(JsonDocument& doc) {
  bool ok = storage_ && storage_->writeJson("/holidays.json", doc);
  if (ok && logger_) logger_->info("holiday", "Holiday configuration updated");
  return ok;
}
