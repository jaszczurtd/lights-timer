# MQTT messages and schedule

MQTT carries the device list, relay commands, daily schedule and reported state.
Broker addresses and access paths are described in the [network guide](network.md).
Each controller needs a unique hostname in its Credentials mapping; `{hostname}`
below is that name, not the controller's IP address.

## Topics

| Topic | Publisher → subscriber | Retained |
| --- | --- | --- |
| `discovered/devices` | Provider → Android | Yes |
| `switch-{hostname}` | Android → controller | No, in the Android app |
| `time-{hostname}` | Android → controller | No, in the Android app |
| `status-{hostname}` | Controller → Android | Yes |

Send commands without retain so the broker does not replay an old command on
reconnection. A retained status or device list is the last published value;
receiving it alone does not prove that the controller is currently connected.

## Device list

An illustrative `discovered/devices` payload is:

```json
{"devices":[{"mac":"AA:BB:CC:DD:EE:01","ip":"192.168.2.171","hostName":"lamp1","switches":4}]}
```

`ip` is the controller's LAN address. `hostName` supplies the suffix for its
topics, and `switches` is the relay count advertised from Credentials. An empty
list is `{"devices":[]}`. The provider's discovery and reachability checks are
described in its [guide](../../RaspberryPi/aqua_topic_provider/README.md).

## Relay command

Publish to `switch-lamp1` to change selected relays:

```json
{"isOn1":true,"isOn3":false}
```

Keys are numbered from `isOn1` to `isOn4`. Use JSON booleans. Firmware processes
the configured number of relays; omitted keys leave those relays unchanged.
It saves changed states and requests a status publication.

## Daily schedule

Publish both fields to `time-lamp1`, for example:

```json
{"dateHourStart":300,"dateHourEnd":600}
```

This sets the first relay's active window to **05:00–10:00 CET/CEST**. Values
are integer minutes after local midnight, not hours or Unix timestamps. Use
values from 0 to 1440; 1440 can serve as the exclusive end of the day. Android's
time picker allows times from 00:00 to 23:59.

The interval includes the start and excludes the end: `start <= now < end`.
Equal endpoints give an empty window. A window crossing midnight is unsupported;
setting a start later than the end does not create an overnight schedule.
Firmware uses CET/CEST with automatic summer-time changes, independently of
the phone's timezone.

Only the **first relay** follows the schedule. The remaining relays use manual
commands and buttons. Manual changes to the first relay remain until the next
observed transition into or out of the schedule window. Changing the schedule
also evaluates the new window and can cause such a transition. The firmware
does not continuously overwrite manual changes while the window state is unchanged.

Firmware saves both endpoints and uses them after restart. See the
[startup description](../../TimerNTP/README.md#startup-and-local-controls) for
clock availability and relay restoration.

## Reported state

`status-{hostname}` reports these fields:

| Field | Meaning |
| --- | --- |
| `status` | `ok` for a normal report; `JSON build failed` for the fallback error report |
| `build` | Firmware build date/time string |
| `dateHourStart`, `dateHourEnd` | Saved schedule endpoints in minutes |
| `isOn1` … `isOn4` | Four boolean relay-state slots; use the advertised relay count for the UI |
| `localTime` | Local date/time as `DD/MM/YYYY HH:MM:SS`, or `not available yet.` |
| `localMillis` | Milliseconds since boot, not a wall-clock timestamp |
| `wifi` | Display string in the form `5/<strength>`; not an RSSI value in dBm |
| `temperatureAvailable` | Whether the firmware has a valid temperature sample |
| `temperatureC` | Temperature in degrees Celsius, or JSON `null` when unavailable |

The fallback JSON error report contains only `status`. A previously valid
temperature can remain the last available sample; this payload has no sample
timestamp. Parsing uses cJSON in firmware and in the provider.

## Connection checks

Install `mosquitto-clients` on the test computer. For port 1883, use the server
itself or a WireGuard peer that can reach the broker. Open **two Bash terminals**
and enter the same connection settings in each; the current directory does not matter:

```bash
read -r -p 'Broker WireGuard IP: ' MQTT_HOST
read -r -p 'MQTT username: ' MQTT_USER
read -r -s -p 'MQTT password: ' MQTT_PASSWORD
echo
```

Start the subscriber in terminal 1 and leave it running:

```bash
mosquitto_sub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'lights-timer/check' -v
```

Then publish in terminal 2:

```bash
mosquitto_pub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'lights-timer/check' -m 'hello'
```

Expect `lights-timer/check hello` in terminal 1. The message is not retained,
so the subscriber must be connected before publication. Stop it with `Ctrl+C`.

To check TLS, set the hostname covered by the certificate and CA path in **both**
terminals. On the server, the CA installed by the network guide is
`/etc/mosquitto/ca.crt`; copy that CA to the test computer if needed:

```bash
read -r -p 'Broker TLS hostname: ' MQTT_HOST
read -r -p 'CA certificate path: ' MQTT_CA
```

Start terminal 1's subscriber first:

```bash
mosquitto_sub -h "$MQTT_HOST" -p 8883 --cafile "$MQTT_CA" \
  -u "$MQTT_USER" -P "$MQTT_PASSWORD" -t 'lights-timer/check' -v
```

Then publish from terminal 2 and expect the same message:

```bash
mosquitto_pub -h "$MQTT_HOST" -p 8883 --cafile "$MQTT_CA" \
  -u "$MQTT_USER" -P "$MQTT_PASSWORD" -t 'lights-timer/check' -m 'hello'
```

After the provider and firmware start, inspect their retained reports. In a
terminal configured for port 1883, set the broker's WireGuard address again and
enter the device hostname from Credentials:

```bash
read -r -p 'Broker WireGuard IP: ' MQTT_HOST
read -r -p 'Device hostname: ' DEVICE_HOSTNAME
mosquitto_sub -h "$MQTT_HOST" -p 1883 -u "$MQTT_USER" -P "$MQTT_PASSWORD" \
  -t 'discovered/devices' -t "status-$DEVICE_HOSTNAME" -v
```

Expect a device entry and a status report matching the fields above. A list
without status calls for checking NTP, WireGuard and MQTT startup on the
controller. For CLI options, see the
[Mosquitto publisher manual](https://mosquitto.org/man/mosquitto_pub-1.html) and
[subscriber manual](https://mosquitto.org/man/mosquitto_sub-1.html).
