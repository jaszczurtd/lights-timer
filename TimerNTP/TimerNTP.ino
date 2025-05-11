#include "NTPMachine.h"
#include "MyWebServer.h"
#include "MyHardware.h"
#include "DiscoverMe.h"

#include <WiFiUdp.h>

NTPMachine ntp;
MyWebServer web;
MyHardware hardware;
DiscoverMe discover;

static bool initialized;

void setup() {
  debugInit();
 
  ntp.start(&hardware, &web);
  discover.start(&ntp, &hardware);

  initialized = true;

  deb("System has started.");
}

void loop() {

  if(!initialized) {
    return;
  }

  ntp.stateMachine();
  discover.handleDiscoveryRequests();
  
  delay(1);
}

