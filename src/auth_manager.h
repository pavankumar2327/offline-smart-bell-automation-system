#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

class StorageManager;
class Logger;

enum class Role : uint8_t { Viewer = 0, Operator = 1, Admin = 2 };

class AuthManager {
 public:
  void begin(StorageManager* storage, Logger* logger);
  void loop();
  bool login(const String& username, const String& password, String& token, String& role);
  void logout(const String& token);
  bool authorize(AsyncWebServerRequest* request, Role minimumRole);
  bool getUsers(JsonDocument& out);
  bool saveUsers(JsonDocument& doc);
  String currentUser(AsyncWebServerRequest* request);
  static String hashPassword(const String& password);

 private:
  struct Session {
    bool active = false;
    String token;
    String username;
    String role;
    uint32_t lastSeenMs = 0;
  };

  String tokenFromRequest(AsyncWebServerRequest* request);
  Role roleValue(const String& role);
  Session* findSession(const String& token);

  StorageManager* storage_ = nullptr;
  Logger* logger_ = nullptr;
  Session sessions_[8];
};

