#include "NTPMachine.h"
#include "MyWebServer.h"
#include "MyHardware.h"

#include <WiFiUdp.h>

NTPMachine ntp;
MyWebServer web;
MyHardware hardware;

static bool initialized;

const int udpPort = 12345;
char packetBuffer[255];
WiFiUDP udp;
bool multicastInitialized = false;


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

  handleDiscoveryRequests();
  
  delay(1);
}

void handleDiscoveryRequests() {

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

        if (strcmp(packetBuffer, "PICO_DISCOVER") == 0) {
          String response = "PICO_FOUND|" + WiFi.macAddress() + "|" + WiFi.localIP().toString();
          udp.beginPacket(udp.remoteIP(), udp.remotePort());
          udp.write(response.c_str());
          udp.endPacket();
        }
      }

    }
  }

}

