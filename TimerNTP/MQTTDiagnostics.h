#ifndef MQTT_DIAGNOSTICS_H
#define MQTT_DIAGNOSTICS_H

#pragma once

#include "Config.h"
#include "watchdog.h"

#include <stdint.h>
#include <tools.h>

#define D_REASON "reason"
#define D_BUILD "build"
#define D_HOSTNAME "hostname"
#define D_MAC "mac"
#define D_BOOT_MILLIS "bootMillis"
#define D_WATCHDOG_TIMEOUT_MS "watchdogTimeoutMs"
#define D_FREE_HEAP "freeHeap"
#define D_WDT_BOOT_COUNT "wdtBootCount"
#define D_LAST_STATE_BEFORE_RESET "lastStateBeforeReset"
#define D_LAST_STATE_BEFORE_RESET_KNOWN "lastStateBeforeResetKnown"
#define D_LAST_STATE_BEFORE_RESET_NAME "lastStateBeforeResetName"
#define D_LAST_UPTIME_BEFORE_RESET_MS "lastUptimeBeforeResetMs"
#define D_LAST_PHASE_BEFORE_RESET_KNOWN "lastPhaseBeforeResetKnown"
#define D_LAST_PHASE_BEFORE_RESET_RAW "lastPhaseBeforeResetRaw"
#define D_LAST_PHASE_BEFORE_RESET "lastPhaseBeforeReset"
#define D_PHASE_AT_PUBLISH "phaseAtPublish"
#define D_QUEUED_EVENTS "queuedEvents"
#define D_DROPPED_EVENTS "droppedEvents"
#define D_RESET_REASON "resetReason"
#define D_RESET_REASON_CODE "resetReasonCode"
#define D_WATCHDOG_RESET_ON_BOOT "watchdogResetOnBoot"
#define D_BROWNOUT_SUSPECTED "brownoutSuspected"
#define D_LAST_FAULT_VALID "lastFaultValid"
#define D_LAST_FAULT_PC "lastFaultPc"
#define D_LAST_FAULT_LR "lastFaultLr"
#define D_LAST_FAULT_PSR "lastFaultPsr"
#define D_STACK_GUARD_ARMED "stackGuardArmed"
#define D_LOCAL_MILLIS "localMillis"
#define D_TRANSITION_LOCAL_MILLIS "transitionLocalMillis"
#define D_FROM "from"
#define D_TO "to"
#define D_FAILED_PINGS "failedPings"
#define D_MAX_FAILED_PINGS "maxFailedPings"
#define D_DEGRADED_THRESHOLD "degradedThreshold"
#define D_BROKER_AVAILABLE "brokerAvailable"
#define D_BROKER_PING_MS "brokerPingMs"
#define D_DS18B20_TEMPERATURE_AVAILABLE "ds18b20TemperatureAvailable"
#define D_DS18B20_TEMPERATURE_C "ds18b20TemperatureC"
#define D_RP2040_TEMPERATURE_C "rp2040TemperatureC"
#define D_WIFI_STRENGTH "wifiStrength"
#define D_ERROR "error"

#define D_REASON_WATCHDOG "watchdog"
#define D_REASON_BOOT_CAUSE "boot_cause"
#define D_REASON_PING_HEALTH_TRANSITION "ping_health_transition"
#define D_ERROR_JSON_BUILD_FAILED "json_build_failed"

class NTPMachine;
class MyHardware;

class MQTTDiagnostics {
public:
  explicit MQTTDiagnostics(const char* buildDateTime = nullptr);

  void prepareBootCauseEventIfNeeded(NTPMachine& ntp);
  void prepareWatchdogEventIfNeeded(NTPMachine& ntp);
  void onMqttSessionStart();
  void onMqttSessionStop();
  void processPingHealthProbe();
  void publishPendingIfConnected(NTPMachine& ntp, MyHardware& hardware);
  bool isBrokerAvailable() const { return brokerAvailable; }

private:
  enum class DiagnosticEventType : uint8_t {
    BootCause = 0,
    WatchdogReboot,
    PingHealthTransition
  };

  struct BootCauseEventData {
    hal_reset_reason_t resetReason = HAL_RESET_REASON_UNKNOWN;
    bool brownoutSuspected = false;
    bool watchdogResetOnBoot = false;
    uint32_t wdtBootCount = 0;
    bool lastFaultValid = false;
    hal_fault_info_t lastFault = {};
    bool stackGuardArmed = false;
    uint32_t bootMillis = 0;
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
    DiagnosticEventType type = DiagnosticEventType::BootCause;
    BootCauseEventData bootCause = {};
    WatchdogEventData watchdog = {};
    PingHealthEventData pingHealth = {};
  };

  static constexpr uint8_t kDiagnosticsQueueSize = 8;

  const char* buildDateTime = nullptr;

  bool watchdogEventPrepared = false;
  bool bootCauseEventPrepared = false;
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
  static const char* resetReasonToString(hal_reset_reason_t reason);

  void enqueueEvent(const DiagnosticEvent& event);
  bool hasQueuedEvent() const;
  const DiagnosticEvent* getOldestEvent() const;
  void popOldestEvent();

  bool publishBootCauseEvent(const DiagnosticEvent& event, NTPMachine& ntp, MyHardware& hardware);
  bool publishWatchdogEvent(const DiagnosticEvent& event, NTPMachine& ntp, MyHardware& hardware);
  bool publishPingHealthEvent(const DiagnosticEvent& event, MyHardware& hardware);
};

#endif
