
#include <hal/hal.h>
#include <cstdlib>
#include "DiscoverMe.h"
#include "Logic.h"
#include "NTPMachine.h"
#include "MyHardware.h"

MyHardware& DiscoverMe::hardware() { return logic.hardwareObj(); }

void DiscoverMe::start(void) {

  const char* mac = hardware().getMyMAC();
  unsigned long seed = hal_micros64();

  for (size_t i = 0; i < strlen(mac); i++) {
    seed ^= mac[i] << (i % 4);
  }

  std::srand((unsigned int)seed);
}

void DiscoverMe::handleDiscoveryRequests() {
  if (!hal_wifi_is_connected()) {
    pendingResponse = false;
    if (multicastInitialized) {
      hal_udp_stop();
      multicastInitialized = false;
    }
    return;
  }

  if (!multicastInitialized) {
    if (!hal_udp_begin(UDP_PORT)) {
      deb("UDP begin failed on port %d", UDP_PORT);
      return;
    }
    multicastInitialized = true;
    deb("\nStart!\nMulticast on port %d has been initialized", UDP_PORT);
    deb("build datetime: %s", BuildDateTime);
  }

  int packetSize = hal_udp_parse_packet();
  if (packetSize) {
    if (packetSize < 0 || (size_t)packetSize >= sizeof(packetBuffer)) {
      deb("Received packet too large (%d bytes), ignoring", packetSize);
      return;
    }

    int n = hal_udp_read((uint8_t *)packetBuffer, (uint16_t)packetSize);
    if (n < 0) return;
    packetBuffer[n] = '\0';

    deb("received discovery packet:%s", packetBuffer);

    if (strcmp(packetBuffer, DISCOVER_PACKET) == 0) {
      unsigned long delayMs = 5UL + (unsigned long)(std::rand() % (300 - 5));
      scheduledResponseTime = hal_millis() + delayMs;
      bool hasRemoteIp = hal_udp_remote_ip(remoteIp, sizeof(remoteIp));
      remotePort = hal_udp_remote_port();

      pendingResponse = hasRemoteIp && (remotePort > 0);
      if (pendingResponse) {
        deb("scheduled response in %lu ms", delayMs);
      } else {
        deb("discovery packet without valid remote endpoint, ignoring");
      }
    }
  }

  if (pendingResponse && hal_millis() >= scheduledResponseTime) {
    char response[128];
    snprintf(response, sizeof(response), "%s|%s|%s|%s|%s",
      RESPONSE_PACKET,
      hardware().getMyMAC(),
      hardware().getMyIP(),
      hardware().getMyHostname(),
      hardware().getAmountOfSwitches());

    bool sent = false;
    if (hal_udp_begin_packet(remoteIp, remotePort)) {
      hal_udp_write_str(response);
      sent = hal_udp_end_packet();
    }

    pendingResponse = false;
    deb(sent ? "response sent" : "response failed");
  }
}
