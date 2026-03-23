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
        if(WIFI_CONNECTED) {
          WiFi.setTimeout(MAX_TIMEOUT);

          deb("Connected to WiFi. Local IP address: %s", hardware().getMyIP());
          deb("ping target: %s", srv1.c_str());
          deb("DNS IP:%s", WiFi.dnsIP().toString().c_str());

          hal_watchdog_feed();
          hardware().drawCenteredText("CONNECTED");
          hardware().configureOTAUpdates();

          setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
          tzset();
          configTime(0, 0, ntpServer0, nullptr);

          ntpTimeoutTimer.begin(nullptr, NTP_TIMEOUT_MS);
          currentState = STATE_NTP_SYNCHRO;
        }
      }
    }
    break;

    case STATE_NTP_SYNCHRO: {
      if (WIFI_CONNECTED) {
        hardware().drawCenteredText("NTP SYNCHRO");

        if(time(nullptr) > 24 * 3600 * 2) {
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
      if (WIFI_CONNECTED) {

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
      if (WIFI_CONNECTED) {
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
      if (WIFI_CONNECTED) {

        if (ntpReSyncTimer.available()) {
          ntpReSyncTimer.restart();
          configTime(0, 0, ntpServer0, nullptr);
        }

        if (pingTimer.available()) {
          pingTimer.restart();

          unsigned long t_ping = hal_millis();
          int res1 = WiFi.ping(ping1Target);
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
  if (WIFI_CONNECTED && isBrokerAvailable()) {  
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
    if (WIFI_CONNECTED && currentState >= STATE_CONNECTED) {
      deb("%s, IP:%s, wg IP:%s, host:%s, mac:%s, heap:%ld bytes, wifi: %d/5",
          getTimeFormatted(),
          hardware().getMyIP(),
          getWireguardLocalIP(hardware().getMyMAC()),
          getFriendlyHostname(hardware().getMyMAC()),
          hardware().getMyMAC(),
          hal_get_free_heap(),
          hardware().getWifiStrength());
    }
  }
}

void NTPMachine::evaluateTimeCondition() {
  time_t now;
  struct tm timeinfo;

  time(&now);
  localtime_r(&now, &timeinfo);
  
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);

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


