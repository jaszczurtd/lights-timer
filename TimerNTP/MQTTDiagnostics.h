#ifndef MQTT_DIAGNOSTICS_H
#define MQTT_DIAGNOSTICS_H

#pragma once

#include "Config.h"
#include "watchdog.h"

#include <stdint.h>
#include <tools.h>

class NTPMachine;
class MyHardware;

class MQTTDiagnostics {
public:
  explicit MQTTDiagnostics(const char* buildDateTime = nullptr);

  void prepareWatchdogEventIfNeeded(NTPMachine& ntp);
  void onMqttSessionStart();
  void onMqttSessionStop();
  void processPingHealthProbe();
  void publishPendingIfConnected(NTPMachine& ntp, MyHardware& hardware);

private:
  enum class DiagnosticEventType : uint8_t {
    WatchdogReboot = 0,
    PingHealthTransition
  };

  enum class PingHealthState : uint8_t {
    Unknown = 0,
    Healthy,
    Degraded,
    Unreachable
  };

  struct WatchdogEventData {
    uint32_t wdtBootCount = 0;
    int lastStateBeforeReset = -1;
    uint32_t lastUptimeBeforeResetMs = 0;
    WatchdogPhase lastPhaseBeforeReset = WatchdogPhase::Unknown;
    uint8_t lastPhaseBeforeResetRaw = static_cast<uint8_t>(WatchdogPhase::Unknown);
  };

  struct PingHealthEventData {
    PingHealthState from = PingHealthState::Unknown;
    PingHealthState to = PingHealthState::Unknown;
    int failedPings = 0;
    int maxFailedPings = 0;
    int degradedThreshold = 0;
    bool brokerAvailable = false;
    unsigned long brokerPingMs = 0;
    uint32_t transitionMillis = 0;
  };

  struct DiagnosticEvent {
    DiagnosticEventType type = DiagnosticEventType::WatchdogReboot;
    WatchdogEventData watchdog = {};
    PingHealthEventData pingHealth = {};
  };

  static constexpr uint8_t kDiagnosticsQueueSize = 8;

  const char* buildDateTime = nullptr;

  bool watchdogEventPrepared = false;
  PingHealthState pingHealthState = PingHealthState::Unknown;
  bool brokerAvailable = false;
  unsigned long brokerPingMs = 0;
  int failedPingsCnt = 0;
  SmartTimers pingTimer;
  bool pingTimerStarted = false;

  DiagnosticEvent diagnosticsQueue[kDiagnosticsQueueSize];
  uint8_t diagnosticsQueueHead = 0;
  uint8_t diagnosticsQueueCount = 0;
  uint32_t droppedEventsCount = 0;

  char topic[MQTT_MAX_TOPIC_LENGTH];
  char response[MQTT_MAX_BUFFER_LENGTH];

  static PingHealthState classifyPingHealthState(int failedPings, int maxFailedPings);
  static const char* pingHealthStateToString(PingHealthState state);
  static const char* ntpStateToString(int state);

  void enqueueEvent(const DiagnosticEvent& event);
  bool hasQueuedEvent() const;
  const DiagnosticEvent* getOldestEvent() const;
  void popOldestEvent();

  bool publishWatchdogEvent(const DiagnosticEvent& event, NTPMachine& ntp, MyHardware& hardware);
  bool publishPingHealthEvent(const DiagnosticEvent& event, MyHardware& hardware);
};

#endif
