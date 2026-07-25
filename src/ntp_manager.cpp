#include "ntp_manager.h"

#include <WiFi.h>
#include <time.h>
#include "app_config.h"
#include "logger.h"
#include "rtc_manager.h"

void NTPManager::begin(RTCManager* rtc, Logger* logger) {
  rtc_ = rtc;
  logger_ = logger;
  configTime(AppConfig::GMT_OFFSET_SEC, AppConfig::DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void NTPManager::loop(bool wifiConnected) {
  if (!wifiConnected) return;
  if (millis() - lastAttemptMs_ >= AppConfig::NTP_RETRY_MS) syncNow();
}

bool NTPManager::syncNow() {
  lastAttemptMs_ = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (logger_) logger_->warn("sync", "NTP skipped: Wi-Fi disconnected");
    return false;
  }
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 250)) {
    if (logger_) logger_->warn("sync", "NTP sync failed");
    return false;
  }
  time_t raw;
  time(&raw);
  if (raw < 1700000000) return false;
  synced_ = true;
  lastSource_ = "NTP";
  lastSyncEpoch_ = static_cast<uint32_t>(raw);
  if (rtc_) rtc_->setEpoch(lastSyncEpoch_, "NTP");
  if (logger_) logger_->info("sync", "NTP sync successful");
  return true;
}

bool NTPManager::synced() const { return synced_; }
String NTPManager::status() const { return synced_ ? "synced" : "not-synced"; }
String NTPManager::lastSyncSource() const { return lastSource_; }
uint32_t NTPManager::lastSyncEpoch() const { return lastSyncEpoch_; }

void NTPManager::noteBrowserSync(uint32_t epoch) {
  synced_ = true;
  lastSource_ = "Browser";
  lastSyncEpoch_ = epoch;
}

