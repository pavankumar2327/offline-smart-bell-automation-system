#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include "app_config.h"
#include "auth_manager.h"
#include "holiday_manager.h"
#include "lcd_manager.h"
#include "logger.h"
#include "ntp_manager.h"
#include "profile_manager.h"
#include "relay_controller.h"
#include "rtc_manager.h"
#include "scheduler.h"
#include "storage_manager.h"
#include "watchdog_manager.h"
#include "web_server.h"

StorageManager storage;
Logger logger;
RTCManager rtc;
NTPManager ntp;
RelayController relay;
HolidayManager holidays;
ProfileManager profiles;
Scheduler scheduler;
AuthManager auth;
WatchdogManager watchdog;
WebServerManager web;
LcdManager lcd;

uint32_t lastWifiAttemptMs = 0;
uint32_t lastStatusLogMs = 0;
uint32_t lastRtcDebugMs = 0;

void scanI2CBus() {
  Serial.println("[I2C] Scanning addresses 0x03 to 0x77");
  uint8_t found = 0;
  for (uint8_t address = 0x03; address <= 0x77; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("[I2C] Device found at 0x");
      if (address < 16) Serial.print('0');
      Serial.println(address, HEX);
      found++;
    }
  }
  Serial.print("[I2C] Scan complete. Devices found=");
  Serial.println(found);
  if (found == 0) {
    Serial.println("[I2C] No devices detected. Stop I2C debugging here: check SDA/SCL, power, ground, pullups, and soldering.");
  }
}

void configureWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(AppConfig::HOSTNAME);

  JsonDocument settings;
  storage.readJson("/settings.json", settings);
  const char* apSsid = settings["wifi"]["apSsid"] | AppConfig::AP_SSID;
  const char* apPass = settings["wifi"]["apPassword"] | AppConfig::AP_PASSWORD;
  WiFi.softAP(apSsid, apPass);

  String staSsid = settings["wifi"]["staSsid"] | "";
  String staPass = settings["wifi"]["staPassword"] | "";
  if (!staSsid.isEmpty()) {
    WiFi.begin(staSsid.c_str(), staPass.c_str());
    logger.info("wifi", "Connecting to STA SSID: " + staSsid);
  } else {
    logger.warn("wifi", "STA credentials not configured; AP mode remains active");
  }
}

void maintainWiFi() {
  if (watchdog.status() == "enabled") watchdog.markWifi();
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttemptMs < AppConfig::WIFI_RECONNECT_MS) return;
  lastWifiAttemptMs = millis();
  JsonDocument settings;
  storage.readJson("/settings.json", settings);
  String staSsid = settings["wifi"]["staSsid"] | "";
  String staPass = settings["wifi"]["staPassword"] | "";
  if (!staSsid.isEmpty()) {
    WiFi.disconnect(false);
    WiFi.begin(staSsid.c_str(), staPass.c_str());
    logger.warn("wifi", "Retrying STA connection");
  }
}

void periodicStatusLog() {
  if (millis() - lastStatusLogMs < AppConfig::STATUS_LOG_MS) return;
  lastStatusLogMs = millis();
  logger.info("system", "Uptime=" + String(millis() / 1000) + "s heap=" + String(ESP.getFreeHeap()));
}

void periodicRtcDebug() {
  if (millis() - lastRtcDebugMs < 1000) return;
  lastRtcDebugMs = millis();
  Serial.print("[RTC] 1s time=");
  Serial.print(rtc.isoTime());
  Serial.print(" status=");
  Serial.println(rtc.status());
}

void setup() {
  Serial.begin(AppConfig::SERIAL_BAUD);
  Serial.println();
  Serial.println("===== SMART BELL I2C DIAGNOSTIC BUILD =====");
  Serial.println("[BOOT] Temporary diagnostics enabled");
  storage.begin();
  storage.ensureDefaults();
  logger.begin(&storage);
  logger.info("system", "Boot: reset reason " + String(esp_reset_reason()));

  rtc.begin(&logger);
  scanI2CBus();
  relay.begin(&logger);
  holidays.begin(&storage, &logger);
  profiles.begin(&storage, &logger);
  auth.begin(&storage, &logger);
  scheduler.begin(&profiles, &holidays, &relay, &rtc, &logger);

  configureWiFi();
  ntp.begin(&rtc, &logger);

  JsonDocument settings;
  storage.readJson("/settings.json", settings);
  watchdog.begin(&logger, settings["watchdogEnabled"] | true);
  web.begin(&storage, &auth, &profiles, &holidays, &scheduler, &rtc, &ntp, &relay, &logger, &watchdog);
  lcd.begin(&rtc, &ntp, &profiles, &holidays, &scheduler, &relay);
  Serial.print("[DIAG] RTC detected=");
  Serial.println(rtc.available() ? "YES" : "NO");
  Serial.print("[DIAG] LCD detected=");
  Serial.print(lcd.available() ? "YES" : "NO");
  if (lcd.available()) {
    Serial.print(" address=0x");
    if (lcd.address() < 16) Serial.print('0');
    Serial.print(lcd.address(), HEX);
  }
  Serial.println();
  if (!rtc.available() || !lcd.available()) {
    Serial.println("[DIAG] One or more I2C devices missing. Do not debug scheduler/LCD logic yet.");
    Serial.println("[DIAG] Root cause is likely hardware: SDA/SCL swapped/open, missing common GND, wrong power, weak/no pullups, address mismatch, or solder fault.");
  } else {
    Serial.println("[DIAG] Both RTC and LCD detected. Continue firmware initialization debugging if LCD/time still fail.");
  }
}

void loop() {
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  maintainWiFi();
  ntp.loop(wifiConnected);
  relay.loop();
  scheduler.loop();
  watchdog.markScheduler();
  auth.loop();
  web.loop();
  lcd.loop(wifiConnected, wifiConnected ? WiFi.localIP() : WiFi.softAPIP());
  periodicRtcDebug();
  periodicStatusLog();
  watchdog.loop();
  yield();
}
