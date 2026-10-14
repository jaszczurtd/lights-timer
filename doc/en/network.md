# Network and MQTT broker

Pico W controllers reach Mosquitto through WireGuard on TCP 1883. Android
connects over TLS on TCP 8883. The provider discovers controllers directly on
the LAN, so it must share their broadcast domain even when MQTT uses a tunnel.

This deployment uses a Raspberry Pi server and IPv4. TimerNTP uses WireGuard
both for the existing network installation and to exercise that HAL implementation.
HAL also supports MQTT over TLS; using it in TimerNTP would require changes to
firmware, credentials and broker routing.

## Prerequisites

Prepare these before starting the broker:

- A working WireGuard server interface `wg0`, with address `10.8.0.1/24` for
  this example. Configure a peer for each controller with its public key and
  unique tunnel address. Match these to the
  [Credentials settings](../../libraries/Credentials/README.md). The
  [WireGuard quick start](https://www.wireguard.com/quickstart/) describes key
  generation and interface configuration.
- Working LAN access between the provider and controllers, including UDP
  broadcast. Set the broadcast address for your subnet in
  [provider.h](../../RaspberryPi/aqua_topic_provider/provider.h). OTA discovery
  has separate settings in the firmware project manifest.
- DNS and NTP access for the controllers before the tunnel starts. TimerNTP
  requires fresh NTP synchronization before starting WireGuard.
- A broker hostname reachable from Android, with DNS and router forwarding
  if the server is behind NAT.
- A PEM server certificate chain covering that hostname, its matching private
  key and the CA certificate that validates the chain. Obtain these through
  your public or private CA. The server key must be usable unattended by
  Mosquitto. Use the same CA for the CLI checks and the Android app.

On the server, check WireGuard before continuing:

```bash
sudo systemctl status wg-quick@wg0.service
ip -4 address show dev wg0
sudo wg show wg0
```

Expect an active interface with the intended address and configured peers.
A handshake appears only after a peer connects.

## Install and configure Mosquitto

Run the server commands from any directory on Raspberry Pi OS or Debian:

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
sudo mosquitto_passwd -c /etc/mosquitto/passwd your_username
sudo chown root:mosquitto /etc/mosquitto/passwd
sudo chmod 640 /etc/mosquitto/passwd
```

Replace `your_username` with the MQTT account name. Use `-c` only when creating
the password file; omit it when adding users to an existing file.

Install the certificates prepared above, replacing the `/path/to/...` inputs:

```bash
sudo install -d -o root -g mosquitto -m 750 /etc/mosquitto/certs
sudo install -o root -g mosquitto -m 640 /path/to/server-chain.crt /etc/mosquitto/certs/server.crt
sudo install -o root -g mosquitto -m 640 /path/to/server.key /etc/mosquitto/certs/server.key
sudo install -o root -g root -m 644 /path/to/ca.crt /etc/mosquitto/ca.crt
```

Use one file, `/etc/mosquitto/conf.d/lights-timer.conf`, for both listeners:

```ini
allow_anonymous false
password_file /etc/mosquitto/passwd

listener 1883 10.8.0.1
protocol mqtt

listener 8883
socket_domain ipv4
protocol mqtt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
```

The main Mosquitto configuration must include `/etc/mosquitto/conf.d`.
Replace any earlier configuration for these listeners rather than loading both
copies. Authentication settings above apply to both listeners; this example
does not enable per-listener authentication settings.

TCP 1883 binds only the WireGuard address. The TLS listener accepts MQTT
username/password authentication. The Android app does not supply a client
certificate, so enabling mTLS would require app changes. Its CA handling and
current error behavior are described in the [Android guide](../../Android/README.md).
See the [Mosquitto configuration manual](https://mosquitto.org/man/mosquitto-conf-5.html)
for listener, certificate and authentication options.

Because the broker binds the tunnel address, start it after WireGuard. Run
`sudo systemctl edit mosquitto` and add this unit override:

```ini
[Unit]
Requires=wg-quick@wg0.service
After=wg-quick@wg0.service
```

Then start the broker and inspect its logs:

```bash
sudo systemctl daemon-reload
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo journalctl -u mosquitto -n 50 --no-pager
sudo ss -lntup
```

Expect an active service, TCP 1883 bound to `10.8.0.1` and TCP 8883 listening
on IPv4. Resolve certificate permissions, duplicate listeners or missing `wg0`
before starting the provider.

## Traffic and firewall

The following table identifies the receiving host; OTA traffic does not
necessarily pass through the broker server.

| Receiver | Port/protocol | Source and purpose |
| --- | --- | --- |
| WireGuard server | 51820/UDP | Controller uplink, tunnel endpoint; match the configured endpoint port |
| Mosquitto | 1883/TCP | WireGuard peers and the provider on the server; keep off the public Internet |
| Mosquitto | 8883/TCP | Android and TLS clients |
| Controllers and provider | 12345/UDP | LAN discovery requests and replies |
| Controller | 8266/UDP | OTA uploader; match `ota.port` and the firmware port |
| OTA uploader | 8266/TCP and UDP | TCP connection initiated by the controller and UDP discovery replies; match `ota.listenPort` |

For a server managed directly with iptables, this **example for a first IPv4 installation**
filters the project ports without changing the policy for unrelated traffic.
Set `LT_LAN_IF` and `LT_LAN_SUBNET` to your actual LAN. If a firewall manager
already controls the server, express the same rules there. If `LIGHTS_TIMER`
already exists, update its rules instead of repeating chain creation.

```bash
LT_LAN_IF=eth0
LT_LAN_SUBNET=192.168.2.0/24
sudo iptables -N LIGHTS_TIMER
sudo iptables -A LIGHTS_TIMER -i lo -p tcp --dport 1883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -i wg0 -s 10.8.0.0/24 -p tcp --dport 1883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p tcp --dport 1883 -j DROP
sudo iptables -A LIGHTS_TIMER -i "$LT_LAN_IF" -s "$LT_LAN_SUBNET" -p udp --dport 12345 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p udp --dport 12345 -j DROP
sudo iptables -A LIGHTS_TIMER -p udp --dport 51820 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -p tcp --dport 8883 -j ACCEPT
sudo iptables -A LIGHTS_TIMER -j RETURN
sudo iptables -I INPUT 1 -j LIGHTS_TIMER
```

The explicit DROP rules reject other sources for the internal ports even when
the INPUT policy is ACCEPT. Inserting this chain before broader accepts is
essential; rule order and chain policies are described in the
[iptables manual](https://man7.org/linux/man-pages/man8/iptables.8.html).
This example assumes outbound traffic is allowed. For an IPv6 deployment,
configure corresponding listeners and filtering separately.

If trusted LAN clients also need plaintext MQTT, add a separate listener bound
to the server's LAN address and an allow rule for that interface and subnet
before the TCP 1883 DROP rule. Keep the public interface excluded.

OTA needs rules on the **uploading computer** for replies and the incoming TCP
connection from the controller's LAN address. Permit those only on the intended
LAN; see the [firmware OTA procedure](../../TimerNTP/README.md#ota-updates).

If this server uses `netfilter-persistent`, save the rules after checking access:

```bash
sudo netfilter-persistent save
```

That command requires the persistence package; on Debian it is provided by
`netfilter-persistent`, commonly installed with `iptables-persistent`.

## Verify and continue

Run the [MQTT connection checks](mqtt.md#connection-checks) on both listeners.
The subscriber must print the message sent by the publisher. Then install the
[provider](../../RaspberryPi/aqua_topic_provider/README.md), prepare the
[firmware](../../TimerNTP/README.md) and configure
[Android](../../Android/README.md).
