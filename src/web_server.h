#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class StorageManager;
class AuthManager;
class ProfileManager;
class HolidayManager;
class Scheduler;
class RTCManager;
class NTPManager;
class RelayController;
class Logger;
class WatchdogManager;

class WebServerManager {
 public:
  void begin(StorageManager* storage, AuthManager* auth, ProfileManager* profiles, HolidayManager* holidays,
             Scheduler* scheduler, RTCManager* rtc, NTPManager* ntp, RelayController* relay,
             Logger* logger, WatchdogManager* watchdog);
  void loop();

 private:
  void setupRoutes();
  void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code = 200);
  void sendOk(AsyncWebServerRequest* request);
  void scheduleRestart();

  AsyncWebServer server_{80};
  StorageManager* storage_ = nullptr;
  AuthManager* auth_ = nullptr;
  ProfileManager* profiles_ = nullptr;
  HolidayManager* holidays_ = nullptr;
  Scheduler* scheduler_ = nullptr;
  RTCManager* rtc_ = nullptr;
  NTPManager* ntp_ = nullptr;
  RelayController* relay_ = nullptr;
  Logger* logger_ = nullptr;
  WatchdogManager* watchdog_ = nullptr;
  uint32_t restartAtMs_ = 0;
};

