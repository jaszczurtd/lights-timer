#include "Credentials.h"
#include <hal/hal.h>
#include "NTPMachine.h"
#include "Logic.h"
#include "MyHardware.h"
#include "MQTTClient.h"

MyHardware& NTPMachine::hardware() { return logic.hardwareObj(); }
MQTTClient& NTPMachine::mqtt() { return logic.mqttObj(); }

void NTPMachine::start() {
  currentState = STATE_NOT_CONNECTED;

  ping1Target.fromString(MQTT_BROKER);
  srv1 = ping1Target.toString();

  hardware().start();

  long s = 0, e = 0;
  hardware().loadStartEnd(&s, &e);
  hardware().loadSwitches();
  hardware().extractTime(s, e);
  hardware().applyRelays();

  evaluateRelayTimer.begin(nullptr, EVALUATE_TIME_FOR_RELAY_MS);
  loopLogTimer.begin(nullptr, PRINT_INTERVAL_MS);
}

int NTPMachine::getCurrentState(void) {
  return currentState;
}

const char *NTPMachine::getTimeFormatted(void) {
  return (const char *)buffer;
}

void NTPMachine::reconnect(void) {
  failedPingsCNT = 0;
  currentState = STATE_NOT_CONNECTED;
  hardware().drawCenteredText("NO CONNECTION");
}

void NTPMachine::stateMachine(void) {

  switch(currentState) {
    case STATE_NOT_CONNECTED: {
      deb("Not connected to WiFi. Trying to reconnect...");

      memset(buffer, 0, sizeof(buffer));

      mqtt().stop();

      hardware().restartWiFi();
      hardware().drawCenteredText("CONNECTING...");

      wifiTimeoutTimer.begin(nullptr, WIFI_TIMEOUT_MS);
      connectingPollTimer.begin(nullptr, 200);

      currentState = STATE_CONNECTING;
    }
    break;

    case STATE_CONNECTING: {

      if (wifiTimeoutTimer.available()) {
        deb("\nWiFi connection timeout!");
        wifiTimeoutTimer.abort();
        reconnect();
        break;
      }

      if (connectingPollTimer.available()) {
        connectingPollTimer.restart();
        if(hal_wifi_is_connected()) {
          hal_wifi_set_timeout_ms(MAX_TIMEOUT);

          char dns_ip[32] = {0};
          if (!hal_wifi_get_dns_ip(dns_ip, sizeof(dns_ip))) {
            snprintf(dns_ip, sizeof(dns_ip), "%s", "0.0.0.0");
          }

          deb("Connected to WiFi. Local IP address: %s", hardware().getMyIP());
          deb("ping target: %s", srv1.c_str());
          deb("DNS IP:%s", dns_ip);

          hal_watchdog_feed();
          hardware().drawCenteredText("CONNECTED");
          hardware().configureOTAUpdates();

          hal_time_set_timezone("CET-1CEST,M3.5.0/2,M10.5.0/3");
          hal_time_sync_ntp(ntpServer0, nullptr);

          ntpTimeoutTimer.begin(nullptr, NTP_TIMEOUT_MS);
          currentState = STATE_NTP_SYNCHRO;
        }
      }
    }
    break;

    case STATE_NTP_SYNCHRO: {
      if (hal_wifi_is_connected()) {
        hardware().drawCenteredText("NTP SYNCHRO");

        if (hal_time_is_synced(24UL * 3600UL * 2UL)) {
          ntpTimeoutTimer.abort();
          currentState = STATE_WIREGUARD_CONNECT;
          localTimeHasBeenSet = true;

          deb("Starting WireGuard...");

          hal_watchdog_feed();

          IPAddress localIP, allowedIP, allowedMask;

          localIP.fromString(getWireguardLocalIP(hardware().getMyMAC()));
          allowedIP.fromString(WG_ALLOWED_IP);
          allowedMask.fromString(WG_ALLOWED_MASK);

          if (!wg.beginAdvanced(
              localIP,
              getWireguardPrivateKey(hardware().getMyMAC()),
              WG_ENDPOINT,
              WG_SERVER_PUBLIC_KEY,
              WG_ENDPOINT_PORT,
              allowedIP,
              allowedMask
            )) {
            deb("WireGuard initialization failed.");
            reconnect();
            break;
          }
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
      if (hal_wifi_is_connected()) {

        if (wgHandshakeTimer.available()) {
          wgHandshakeTimer.restart();
          if (!wg.peerUp()) {
            IPAddress kicker;
            kicker.fromString(getWireguardLocalIP(hardware().getMyMAC()));
            wg.kickHandshake(kicker, 9);
            deb("WG not ready yet (no session key). Handshake kicked.");
            break;
          }
          currentState = STATE_WIREGUARD_CONNECTED;
        }
      } else {
        reconnect();
      }
    }
    break;

    case STATE_WIREGUARD_CONNECTED: {
      if (hal_wifi_is_connected()) {
        currentState = STATE_CONNECTED;

        mqtt().start(MQTT_BROKER_WIREGUARD, MQTT_BROKER_PORT);
        hardware().clearDisplay();
        hal_watchdog_feed();

        ntpReSyncTimer.begin(nullptr, (unsigned long)HOURS_SYNC_INTERVAL * 3600 * 1000UL);
        pingTimer.begin(nullptr, NEXT_PING_TIME);

        break;

      } else {
        reconnect();
      }
    }
    break;

    case STATE_CONNECTED: {
      if (hal_wifi_is_connected()) {

        if (ntpReSyncTimer.available()) {
          ntpReSyncTimer.restart();
          hal_time_sync_ntp(ntpServer0, nullptr);
        }

        if (pingTimer.available()) {
          pingTimer.restart();

          unsigned long t_ping = hal_millis();
          int res1 = hal_wifi_ping(srv1.c_str());
          dt1 = hal_millis() - t_ping;
          hal_watchdog_feed();

          isBAvailable = res1 >= 0;
          if(isBAvailable) {
            if(failedPingsCNT > 0) {
              failedPingsCNT--;
            }
          } else {
            if(++failedPingsCNT > MAX_FAILED_PINGS) {
              reconnect();
            }
          }

          deb("ping test:%ldms isBrokerAvailable:%s failed pings:%d",
              lastBrokerRespoinsePingTime(), (isBrokerAvailable()) ? "true" : "false",
              failedPingsCNT);
        }

        mqtt().handleMQTTClient();
        hardware().updateDisplayInNormalOperationMode();
        hardware().hardwareLoop();

      } else {
        reconnect();
      }
    }
    break;
  }

  hardware().updateBuildInLed();
  if (hal_wifi_is_connected()) {
    hardware().handleOTAUpdates();
  }

  if (evaluateRelayTimer.available()) {
    evaluateRelayTimer.restart();
    if(localTimeHasBeenSet) {
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

bool NTPMachine::isBrokerAvailable(void) {
  return isBAvailable;
}

unsigned long NTPMachine::lastBrokerRespoinsePingTime(void) {
  return dt1;
}


