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

> Design note: JaszczurHAL supports MQTTS through its HAL MQTT and BearSSL
> transport, so a direct TLS device path is technically available. TimerNTP
> intentionally uses WireGuard because the deployment infrastructure is
> WireGuard-based and the firmware serves as a practical testbed for the
> JaszczurHAL tunnel implementation. Moving the device path to MQTTS would also
> require corresponding credential, broker-routing, and firmware configuration
> changes.

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

- Firmware: **C++** using the portable JaszczurHAL `app_start()` / `app_task0()`
  contract.
- **Current firmware dependency (required):** [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL)
- Native build/runtime: official Pico SDK with CYW43/lwIP, selected through the
  JaszczurHAL `rp2040` target and `picow` board profile.
- In this project, JaszczurHAL covers:
  - WireGuard client/tunnel
  - MQTT transport over the HAL TCP stack
  - NTP/time synchronization
  - SSD1306 OLED display over HAL I2C
  - EEPROM/KV persistence (`hal_eeprom` / `hal_kv`)
  - authenticated native OTA with trial confirmation and rollback
  - UDP discovery primitives and watchdog/common hardware APIs
  - DS18B20 temperature acquisition
- Key features:
  - Wi-Fi uplink through the native CYW43/lwIP stack
  - `cJSON`-based status/config payloads
  - state machine
  - relay control + schedule window logic
  - discovery responder (UDP)
  - hardware watchdog

**JaszczurHAL API reference:** [JaszczurHAL_API.md](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/JaszczurHAL_API.md)

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

- The firmware workflow is **VS Code-first**. Build and upload are driven by
  JaszczurHAL's shared `jh-vscode` entrypoint through project tasks in
  `TimerNTP/.vscode/`.
- Open `TimerNTP` as your workspace folder in VS Code.
- Prerequisites:
  - [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL) checked out at
    `../libraries/JaszczurHAL` relative to this repository
  - JaszczurHAL setup completed with `./runmefirst.sh`; it prepares the pinned
    Pico SDK, RP toolchain, picotool, and remaining managed dependencies
  - Python 3 for the shared monitor and upload tooling
- The tracked manifest selects target `rp2040` and board `picow`. Target/board
  overrides are stored locally in `TimerNTP/.vscode/jaszczurhal.local.json`.
- Use provided VS Code tasks (Command Palette -> Tasks: Run Task):
  - `Project: Build`
  - `Project: Build (Debug)`
  - `Project: Upload` (serial)
  - `Project: Upload (UF2 / BOOTSEL)`
  - `Project: Upload (OTA)`
  - `Project: Discover OTA devices`
  - `Project: Serial Monitor`
  - `Project: Refresh IntelliSense`
  - `Project: Clear USB Identity`
- From the `TimerNTP` directory, the equivalent command-line build is:

  ```bash
  ../../libraries/JaszczurHAL/vscode/entry/jh-vscode build --project .
  ```

- Main artifacts are copied to `TimerNTP/.build/`: `firmware.elf`,
  `firmware.bin`, `firmware.uf2`, and `firmware.ota`. The signed OTA container
  is generated when the OTA upload workflow receives
  `TIMER_NTP_OTA_PASSWORD`.

#### Credentials setup

Firmware links the external precompiled library at
`../libraries/Credentials/src/cortex-m0plus/libCredentials.a`. The author's
library is private and is not distributed in this repository. The tracked
`libraries/Credentials` directory is a complete replacement template with the
same API, but without author secrets or private encoding code.

If `../libraries/Credentials` already exists, it is used unchanged. For a fresh
checkout, run from the `lights-timer` directory:

```bash
./scripts/setup-credentials.sh
```

The script refuses to overwrite an existing library. It creates private files:

```text
../libraries/Credentials/config/CredentialsData.local.h
../libraries/Credentials/config/MacHostMapping.local.cpp
```

Replace all placeholders, set `CREDENTIALS_LOCAL_CONFIGURED` to `1`, then run:

```bash
../libraries/Credentials/scripts/build.sh rp2040
```

The `*.local.*` files and generated archives are ignored by Git. Detailed
instructions are in `libraries/Credentials/README.md`.

#### MAC Readout (for MacHostMapping)

Each device entry in
`../libraries/Credentials/config/MacHostMapping.local.cpp` needs the Pico W
factory MAC, hostname, relay count, WireGuard address, and private key.

Obtain the MAC from the Wi-Fi access point's client/DHCP table after the board
starts its station connection, or use a small native JaszczurHAL diagnostic
that calls:

```cpp
char mac[18] = {};
hal_wifi_get_mac(mac, sizeof(mac));
```

The JaszczurHAL `15_wifi` example provides a ready native Wi-Fi diagnostic.
MAC matching in the Credentials template ignores separators and letter case.
After adding the entry, rebuild the Credentials archive and TimerNTP firmware.

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
