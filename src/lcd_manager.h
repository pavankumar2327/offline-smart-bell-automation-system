#pragma once

#include <Arduino.h>

class HolidayManager;
class NTPManager;
class ProfileManager;
class RelayController;
class RTCManager;
class Scheduler;

class LcdManager {
 public:
  void begin(RTCManager* rtc, NTPManager* ntp, ProfileManager* profiles, HolidayManager* holidays,
             Scheduler* scheduler, RelayController* relay);
  void loop(bool wifiConnected, const IPAddress& dashboardIp);
  bool available() const;
  uint8_t address() const;

 private:
  enum class Screen : uint8_t { Home, NextBell, WifiSync, DashboardIp, Holiday };
  enum class Priority : uint8_t { Normal, Holiday, BellActive, WifiLost, Critical };

  bool detectDisplay();
  void initializeDisplay();
  void write4(uint8_t value);
  void pulseEnable(uint8_t value);
  void command(uint8_t value);
  void writeChar(char value);
  void setCursor(uint8_t col, uint8_t row);
  void backlight(bool on);
  void show(const String& line1, const String& line2, bool force = false);
  void showBoot();
  void showOverride(Priority priority, bool wifiConnected);
  void showNormal(bool wifiConnected, const IPAddress& dashboardIp);
  void advanceNormalScreen();
  String fit(String value) const;
  String timeProfileLine();

  RTCManager* rtc_ = nullptr;
  NTPManager* ntp_ = nullptr;
  ProfileManager* profiles_ = nullptr;
  HolidayManager* holidays_ = nullptr;
  Scheduler* scheduler_ = nullptr;
  RelayController* relay_ = nullptr;

  bool available_ = false;
  bool backlightOn_ = true;
  uint8_t address_ = 0;
  Screen screen_ = Screen::Home;
  uint32_t bootUntilMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
  uint32_t lastRotateMs_ = 0;
  uint32_t wifiLostUntilMs_ = 0;
  bool lastWifiConnected_ = false;
  String lastLine1_;
  String lastLine2_;
};
