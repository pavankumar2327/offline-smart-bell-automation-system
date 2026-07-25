 # Offline Smart Bell Automation System

Embedded Automation System using ESP32 for Educational Institutions

---

An embedded automation system that automates bell scheduling for schools and colleges using ESP32, DS3231 RTC, LittleFS, and a secure web dashboard.

The system works completely offline while supporting browser-based configuration, holiday management, authentication, watchdog recovery, and automatic RTC synchronization.

## Highlights

✔ Completely Offline Operation

✔ RTC Based Scheduling

✔ Browser Dashboard

✔ Role Based Authentication

✔ Holiday Management

✔ Automatic RTC Synchronization

✔ LittleFS JSON Storage

✔ Watchdog Recovery

✔ Modular Firmware Architecture

✔ SSR Relay Control

## Hardware

• ESP32 Dev Board

• DS3231 RTC

• SSR Relay Module

• LCD Display

• Power Supply

• Bell Output
## Software

PlatformIO

Arduino Framework

C++

HTML

CSS

JavaScript

LittleFS

Git

GitHub
## Hardware Pins

Edit `src/app_config.h` before deployment.

- SSR relay output: RX2 / GPIO16
- RTC I2C SDA: GPIO 21
- RTC I2C SCL: GPIO 22

## Build And Upload

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

## Default Access

- Wi-Fi AP SSID: `SmartBell-Setup`
- AP password: `smartbell123`
- Dashboard: `http://192.168.4.1`
- Admin username: `admin`
- Admin password: `admin123`

Change the default password immediately from User Management.

## Data Files

LittleFS stores JSON documents under:

- `/schedules.json`
- `/profiles.json`
- `/holidays.json`
- `/settings.json`
- `/users.json`
- `/logs.json`

The firmware validates each file at boot and recreates safe defaults when missing or corrupt.

## Architecture Notes

- No scheduler or relay path uses `delay()`.
- Relay activation, repeats, continuous mode, watchdog feeding, NTP polling, Wi-Fi reconnect, and status logging are driven by cooperative `millis()` state machines.
- Time priority is NTP, browser dashboard sync, then DS3231 RTC.
- Successful NTP/browser sync updates the RTC.
- Holidays suppress automatic schedules but do not block authorized manual bell control.
