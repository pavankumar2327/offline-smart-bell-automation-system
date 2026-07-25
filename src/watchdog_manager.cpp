#include "watchdog_manager.h"

#include <esp_task_wdt.h>
#include "app_config.h"
#include "logger.h"

void WatchdogManager::begin(Logger* logger, bool enabled) {
  logger_ = logger;
  enabled_ = enabled;
  schedulerMs_ = webMs_ = wifiMs_ = millis();
  if (!enabled_) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t config = {
      .timeout_ms = AppConfig::WATCHDOG_TIMEOUT_SEC * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&config);
#else
  esp_task_wdt_init(AppConfig::WATCHDOG_TIMEOUT_SEC, true);
#endif
  esp_task_wdt_add(nullptr);
  if (logger_) logger_->info("watchdog", "Watchdog enabled");
}

void WatchdogManager::loop() {
  if (!enabled_) return;
  uint32_t now = millis();
  bool healthy = now - schedulerMs_ < 10000 && now - webMs_ < 10000 && now - wifiMs_ < 10000;
  if (healthy) esp_task_wdt_reset();
}

void WatchdogManager::markScheduler() { schedulerMs_ = millis(); }
void WatchdogManager::markWeb() { webMs_ = millis(); }
void WatchdogManager::markWifi() { wifiMs_ = millis(); }
String WatchdogManager::status() const { return enabled_ ? "enabled" : "disabled"; }

