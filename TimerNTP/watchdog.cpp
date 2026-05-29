#include "watchdog.h"

#include <hal/hal.h>
#include <tools.h>

namespace {
NOINIT int g_lastStateBeforeReset = 0;
NOINIT uint32_t g_lastUptimeBeforeResetMs = 0;
NOINIT uint8_t g_lastPhaseBeforeReset = 0;

WatchdogPhase phaseFromStorage(uint8_t raw) {
  switch (raw) {
    case static_cast<uint8_t>(WatchdogPhase::Boot):
      return WatchdogPhase::Boot;
    case static_cast<uint8_t>(WatchdogPhase::BootStartup):
      return WatchdogPhase::BootStartup;
    case static_cast<uint8_t>(WatchdogPhase::StateNotConnected):
      return WatchdogPhase::StateNotConnected;
    case static_cast<uint8_t>(WatchdogPhase::StateConnecting):
      return WatchdogPhase::StateConnecting;
    case static_cast<uint8_t>(WatchdogPhase::NtpSyncStart):
      return WatchdogPhase::NtpSyncStart;
    case static_cast<uint8_t>(WatchdogPhase::StateNtpSynchro):
      return WatchdogPhase::StateNtpSynchro;
    case static_cast<uint8_t>(WatchdogPhase::WireguardBegin):
      return WatchdogPhase::WireguardBegin;
    case static_cast<uint8_t>(WatchdogPhase::StateWireguardConnect):
      return WatchdogPhase::StateWireguardConnect;
    case static_cast<uint8_t>(WatchdogPhase::WireguardPeerUpCheck):
      return WatchdogPhase::WireguardPeerUpCheck;
    case static_cast<uint8_t>(WatchdogPhase::WireguardHandshakeKick):
      return WatchdogPhase::WireguardHandshakeKick;
    case static_cast<uint8_t>(WatchdogPhase::StateWireguardConnected):
      return WatchdogPhase::StateWireguardConnected;
    case static_cast<uint8_t>(WatchdogPhase::MqttStart):
      return WatchdogPhase::MqttStart;
    case static_cast<uint8_t>(WatchdogPhase::StateConnected):
      return WatchdogPhase::StateConnected;
    case static_cast<uint8_t>(WatchdogPhase::ConnectedPing):
      return WatchdogPhase::ConnectedPing;
    case static_cast<uint8_t>(WatchdogPhase::ConnectedMqttHandle):
      return WatchdogPhase::ConnectedMqttHandle;
    case static_cast<uint8_t>(WatchdogPhase::ConnectedDisplayUpdate):
      return WatchdogPhase::ConnectedDisplayUpdate;
    case static_cast<uint8_t>(WatchdogPhase::HardwareLoop):
      return WatchdogPhase::HardwareLoop;
    case static_cast<uint8_t>(WatchdogPhase::OtaLoop):
      return WatchdogPhase::OtaLoop;
    case static_cast<uint8_t>(WatchdogPhase::EvaluateTimeCondition):
      return WatchdogPhase::EvaluateTimeCondition;
    case static_cast<uint8_t>(WatchdogPhase::ReconnectBegin):
      return WatchdogPhase::ReconnectBegin;
    case static_cast<uint8_t>(WatchdogPhase::ReconnectWireguardEnd):
      return WatchdogPhase::ReconnectWireguardEnd;
    case static_cast<uint8_t>(WatchdogPhase::ReconnectMqttStop):
      return WatchdogPhase::ReconnectMqttStop;
    case static_cast<uint8_t>(WatchdogPhase::ReconnectWifiRestart):
      return WatchdogPhase::ReconnectWifiRestart;
    default:
      return WatchdogPhase::Unknown;
  }
}
}

void Watchdog::start(int initialState, WatchdogStateNameResolver stateNameResolver) {
  watchdogResetOnBoot = hal_watchdog_caused_reboot();
  lastStateBeforeReset = -1;
  lastUptimeBeforeResetMs = 0;
  bootCount = 0;
  currentPhase = WatchdogPhase::Boot;
  lastPhaseBeforeReset = WatchdogPhase::Unknown;
  lastPhaseBeforeResetRaw = static_cast<uint8_t>(WatchdogPhase::Unknown);

  if (watchdogResetOnBoot) {
    lastStateBeforeReset = g_lastStateBeforeReset;
    lastUptimeBeforeResetMs = g_lastUptimeBeforeResetMs;
    lastPhaseBeforeResetRaw = g_lastPhaseBeforeReset;
    lastPhaseBeforeReset = phaseFromStorage(g_lastPhaseBeforeReset);
    const char* stateName = stateNameResolver ? stateNameResolver(lastStateBeforeReset) : "unknown";
    deb("Watchdog reboot detected. Last state=%d (%s), uptime=%lu ms, phase=%s",
        lastStateBeforeReset,
        stateName ? stateName : "unknown",
        (unsigned long)lastUptimeBeforeResetMs,
        phaseToString(lastPhaseBeforeReset));
  } else {
    deb("Clean boot (watchdog did not cause reboot)");
  }

  g_lastStateBeforeReset = 0;
  g_lastUptimeBeforeResetMs = 0;
  g_lastPhaseBeforeReset = static_cast<uint8_t>(WatchdogPhase::BootStartup);

  // Mark boot phase immediately so diagnostics always include a meaningful
  // first breadcrumb from early startup.
  setPhase(WatchdogPhase::BootStartup, initialState);
}

void Watchdog::setPhase(WatchdogPhase phase, int currentState) {
  if (currentPhase == phase) {
    return;
  }
  currentPhase = phase;
  saveNTPState(currentState);
}

void Watchdog::saveNTPState(int currentState) {
  g_lastStateBeforeReset = currentState;
  g_lastUptimeBeforeResetMs = hal_millis();
  g_lastPhaseBeforeReset = static_cast<uint8_t>(currentPhase);
}

void Watchdog::setBootCount(uint32_t count) {
  bootCount = count;
}

bool Watchdog::wasResetOnBoot() const {
  return watchdogResetOnBoot;
}

int Watchdog::getLastStateBeforeReset() const {
  return lastStateBeforeReset;
}

uint32_t Watchdog::getLastUptimeBeforeResetMs() const {
  return lastUptimeBeforeResetMs;
}

uint32_t Watchdog::getBootCount() const {
  return bootCount;
}

WatchdogPhase Watchdog::getCurrentPhase() const {
  return currentPhase;
}

WatchdogPhase Watchdog::getLastPhaseBeforeReset() const {
  return lastPhaseBeforeReset;
}

uint8_t Watchdog::getLastPhaseBeforeResetRaw() const {
  return lastPhaseBeforeResetRaw;
}

const char* Watchdog::phaseToString(WatchdogPhase phase) {
  switch (phase) {
    case WatchdogPhase::Boot:
      return "boot";
    case WatchdogPhase::BootStartup:
      return "boot_startup";
    case WatchdogPhase::StateNotConnected:
      return "state_not_connected";
    case WatchdogPhase::StateConnecting:
      return "state_connecting";
    case WatchdogPhase::NtpSyncStart:
      return "ntp_sync_start";
    case WatchdogPhase::StateNtpSynchro:
      return "state_ntp_synchro";
    case WatchdogPhase::WireguardBegin:
      return "wireguard_begin";
    case WatchdogPhase::StateWireguardConnect:
      return "state_wireguard_connect";
    case WatchdogPhase::WireguardPeerUpCheck:
      return "wireguard_peer_up_check";
    case WatchdogPhase::WireguardHandshakeKick:
      return "wireguard_handshake_kick";
    case WatchdogPhase::StateWireguardConnected:
      return "state_wireguard_connected";
    case WatchdogPhase::MqttStart:
      return "mqtt_start";
    case WatchdogPhase::StateConnected:
      return "state_connected";
    case WatchdogPhase::ConnectedPing:
      return "connected_ping";
    case WatchdogPhase::ConnectedMqttHandle:
      return "connected_mqtt_handle";
    case WatchdogPhase::ConnectedDisplayUpdate:
      return "connected_display_update";
    case WatchdogPhase::HardwareLoop:
      return "hardware_loop";
    case WatchdogPhase::OtaLoop:
      return "ota_loop";
    case WatchdogPhase::EvaluateTimeCondition:
      return "evaluate_time_condition";
    case WatchdogPhase::ReconnectBegin:
      return "reconnect_begin";
    case WatchdogPhase::ReconnectWireguardEnd:
      return "reconnect_wireguard_end";
    case WatchdogPhase::ReconnectMqttStop:
      return "reconnect_mqtt_stop";
    case WatchdogPhase::ReconnectWifiRestart:
      return "reconnect_wifi_restart";
    default:
      return "unknown";
  }
}
