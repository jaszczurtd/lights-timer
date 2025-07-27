
# MQTT-Controlled Relay System

This project implements a simple(?) IoT system for controlling relay modules using an Android application via MQTT (TLS/x509). The system consists of three main components:

1. **Android App** – graphical user interface for users to manage discovered devices and toggle relays.
2. **Raspberry Pi Pico W** – execution unit that controls physical relays, handles WiFi, MQTT, time sync, and device discovery.
3. **Raspberry Pi (Provider)** – MQTT mosquitto broker, and discovery daemon that detects active Pico W units and updates the Android app via MQTT.

---

## 🔐 Network & Security Model

- The system is designed to work **within a mosquitto over TLS/x509 certificates**.
- Raspberry Pi/mosquitto requires external IP for communication (and opened port 8883).

---

## 🟢 Android App

- Written in **Java** and uses **Kotlin** for UI utilities.
- Uses **Eclipse Paho MQTT client**.
- Dynamically builds device list based on `AQUA_DEVICES_UPDATE` topic.
- Subscribes to individual device topics for real-time updates.
- Supports toggling relays and setting time-based schedules.

---

## 🟡 Raspberry Pi Pico W

  - Firmware is written in **C++**,
  - JSON parsing is done using `cJSON` (included).
- Features:
  - WiFiClientSecure + x509 (Earlephilhower core)
  - NTP time sync
  - state machine
  - OLED display (Adafruit_SSD1306)
  - MQTT subscribe/publish (PubSubClient)
  - EEPROM persistence
  - Hardware watchdog
  - Discovery detect & responder (UDP)
  - OTA updates (ArduinoOTA)

---

## 🔴 Raspberry Pi MQTT Provider

- Written in **C** and runs as a `systemd` service.
- Periodically:
  - Broadcasts UDP discovery
  - Receives UDP responses from Pico W
  - Verifies the availability of Pico W devices (TCP connection in order to keep only active devices on the list)
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

## 🌐 Network Requirements

For the system to operate correctly, the local network and Raspberry Pi must be properly configured:

### 1. Local Network Configuration

- **WiFi local network must allow broadcast UDP traffic.**

### 2. Required Open Ports on Raspberry Pi 5

| Port  | Protocol | Purpose                        | Required |
|-------|----------|--------------------------------|----------|
| 8883  | TCP      | MQTT over TLS                  | ✅ yes   |
| 1883  | TCP      | MQTT without TLS (optional)    | ⚠️ optional |
| 8266  | TCP      | OTA firmware updates (optional)| ⚠️ optional |
| 12345 | UDP      | Device discovery via broadcast | ✅ yes   |

### 3. Example: Opening Required Ports with iptables

```bash
# MQTT over TLS
sudo iptables -A INPUT -p tcp --dport 8883 -j ACCEPT

# (Optional) Plain MQTT
sudo iptables -A INPUT -p tcp --dport 1883 -j ACCEPT

# (Optional) OTA firmware updates
sudo iptables -A INPUT -p tcp --dport 8266 -j ACCEPT

# UDP broadcast discovery
sudo iptables -A INPUT -p udp --dport 12345 -j ACCEPT

# Save firewall rules
sudo netfilter-persistent save
```
> ⚠️ If you want to use a different port for discovery or OTA, adjust the rules accordingly.  
> All MQTT clients (Pico W and Android app) must be able to reach the broker at TCP port 8883.

## ⚙️ Setup Instructions

### 1. MQTT Broker on Raspberry Pi and required libraries to build the provider app

```bash
sudo apt update
sudo apt install libmosquitto-dev libmosquitto1 mosquitto-clients
sudo apt-get install libcjson-dev
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

## 🔐 Mosquitto MQTT Broker – TLS Setup

To enable secure MQTT communication, you must configure Mosquitto to use TLS with your own x.509 certificate authority (CA). Here's a **generalized and anonymized** setup procedure:

### 1. Open Firewall Port
```bash
sudo iptables -A INPUT -p tcp --dport 8883 -j ACCEPT
sudo netfilter-persistent save
```

### 2. Create Directory Structure for certificates generation process
```bash
mkdir -p ~/mqtt-certs/{ca,server,config}
cd ~/mqtt-certs
```

### 3. Generate Certificates using OpenSSL

Edit your OpenSSL configuration file (`config/server-openssl.cnf`):
```ini
[req]
default_bits       = 2048
prompt             = no
default_md         = sha256
req_extensions     = req_ext
distinguished_name = dn

[dn]
C = YOUR_COUNTRY
ST = YOUR_STATE
L = YOUR_CITY
O = YOUR_ORG
OU = YOUR_UNIT
CN = your.mqtt.domain
emailAddress = your@email.com

[req_ext]
subjectAltName = @alt_names

[alt_names]
IP.1 = your.internal.ip
DNS.1 = your.mqtt.domain
```
⚠️ The CN field must contain the same domain (or IP address) where the MQTT broker is hosted!
Otherwise, certificate verification will fail and Mosquitto will reject the connection.

Then execute:
```bash
# Certificate Authority
openssl genrsa -out ca/ca.key 4096
openssl req -x509 -new -nodes -key ca/ca.key -sha256 -days 3650 -out ca/ca.crt -config config/server-openssl.cnf

# Server Certificate
openssl genrsa -out server/server.key 2048
openssl req -new -key server/server.key -out server/server.csr -config config/server-openssl.cnf
openssl x509 -req -in server/server.csr -CA ca/ca.crt -CAkey ca/ca.key -CAcreateserial \
-out server/server.crt -days 3650 -sha256 \
-extfile config/server-openssl.cnf -extensions req_ext
```

### 4. Install Certificates
```bash
sudo systemctl stop mosquitto
sudo cp ca/ca.crt server/server.crt server/server.key /etc/mosquitto/certs/
sudo chown -R mosquitto: /etc/mosquitto/certs
sudo chmod 750 /etc/mosquitto/certs
sudo chmod 640 /etc/mosquitto/certs/*.crt /etc/mosquitto/certs/*.key
```

### 5. Configure Mosquitto

**/etc/mosquitto/mosquitto.conf**:
```ini
pid_file /run/mosquitto/mosquitto.pid

persistence true
persistence_location /var/lib/mosquitto/
persistence_file mosquitto.db
autosave_interval 300
user mosquitto

log_dest file /var/log/mosquitto/mosquitto.log

include_dir /etc/mosquitto/conf.d

log_type all
connection_messages true
```

**/etc/mosquitto/conf.d/tls.conf**:
```ini
listener 8883
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
use_identity_as_username false
allow_anonymous false
```

**/etc/mosquitto/conf.d/plain.conf**:
```ini
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```
### 6. Set password for broker (additional layer of security)

To generate a password for Mosquitto, use the following command:

```bash
mosquitto_passwd -c /etc/mosquitto/passwd your_username
```

This will create (or overwrite) the password file and prompt you to enter a password for `your_username`.
Make sure the path matches the one used in your Mosquitto configuration (`password_file`).
```
```
### 7. Restart and Test
```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto

# Test TLS connection
mosquitto_pub -h your.mqtt.domain -p 8883 --cafile /etc/mosquitto/certs/ca.crt -u youruser -P yourpass -t test -m "Hello over TLS"

# In a separate console:
mosquitto_sub -h your.mqtt.domain -p 8883 --cafile /etc/mosquitto/certs/ca.crt -u youruser -P yourpass -t test
```

### 2. MQTT Provider Daemon

```bash

cd lights-timer/RaspberryPi/aqua_topic_provider/
make

#edit service file - adjust the path to executable, etc
sudo nano mqtt-devices-provider.service

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
ExecStart=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider
WorkingDirectory=/home/pi/Documents/lights-timer/RaspberryPi/aqua_topic_provider/provider
Restart=always
RestartSec=5
User=pi

[Install]
WantedBy=multi-user.target
```
Additionally, the Raspberry Pi provider must be executed at least once from the console to initialize and store the MQTT broker credentials.

### 3. Pico W Firmware

- Flash via Arduino IDE (2.3.xx) using the [Earlephilhower RP2040 core](https://github.com/earlephilhower/arduino-pico)
- Set WiFi and MQTT credentials in `Credentials.h`, and certificate in `ca_cert.c`. For detailed info, please take a look below.

### 4. Android App

- Open in Android Studio (and gradle sync).
- Supports Android 6.0+.
- MQTT credentials are requested on first launch.

## 💡 Future Improvements

- temperature & hear and cooling system for aqua tanks?

## 🧩 Required Local Configuration for Pico W / Arduino compilation

Project requires Arduino tools library - included as submodule (git clone with --recurse-submodules)
Available also [here](https://github.com/jaszczurtd/arduinoTools)

It is important to copy the contents of the `"libraries"` folder into the `"libraries"` directory managed by the Arduino environment.
But before that, it is essential to properly configure and complete the content of the following files:
`libraries/Credentials/ca_cert.c` and `libraries/Credentials/MacHostMapping.h/.cpp` — these must be adapted to:

* the WiFi SSID and password,
* the MQTT broker login/password/IP (or domain),
* the x.509 certificate,
* the MAC-to-hostname mapping and number of relays.

## 📦 Included Library: cJSON

This project includes [cJSON](https://github.com/DaveGamble/cJSON) for JSON parsing on embedded devices.

> cJSON aims to be the **simplest** and **smallest** possible JSON parser in C that’s also **fully functional**. It supports encoding, decoding, and manipulating JSON data using a lightweight DOM-style tree.

License: MIT (see `cJSON/LICENSE`)

## 📄 Project license

MIT License – see `LICENSE` file.
