#include "web_server.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "app_config.h"
#include "auth_manager.h"
#include "holiday_manager.h"
#include "logger.h"
#include "ntp_manager.h"
#include "profile_manager.h"
#include "relay_controller.h"
#include "rtc_manager.h"
#include "scheduler.h"
#include "storage_manager.h"
#include "watchdog_manager.h"

namespace {
String jsonString(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  return out;
}

bool copyBody(JsonVariant& json, JsonDocument& doc) {
  if (json.isNull()) return false;
  doc.set(json);
  return true;
}
}

void WebServerManager::begin(StorageManager* storage, AuthManager* auth, ProfileManager* profiles, HolidayManager* holidays,
                             Scheduler* scheduler, RTCManager* rtc, NTPManager* ntp, RelayController* relay,
                             Logger* logger, WatchdogManager* watchdog) {
  storage_ = storage;
  auth_ = auth;
  profiles_ = profiles;
  holidays_ = holidays;
  scheduler_ = scheduler;
  rtc_ = rtc;
  ntp_ = ntp;
  relay_ = relay;
  logger_ = logger;
  watchdog_ = watchdog;
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  setupRoutes();
  server_.begin();
  if (logger_) logger_->info("web", "HTTP server started");
}

void WebServerManager::loop() {
  if (watchdog_) watchdog_->markWeb();
  if (restartAtMs_ && millis() >= restartAtMs_) ESP.restart();
}

void WebServerManager::setupRoutes() {
  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(LittleFS, "/index.html", "text/html"); });
  server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  auto* login = new AsyncCallbackJsonWebHandler("/api/auth/login", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    String token, role;
    bool ok = auth_->login(body["username"] | "", body["password"] | "", token, role);
    if (!ok) return request->send(401, "application/json", "{\"ok\":false,\"error\":\"bad_credentials\"}");
    JsonDocument out;
    out["ok"] = true;
    out["token"] = token;
    out["role"] = role;
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", jsonString(out));
    response->addHeader("Set-Cookie", "SBSESSION=" + token + "; Path=/; SameSite=Strict");
    request->send(response);
  });
  server_.addHandler(login);

  server_.on("/api/auth/logout", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    auth_->logout(request->hasHeader("Authorization") ? request->getHeader("Authorization")->value() : "");
    sendOk(request);
  });

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    JsonDocument out;
    out["deviceName"] = "Smart Bell Controller";
    out["time"] = rtc_->isoTime();
    out["epoch"] = rtc_->utcEpoch();
    out["rtcStatus"] = rtc_->status();
    out["wifiStatus"] = WiFi.status() == WL_CONNECTED ? "connected" : "ap/fallback";
    out["staIp"] = WiFi.localIP().toString();
    out["apIp"] = WiFi.softAPIP().toString();
    out["activeProfileId"] = profiles_->activeProfileId();
    out["activeProfile"] = profiles_->activeProfileName();
    out["ntpStatus"] = ntp_->status();
    out["lastSyncSource"] = ntp_->lastSyncSource();
    out["uptimeSec"] = millis() / 1000;
    out["heapFree"] = ESP.getFreeHeap();
    out["storageUsed"] = storage_->usedBytes();
    out["storageTotal"] = storage_->totalBytes();
    out["relay"] = relay_->state();
    out["watchdog"] = watchdog_->status();
    out["scheduler"] = scheduler_->lastSchedulerStatus();
    JsonDocument next;
    if (scheduler_->nextBell(next)) out["nextBell"].set(next.as<JsonObject>());
    sendJson(request, out);
  });

  server_.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    JsonDocument doc;
    storage_->readJson("/settings.json", doc);
    sendJson(request, doc);
  });

  auto* settingsPut = new AsyncCallbackJsonWebHandler("/api/settings", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Admin)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    storage_->writeJson("/settings.json", body) ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });
  settingsPut->setMethod(HTTP_PUT);
  server_.addHandler(settingsPut);

  server_.on("/api/profiles", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    JsonDocument doc;
    profiles_->getProfiles(doc);
    sendJson(request, doc);
  });

  auto* profilesPut = new AsyncCallbackJsonWebHandler("/api/profiles", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    profiles_->saveProfiles(body) ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });
  profilesPut->setMethod(HTTP_PUT);
  server_.addHandler(profilesPut);

  auto* profileActivate = new AsyncCallbackJsonWebHandler("/api/profiles/activate", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    profiles_->activate(body["id"] | "") ? sendOk(request) : request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_profile\"}");
  });
  server_.addHandler(profileActivate);

  server_.on("/api/schedules", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    String profile = request->hasParam("profile") ? request->getParam("profile")->value() : profiles_->activeProfileId();
    JsonDocument out;
    profiles_->getSchedule(profile, out);
    out["profileId"] = profile;
    sendJson(request, out);
  });

  auto* schedulesPut = new AsyncCallbackJsonWebHandler("/api/schedules", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    String profile = body["profileId"] | profiles_->activeProfileId();
    JsonArray entries = body["entries"].as<JsonArray>();
    String error;
    if (!scheduler_->validateEntries(entries, error)) {
      JsonDocument out;
      out["ok"] = false;
      out["error"] = error;
      return sendJson(request, out, 400);
    }
    profiles_->saveSchedule(profile, entries) ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });
  schedulesPut->setMethod(HTTP_PUT);
  server_.addHandler(schedulesPut);

  server_.on("/api/holidays", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    JsonDocument doc;
    holidays_->getAll(doc);
    sendJson(request, doc);
  });

  auto* holidaysPut = new AsyncCallbackJsonWebHandler("/api/holidays", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    holidays_->save(body) ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });
  holidaysPut->setMethod(HTTP_PUT);
  server_.addHandler(holidaysPut);

  server_.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Admin)) return;
    JsonDocument raw;
    auth_->getUsers(raw);
    JsonDocument out;
    JsonArray users = out["users"].to<JsonArray>();
    for (JsonObject u : raw["users"].as<JsonArray>()) {
      JsonObject v = users.add<JsonObject>();
      v["username"] = u["username"];
      v["role"] = u["role"];
    }
    sendJson(request, out);
  });

  auto* usersPut = new AsyncCallbackJsonWebHandler("/api/users", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Admin)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    JsonDocument existing;
    auth_->getUsers(existing);
    for (JsonObject u : body["users"].as<JsonArray>()) {
      if (u["password"].is<const char*>()) {
        u["passwordHash"] = AuthManager::hashPassword(u["password"].as<String>());
        u.remove("password");
      } else if (!u["passwordHash"].is<const char*>()) {
        String username = u["username"] | "";
        for (JsonObject old : existing["users"].as<JsonArray>()) {
          if (username == old["username"].as<String>()) {
            u["passwordHash"] = old["passwordHash"];
            break;
          }
        }
      }
    }
    auth_->saveUsers(body) ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });
  usersPut->setMethod(HTTP_PUT);
  server_.addHandler(usersPut);

  server_.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Viewer)) return;
    JsonDocument doc;
    logger_->getLogs(doc);
    sendJson(request, doc);
  });

  server_.on("/api/logs", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Admin)) return;
    logger_->clear() ? sendOk(request) : request->send(500, "application/json", "{\"ok\":false}");
  });

  auto* bellPost = new AsyncCallbackJsonWebHandler("/api/bell", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    String action = body["action"] | "ring";
    bool ok = true;
    if (action == "ring" || action == "test") ok = relay_->ring(body["duration"] | 3000, body["repeatCount"] | 1, body["repeatInterval"] | 1000, "manual " + action);
    else if (action == "continuous") relay_->continuousOn("dashboard");
    else if (action == "stop") relay_->stop("dashboard");
    else ok = false;
    ok ? sendOk(request) : request->send(409, "application/json", "{\"ok\":false,\"error\":\"relay_busy_or_invalid\"}");
  });
  server_.addHandler(bellPost);

  auto* browserSync = new AsyncCallbackJsonWebHandler("/api/time/sync-browser", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!auth_->authorize(request, Role::Operator)) return;
    JsonDocument body;
    if (!copyBody(json, body)) return request->send(400, "application/json", "{\"ok\":false}");
    uint32_t epoch = body["epoch"] | 0;
    if (epoch < 1700000000) return request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_epoch\"}");
    if (rtc_->setEpoch(epoch, "Browser")) ntp_->noteBrowserSync(epoch);
    sendOk(request);
  });
  server_.addHandler(browserSync);

  server_.on("/api/time/sync-ntp", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Operator)) return;
    ntp_->syncNow() ? sendOk(request) : request->send(503, "application/json", "{\"ok\":false,\"error\":\"ntp_failed\"}");
  });

  server_.on("/api/device/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!auth_->authorize(request, Role::Admin)) return;
    scheduleRestart();
    sendOk(request);
  });

  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) return request->send(204);
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });
}

void WebServerManager::sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code) {
  request->send(code, "application/json", jsonString(doc));
}

void WebServerManager::sendOk(AsyncWebServerRequest* request) {
  request->send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::scheduleRestart() {
  restartAtMs_ = millis() + 750;
  if (logger_) logger_->warn("system", "Restart requested from dashboard");
}
