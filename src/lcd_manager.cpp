#include "lcd_manager.h"

#include <ArduinoJson.h>
#include <Wire.h>
#include <WiFi.h>
#include "app_config.h"
#include "holiday_manager.h"
#include "ntp_manager.h"
#include "profile_manager.h"
#include "relay_controller.h"
#include "rtc_manager.h"
#include "scheduler.h"

namespace {
constexpr uint8_t LCD_BACKLIGHT = 0x08;
constexpr uint8_t LCD_ENABLE = 0x04;
constexpr uint8_t LCD_RS = 0x01;
constexpr uint8_t LCD_FUNCTION_SET = 0x28;
constexpr uint8_t LCD_DISPLAY_ON = 0x0C;
constexpr uint8_t LCD_CLEAR = 0x01;
constexpr uint8_t LCD_ENTRY_MODE = 0x06;
constexpr uint8_t LCD_SET_DDRAM = 0x80;
}

void LcdManager::begin(RTCManager* rtc, NTPManager* ntp, ProfileManager* profiles, HolidayManager* holidays,
                       Scheduler* scheduler, RelayController* relay) {
  Serial.println("[LCD] Initialization started");
  rtc_ = rtc;
  ntp_ = ntp;
  profiles_ = profiles;
  holidays_ = holidays;
  scheduler_ = scheduler;
  relay_ = relay;
  available_ = detectDisplay();
  if (!available_) {
    Serial.println("[LCD] I2C LCD not detected at 0x27 or 0x3F");
    Serial.println("[LCD] Likely issue: LCD SDA/SCL wiring, power, ground, backpack address, or bad solder joint");
    return;
  }
  Serial.print("[LCD] I2C LCD detected at 0x");
  if (address_ < 16) Serial.print('0');
  Serial.println(address_, HEX);
  initializeDisplay();
  Serial.println("[LCD] LCD initialization completed");
  bootUntilMs_ = millis() + AppConfig::LCD_BOOT_SCREEN_MS;
  showBoot();
}

void LcdManager::loop(bool wifiConnected, const IPAddress& dashboardIp) {
  if (!available_) return;
  const uint32_t now = millis();
  if (now - lastRefreshMs_ < AppConfig::LCD_REFRESH_MS) return;
  lastRefreshMs_ = now;

  if (!wifiConnected && lastWifiConnected_) {
    wifiLostUntilMs_ = now + 8000UL;
  }
  lastWifiConnected_ = wifiConnected;

  if (now < bootUntilMs_) {
    showBoot();
    return;
  }

  Priority priority = Priority::Normal;
  String holidayReason;
  if (rtc_ && !rtc_->available()) priority = Priority::Critical;
  else if (relay_ && relay_->isActive()) priority = Priority::BellActive;
  else if (holidays_ && rtc_ && holidays_->isHoliday(*rtc_, &holidayReason)) priority = Priority::Holiday;
  else if (!wifiConnected && now < wifiLostUntilMs_) priority = Priority::WifiLost;

  if (priority == Priority::Normal) showNormal(wifiConnected, dashboardIp);
  else showOverride(priority, wifiConnected);
}

bool LcdManager::available() const { return available_; }

uint8_t LcdManager::address() const { return address_; }

bool LcdManager::detectDisplay() {
  const uint8_t candidates[] = {AppConfig::LCD_I2C_ADDR_PRIMARY, AppConfig::LCD_I2C_ADDR_FALLBACK};
  for (uint8_t candidate : candidates) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address_ = candidate;
      return true;
    }
  }
  return false;
}

void LcdManager::initializeDisplay() {
  delayMicroseconds(50000);
  write4(0x30);
  delayMicroseconds(4500);
  write4(0x30);
  delayMicroseconds(4500);
  write4(0x30);
  delayMicroseconds(150);
  write4(0x20);
  command(LCD_FUNCTION_SET);
  command(LCD_DISPLAY_ON);
  command(LCD_CLEAR);
  delayMicroseconds(2000);
  command(LCD_ENTRY_MODE);
  backlight(true);
}

void LcdManager::write4(uint8_t value) {
  Wire.beginTransmission(address_);
  Wire.write(value | (backlightOn_ ? LCD_BACKLIGHT : 0));
  Wire.endTransmission();
  pulseEnable(value);
}

void LcdManager::pulseEnable(uint8_t value) {
  uint8_t data = value | (backlightOn_ ? LCD_BACKLIGHT : 0);
  Wire.beginTransmission(address_);
  Wire.write(data | LCD_ENABLE);
  Wire.endTransmission();
  delayMicroseconds(1);
  Wire.beginTransmission(address_);
  Wire.write(data & ~LCD_ENABLE);
  Wire.endTransmission();
  delayMicroseconds(50);
}

void LcdManager::command(uint8_t value) {
  write4(value & 0xF0);
  write4((value << 4) & 0xF0);
}

void LcdManager::writeChar(char value) {
  write4((value & 0xF0) | LCD_RS);
  write4(((value << 4) & 0xF0) | LCD_RS);
}

void LcdManager::setCursor(uint8_t col, uint8_t row) {
  static const uint8_t rowOffsets[] = {0x00, 0x40};
  command(LCD_SET_DDRAM | (col + rowOffsets[row % AppConfig::LCD_ROWS]));
}

void LcdManager::backlight(bool on) {
  backlightOn_ = on;
  Wire.beginTransmission(address_);
  Wire.write(backlightOn_ ? LCD_BACKLIGHT : 0);
  Wire.endTransmission();
}

void LcdManager::show(const String& line1, const String& line2, bool force) {
  String l1 = fit(line1);
  String l2 = fit(line2);
  if (!force && l1 == lastLine1_ && l2 == lastLine2_) return;

  if (force || l1 != lastLine1_) {
    setCursor(0, 0);
    for (uint8_t i = 0; i < AppConfig::LCD_COLUMNS; i++) writeChar(l1[i]);
    lastLine1_ = l1;
  }
  if (force || l2 != lastLine2_) {
    setCursor(0, 1);
    for (uint8_t i = 0; i < AppConfig::LCD_COLUMNS; i++) writeChar(l2[i]);
    lastLine2_ = l2;
  }
}

void LcdManager::showBoot() {
  show("SMART BELL v1", "Initializing...");
}

void LcdManager::showOverride(Priority priority, bool wifiConnected) {
  (void)wifiConnected;
  if (priority == Priority::Critical) {
    show("RTC ERROR", "Check Module");
    return;
  }
  if (priority == Priority::BellActive) {
    uint16_t seconds = relay_ ? relay_->activeRemainingSeconds() : 0;
    show("*** BELL ON ***", seconds ? "Duration: " + String(seconds) + "s" : "Duration: ON");
    return;
  }
  if (priority == Priority::Holiday) {
    show("HOLIDAY MODE", "No Bell Today");
    return;
  }
  if (priority == Priority::WifiLost) {
    show("WiFi Lost", "RTC Mode Active");
  }
}

void LcdManager::showNormal(bool wifiConnected, const IPAddress& dashboardIp) {
  if (millis() - lastRotateMs_ >= AppConfig::LCD_ROTATION_MS) advanceNormalScreen();

  if (screen_ == Screen::Home) {
    show("SMART BELL SYS", timeProfileLine());
    return;
  }
  if (screen_ == Screen::NextBell) {
    JsonDocument next;
    if (scheduler_ && scheduler_->nextBell(next)) show("Next Bell:", String(next["time"] | "--:--") + " " + String(next["label"] | "Bell"));
    else show("Next Bell:", "No Bell Set");
    return;
  }
  if (screen_ == Screen::WifiSync) {
    show(wifiConnected ? "WiFi Connected" : "WiFi Offline", (ntp_ && ntp_->synced()) ? "NTP Synced" : "RTC Active");
    return;
  }
  if (screen_ == Screen::DashboardIp) {
    show("Dashboard IP", dashboardIp.toString());
    return;
  }
  if (screen_ == Screen::Holiday) {
    String reason;
    if (holidays_ && rtc_ && holidays_->isHoliday(*rtc_, &reason)) show("HOLIDAY MODE", "No Bell Today");
    else advanceNormalScreen();
  }
}

void LcdManager::advanceNormalScreen() {
  lastRotateMs_ = millis();
  screen_ = static_cast<Screen>((static_cast<uint8_t>(screen_) + 1) % 4);
}

String LcdManager::fit(String value) const {
  value.replace('\n', ' ');
  value.replace('\r', ' ');
  value.toUpperCase();
  if (value.length() > AppConfig::LCD_COLUMNS) value = value.substring(0, AppConfig::LCD_COLUMNS);
  while (value.length() < AppConfig::LCD_COLUMNS) value += ' ';
  return value;
}

String LcdManager::timeProfileLine() {
  String profile = profiles_ ? profiles_->activeProfileName() : "PROFILE";
  profile.toUpperCase();
  if (profile.length() > 12) profile = profile.substring(0, 12);
  return (rtc_ ? rtc_->hhmm() : "--:--") + " " + profile;
}
