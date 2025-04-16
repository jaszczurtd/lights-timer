#include "NTPMachine.h"
#include "MyWebServer.h"
#include "MyHardware.h"

NTPMachine ntp;
MyWebServer web;
MyHardware hardware;

static bool initialized;

void setup() {
  debugInit();
 
  ntp.start(&hardware, &web);

  initialized = true;

  deb("System has started.");
}

void loop() {

  if(!initialized) {
    return;
  }

  ntp.stateMachine();

  delay(1);
}
