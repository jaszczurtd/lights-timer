#ifndef WATCHDOG_C
#define WATCHDOG_C

#pragma once

#include <stdint.h>

enum class WatchdogPhase : uint8_t {
  Boot = 0,
  BootStartup,
  StateNotConnected,
  StateConnecting,
  NtpSyncStart,
  StateNtpSynchro,
  WireguardBegin,
  StateWireguardConnect,
  WireguardPeerUpCheck,
  WireguardHandshakeKick,
  StateWireguardConnected,
  MqttStart,
  StateConnected,
  ConnectedPing,
  ConnectedMqttHandle,
  ConnectedDisplayUpdate,
  HardwareLoop,
  OtaLoop,
  EvaluateTimeCondition,
  ReconnectBegin,
  ReconnectWireguardEnd,
  ReconnectMqttStop,
  ReconnectWifiRestart,
  Unknown = 255
};

using WatchdogStateNameResolver = const char* (*)(int state);

class Watchdog {
public:
  void start(int initialState, WatchdogStateNameResolver stateNameResolver);
  void setPhase(WatchdogPhase phase, int currentState);
  void saveNTPState(int currentState);
  void setBootCount(uint32_t count);

  bool wasResetOnBoot() const;
  int getLastStateBeforeReset() const;
  uint32_t getLastUptimeBeforeResetMs() const;
  uint32_t getBootCount() const;
  WatchdogPhase getCurrentPhase() const;
  WatchdogPhase getLastPhaseBeforeReset() const;
  uint8_t getLastPhaseBeforeResetRaw() const;

  static const char* phaseToString(WatchdogPhase phase);

private:
  bool watchdogResetOnBoot = false;
  int lastStateBeforeReset = -1;
  uint32_t lastUptimeBeforeResetMs = 0;
  uint32_t bootCount = 0;
  WatchdogPhase currentPhase = WatchdogPhase::Boot;
  WatchdogPhase lastPhaseBeforeReset = WatchdogPhase::Unknown;
  uint8_t lastPhaseBeforeResetRaw = static_cast<uint8_t>(WatchdogPhase::Unknown);
};

#endif
