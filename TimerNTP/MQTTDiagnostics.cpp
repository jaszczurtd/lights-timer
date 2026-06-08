#include "MQTTDiagnostics.h"

#include "NTPMachine.h"
#include "MyHardware.h"

#include <hal/hal.h>
#include <utils/cJSON.h>
#include <tools.h>

#include <cstdio>
#include <cstring>

namespace {
void buildDiagnosticTopic(char* out, size_t outSize, const char* hostName, const char* eventName) {
  if (!out || outSize == 0 || !hostName || !eventName) {
    return;
  }

  snprintf(out, outSize, "%s/%s/%s", MQTT_TOPIC_DIAGNOSTICS, hostName, eventName);
}

bool addDiagnosticTemperatures(cJSON* root, MyHardware& hardware) {
  if (!root) {
    return false;
  }

  float ds18b20TemperatureC = 0.0f;
  const bool ds18b20TemperatureAvailable = hardware.getDs18b20TemperatureC(&ds18b20TemperatureC);
  if (!cJSON_AddBoolToObject(root, D_DS18B20_TEMPERATURE_AVAILABLE, ds18b20TemperatureAvailable)) {
    return false;
  }
  if (ds18b20TemperatureAvailable) {
    if (!cJSON_AddNumberToObject(root, D_DS18B20_TEMPERATURE_C, (double)ds18b20TemperatureC)) {
      return false;
    }
  } else {
    if (!cJSON_AddNullToObject(root, D_DS18B20_TEMPERATURE_C)) {
      return false;
    }
  }

  if (!cJSON_AddNumberToObject(root, D_RP2040_TEMPERATURE_C, (double)hal_read_chip_temp())) {
    return false;
  }

  char strength[10];

  snprintf(strength, sizeof(strength), "5/%d", hal_wifi_get_strength());
  if (!cJSON_AddStringToObject(root, D_WIFI_STRENGTH, strength)) {
    return false;
  }

  return true;
}
} // namespace

MQTTDiagnostics::MQTTDiagnostics(const char* buildDateTimeArg)
    : buildDateTime((buildDateTimeArg && buildDateTimeArg[0] != '\0') ? buildDateTimeArg : "unknown") {}

MQTTDiagnostics::PingHealthState MQTTDiagnostics::classifyPingHealthState(int failedPings, int maxFailedPings) {
  if (maxFailedPings <= 0) {
    return PingHealthState::Healthy;
  }

  if (failedPings >= maxFailedPings) {
    return PingHealthState::Unreachable;
  }

  const int degradedThreshold = (maxFailedPings + 1) / 2;
  if (failedPings >= degradedThreshold) {
    return PingHealthState::Degraded;
  }

  return PingHealthState::Healthy;
}

const char* MQTTDiagnostics::pingHealthStateToString(PingHealthState state) {
  switch (state) {
    case PingHealthState::Healthy:
      return "healthy";
    case PingHealthState::Degraded:
      return "degraded";
    case PingHealthState::Unreachable:
      return "unreachable";
    case PingHealthState::Unknown:
    default:
      return "unknown";
  }
}

const char* MQTTDiagnostics::ntpStateToString(int state) {
  switch (state) {
    case STATE_NOT_CONNECTED:
      return "not_connected";
    case STATE_CONNECTING:
      return "connecting";
    case STATE_NTP_SYNCHRO:
      return "ntp_synchro";
    case STATE_WIREGUARD_CONNECT:
      return "wireguard_connect";
    case STATE_WIREGUARD_CONNECTED:
      return "wireguard_connected";
    case STATE_CONNECTED:
      return "connected";
    default:
      return "unknown";
  }
}

const char* MQTTDiagnostics::resetReasonToString(hal_reset_reason_t reason) {
  const char* name = hal_reset_reason_str(reason);
  if (!name || name[0] == '\0') {
    return "UNKNOWN";
  }
  return name;
}

void MQTTDiagnostics::prepareBootCauseEventIfNeeded(NTPMachine& ntp) {
  if (bootCauseEventPrepared) {
    return;
  }

  bootCauseEventPrepared = true;

  const NTPMachine::WatchdogTelemetry telemetry = ntp.getWatchdogTelemetry();

  DiagnosticEvent event = {};
  event.type = DiagnosticEventType::BootCause;
  event.bootCause.resetReason = telemetry.resetReason;
  event.bootCause.brownoutSuspected = telemetry.brownoutSuspected;
  event.bootCause.watchdogResetOnBoot = telemetry.watchdogResetOnBoot;
  event.bootCause.wdtBootCount = telemetry.wdtBootCount;
  event.bootCause.lastFaultValid = telemetry.lastFaultValid;
  event.bootCause.lastFault = telemetry.lastFault;
  event.bootCause.stackGuardArmed = telemetry.stackGuardArmed;
  event.bootCause.bootMillis = hal_millis();
  enqueueEvent(event);

  deb("MQTT diagnostics: boot cause event queued (reason=%s, watchdog=%s, brownout=%s, fault=%s)",
      resetReasonToString(event.bootCause.resetReason),
      event.bootCause.watchdogResetOnBoot ? "true" : "false",
      event.bootCause.brownoutSuspected ? "true" : "false",
      event.bootCause.lastFaultValid ? "true" : "false");
}

void MQTTDiagnostics::enqueueEvent(const DiagnosticEvent& event) {
  if (diagnosticsQueueCount >= kDiagnosticsQueueSize) {
    diagnosticsQueueHead = (uint8_t)((diagnosticsQueueHead + 1U) % kDiagnosticsQueueSize);
    diagnosticsQueueCount = (uint8_t)(kDiagnosticsQueueSize - 1U);
    droppedEventsCount++;
    deb("MQTT diagnostics: queue full, dropped oldest event (dropped total=%lu)",
        (unsigned long)droppedEventsCount);
  }

  const uint8_t insertIndex =
      (uint8_t)((diagnosticsQueueHead + diagnosticsQueueCount) % kDiagnosticsQueueSize);
  diagnosticsQueue[insertIndex] = event;
  diagnosticsQueueCount++;
}

bool MQTTDiagnostics::hasQueuedEvent() const {
  return diagnosticsQueueCount > 0;
}

const MQTTDiagnostics::DiagnosticEvent* MQTTDiagnostics::getOldestEvent() const {
  if (!hasQueuedEvent()) {
    return nullptr;
  }
  return &diagnosticsQueue[diagnosticsQueueHead];
}

void MQTTDiagnostics::popOldestEvent() {
  if (!hasQueuedEvent()) {
    return;
  }

  diagnosticsQueueHead = (uint8_t)((diagnosticsQueueHead + 1U) % kDiagnosticsQueueSize);
  diagnosticsQueueCount--;
}

void MQTTDiagnostics::prepareWatchdogEventIfNeeded(NTPMachine& ntp) {
  if (watchdogEventPrepared) {
    return;
  }

  watchdogEventPrepared = true;
  const NTPMachine::WatchdogTelemetry telemetry = ntp.getWatchdogTelemetry();
  if (!telemetry.watchdogResetOnBoot) {
    return;
  }

  DiagnosticEvent event = {};
  event.type = DiagnosticEventType::WatchdogReboot;
  event.watchdog.wdtBootCount = telemetry.wdtBootCount;
  event.watchdog.lastStateBeforeReset = telemetry.lastStateBeforeReset;
  event.watchdog.lastUptimeBeforeResetMs = telemetry.lastUptimeBeforeResetMs;
  event.watchdog.lastPhaseBeforeReset = telemetry.lastPhaseBeforeReset;
  event.watchdog.lastPhaseBeforeResetRaw = telemetry.lastPhaseBeforeResetRaw;
  enqueueEvent(event);

  deb("MQTT diagnostics: watchdog reboot event queued (wdtBootCount=%lu, lastState=%d, lastUptime=%lu ms, phase=%s raw=%u)",
      (unsigned long)event.watchdog.wdtBootCount,
      event.watchdog.lastStateBeforeReset,
      (unsigned long)event.watchdog.lastUptimeBeforeResetMs,
      Watchdog::phaseToString(event.watchdog.lastPhaseBeforeReset),
      (unsigned)event.watchdog.lastPhaseBeforeResetRaw);
}

void MQTTDiagnostics::onMqttSessionStart() {
  pingTimer.begin(nullptr, NEXT_PING_TIME);
  pingTimerStarted = true;
  failedPingsCnt = 0;
  brokerAvailable = false;
  brokerPingMs = 0;
  pingHealthState = PingHealthState::Unknown;
}

void MQTTDiagnostics::onMqttSessionStop() {
  if (pingTimerStarted) {
    pingTimer.abort();
  }
  pingTimerStarted = false;
  failedPingsCnt = 0;
  brokerAvailable = false;
  brokerPingMs = 0;
  pingHealthState = PingHealthState::Unknown;
}

void MQTTDiagnostics::processPingHealthProbe() {
  if (!pingTimerStarted) {
    pingTimer.begin(nullptr, NEXT_PING_TIME);
    pingTimerStarted = true;
  }

  if (!pingTimer.available()) {
    return;
  }

  pingTimer.restart();

  const unsigned long pingStartedAt = hal_millis();
  const int pingResult = hal_wifi_ping_ex(MQTT_BROKER_WIREGUARD, PING_TIMEOUT_MS);
  brokerPingMs = hal_millis() - pingStartedAt;
  hal_watchdog_feed();

  brokerAvailable = pingResult >= 0;
  if (brokerAvailable) {
    if (failedPingsCnt > 0) {
      failedPingsCnt--;
    }
  } else if (failedPingsCnt < MAX_FAILED_PINGS) {
    failedPingsCnt++;
  }

  deb("ping test:%ldms isBrokerAvailable:%s failed pings:%d/%d",
      brokerPingMs,
      brokerAvailable ? "true" : "false",
      failedPingsCnt,
      MAX_FAILED_PINGS);

  const int maxFailedPings = MAX_FAILED_PINGS;
  const int failedPings = failedPingsCnt;
  const PingHealthState currentState = classifyPingHealthState(failedPings, maxFailedPings);

  if (pingHealthState == PingHealthState::Unknown) {
    pingHealthState = currentState;
    return;
  }

  if (currentState == pingHealthState) {
    return;
  }

  DiagnosticEvent event = {};
  event.type = DiagnosticEventType::PingHealthTransition;
  event.pingHealth.from = pingHealthState;
  event.pingHealth.to = currentState;
  event.pingHealth.failedPings = failedPings;
  event.pingHealth.maxFailedPings = maxFailedPings;
  event.pingHealth.degradedThreshold = (maxFailedPings + 1) / 2;
  event.pingHealth.brokerAvailable = brokerAvailable;
  event.pingHealth.brokerPingMs = brokerPingMs;
  event.pingHealth.transitionMillis = hal_millis();
  enqueueEvent(event);

  pingHealthState = currentState;

  deb("MQTT diagnostics: ping health transition %s -> %s (%d/%d, broker=%s, ping=%lums)",
      pingHealthStateToString(event.pingHealth.from),
      pingHealthStateToString(event.pingHealth.to),
      event.pingHealth.failedPings,
      maxFailedPings,
      event.pingHealth.brokerAvailable ? "up" : "down",
      event.pingHealth.brokerPingMs);
}

bool MQTTDiagnostics::publishWatchdogEvent(const DiagnosticEvent& event,
                                           NTPMachine& ntp,
                                           MyHardware& hardware) {
  cJSON *root = nullptr;
  char *json = nullptr;
  const NTPMachine::WatchdogTelemetry telemetry = ntp.getWatchdogTelemetry();

  const bool lastStateKnown =
      event.watchdog.lastStateBeforeReset >= STATE_NOT_CONNECTED &&
      event.watchdog.lastStateBeforeReset <= STATE_CONNECTED;
  const bool lastPhaseKnown = event.watchdog.lastPhaseBeforeReset != WatchdogPhase::Unknown;

  char lastStateName[48];
  char lastPhaseName[48];
  memset(lastStateName, 0, sizeof(lastStateName));
  memset(lastPhaseName, 0, sizeof(lastPhaseName));

  if (lastStateKnown) {
    const char* mapped = ntpStateToString(event.watchdog.lastStateBeforeReset);
    snprintf(lastStateName, sizeof(lastStateName), "%s", mapped ? mapped : "unknown");
  } else {
    snprintf(lastStateName, sizeof(lastStateName), "unmapped_state_%d", event.watchdog.lastStateBeforeReset);
  }

  if (lastPhaseKnown) {
    snprintf(lastPhaseName, sizeof(lastPhaseName), "%s",
             Watchdog::phaseToString(event.watchdog.lastPhaseBeforeReset));
  } else {
    snprintf(lastPhaseName, sizeof(lastPhaseName), "unmapped_phase_%u",
             (unsigned)event.watchdog.lastPhaseBeforeResetRaw);
  }

  memset(response, 0, sizeof(response));
  root = cJSON_CreateObject();
  NONULL(root);

  NONULL(cJSON_AddStringToObject(root, D_REASON, D_REASON_WATCHDOG));
  NONULL(cJSON_AddStringToObject(root, D_BUILD, buildDateTime));
  NONULL(cJSON_AddStringToObject(root, D_HOSTNAME, hardware.getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, D_MAC, hardware.getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, D_BOOT_MILLIS, (double)hal_millis()));
  NONULL(cJSON_AddNumberToObject(root, D_WATCHDOG_TIMEOUT_MS, WATCHDOG_TIME));
  NONULL(cJSON_AddNumberToObject(root, D_FREE_HEAP, (double)hal_get_free_heap()));
  NONULL(cJSON_AddNumberToObject(root, D_WDT_BOOT_COUNT, (double)event.watchdog.wdtBootCount));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_STATE_BEFORE_RESET, event.watchdog.lastStateBeforeReset));
  NONULL(cJSON_AddBoolToObject(root, D_LAST_STATE_BEFORE_RESET_KNOWN, lastStateKnown));
  NONULL(cJSON_AddStringToObject(root, D_LAST_STATE_BEFORE_RESET_NAME, lastStateName));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_UPTIME_BEFORE_RESET_MS,
                                 (double)event.watchdog.lastUptimeBeforeResetMs));
  NONULL(cJSON_AddBoolToObject(root, D_LAST_PHASE_BEFORE_RESET_KNOWN, lastPhaseKnown));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_PHASE_BEFORE_RESET_RAW,
                                 (double)event.watchdog.lastPhaseBeforeResetRaw));
  NONULL(cJSON_AddStringToObject(root, D_LAST_PHASE_BEFORE_RESET, lastPhaseName));
  NONULL(cJSON_AddStringToObject(root, D_PHASE_AT_PUBLISH,
                                 Watchdog::phaseToString(telemetry.currentPhase)));
  if (!addDiagnosticTemperatures(root, hardware)) {
    goto error;
  }
  NONULL(cJSON_AddNumberToObject(root, D_QUEUED_EVENTS, (double)diagnosticsQueueCount));
  NONULL(cJSON_AddNumberToObject(root, D_DROPPED_EVENTS, (double)droppedEventsCount));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_WATCHDOG);
  deb("MQTT: topic:%s publish watchdog reboot event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: watchdog event publish failed, state=%d", hal_mqtt_state());
    return false;
  }

  return true;

error:
  if (json) {
    cJSON_free(json);
  }
  if (root) {
    cJSON_Delete(root);
  }
  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_WATCHDOG);
  memset(response, 0, sizeof(response));
  snprintf(response, sizeof(response),
           "{\"%s\":\"%s\",\"%s\":\"%s\"}",
           D_REASON,
           D_REASON_WATCHDOG,
           D_ERROR,
           D_ERROR_JSON_BUILD_FAILED);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: watchdog event fallback publish failed, state=%d", hal_mqtt_state());
  }
  return false;
}

bool MQTTDiagnostics::publishBootCauseEvent(const DiagnosticEvent& event,
                                            NTPMachine& ntp,
                                            MyHardware& hardware) {
  cJSON *root = nullptr;
  char *json = nullptr;
  const NTPMachine::WatchdogTelemetry telemetry = ntp.getWatchdogTelemetry();

  memset(response, 0, sizeof(response));
  root = cJSON_CreateObject();
  NONULL(root);

  NONULL(cJSON_AddStringToObject(root, D_REASON, D_REASON_BOOT_CAUSE));
  NONULL(cJSON_AddStringToObject(root, D_BUILD, buildDateTime));
  NONULL(cJSON_AddStringToObject(root, D_HOSTNAME, hardware.getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, D_MAC, hardware.getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, D_BOOT_MILLIS, (double)event.bootCause.bootMillis));
  NONULL(cJSON_AddStringToObject(root, D_RESET_REASON, resetReasonToString(event.bootCause.resetReason)));
  NONULL(cJSON_AddNumberToObject(root, D_RESET_REASON_CODE, (double)event.bootCause.resetReason));
  NONULL(cJSON_AddBoolToObject(root, D_WATCHDOG_RESET_ON_BOOT, event.bootCause.watchdogResetOnBoot));
  NONULL(cJSON_AddBoolToObject(root, D_BROWNOUT_SUSPECTED, event.bootCause.brownoutSuspected));
  NONULL(cJSON_AddBoolToObject(root, D_LAST_FAULT_VALID, event.bootCause.lastFaultValid));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_FAULT_PC, (double)event.bootCause.lastFault.pc));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_FAULT_LR, (double)event.bootCause.lastFault.lr));
  NONULL(cJSON_AddNumberToObject(root, D_LAST_FAULT_PSR, (double)event.bootCause.lastFault.psr));
  NONULL(cJSON_AddBoolToObject(root, D_STACK_GUARD_ARMED, event.bootCause.stackGuardArmed));
  NONULL(cJSON_AddNumberToObject(root, D_WDT_BOOT_COUNT, (double)event.bootCause.wdtBootCount));
  NONULL(cJSON_AddStringToObject(root, D_PHASE_AT_PUBLISH,
                                 Watchdog::phaseToString(telemetry.currentPhase)));
  if (!addDiagnosticTemperatures(root, hardware)) {
    goto error;
  }
  NONULL(cJSON_AddNumberToObject(root, D_QUEUED_EVENTS, (double)diagnosticsQueueCount));
  NONULL(cJSON_AddNumberToObject(root, D_DROPPED_EVENTS, (double)droppedEventsCount));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_BOOT_CAUSE);
  deb("MQTT: topic:%s publish boot cause event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: boot cause event publish failed, state=%d", hal_mqtt_state());
    return false;
  }

  return true;

error:
  if (json) {
    cJSON_free(json);
  }
  if (root) {
    cJSON_Delete(root);
  }
  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_BOOT_CAUSE);
  memset(response, 0, sizeof(response));
  snprintf(response, sizeof(response),
           "{\"%s\":\"%s\",\"%s\":\"%s\"}",
           D_REASON,
           D_REASON_BOOT_CAUSE,
           D_ERROR,
           D_ERROR_JSON_BUILD_FAILED);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: boot cause fallback publish failed, state=%d", hal_mqtt_state());
  }
  return false;
}

bool MQTTDiagnostics::publishPingHealthEvent(const DiagnosticEvent& event, MyHardware& hardware) {
  cJSON *root = nullptr;
  char *json = nullptr;

  memset(response, 0, sizeof(response));
  root = cJSON_CreateObject();
  NONULL(root);

  NONULL(cJSON_AddStringToObject(root, D_REASON, D_REASON_PING_HEALTH_TRANSITION));
  NONULL(cJSON_AddStringToObject(root, D_BUILD, buildDateTime));
  NONULL(cJSON_AddStringToObject(root, D_HOSTNAME, hardware.getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, D_MAC, hardware.getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, D_LOCAL_MILLIS, (double)hal_millis()));
  NONULL(cJSON_AddNumberToObject(root, D_TRANSITION_LOCAL_MILLIS,
                                 (double)event.pingHealth.transitionMillis));
  NONULL(cJSON_AddStringToObject(root, D_FROM, pingHealthStateToString(event.pingHealth.from)));
  NONULL(cJSON_AddStringToObject(root, D_TO, pingHealthStateToString(event.pingHealth.to)));
  NONULL(cJSON_AddNumberToObject(root, D_FAILED_PINGS, event.pingHealth.failedPings));
  NONULL(cJSON_AddNumberToObject(root, D_MAX_FAILED_PINGS, event.pingHealth.maxFailedPings));
  NONULL(cJSON_AddNumberToObject(root, D_DEGRADED_THRESHOLD, event.pingHealth.degradedThreshold));
  NONULL(cJSON_AddBoolToObject(root, D_BROKER_AVAILABLE, event.pingHealth.brokerAvailable));
  NONULL(cJSON_AddNumberToObject(root, D_BROKER_PING_MS, (double)event.pingHealth.brokerPingMs));
  if (!addDiagnosticTemperatures(root, hardware)) {
    goto error;
  }
  NONULL(cJSON_AddNumberToObject(root, D_QUEUED_EVENTS, (double)diagnosticsQueueCount));
  NONULL(cJSON_AddNumberToObject(root, D_DROPPED_EVENTS, (double)droppedEventsCount));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_PING_HEALTH);
  deb("MQTT: topic:%s publish ping health event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: ping health event publish failed, state=%d", hal_mqtt_state());
    return false;
  }

  return true;

error:
  if (json) {
    cJSON_free(json);
  }
  if (root) {
    cJSON_Delete(root);
  }
  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_PING_HEALTH);
  memset(response, 0, sizeof(response));
  snprintf(response, sizeof(response),
           "{\"%s\":\"%s\",\"%s\":\"%s\"}",
           D_REASON,
           D_REASON_PING_HEALTH_TRANSITION,
           D_ERROR,
           D_ERROR_JSON_BUILD_FAILED);
  if (!hal_mqtt_publish_str(topic, response, true)) {
    deb("MQTT diagnostics: ping health event fallback publish failed, state=%d", hal_mqtt_state());
  }
  return false;
}

void MQTTDiagnostics::publishPendingIfConnected(NTPMachine& ntp, MyHardware& hardware) {
  const DiagnosticEvent* event = getOldestEvent();
  if (!event) {
    return;
  }

  bool sent = false;
  switch (event->type) {
    case DiagnosticEventType::BootCause:
      sent = publishBootCauseEvent(*event, ntp, hardware);
      break;
    case DiagnosticEventType::WatchdogReboot:
      sent = publishWatchdogEvent(*event, ntp, hardware);
      break;
    case DiagnosticEventType::PingHealthTransition:
      sent = publishPingHealthEvent(*event, hardware);
      break;
    default:
      break;
  }

  if (sent) {
    popOldestEvent();
  }
}
