# Device provider

The provider discovers TimerNTP controllers on the LAN and publishes their list
to MQTT. Android reads that list to display the available devices. Prepare the
[network and broker](../../doc/en/network.md) before starting it.

## Build

Run these commands on the Raspberry Pi server, from
`lights-timer/RaspberryPi/aqua_topic_provider`:

```bash
sudo apt update
sudo apt install -y build-essential libmosquitto-dev libmosquitto1 \
  mosquitto-clients libcjson-dev
```

Review [provider.h](provider.h) before compiling. It defines the LAN broadcast
address, discovery port and interval, MQTT address and port, topic and client
ID. The broadcast address must match the controllers' LAN, and the MQTT address
must reach the broker through WireGuard. Rebuild after changing these constants:

```bash
make
```

The result is the `provider` executable in this directory. It links the system
Mosquitto and cJSON libraries.

## Create the MQTT credentials file

Read [mqtt-devices-provider.service](mqtt-devices-provider.service) and choose
the account that will run it (`User`). As that account, run from this directory:

```bash
./provider
```

On its first interactive start, the program asks for the MQTT username and
password and saves them in that account's `~/.mqtt_provider_auth` with mode
`0600`. Stop it with `Ctrl+C` after the file has been created and the broker
connection succeeds. An empty device list is expected before controllers start.

Create this file before enabling unattended startup: systemd cannot answer the
login prompt. Credentials saved under another account's home directory are not
used by the service.

## Install the service

Edit `ExecStart`, `WorkingDirectory` and `User` in the service file to match the
installation. It requires `wg-quick@wg0.service`; configure that interface first,
or update both service dependencies if your WireGuard interface has another name.

From the same directory, install and start the service:

```bash
sudo cp mqtt-devices-provider.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mqtt-devices-provider.service
sudo systemctl status mqtt-devices-provider.service
sudo journalctl -u mqtt-devices-provider.service -n 50 --no-pager
```

Expect an active service and successful MQTT connection messages. If it keeps
restarting, check the credentials file for `User`, WireGuard, broker access and
the paths in the service file.

## Discovery and verification

The provider broadcasts `AQUA_DISCOVER` and collects `AQUA_FOUND|...` responses.
Its reachability check accepts a TCP connection or a connection refusal from
the device; this does not establish that its MQTT connection is healthy.

It publishes a retained device list on `discovered/devices`. Use the subscriber
procedure in the [MQTT reference](../../doc/en/mqtt.md) to inspect it. Check the
controller's status topic separately. If the list is empty, check the broadcast
address, shared LAN segment and discovery rules in the firewall.
