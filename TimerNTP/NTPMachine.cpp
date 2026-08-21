#include "Credentials.h"
#include <hal/hal.h>
#include "NTPMachine.h"
#include "Logic.h"
#include "MyHardware.h"
#include "MQTTClient.h"
#include "CredentialValues.h"

MyHardware& NTPMachine::hardware() { return logic.hardwareObj(); }
MQTTClient& NTPMachine::mqtt() { return logic.mqttObj(); }

namespace {

constexpr uint64_t kMinimumValidUnix = UINT64_C(24) * UINT64_C(3600) * 2u;

bool ntpSyncComplete(const hal_time_status_t &status) {
  return status.valid && status.source == HAL_TIME_SOURCE_NTP &&
         status.ntp_state == HAL_TIME_NTP_SYNCHRONIZED &&
         status.unix_time >= kMinimumValidUnix;
}

} // namespace

void NTPMachine::initializeRtc(void) {
  hal_rtc_config_t config = {};
  config.chip = HAL_RTC_CHIP_INTERNAL;
  config.bus.internal.clock_source = HAL_RTC_CLOCK_SOURCE_AUTO;

  hal_status_t status = hal_rtc_init_ex(&config, &rtc);
  if (status != HAL_OK) {
    deb("Internal RTC unavailable: %s", hal_status_to_string(status));
    rtc = nullptr;
    return;
  }

  status = hal_time_attach_rtc_ex(rtc, HAL_TIME_RTC_RESTORE_IF_VALID |
                                           HAL_TIME_RTC_WRITE_AFTER_NTP);
  if (status != HAL_OK) {
    deb("Internal RTC attach failed: %s", hal_status_to_string(status));
    hal_rtc_deinit(rtc);
    rtc = nullptr;
    return;
  }

  hal_time_status_t timeStatus = {};
  if (hal_time_get_status_ex(&timeStatus) == HAL_OK && timeStatus.valid) {
    localTimeHasBeenSet = timeStatus.unix_time >= kMinimumValidUnix;
    deb("Runtime clock restored from RTC: unix=%llu",
        (unsigned long long)timeStatus.unix_time);
  } else {
    deb("Internal RTC is waiting for the first NTP synchronization");
  }
}

void NTPMachine::start() {
  watchdog.start(STATE_NOT_CONNECTED, stateName);
  setNTPState(STATE_NOT_CONNECTED);

#if ENABLE_STACK_GUARD
  if (hal_stack_guard_init()) {
    deb("Stack guard initialized");
  } else {
    deb("Stack guard unavailable on this target");
  }
#else
  deb("Stack guard disabled by config");
#endif

  if (!hal_time_set_timezone("CET-1CEST,M3.5.0/2,M10.5.0/3")) {
    derr("Failed to configure CET/CEST timezone");
  }

  initializeRtc();

  // Native RP WiFi exposes the factory MAC only after STA hardware
  // initialization. Device I/O configuration depends on that MAC.
  hardware().restartWiFi();
  hardware().start();

  long s = 0, e = 0;
  hardware().loadStartEnd(&s, &e);
  hardware().loadSwitches();
  hardware().extractTime(s, e);
  // Keep cold-boot fail-safe, but after watchdog reboot restore relays from
  // persisted switch state to avoid unexpected OFF transition.
  hardware().applyRelays(watchdog.wasResetOnBoot());

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
      deb("Not connected to WiFi. Trying to reconnect to %s...",
          credentialValue(CR_WIFI_SSID));

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
        deb("\n%s: WiFi connection timeout!", credentialValue(CR_WIFI_SSID));
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

          deb("Connected to WiFi %s. Local IP address: %s",
              credentialValue(CR_WIFI_SSID), hardware().getMyIP());
          deb("ping target: %s", credentialValue(CR_MQTT_BROKER_WIREGUARD));
          deb("DNS IP:%s", dns_ip);

          hal_watchdog_feed();
          hardware().drawCenteredText("CONNECTED");

          setWatchdogPhase(WatchdogPhase::NtpSyncStart);
          const hal_status_t ntpStatus =
              hal_time_sync_ntp_ex(credentialValue(CR_NTPSERVER0), nullptr);
          if (ntpStatus != HAL_OK) {
            derr("NTP request failed: %s", hal_status_to_string(ntpStatus));
            reconnect();
            break;
          }

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

        hal_time_status_t timeStatus = {};
        const hal_status_t status = hal_time_get_status_ex(&timeStatus);
        if (status != HAL_OK) {
          derr("Time status failed: %s", hal_status_to_string(status));
          reconnect();
          break;
        }

        if (ntpSyncComplete(timeStatus)) {
          ntpTimeoutTimer.abort();
          setNTPState(STATE_WIREGUARD_CONNECT);
          localTimeHasBeenSet = true;

          setWatchdogPhase(WatchdogPhase::WireguardBegin);

          if (!hal_wireguard_begin_advanced_text(
              getWireguardLocalIP(hardware().getMyMAC()),
              getWireguardPrivateKey(hardware().getMyMAC()),
              credentialValue(CR_WG_ENDPOINT),
              credentialValue(CR_WG_SERVER_PUBLIC_KEY),
              credentialIntValue(CR_WG_ENDPOINT_PORT),
              credentialValue(CR_WG_ALLOWED_IP),
              credentialValue(CR_WG_ALLOWED_MASK))) {
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

        if (timeStatus.ntp_state == HAL_TIME_NTP_FAILED) {
          derr("NTP synchro error: %s",
               hal_status_to_string(timeStatus.last_ntp_status));
          ntpTimeoutTimer.abort();
          reconnect();
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
        mqtt().start(
            credentialValue(CR_MQTT_BROKER_WIREGUARD),
            credentialIntValue(CR_MQTT_BROKER_PORT));
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
          const hal_status_t status =
              hal_time_sync_ntp_ex(credentialValue(CR_NTPSERVER0), nullptr);
          if (status != HAL_OK) {
            derr("NTP resync request failed: %s", hal_status_to_string(status));
          }
        }

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

  deb("now_time:%ld buffer:%s", now_time, buffer);

  hardware().checkConditionsForStartEnAction(now_time);
}

long NTPMachine::getTimeNow(void) {
  return now_time;
}

void NTPMachine::setWatchdogPhase(WatchdogPhase phase) {
  hal_watchdog_feed();
  watchdog.setPhase(phase, currentState);
}

const char* NTPMachine::stateName(int state) {
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
