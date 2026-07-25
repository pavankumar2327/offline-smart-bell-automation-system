#include "auth_manager.h"

#include <mbedtls/sha256.h>
#include "app_config.h"
#include "logger.h"
#include "storage_manager.h"

void AuthManager::begin(StorageManager* storage, Logger* logger) {
  storage_ = storage;
  logger_ = logger;
}

void AuthManager::loop() {
  uint32_t now = millis();
  for (auto& s : sessions_) {
    if (s.active && now - s.lastSeenMs > AppConfig::SESSION_TIMEOUT_MS) s.active = false;
  }
}

bool AuthManager::login(const String& username, const String& password, String& token, String& role) {
  JsonDocument doc;
  if (!storage_ || !storage_->readJson("/users.json", doc)) return false;
  String hash = hashPassword(password);
  for (JsonObject u : doc["users"].as<JsonArray>()) {
    if (username == u["username"].as<String>() && hash == u["passwordHash"].as<String>()) {
      Session* slot = nullptr;
      for (auto& s : sessions_) {
        if (!s.active) {
          slot = &s;
          break;
        }
      }
      if (!slot) slot = &sessions_[0];
      uint64_t mac = ESP.getEfuseMac();
      token = String(random(0x7fffffff), HEX) + String(micros(), HEX) + String((uint32_t)mac, HEX) + String((uint32_t)(mac >> 32), HEX);
      role = u["role"] | "viewer";
      slot->active = true;
      slot->token = token;
      slot->username = username;
      slot->role = role;
      slot->lastSeenMs = millis();
      if (logger_) logger_->info("auth", "Login: " + username);
      return true;
    }
  }
  if (logger_) logger_->warn("auth", "Failed login: " + username);
  return false;
}

void AuthManager::logout(const String& token) {
  String normalized = token;
  normalized.replace("Bearer ", "");
  Session* s = findSession(normalized);
  if (s) s->active = false;
}

bool AuthManager::authorize(AsyncWebServerRequest* request, Role minimumRole) {
  String token = tokenFromRequest(request);
  Session* s = findSession(token);
  if (!s || roleValue(s->role) < minimumRole) {
    request->send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
  }
  s->lastSeenMs = millis();
  return true;
}

bool AuthManager::getUsers(JsonDocument& out) {
  return storage_ && storage_->readJson("/users.json", out);
}

bool AuthManager::saveUsers(JsonDocument& doc) {
  bool ok = storage_ && storage_->writeJson("/users.json", doc);
  if (ok && logger_) logger_->info("auth", "Users updated");
  return ok;
}

String AuthManager::currentUser(AsyncWebServerRequest* request) {
  Session* s = findSession(tokenFromRequest(request));
  return s ? s->username : "";
}

String AuthManager::hashPassword(const String& password) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(password.c_str()), password.length());
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  char out[65];
  for (uint8_t i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", hash[i]);
  out[64] = 0;
  return String(out);
}

String AuthManager::tokenFromRequest(AsyncWebServerRequest* request) {
  if (request->hasHeader("Authorization")) {
    String h = request->getHeader("Authorization")->value();
    h.replace("Bearer ", "");
    return h;
  }
  if (request->hasHeader("Cookie")) {
    String cookie = request->getHeader("Cookie")->value();
    int p = cookie.indexOf("SBSESSION=");
    if (p >= 0) {
      int start = p + 10;
      int end = cookie.indexOf(';', start);
      return end > start ? cookie.substring(start, end) : cookie.substring(start);
    }
  }
  return "";
}

AuthManager::Session* AuthManager::findSession(const String& token) {
  if (token.isEmpty()) return nullptr;
  for (auto& s : sessions_) {
    if (s.active && s.token == token) return &s;
  }
  return nullptr;
}

Role AuthManager::roleValue(const String& role) {
  if (role == "admin") return Role::Admin;
  if (role == "operator") return Role::Operator;
  return Role::Viewer;
}
