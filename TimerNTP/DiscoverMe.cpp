
#include "DiscoverMe.h"

DiscoverMe::DiscoverMe() {  }

void DiscoverMe::start(NTPMachine *n) {
  srand((unsigned int)time_us_64());
  ntp = n;
}

void DiscoverMe::handleDiscoveryRequests() {
  if (!WIFI_CONNECTED) return;

  if (!multicastInitialized) {
    udp.begin(udpPort);
    multicastInitialized = true;
    deb("Multicast has been initialized");
  }

  int packetSize = udp.parsePacket();
  if (packetSize) {
    udp.read(packetBuffer, sizeof(packetBuffer));
    packetBuffer[packetSize] = '\0';

    deb("received discovery packet:%s", packetBuffer);

    if (strcmp(packetBuffer, "PICO_DISCOVER") == 0) {
      unsigned long delayMs = random(5, 250);
      scheduledResponseTime = millis() + delayMs;
      remoteIp = udp.remoteIP();
      remotePort = udp.remotePort();
      pendingResponse = true;
      deb("scheduled response in %lu ms", delayMs);
    }
  }

  if (pendingResponse && millis() >= scheduledResponseTime) {
    String response = "PICO_FOUND|" + 
      WiFi.macAddress() + "|" + 
      WiFi.localIP().toString() + "|" + 
      String(ntp->getMyHostname()) + "|" +
      String(ntp->getAmountOfSwitches());

    udp.beginPacket(remoteIp, remotePort);
    udp.write(response.c_str());
    udp.endPacket();

    pendingResponse = false;
    deb("response sent");
  }
}
