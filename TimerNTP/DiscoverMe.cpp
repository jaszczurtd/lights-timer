
#include "DiscoverMe.h"
#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"

MyHardware& DiscoverMe::hardware() { return logic.hardwareObj(); }

void DiscoverMe::start(void) {

  const char* mac = hardware().getMyMAC();
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
    if (!udp.begin(UDP_PORT)) {
      deb("UDP begin failed on port %d", UDP_PORT);
      return;
    }
    multicastInitialized = true;
    deb("\nStart!\nMulticast on port %d has been initialized", UDP_PORT);
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

    if (strcmp(packetBuffer, DISCOVER_PACKET) == 0) {
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
    snprintf(response, sizeof(response), "%s|%s|%s|%s|%s",
      RESPONSE_PACKET,
      hardware().getMyMAC(),
      hardware().getMyIP(),
      hardware().getMyHostname(),
      hardware().getAmountOfSwitches());

    udp.beginPacket(remoteIp, remotePort);
    udp.write(response);
    udp.endPacket();

    pendingResponse = false;
    deb("response sent");
  }
}
