#pragma once

#include <Arduino.h>

namespace AppConfig {
static constexpr uint8_t RELAY_PIN = 16;  // RX2 / GPIO16: SSR control through ULN2003
static constexpr bool RELAY_ACTIVE_HIGH = true;

static constexpr uint8_t I2C_SDA_PIN = 21;
static constexpr uint8_t I2C_SCL_PIN = 22;
static constexpr uint8_t LCD_I2C_ADDR_PRIMARY = 0x27;
static constexpr uint8_t LCD_I2C_ADDR_FALLBACK = 0x3F;
static constexpr uint8_t LCD_COLUMNS = 20;
static constexpr uint8_t LCD_ROWS = 2;
static constexpr uint32_t LCD_ROTATION_MS = 4000;
static constexpr uint32_t LCD_BOOT_SCREEN_MS = 2000;
static constexpr uint32_t LCD_REFRESH_MS = 500;

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t LOOP_YIELD_MS = 0;

static constexpr const char* AP_SSID = "SmartBell-Setup";
static constexpr const char* AP_PASSWORD = "smartbell123";

static constexpr const char* HOSTNAME = "smart-bell";
static constexpr long GMT_OFFSET_SEC = 19800;
static constexpr int DAYLIGHT_OFFSET_SEC = 0;

static constexpr uint32_t NTP_RETRY_MS = 60000;
static constexpr uint32_t WIFI_RECONNECT_MS = 15000;
static constexpr uint32_t STATUS_LOG_MS = 300000;
static constexpr uint32_t SESSION_TIMEOUT_MS = 8UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t BROWSER_SYNC_THRESHOLD_SEC = 30;
static constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 15;

static constexpr size_t MAX_LOGS = 180;
static constexpr size_t MAX_SESSIONS = 8;
}
