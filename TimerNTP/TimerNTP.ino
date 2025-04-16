#include "NTPMachine.h"
#include "MyWebServer.h"
#include "MyHardware.h"

#include <WiFiUdp.h>

NTPMachine ntp;
MyWebServer web;
MyHardware hardware;

static bool initialized;

const char* multicastIP = "239.255.255.250";
const int udpPort = 12345;

WiFiUDP udp;
unsigned long nextResponseTime = 0;
bool responsePending = false;
IPAddress responderIP;
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
      udp.beginMulticast(WiFi.localIP(), IPAddress().fromString(multicastIP), udpPort);
      multicastInitialized = true;
      deb("Multicast has been initialized");
    }

    char packet[32];
    int packetSize = udp.parsePacket();
    
    if (packetSize > 0 && udp.read(packet, sizeof(packet)) > 0) {
      if (strcmp(packet, "DISCOVER") == 0) {
        nextResponseTime = millis() + random(50);
        responderIP = udp.remoteIP();
        responsePending = true;
      }
    }

    if (responsePending && millis() >= nextResponseTime) {
      sendResponse();
      responsePending = false;
    }
  }

}

void sendResponse() {
  char response[64];
  snprintf(response, sizeof(response), 
    "PICO|%s|%s",
    WiFi.macAddress().c_str(),
    WiFi.localIP().toString().c_str()
  );
  
  udp.beginPacket(responderIP, udp.remotePort());
  udp.write(response);
  udp.endPacket();
}
