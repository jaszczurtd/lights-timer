
#include "DiscoverMe.h"

DiscoverMe::DiscoverMe() {  }

void DiscoverMe::start(NTPMachine *n) {
  ntp = n;
}

void DiscoverMe::handleDiscoveryRequests() {

  if(WIFI_CONNECTED) {
    if(!multicastInitialized) {
      udp.begin(udpPort);
  
      multicastInitialized = true;
      deb("Multicast has been initialized");
    }

    if(multicastInitialized) {
      int packetSize = udp.parsePacket();
      if (packetSize) {
        udp.read(packetBuffer, sizeof(packetBuffer));
        packetBuffer[packetSize] = '\0';

        deb("received discovery packet:%s", packetBuffer);

        if (strcmp(packetBuffer, "PICO_DISCOVER") == 0) {
          delay(random(5, 50));

          String response = "PICO_FOUND|" + 
            WiFi.macAddress() + "|" + 
            WiFi.localIP().toString() + "|" + 
            String(ntp->getMyHostname()) + "|" +
            String(ntp->getAmountOfSwitches())
          ;
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.write(response.c_str());
          udp.endPacket();
        }
      }
    }
  }
}

