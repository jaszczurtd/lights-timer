# MQTT-Controlled Relay System (WireGuard + TLS)

This repository implements an IoT system for controlling relay modules from an Android app via MQTT.

The system has **two MQTT access paths**:

- **Private (required for devices):** MQTT over WireGuard on `1883` (no TLS, tunnel provides transport security).
- **Public (for Android without WireGuard):** MQTT over TLS on `8883` (encrypted, certificate-based server identity, optional client certs).

The system consists of three main components:

1. **Android App** – UI to discover devices and toggle relays (MQTT over TLS `8883`).
2. **Raspberry Pi Pico W** – relay controller node (Wi-Fi uplink + WireGuard client + MQTT to `1883`).
3. **Raspberry Pi 5 (Provider)** – WireGuard server, Mosquitto MQTT broker (both `1883` and `8883`), and discovery/provider daemon.

---

## Architecture & Traffic Flow

### Pico W (Wi‑Fi + mandatory WireGuard)
- Pico W devices connect to the local network via **Wi‑Fi**.
- The Raspberry Pi 5 must be in the **same Wi‑Fi/LAN segment** because device discovery uses **UDP broadcast** (local network only).
- Over that uplink, Pico W devices establish a **WireGuard tunnel** to the Pi 5 (`10.8.0.0/24`).
- Pico W devices publish/subscribe MQTT via **`10.8.0.1:1883`** (inside WireGuard).

### Android (no WireGuard required)
- The Android app connects to Mosquitto via **MQTT over TLS on `8883`**.
- This allows Android to work from outside the home network **without requiring WireGuard**.

---

## Network & Security Model

### MQTT on 1883 (internal only)
- `1883` is used by Pico W nodes via WireGuard (and optionally by other trusted hosts on LAN).
- **`1883` must NOT be exposed to the public Internet.**
- Restrict it to:
  - WireGuard subnet (`10.8.0.0/24`) and/or
  - local LAN subnet(s)

### MQTT on 8883 (public, TLS)
- `8883` is exposed for Android clients that do not run WireGuard.
- Traffic is encrypted with TLS.
- Authentication should include:
  - **MQTT username/password**, and optionally
  - **client certificates (mTLS)** if you want per-device certificate control.

---

## Android App

- Written in **Java** (with some **Kotlin** UI utilities).
- Uses **Eclipse Paho MQTT client**.
- Dynamically builds device list based on `AQUA_DEVICES_UPDATE`.
- Subscribes to per-device topics for state updates.
- Supports relay toggling and time-based schedules.

---

## Raspberry Pi Pico W

- Firmware: **C++** (Arduino).
- JSON parsing: `cJSON` (included).
- Key features:
  - Wi‑Fi uplink (Earlephilhower RP2040 core)
  - **WireGuard client using `arduino-wireguard-pico-w`**
  - MQTT subscribe/publish (`PubSubClient`) to `10.8.0.1:1883`
  - NTP time sync (optional but recommended for schedules)
  - state machine
  - OLED display (`Adafruit_SSD1306`)
  - EEPROM persistence
  - hardware watchdog
  - discovery responder (UDP)
  - OTA updates (`ArduinoOTA`) (optional)

**WireGuard library (Pico W):** https://github.com/jaszczurtd/arduino-wireguard-pico-w

---

## Raspberry Pi MQTT Provider

- Written in **C** and runs as a `systemd` service.
- Periodically:
  - broadcasts UDP discovery
  - receives UDP responses from Pico W
  - verifies availability of Pico W devices
  - publishes device list to `AQUA_DEVICES_UPDATE`

---

## MQTT Topics

| Topic                                | Direction       | Payload Example                              | Purpose                   |
|--------------------------------------|-----------------|----------------------------------------------|---------------------------|
| `AQUA_DEVICES_UPDATE`                | Provider → App  | `{"devices":[...]}`                          | Broadcasts device list    |
| `AQUA_DEVICE_SWITCH_SET/{hostname}`  | App → Pico W    | `{"isOn1": true}`                            | Relay toggle              |
| `AQUA_DEVICE_TIME_SET/{hostname}`    | App → Pico W    | `{"dateHourStart": 300, "dateHourEnd": 600}` | Schedule relays           |
| `AQUA_DEVICE_STATUS/{hostname}`      | Pico W → App    | `{"status":"ok",...}`                        | Reports full device state |

---

## Network Requirements

### 1) Local Wi‑Fi / LAN
Pico W nodes must be on the **same local LAN/Wi‑Fi segment as the Pi 5** because discovery uses **UDP broadcast**.

### 2) Required Open Ports on Raspberry Pi 5

| Port  | Protocol | Purpose                                                | Required |
|-------|----------|--------------------------------------------------------|----------|
| 51820 | UDP      | WireGuard server endpoint (Pico W → Pi 5)              | ✅ yes   |
| 1883  | TCP      | MQTT for Pico W / internal clients (WireGuard/LAN only)| ✅ yes*  |
| 8883  | TCP      | MQTT over TLS for Android clients (public)             | ✅ yes   |
| 8266  | TCP      | OTA firmware updates (optional)                        | ⚠️ optional |
| 12345 | UDP      | Device discovery via broadcast (LAN only)              | ✅ yes   |

\* **Security requirement:** `1883` must be reachable **only** from WireGuard and/or LAN. Do not expose it publicly.

---

## Firewall (iptables) Example

Replace `LAN_SUBNET` with your local network CIDR (e.g. `192.168.2.0/24`).

```bash
# WireGuard server
sudo iptables -A INPUT -p udp --dport 51820 -j ACCEPT

# MQTT over TLS (public for Android)
sudo iptables -A INPUT -p tcp --dport 8883 -j ACCEPT

# MQTT plaintext (internal only)
sudo iptables -A INPUT -p tcp --dport 1883 -s 10.8.0.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 1883 -s LAN_SUBNET -j ACCEPT

# (Optional) OTA firmware updates
sudo iptables -A INPUT -p tcp --dport 8266 -j ACCEPT

# UDP discovery (LAN only)
sudo iptables -A INPUT -p udp --dport 12345 -s LAN_SUBNET -j ACCEPT

# Save firewall rules
sudo netfilter-persistent save
```

---

## Setup Instructions

### 1) Mosquitto MQTT Broker on Raspberry Pi

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients libmosquitto-dev libcjson-dev
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

### 2) Mosquitto Configuration

#### 2.1 Passwords (recommended for both 1883 and 8883)

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd your_username
```

#### 2.2 Listener 1883 (internal only)
Bind `1883` to WireGuard (and optionally LAN), and keep it off the public Internet.

Example `/etc/mosquitto/conf.d/internal.conf`:

```ini
# WireGuard-only listener (recommended)
listener 1883 10.8.0.1
allow_anonymous false
password_file /etc/mosquitto/passwd

# (Optional) LAN listener if you have trusted LAN clients:
# listener 1883 192.168.X.Y
# allow_anonymous false
# password_file /etc/mosquitto/passwd
```

#### 2.3 Listener 8883 (TLS for Android)
Example `/etc/mosquitto/conf.d/tls.conf`:

```ini
listener 8883
protocol mqtt

allow_anonymous false
password_file /etc/mosquitto/passwd

# Server TLS (required)
cafile /etc/ssl/certs/ca-certificates.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key

# Optional: require client certificates (mTLS)
# require_certificate true
# use_identity_as_username true
```

Restart Mosquitto:

```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

Quick test (from a WireGuard peer) on `1883`:

```bash
mosquitto_pub -h 10.8.0.1 -p 1883 -u your_username -P your_password -t test -m "Hello over WireGuard"
mosquitto_sub -h 10.8.0.1 -p 1883 -u your_username -P your_password -t test
```

Quick test (TLS) on `8883`:

```bash
mosquitto_pub -h YOUR_PUBLIC_HOST -p 8883 --cafile /etc/mosquitto/certs/ca.crt \
  -u your_username -P your_password -t test -m "Hello over TLS"
```

> For Android, use a certificate chain that the phone trusts (public CA) or bundle/ship your CA certificate if you use a private CA.

---

### 3) MQTT Provider Daemon

```bash
cd lights-timer/RaspberryPi/aqua_topic_provider/
make

# Edit service file (paths, user, etc.)
sudo nano mqtt-devices-provider.service

sudo cp mqtt-devices-provider.service /etc/systemd/system/
sudo systemctl enable mqtt-devices-provider
sudo systemctl start mqtt-devices-provider
```

Example `mqtt-devices-provider.service`:

```ini
[Unit]
Description=MQTT device discovery service
After=network-online.target
Wants=network-online.target

[Service]
ExecStart=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider
WorkingDirectory=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider/provider
Restart=always
RestartSec=5
User=pi

[Install]
WantedBy=multi-user.target
```

---

### 4) Pico W Firmware

- Flash via Arduino IDE (2.3.x) using the **Earlephilhower RP2040** core.
- Configure:
  - Wi‑Fi SSID/password (LAN uplink)
  - WireGuard keys + peer endpoint (server) + allowed IPs (`10.8.0.0/24`)
  - MQTT broker address: **`10.8.0.1`**, port **`1883`**
  - MQTT username/password

---

### 5) Android App

- Open in Android Studio (Gradle sync).
- Supports Android 6.0+.
- Configure broker host/port:
  - `YOUR_PUBLIC_HOST:8883` (TLS)
- Provide MQTT username/password.
- Provide CA/server certificate settings as required by your TLS deployment.

---

## Included Library: cJSON

This project includes https://github.com/DaveGamble/cJSON for lightweight JSON parsing on embedded devices.

License: MIT (see `cJSON/LICENSE`)

---

## Project license

MIT License – see `LICENSE` file.