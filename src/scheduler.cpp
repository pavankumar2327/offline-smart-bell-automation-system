#include "scheduler.h"

#include "holiday_manager.h"
#include "logger.h"
#include "profile_manager.h"
#include "relay_controller.h"
#include "rtc_manager.h"

void Scheduler::begin(ProfileManager* profiles, HolidayManager* holidays, RelayController* relay, RTCManager* rtc, Logger* logger) {
  profiles_ = profiles;
  holidays_ = holidays;
  relay_ = relay;
  rtc_ = rtc;
  logger_ = logger;
}

void Scheduler::loop() {
  if (millis() - lastCheckMs_ < 500) return;
  lastCheckMs_ = millis();
  if (!profiles_ || !relay_ || !rtc_) return;

  if (millis() - lastDebugMs_ >= 1000) {
    lastDebugMs_ = millis();
    Serial.print("[SCHED] Current RTC time=");
    Serial.print(rtc_->isoTime());
    Serial.print(" hhmm=");
    Serial.print(rtc_->hhmm());
    Serial.print(" activeProfile=");
    Serial.print(profiles_ ? profiles_->activeProfileId() : "-");
    Serial.print(" rtcStatus=");
    Serial.println(rtc_->status());
  }

  String holidayReason;
  if (holidays_ && holidays_->isHoliday(*rtc_, &holidayReason)) {
    status_ = "holiday: " + holidayReason;
    return;
  }
  if (relay_->isBusy()) {
    status_ = "relay busy";
    return;
  }

  String now = rtc_->hhmm();
  String today = rtc_->dateYmd();
  if (today == lastFiredDate_ && now == lastFiredMinute_) return;

  JsonDocument schedule;
  if (!profiles_->getSchedule(profiles_->activeProfileId(), schedule)) {
    status_ = "schedule unavailable";
    return;
  }

  for (JsonObject entry : schedule["entries"].as<JsonArray>()) {
    if (shouldFire(entry, now)) {
      uint32_t duration = entry["duration"] | 3000;
      uint8_t repeatCount = entry["repeatCount"] | 1;
      uint32_t interval = entry["repeatInterval"] | 1000;
      String label = entry["label"] | "Scheduled Bell";
      if (relay_->ring(duration, repeatCount, interval, "schedule: " + label)) {
        lastFiredDate_ = today;
        lastFiredMinute_ = now;
        status_ = "fired " + label;
        if (logger_) logger_->info("schedule", "Executed " + label + " at " + now);
      }
      return;
    }
  }
  status_ = "waiting";
}

bool Scheduler::validateEntries(JsonArray entries, String& error) {
  for (size_t i = 0; i < entries.size(); i++) {
    JsonObject a = entries[i];
    String t = a["time"] | "";
    if (t.length() != 5 || t.charAt(2) != ':' || t.substring(0, 2).toInt() > 23 || t.substring(3).toInt() > 59) {
      error = "Invalid time at row " + String(i + 1);
      return false;
    }
    if ((a["duration"] | 0) <= 0 || (a["repeatCount"] | 0) <= 0) {
      error = "Invalid bell duration or repeat count at row " + String(i + 1);
      return false;
    }
    for (size_t j = i + 1; j < entries.size(); j++) {
      JsonObject b = entries[j];
      if (t == b["time"].as<String>()) {
        error = "Duplicate schedule time: " + t;
        return false;
      }
    }
  }
  return true;
}

bool Scheduler::nextBell(JsonDocument& out) {
  if (!profiles_ || !rtc_) return false;
  JsonDocument schedule;
  if (!profiles_->getSchedule(profiles_->activeProfileId(), schedule)) return false;
  String now = rtc_->hhmm();
  String best = "";
  JsonObject bestEntry;
  for (JsonObject entry : schedule["entries"].as<JsonArray>()) {
    if (!(entry["enabled"] | true)) continue;
    String t = entry["time"] | "";
    if (t >= now && (best.isEmpty() || t < best)) {
      best = t;
      bestEntry = entry;
    }
  }
  if (best.isEmpty()) {
    for (JsonObject entry : schedule["entries"].as<JsonArray>()) {
      if (!(entry["enabled"] | true)) continue;
      String t = entry["time"] | "";
      if (best.isEmpty() || t < best) {
        best = t;
        bestEntry = entry;
      }
    }
  }
  if (best.isEmpty()) return false;
  out["time"] = best;
  out["label"] = bestEntry["label"] | "Bell";
  out["profile"] = profiles_->activeProfileName();
  return true;
}

String Scheduler::lastSchedulerStatus() const { return status_; }

bool Scheduler::shouldFire(JsonObject entry, const String& nowHhmm) {
  if (!(entry["enabled"] | true)) return false;
  return nowHhmm == entry["time"].as<String>();
}
