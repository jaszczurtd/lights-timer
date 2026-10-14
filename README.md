# Lights Timer

Lights Timer controls up to four relays on a Raspberry Pi Pico W from an Android
app or local buttons. A daily schedule controls the first relay. The controller
also reads a DS18B20 temperature sensor and shows its state on an SSD1306 display.

The system has three parts:

- **TimerNTP firmware** connects the Pico W to Wi-Fi, synchronizes its clock,
  and exchanges MQTT messages through WireGuard.
- **Raspberry Pi server** runs WireGuard, Mosquitto and a device provider. The
  provider discovers controllers on the local network and publishes their list.
- **Android app** connects to Mosquitto over TLS, displays the discovered
  controllers and sends relay and schedule commands.

Controllers and the provider must share a LAN segment for UDP discovery. Android
can connect from outside that network. The daily schedule uses CET/CEST and must
fit within one day; see the [MQTT and schedule reference](doc/en/mqtt.md).

## Setup

Follow the guides in this order:

1. Prepare [the network and MQTT broker](doc/en/network.md).
2. Build and configure [the device provider](RaspberryPi/aqua_topic_provider/README.md).
3. Prepare and upload [the Pico W firmware](TimerNTP/README.md); that guide
   includes workstation preparation and [Credentials setup](libraries/Credentials/README.md).
4. Configure and run [the Android app](Android/README.md).

Firmware uses [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL) and the
official Pico SDK. Its setup guide describes the required checkout layout,
device tree, VS Code tasks, command-line workflow and OTA updates.

## License

The project is distributed under the [MIT License](LICENSE).
