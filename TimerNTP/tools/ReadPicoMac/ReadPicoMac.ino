#include <JaszczurHAL.h>

namespace {
constexpr unsigned long PRINT_INTERVAL_MS = 5000;
char mac[sizeof("FF:FF:FF:FF:FF:FF")] = {0};
unsigned long lastPrint = 0;

void printMac() {
  if (hal_wifi_get_mac(mac, sizeof(mac))) {
    Serial.print("Pico MAC: ");
    Serial.println(mac);
  } else {
    Serial.println("Pico MAC: unavailable (hal_wifi_get_mac failed)");
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("=== Pico MAC readout ===");
  printMac();
  lastPrint = millis();
}

void loop() {
  if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = millis();
    printMac();
  }
}
