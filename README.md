
# MQTT-Controlled Relay System

This project implements a complete IoT system for controlling relay modules using an Android application via MQTT. The system consists of three main components:

1. **Android App** – graphical user interface for users to manage discovered devices and toggle relays.
2. **Raspberry Pi Pico W** – execution unit that controls physical relays, handles WiFi, MQTT, time sync, and device discovery.
3. **Raspberry Pi (Provider)** – discovery daemon that detects active Pico W units and updates the Android app via MQTT.

---

## 🔐 Network & Security Model

- The system is designed to work **within a WireGuard VPN network** (typically `10.8.0.x`).
- The MQTT broker and Android device are connected via VPN.
- **Pico W connects directly to the broker over LAN** (no WireGuard support in Arduino).
- VPN ensures authentication and encryption of all traffic beyond the local network.

---

## 🟢 Android App

- Written in **Java** and uses **Kotlin** for UI utilities.
- Uses **Eclipse Paho MQTT client**.
- Dynamically builds device list based on `AQUA_DEVICES_UPDATE` topic.
- Subscribes to individual device topics for real-time updates.
- Supports toggling relays and setting time-based schedules.

---

## 🟡 Raspberry Pi Pico W

- Written in **C++**, using **Arduino only for**:
  - `setup()` / `loop()`
  - WiFi management (Earlephilhower core)
  - Some timing functions
- MQTT client implemented manually.
- JSON parsing done using `cJSON` (included).
- Features:
  - WiFi connection with timeout
  - MQTT subscribe/publish
  - NTP time sync
  - EEPROM persistence
  - Hardware watchdog
  - OLED display support
  - Discovery responder (UDP)

---

## 🔴 Raspberry Pi MQTT Provider

- Written in **C** and runs as a `systemd` service.
- Periodically:
  - Broadcasts UDP discovery
  - Receives responses from Pico W
  - Verifies availability via TCP
  - Publishes device list to `AQUA_DEVICES_UPDATE`

---

## 🔄 MQTT Topics

| Topic                                 | Direction        | Payload Example                             | Purpose                    |
|---------------------------------------|------------------|---------------------------------------------|----------------------------|
| `AQUA_DEVICES_UPDATE`                | Provider → App   | `{"devices":[...]}`                         | Broadcasts device list     |
| `AQUA_DEVICE_SWITCH_SET/{hostname}`  | App → Pico W     | `{"isOn1": true}`                           | Relay toggle               |
| `AQUA_DEVICE_TIME_SET/{hostname}`    | App → Pico W     | `{"dateHourStart": 300, "dateHourEnd": 600}`| Schedule relays            |
| `AQUA_DEVICE_STATUS/{hostname}`      | Pico W → App     | `{"status":"ok",...}`                       | Reports full device state  |

---

## 📦 Included Library: cJSON

This project includes [cJSON](https://github.com/DaveGamble/cJSON) for JSON parsing on embedded devices.

> cJSON aims to be the **simplest** and **smallest** possible JSON parser in C that’s also **fully functional**. It supports encoding, decoding, and manipulating JSON data using a lightweight DOM-style tree.

License: MIT (see `cJSON/LICENSE`)

---

## ⚙️ Setup Instructions

### 1. MQTT Broker on Raspberry Pi

```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

### 2. MQTT Provider Daemon

```bash
make
sudo cp mqtt-devices-provider.service /etc/systemd/system/
sudo systemctl enable mqtt-devices-provider
sudo systemctl start mqtt-devices-provider
```

**Example `mqtt-devices-provider.service`:**

```ini
[Unit]
Description=MQTT device discovery service
After=network-online.target
Wants=network-online.target

[Service]
ExecStart=/usr/local/bin/provider
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
Additionally, the Raspberry Pi application must be executed at least once from the console to initialize and store the MQTT broker credentials.

### 3. Pico W Firmware

- Flash via Arduino IDE using the [Earlephilhower RP2040 core](https://github.com/earlephilhower/arduino-pico)
- Set WiFi and MQTT credentials in `Credentials.h`.

### 4. Android App

- Open in Android Studio.
- Supports Android 6.0+.
- MQTT credentials are requested on first launch.

---

## 💡 Future Improvements

- TLS encryption for MQTT.
- OTA firmware update for Pico.
- Enhanced Android-side persistence and control.
- Multi-room/group support.

---

## 📄 License

MIT License – see `LICENSE` file.


---

## 🧩 Required Local Configuration## 🧩 Required Local Configuration (Included as Submodule - --recurse-submodules)

Certain configuration files are essential for compilation and deployment. You must create them manually or add them as a local library in your project. These define credentials and device-specific mappings.

### 🗂 Suggested structure:

- `Credentials.h`
- `Credentials.cpp`
- `MacHostMapping.h`
- `MacHostMapping.cpp`

### 🧾 Example: `Credentials.h`

```cpp
#ifndef CREDENTIALS_C
#define CREDENTIALS_C

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "MacHostMapping.h"

#define ssid "WIFI_SSID"
#define password "WIFI_PASSWORD"

#define MQTT_USER "BROKER_USER_TO_CONFIGURE"
#define MQTT_PASSWORD "BROKER_PASSWORD_TO_CONFIGURE"
#define MQTT_BROKER "192.168.2.100"

#ifdef __cplusplus
extern "C" {
#endif

const char* getFriendlyHostname(const char* mac);
int getSwitchesNumber(const char* mac);

#ifdef __cplusplus
}
#endif

#endif
```

### 🧾 Example: `Credentials.cpp`

```cpp
#include "Credentials.h"

static void normalize_mac(const char* input, char* output, size_t output_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < output_len - 1; i++) {
        if (input[i] != ':') {
            output[j++] = tolower((unsigned char)input[i]);
        }
    }
    output[j] = '\0';
}

static char hostname[32];
static char normalized[20];
static char entry_normalized[20];

const char* getFriendlyHostname(const char* mac) {
    normalize_mac(mac, normalized, sizeof(normalized));
    for (size_t i = 0; i < mac_table_size; i++) {
        normalize_mac(mac_table[i].mac, entry_normalized, sizeof(entry_normalized));
        if (strcmp(entry_normalized, normalized) == 0) {
            return mac_table[i].hostname;
        }
    }
    size_t len = strlen(normalized);
    if (len >= 6) {
        snprintf(hostname, sizeof(hostname), "pico-%s", &normalized[len - 6]);
    } else {
        snprintf(hostname, sizeof(hostname), "pico-%s", normalized);
    }
    return hostname;
}

int getSwitchesNumber(const char* mac) {
    normalize_mac(mac, normalized, sizeof(normalized));
    for (size_t i = 0; i < mac_table_size; i++) {
        normalize_mac(mac_table[i].mac, entry_normalized, sizeof(entry_normalized));
        if (strcmp(entry_normalized, normalized) == 0) {
            return mac_table[i].switches;
        }
    }
    return 0;
}
```

### 🧾 Example: `MacHostMapping.h`

```cpp
#ifndef MACHOSTMAPPING_C
#define MACHOSTMAPPING_C

#pragma once

typedef struct {
    const char* mac;
    const char* hostname;
    int switches;
} MacHostnamePair;

extern const MacHostnamePair mac_table[];
extern const size_t mac_table_size;

#endif
```

### 🧾 Example: `MacHostMapping.cpp`

```cpp
#include "MacHostMapping.h"

const MacHostnamePair mac_table[] = {
    { "28:cd:c1:05:b8:76", "akwarium_duże_w_salonie", 1 },
    { "28:cd:c1:05:b8:64", "akwarium_bojowniki_w_salonie", 2 },
    { "28:cd:c1:05:b8:7a", "akwaria_krewetki_w_sypialni", 3 },
    { "28:cd:c1:05:b8:74", "akwarium_krewetki_w_kuchni", 1 },
    { "28:cd:c1:0e:4b:9d", "akwarium_babki_w_dziupli", 1 },
};

const size_t mac_table_size = sizeof(mac_table) / sizeof(mac_table[0]);
```

⚠️ **Important**: You must update these files to match your own network setup and hardware configuration.
