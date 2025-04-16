
#include "MyHardware.h"

MyHardware::MyHardware() { }

void MyHardware::start(NTPMachine *m) {
  ntp = m;
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);
}

void MyHardware::updateBuildInLed(void) {
  static unsigned long last_blink;
  static int prevState = -1;

  if (ntp->getCurrentState() != prevState) {
    last_blink = millis();
    digitalWrite(LED_BUILTIN, LOW);
    prevState = ntp->getCurrentState();
  }

  switch(ntp->getCurrentState()) {
    case STATE_CONNECTING:
    case STATE_NTP_SYNCHRO: {
      unsigned long interval = (ntp->getCurrentState() == STATE_CONNECTING) ? 100 : 300;
      if (millis() - last_blink > interval) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        last_blink = millis();
      }
    }
    break;
      
    case STATE_CONNECTED: {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    break;
      
    default: {
      digitalWrite(LED_BUILTIN, LOW);
    }
    break;
  }
}

