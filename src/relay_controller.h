#pragma once

#include <Arduino.h>

class Logger;

class RelayController {
 public:
  void begin(Logger* logger);
  void loop();
  bool ring(uint32_t durationMs, uint8_t repeatCount = 1, uint32_t repeatIntervalMs = 0, const String& reason = "manual");
  void continuousOn(const String& reason = "manual");
  void stop(const String& reason = "manual");
  bool isActive() const;
  bool isBusy() const;
  String state() const;
  uint16_t activeRemainingSeconds() const;

 private:
  enum class Mode { Idle, TimedOn, IntervalOff, Continuous };
  void setRelay(bool on);
  Logger* logger_ = nullptr;
  Mode mode_ = Mode::Idle;
  uint32_t durationMs_ = 0;
  uint32_t intervalMs_ = 0;
  uint8_t requestedRepeats_ = 0;
  uint8_t completedRepeats_ = 0;
  uint32_t phaseStartedMs_ = 0;
  bool relayOn_ = false;
  String activeReason_;
};
