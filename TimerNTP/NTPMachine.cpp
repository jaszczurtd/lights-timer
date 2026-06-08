#include "Credentials.h"
#include <hal/hal.h>
#include "NTPMachine.h"
#include "Logic.h"
#include "MyHardware.h"
#include "MQTTClient.h"

MyHardware& NTPMachine::hardware() { return logic.hardwareObj(); }
MQTTClient& NTPMachine::mqtt() { return logic.mqttObj(); }

void NTPMachine::start() {
  watchdog.start(STATE_NOT_CONNECTED, stateNameForTelemetry);
  setNTPState(STATE_NOT_CONNECTED);

#if ENABLE_STACK_GUARD
  stackGuardArmed = hal_stack_guard_init();
  if (stackGuardArmed) {
    deb("Stack guard initialized");
  } else {
    deb("Stack guard unavailable on this target");
  }
#else
  stackGuardArmed = false;
  deb("Stack guard disabled by config");
#endif

  hardware().start();
  watchdog.setBootCount(hardware().markWatchdogBootAndGetCount(watchdog.wasResetOnBoot()));

  long s = 0, e = 0;
  hardware().loadStartEnd(&s, &e);
  hardware().loadSwitches();
  hardware().extractTime(s, e);
  // Keep cold-boot fail-safe, but after watchdog reboot restore relays from
  // persisted switch state to avoid unexpected OFF transition.
  hardware().applyRelays(watchdog.wasResetOnBoot());
  hardware().restartWiFi();

  evaluateRelayTimer.begin(nullptr, EVALUATE_TIME_FOR_RELAY_MS);
  loopLogTimer.begin(nullptr, PRINT_INTERVAL_MS);
}

int NTPMachine::getNTPState(void) {
  return currentState;
}

void NTPMachine::setNTPState(NTPState state) {
  currentState = state;
  watchdog.saveNTPState(currentState);
}

const char *NTPMachine::getTimeFormatted(void) {
  return (const char *)buffer;
}

void NTPMachine::reconnect(void) {
  setWatchdogPhase(WatchdogPhase::ReconnectBegin);
  if (wgStarted) {
    setWatchdogPhase(WatchdogPhase::ReconnectWireguardEnd);
    hal_wireguard_end();
    wgStarted = false;
  }
  hal_watchdog_feed();
  setWatchdogPhase(WatchdogPhase::ReconnectMqttStop);
  mqtt().stop();
  hal_watchdog_feed();
  setWatchdogPhase(WatchdogPhase::ReconnectWifiRestart);
  hardware().restartWiFi();
  setNTPState(STATE_NOT_CONNECTED);
  setWatchdogPhase(WatchdogPhase::StateNotConnected);
  hardware().wakeDisplayForEvent();
  hardware().drawCenteredText("NO CONNECTION");
}

void NTPMachine::stateMachine(void) {

  hal_watchdog_feed();

  switch(getNTPState()) {
    case STATE_NOT_CONNECTED: {
      setWatchdogPhase(WatchdogPhase::StateNotConnected);
      deb("Not connected to WiFi. Trying to reconnect to %s...", WIFI_SSID);

      memset(buffer, 0, sizeof(buffer));
      hardware().drawCenteredText("CONNECTING...");

      wifiTimeoutTimer.begin(nullptr, WIFI_TIMEOUT_MS);
      connectingPollTimer.begin(nullptr, 200);

      setNTPState(STATE_CONNECTING);
    }
    break;

    case STATE_CONNECTING: {
      setWatchdogPhase(WatchdogPhase::StateConnecting);

      if (wifiTimeoutTimer.available()) {
        deb("\n%s: WiFi connection timeout!", WIFI_SSID);
        wifiTimeoutTimer.abort();
        reconnect();
        break;
      }

      if (connectingPollTimer.available()) {
        connectingPollTimer.restart();
        if(hal_wifi_is_connected()) {

          char dns_ip[32] = {0};
          if (!hal_wifi_get_dns_ip(dns_ip, sizeof(dns_ip))) {
            snprintf(dns_ip, sizeof(dns_ip), "%s", "0.0.0.0");
          }

          deb("Connected to WiFi %s. Local IP address: %s", WIFI_SSID, hardware().getMyIP());
          deb("ping target: %s", MQTT_BROKER_WIREGUARD);
          deb("DNS IP:%s", dns_ip);

          hal_watchdog_feed();
          hardware().drawCenteredText("CONNECTED");

          setWatchdogPhase(WatchdogPhase::NtpSyncStart);
          hal_time_set_timezone("CET-1CEST,M3.5.0/2,M10.5.0/3");
          hal_time_sync_ntp(ntpServer0, nullptr);

          ntpTimeoutTimer.begin(nullptr, NTP_TIMEOUT_MS);
          setNTPState(STATE_NTP_SYNCHRO);
        }
      }
    }
    break;

    case STATE_NTP_SYNCHRO: {
      setWatchdogPhase(WatchdogPhase::StateNtpSynchro);
      if (hal_wifi_is_connected()) {
        hardware().drawCenteredText("NTP SYNCHRO");

        if (hal_time_is_synced(24UL * 3600UL * 2UL)) {
          ntpTimeoutTimer.abort();
          setNTPState(STATE_WIREGUARD_CONNECT);
          localTimeHasBeenSet = true;

          setWatchdogPhase(WatchdogPhase::WireguardBegin);

          if (!hal_wireguard_begin_advanced_text(
              getWireguardLocalIP(hardware().getMyMAC()),
              getWireguardPrivateKey(hardware().getMyMAC()),
              WG_ENDPOINT,
              WG_SERVER_PUBLIC_KEY,
              WG_ENDPOINT_PORT,
              WG_ALLOWED_IP,
              WG_ALLOWED_MASK)) {
            hal_watchdog_feed();
            deb("WireGuard initialization failed.");
            reconnect();
            break;
          }
          wgStarted = true;
          hal_watchdog_feed();
          wgHandshakeTimer.begin(nullptr, 500);
          break;
        }

        if (ntpTimeoutTimer.available()) {
          deb("NTP synchro error!");
          ntpTimeoutTimer.abort();
          reconnect();
        }

      } else {
        reconnect();
      }
    }
    break;

    case STATE_WIREGUARD_CONNECT: {
      setWatchdogPhase(WatchdogPhase::StateWireguardConnect);
      if (hal_wifi_is_connected()) {

        if (wgHandshakeTimer.available()) {
          wgHandshakeTimer.restart();
          hal_watchdog_feed();
          setWatchdogPhase(WatchdogPhase::WireguardPeerUpCheck);
          if (!hal_wireguard_peer_up_quick()) {
            setWatchdogPhase(WatchdogPhase::WireguardHandshakeKick);
            if (!hal_wireguard_kick_handshake_text(getWireguardLocalIP(hardware().getMyMAC()), 9, 0)) {
              deb("WG handshake kick failed.");
            } else {
              deb("WG not ready yet (no session key). Handshake kicked.");
            }
            hal_watchdog_feed();
            break;
          }
          hal_watchdog_feed();
          setNTPState(STATE_WIREGUARD_CONNECTED);
        }
      } else {
        reconnect();
      }
    }
    break;

    case STATE_WIREGUARD_CONNECTED: {
      setWatchdogPhase(WatchdogPhase::StateWireguardConnected);
      if (hal_wifi_is_connected()) {
        setNTPState(STATE_CONNECTED);

        setWatchdogPhase(WatchdogPhase::MqttStart);
        mqtt().start(MQTT_BROKER_WIREGUARD, MQTT_BROKER_PORT);
        hal_watchdog_feed();
        hardware().clearDisplay();
        hal_watchdog_feed();

        ntpReSyncTimer.begin(nullptr, (unsigned long)HOURS_SYNC_INTERVAL * 3600 * 1000UL);

        deb("build datetime: %s", BuildDateTime);
        break;

      } else {
        reconnect();
      }
    }
    break;

    case STATE_CONNECTED: {
      setWatchdogPhase(WatchdogPhase::StateConnected);
      if (hal_wifi_is_connected()) {

        if (ntpReSyncTimer.available()) {
          ntpReSyncTimer.restart();
          hal_time_sync_ntp(ntpServer0, nullptr);
        }

        setWatchdogPhase(WatchdogPhase::ConnectedPing);
        mqtt().handleDiagnosticsPingHealth();

        setWatchdogPhase(WatchdogPhase::ConnectedMqttHandle);
        mqtt().handleMQTTClient();
        setWatchdogPhase(WatchdogPhase::ConnectedDisplayUpdate);
        hardware().updateDisplayInNormalOperationMode();

      } else {
        reconnect();
      }
    }
    break;
  }

  setWatchdogPhase(WatchdogPhase::HardwareLoop);
  hardware().hardwareLoop();
  hardware().updateBuildInLed();
  if (hal_wifi_is_connected()) {
    setWatchdogPhase(WatchdogPhase::OtaLoop);
    hardware().handleOTAUpdates();
    hal_watchdog_feed();
  }

  if (evaluateRelayTimer.available()) {
    evaluateRelayTimer.restart();
    if(localTimeHasBeenSet) {
      setWatchdogPhase(WatchdogPhase::EvaluateTimeCondition);
      evaluateTimeCondition();
    }
  }

  if (loopLogTimer.available()) {
    loopLogTimer.restart();
    if (hal_wifi_is_connected() && currentState >= STATE_CONNECTED) {
      deb("%s, IP:%s, wg IP:%s, host:%s, mac:%s, heap:%ld bytes, wifi: %d/5",
          getTimeFormatted(),
          hardware().getMyIP(),
          getWireguardLocalIP(hardware().getMyMAC()),
          getFriendlyHostname(hardware().getMyMAC()),
          hardware().getMyMAC(),
          hal_get_free_heap(),
          hal_wifi_get_strength());
    }
  }

}

void NTPMachine::evaluateTimeCondition() {
  struct tm timeinfo = {};
  if (!hal_time_get_local(&timeinfo)) {
    return;
  }

  if (!hal_time_format_local(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S")) {
    return;
  }

  now_time = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  deb("now_time:%ld buffer:%s ", now_time, buffer);

  hardware().checkConditionsForStartEnAction(now_time);
}

long NTPMachine::getTimeNow(void) {
  return now_time;
}

NTPMachine::WatchdogTelemetry NTPMachine::getWatchdogTelemetry() const {
  WatchdogTelemetry telemetry = {};
  telemetry.watchdogResetOnBoot = watchdog.wasResetOnBoot();
  telemetry.lastStateBeforeReset = watchdog.getLastStateBeforeReset();
  telemetry.lastUptimeBeforeResetMs = watchdog.getLastUptimeBeforeResetMs();
  telemetry.wdtBootCount = watchdog.getBootCount();
  telemetry.currentPhase = watchdog.getCurrentPhase();
  telemetry.lastPhaseBeforeReset = watchdog.getLastPhaseBeforeReset();
  telemetry.lastPhaseBeforeResetRaw = watchdog.getLastPhaseBeforeResetRaw();
  telemetry.resetReason = hal_get_reset_reason();
  telemetry.brownoutSuspected = hal_last_boot_was_brownout();
  telemetry.lastFaultValid = hal_get_last_fault(&telemetry.lastFault);
  telemetry.stackGuardArmed = stackGuardArmed;
  return telemetry;
}

void NTPMachine::setWatchdogPhase(WatchdogPhase phase) {
  hal_watchdog_feed();
  watchdog.setPhase(phase, currentState);
}

const char* NTPMachine::stateNameForTelemetry(int state) {
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
