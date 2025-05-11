
#include "DiscoverMe.h"

DiscoverMe::DiscoverMe() {  }

void DiscoverMe::start(NTPMachine *n, MyHardware *h) {
  ntp = n;
  hardware = h;

  const char* mac = hardware->getMyMAC();
  unsigned long seed = time_us_64();

  for (size_t i = 0; i < strlen(mac); i++) {
    seed ^= mac[i] << (i % 4);
  }

  srand((unsigned int)seed);
}

void DiscoverMe::handleDiscoveryRequests() {
  if (!WIFI_CONNECTED) {
    pendingResponse = false;
    return;
  }

  if (!multicastInitialized) {
    if (!udp.begin(udpPort)) {
      deb("UDP begin failed on port %d", udpPort);
      return;
    }
    multicastInitialized = true;
    deb("Multicast on port %d has been initialized", udpPort);
  }

  int packetSize = udp.parsePacket();
  if (packetSize) {
    if (packetSize < 0 || (size_t)packetSize >= sizeof(packetBuffer)) {
      deb("Received packet too large (%d bytes), ignoring", packetSize);
      return;
    }

    udp.read(packetBuffer, sizeof(packetBuffer));
    packetBuffer[packetSize] = '\0';

    deb("received discovery packet:%s", packetBuffer);

    if (strcmp(packetBuffer, "AQUA_DISCOVER") == 0) {
      unsigned long delayMs = random(5, 300);
      scheduledResponseTime = millis() + delayMs;
      remoteIp = udp.remoteIP();
      remotePort = udp.remotePort();
      pendingResponse = true;
      deb("scheduled response in %lu ms", delayMs);
    }
  }

  if (pendingResponse && millis() >= scheduledResponseTime) {
    char response[128];
    snprintf(response, sizeof(response), "AQUA_FOUND|%s|%s|%s|%s",
      hardware->getMyMAC(),
      hardware->getMyIP(),
      hardware->getMyHostname(),
      hardware->getAmountOfSwitches());

    udp.beginPacket(remoteIp, remotePort);
    udp.write(response);
    udp.endPacket();

    pendingResponse = false;
    deb("response sent");
  }
}
