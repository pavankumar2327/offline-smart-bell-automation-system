#include "rtc_manager.h"

#include <Wire.h>
#include "app_config.h"
#include "logger.h"

void RTCManager::begin(Logger* logger) {
  logger_ = logger;
  Serial.println("[I2C] Initializing Wire bus");
  Serial.print("[I2C] SDA GPIO=");
  Serial.print(AppConfig::I2C_SDA_PIN);
  Serial.print(" SCL GPIO=");
  Serial.println(AppConfig::I2C_SCL_PIN);
  Wire.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  Serial.println("[I2C] Wire.begin() completed");
  Serial.println("[RTC] DS3231 initialization started");
  available_ = rtc_.begin();
  if (!available_) {
    Serial.println("[RTC] DS3231 not detected");
    Serial.println("[RTC] Likely issue: RTC SDA/SCL wiring, power, ground, address 0x68 missing, or module fault");
    if (logger_) logger_->error("rtc", "DS3231 not detected");
    return;
  }
  Serial.println("[RTC] DS3231 detected at expected address 0x68");
  if (rtc_.lostPower()) {
    rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("[RTC] RTC lost power; initialized from firmware build time");
    if (logger_) logger_->warn("rtc", "RTC lost power, initialized from firmware build time");
  }
  Serial.print("[RTC] Current RTC time: ");
  Serial.println(isoTime());
}

bool RTCManager::available() const { return available_; }

DateTime RTCManager::now() {
  if (available_) return rtc_.now();
  return DateTime(2000, 1, 1, 0, 0, 0);
}

bool RTCManager::setEpoch(uint32_t epoch, const String& source) {
  if (!available_) return false;
  rtc_.adjust(DateTime(epoch + AppConfig::GMT_OFFSET_SEC + AppConfig::DAYLIGHT_OFFSET_SEC));
  if (logger_) logger_->info("sync", "RTC updated from " + source);
  return true;
}

uint32_t RTCManager::epoch() { return now().unixtime(); }

uint32_t RTCManager::utcEpoch() {
  uint32_t localEpoch = epoch();
  uint32_t offset = AppConfig::GMT_OFFSET_SEC + AppConfig::DAYLIGHT_OFFSET_SEC;
  return localEpoch > offset ? localEpoch - offset : localEpoch;
}

String RTCManager::isoTime() {
  DateTime t = now();
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
  return String(buf);
}

String RTCManager::hhmm() {
  DateTime t = now();
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.hour(), t.minute());
  return String(buf);
}

String RTCManager::dateYmd() {
  DateTime t = now();
  char buf[11];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.year(), t.month(), t.day());
  return String(buf);
}

uint8_t RTCManager::dayOfWeek() { return now().dayOfTheWeek(); }

String RTCManager::status() const { return available_ ? "ok" : "failed"; }
