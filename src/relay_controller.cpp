#include "relay_controller.h"

#include "app_config.h"
#include "logger.h"

void RelayController::begin(Logger* logger) {
  logger_ = logger;
  pinMode(AppConfig::RELAY_PIN, OUTPUT);
  setRelay(false);
}

void RelayController::loop() {
  const uint32_t now = millis();
  if (mode_ == Mode::TimedOn && now - phaseStartedMs_ >= durationMs_) {
    setRelay(false);
    completedRepeats_++;
    if (completedRepeats_ >= requestedRepeats_) {
      mode_ = Mode::Idle;
      if (logger_) logger_->info("bell", "Completed " + activeReason_);
    } else {
      mode_ = Mode::IntervalOff;
      phaseStartedMs_ = now;
    }
  } else if (mode_ == Mode::IntervalOff && now - phaseStartedMs_ >= intervalMs_) {
    setRelay(true);
    mode_ = Mode::TimedOn;
    phaseStartedMs_ = now;
  }
}

bool RelayController::ring(uint32_t durationMs, uint8_t repeatCount, uint32_t repeatIntervalMs, const String& reason) {
  if (isBusy() || durationMs == 0 || repeatCount == 0) return false;
  durationMs_ = constrain(durationMs, 100UL, 60000UL);
  intervalMs_ = constrain(repeatIntervalMs, 0UL, 60000UL);
  requestedRepeats_ = repeatCount;
  completedRepeats_ = 0;
  activeReason_ = reason;
  phaseStartedMs_ = millis();
  mode_ = Mode::TimedOn;
  setRelay(true);
  if (logger_) logger_->info("bell", "Started " + reason);
  return true;
}

void RelayController::continuousOn(const String& reason) {
  activeReason_ = reason;
  mode_ = Mode::Continuous;
  setRelay(true);
  if (logger_) logger_->warn("bell", "Continuous ON: " + reason);
}

void RelayController::stop(const String& reason) {
  setRelay(false);
  mode_ = Mode::Idle;
  if (logger_) logger_->warn("bell", "Stopped: " + reason);
}

bool RelayController::isActive() const { return relayOn_; }
bool RelayController::isBusy() const { return mode_ != Mode::Idle; }

String RelayController::state() const {
  if (mode_ == Mode::Idle) return "idle";
  if (mode_ == Mode::Continuous) return "continuous";
  if (mode_ == Mode::TimedOn) return "ringing";
  return "interval";
}

uint16_t RelayController::activeRemainingSeconds() const {
  if (mode_ == Mode::Continuous) return 0;
  if (mode_ != Mode::TimedOn) return 0;
  uint32_t elapsed = millis() - phaseStartedMs_;
  if (elapsed >= durationMs_) return 0;
  return static_cast<uint16_t>((durationMs_ - elapsed + 999UL) / 1000UL);
}

void RelayController::setRelay(bool on) {
  relayOn_ = on;
  digitalWrite(AppConfig::RELAY_PIN, AppConfig::RELAY_ACTIVE_HIGH ? on : !on);
}
