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

  NONULL(cJSON_AddStringToObject(root, "reason", "watchdog"));
  NONULL(cJSON_AddStringToObject(root, "build", buildDateTime));
  NONULL(cJSON_AddStringToObject(root, "hostname", hardware.getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, "mac", hardware.getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, "bootMillis", (double)hal_millis()));
  NONULL(cJSON_AddNumberToObject(root, "watchdogTimeoutMs", WATCHDOG_TIME));
  NONULL(cJSON_AddNumberToObject(root, "freeHeap", (double)hal_get_free_heap()));
  NONULL(cJSON_AddNumberToObject(root, "wdtBootCount", (double)event.watchdog.wdtBootCount));
  NONULL(cJSON_AddNumberToObject(root, "lastStateBeforeReset", event.watchdog.lastStateBeforeReset));
  NONULL(cJSON_AddBoolToObject(root, "lastStateBeforeResetKnown", lastStateKnown));
  NONULL(cJSON_AddStringToObject(root, "lastStateBeforeResetName", lastStateName));
  NONULL(cJSON_AddNumberToObject(root, "lastUptimeBeforeResetMs",
                                 (double)event.watchdog.lastUptimeBeforeResetMs));
  NONULL(cJSON_AddBoolToObject(root, "lastPhaseBeforeResetKnown", lastPhaseKnown));
  NONULL(cJSON_AddNumberToObject(root, "lastPhaseBeforeResetRaw",
                                 (double)event.watchdog.lastPhaseBeforeResetRaw));
  NONULL(cJSON_AddStringToObject(root, "lastPhaseBeforeReset", lastPhaseName));
  NONULL(cJSON_AddStringToObject(root, "phaseAtPublish",
                                 Watchdog::phaseToString(telemetry.currentPhase)));
  NONULL(cJSON_AddNumberToObject(root, "queuedEvents", (double)diagnosticsQueueCount));
  NONULL(cJSON_AddNumberToObject(root, "droppedEvents", (double)droppedEventsCount));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_WATCHDOG);
  deb("MQTT: topic:%s publish watchdog reboot event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, false)) {
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
  if (!hal_mqtt_publish_str(topic, "{\"reason\":\"watchdog\",\"error\":\"json_build_failed\"}", false)) {
    deb("MQTT diagnostics: watchdog event fallback publish failed, state=%d", hal_mqtt_state());
  }
  return false;
}

bool MQTTDiagnostics::publishPingHealthEvent(const DiagnosticEvent& event, MyHardware& hardware) {
  cJSON *root = nullptr;
  char *json = nullptr;

  memset(response, 0, sizeof(response));
  root = cJSON_CreateObject();
  NONULL(root);

  NONULL(cJSON_AddStringToObject(root, "reason", "ping_health_transition"));
  NONULL(cJSON_AddStringToObject(root, "build", buildDateTime));
  NONULL(cJSON_AddStringToObject(root, "hostname", hardware.getMyHostname()));
  NONULL(cJSON_AddStringToObject(root, "mac", hardware.getMyMAC()));
  NONULL(cJSON_AddNumberToObject(root, "localMillis", (double)hal_millis()));
  NONULL(cJSON_AddNumberToObject(root, "transitionLocalMillis",
                                 (double)event.pingHealth.transitionMillis));
  NONULL(cJSON_AddStringToObject(root, "from", pingHealthStateToString(event.pingHealth.from)));
  NONULL(cJSON_AddStringToObject(root, "to", pingHealthStateToString(event.pingHealth.to)));
  NONULL(cJSON_AddNumberToObject(root, "failedPings", event.pingHealth.failedPings));
  NONULL(cJSON_AddNumberToObject(root, "maxFailedPings", event.pingHealth.maxFailedPings));
  NONULL(cJSON_AddNumberToObject(root, "degradedThreshold", event.pingHealth.degradedThreshold));
  NONULL(cJSON_AddBoolToObject(root, "brokerAvailable", event.pingHealth.brokerAvailable));
  NONULL(cJSON_AddNumberToObject(root, "brokerPingMs", (double)event.pingHealth.brokerPingMs));
  NONULL(cJSON_AddNumberToObject(root, "queuedEvents", (double)diagnosticsQueueCount));
  NONULL(cJSON_AddNumberToObject(root, "droppedEvents", (double)droppedEventsCount));

  json = cJSON_PrintUnformatted(root);
  NONULL(json);

  strncpy(response, json, sizeof(response) - 1);
  cJSON_free(json);
  cJSON_Delete(root);

  buildDiagnosticTopic(topic, sizeof(topic), hardware.getMyHostname(), MQTT_TOPIC_DIAGNOSTICS_PING_HEALTH);
  deb("MQTT: topic:%s publish ping health event: %s", topic, response);
  if (!hal_mqtt_publish_str(topic, response, false)) {
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
  if (!hal_mqtt_publish_str(topic, "{\"reason\":\"ping_health_transition\",\"error\":\"json_build_failed\"}", false)) {
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
