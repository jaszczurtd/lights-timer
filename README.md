# MQTT-Controlled Relay System (WireGuard + TLS)

This repository implements an IoT system for controlling relay modules from an Android app via MQTT.

The system has **two MQTT access paths**:

- **Private (current project path for devices):** MQTT over WireGuard on `1883` (no TLS, tunnel provides transport security).
- **Public (for Android without WireGuard):** MQTT over TLS on `8883` (encrypted, certificate-based server identity, optional client certs).

The system consists of three main components:

1. **Android App** – UI to discover devices and toggle relays (MQTT over TLS `8883`).
2. **Raspberry Pi Pico W** – relay controller node (Wi-Fi uplink + WireGuard client + MQTT to `1883`).
3. **Raspberry Pi 5 (Provider)** – WireGuard server, Mosquitto MQTT broker (both `1883` and `8883`), and discovery/provider daemon.

---

## Architecture & Traffic Flow

### Pico W (Wi‑Fi + current WireGuard path)
- Pico W devices connect to the local network via **Wi‑Fi**.
- The Raspberry Pi 5 must be in the **same Wi‑Fi/LAN segment** because device discovery uses **UDP broadcast** (local network only).
- Over that uplink, Pico W devices establish a **WireGuard tunnel** to the Pi 5 (`10.8.0.0/24`).
- Pico W devices publish/subscribe MQTT via **`10.8.0.1:1883`** (inside WireGuard).

> Design note: WireGuard is not strictly required for Pico W in theory. A direct TLS path using `PubSubClient` + `WiFiSecure` could be used. In the current codebase, this path is not available because JaszczurHAL does not yet support `WiFiSecure` for this stack. WireGuard was selected here because the maintainer's home infrastructure is already WireGuard-based and this project is also a practical testbed for WireGuard implementation on Pico W.

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
- Dynamically builds device list based on topic `discovered/devices`.
- Subscribes to per-device topics for state updates (`status-{hostname}`).
- Supports relay toggling and time-based schedules.

---

## Raspberry Pi Pico W

- Firmware: **C++** (Arduino).
- **Current firmware dependency (required):** [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL)
- In this project, JaszczurHAL covers:
  - WireGuard client/tunnel
  - MQTT transport (`PubSubClient` backend)
  - NTP/time synchronization
  - OLED display (`Adafruit_SSD1306` backend)
  - EEPROM/KV persistence (`hal_eeprom` / `hal_kv`)
  - OTA wrapper (`ArduinoOTA` backend, optional)
  - UDP discovery primitives and watchdog/common hardware APIs
- Key features:
  - Wi‑Fi uplink (Earlephilhower RP2040 core)
  - `cJSON`-based status/config payloads
  - state machine
  - relay control + schedule window logic
  - discovery responder (UDP)
  - hardware watchdog

**JaszczurHAL API reference:** [JaszczurHAL_API.md](https://github.com/jaszczurtd/JaszczurHAL/blob/main/JaszczurHAL_API.md)

---

## Raspberry Pi MQTT Provider

- Written in **C** and runs as a `systemd` service.
- Periodically:
  - broadcasts UDP discovery
  - receives UDP responses from Pico W
  - verifies availability of Pico W devices
  - publishes device list to `discovered/devices`

---

## MQTT Topics

| Topic                                | Direction       | Payload Example                              | Purpose                   |
|--------------------------------------|-----------------|----------------------------------------------|---------------------------|
| `discovered/devices`                 | Provider -> App  | `{"devices":[...]}`                          | Broadcasts device list    |
| `switch-{hostname}`                  | App -> Pico W    | `{"isOn1": true}`                            | Relay toggle              |
| `time-{hostname}`                    | App -> Pico W    | `{"dateHourStart": 300, "dateHourEnd": 600}` | Schedule relays           |
| `status-{hostname}`                  | Pico W -> App    | `{"status":"ok",...}`                        | Reports full device state |

> Note: topic names above are the literal values used by current code.

---

## Network Requirements

### 1) Local Wi‑Fi / LAN
Pico W nodes must be on the **same local LAN/Wi‑Fi segment as the Pi 5** because discovery uses **UDP broadcast**.

### 2) Required Open Ports on Raspberry Pi 5

| Port  | Protocol | Purpose                                                | Required |
|-------|----------|--------------------------------------------------------|----------|
| 51820 | UDP      | WireGuard server endpoint (Pico W -> Pi 5)              | ✅ yes   |
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
Description=MQTT devices provider
After=network-online.target wg-quick@wg0.service
Requires=network-online.target wg-quick@wg0.service

[Service]
ExecStart=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider/provider
WorkingDirectory=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider
Restart=always
RestartSec=5
User=pi

[Install]
WantedBy=multi-user.target
```

---

### 4) Pico W Firmware

- The current firmware workflow is **VS Code-first**. Build/upload is driven by project tasks and scripts from `.vscode/` and `scripts/` in `TimerNTP`.
- Arduino IDE build/upload is currently **not** a maintained workflow for this repository.
- Open `TimerNTP` as your workspace folder in VS Code.
- Prerequisites:
  - `arduino-cli` available in PATH
  - Earlephilhower RP2040 core installed for `arduino-cli`
  - Python 3 (used by monitor/upload helper scripts)
- Use provided VS Code tasks (Command Palette -> Tasks: Run Task):
  - `Build: Select board (GUI)` or `Build: Select board (interactive)`
  - `Build: Build`
  - `Build: Upload` (serial)
  - `Build: Upload (UF2 / BOOTSEL)`
  - `Build: Monitor (persistent)`
  - `Build: Refresh IntelliSense`
- Available helper scripts are in `TimerNTP/scripts` (for example `select-board.sh`, `upload-uf2.sh`, `refresh-intellisense.sh`, `serial-persistent.py`).

#### Credentials Module (Anonymized Template)

- This repository includes a ready-to-fill Credentials library template in `libraries/Credentials`.
- For firmware builds, the library must be available in your Arduino sketchbook path (`arduino.sketchbookPath`) as `libraries/Credentials`.
- Fill these files before first upload:
  - `libraries/Credentials/Credentials.h`: Wi‑Fi, MQTT, WireGuard, NTP and OTA constants
  - `libraries/Credentials/MacHostMapping.cpp`: per-device MAC mapping (hostname, relay count, WireGuard local IP, private key)
  - `libraries/Credentials/ca_cert.c`: CA certificate (PEM)
- Keep real secrets outside version control (do not commit production credentials/keys).

#### MAC Readout (for MacHostMapping)

- A helper sketch is provided at `TimerNTP/tools/ReadPicoMac/ReadPicoMac.ino`.
- Purpose: print board MAC over serial so you can add/update entries in `libraries/Credentials/MacHostMapping.cpp`.
- Quick usage:
  - connect Pico/Pico W over USB
  - compile/upload the helper sketch ising Arduino IDE and Earlephilhower RP2040 core
  - open serial monitor at `115200`
  - copy the value printed as `Pico MAC: XX:XX:XX:XX:XX:XX` (of course without Pico MAC: at the begining)
  - paste it into a new row in `mac_table`

---

### 5) Android App

- Open in Android Studio (Gradle sync).
- Supports Android 7.0+ (minSdk 24).
- Configure broker host:
  - `YOUR_PUBLIC_HOST` (or IP address, **without port**)
  - app always connects using `ssl://<broker>:8883`
- Provide MQTT username/password.
- Add CA certificate file for TLS pinning:
  - place certificate at `Android/app/src/main/res/raw/ca.crt` (loaded as `R.raw.ca`)
  - this file is intentionally gitignored; provide it locally per environment

---

## cJSON Usage

- Firmware uses cJSON API through JaszczurHAL.
- Raspberry Pi provider links against system `libcjson` (`libcjson-dev` on Debian/Ubuntu).

---

## Project license

MIT License – see `LICENSE` file.