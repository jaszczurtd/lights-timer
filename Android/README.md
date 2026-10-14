# Android app

The app displays controllers discovered by the provider, switches their relays
and sets the first relay's daily schedule. It uses Java, some Kotlin UI code
and the Eclipse Paho MQTT client. Devices need Android 7.0 or newer (API 24).

## Build and connect

1. Prepare the [broker and its TLS certificate](../doc/en/network.md).
2. Put the CA certificate that validates the broker's chain in
   `app/src/main/res/raw/ca.crt`, relative to this directory. Create `raw` if
   needed. Supply a single PEM X.509 CA certificate; the file is ignored by Git
   and is loaded as `R.raw.ca`.
3. Open this `Android` directory in Android Studio and synchronize Gradle. Use
   the Gradle JDK supported by the project's Android Gradle Plugin and install
   the SDK requested by [app/build.gradle.kts](app/build.gradle.kts).
4. Build and run the app on the phone. Enter the broker hostname without a
   scheme or port, plus the MQTT username and password. An IP address can be
   used when the server certificate covers that address.

The app always connects to `ssl://<broker>:8883`. It obtains the device list from
the provider and subscribes to each controller's status topic. An empty list can
mean that discovery has not found a controller yet; inspect the
[provider](../RaspberryPi/aqua_topic_provider/README.md) and
[MQTT messages](../doc/en/mqtt.md) to distinguish that from a broker problem.

## Certificate handling

[MQTTClient.java](app/src/main/java/com/example/aquacontrol/MQTTClient.java)
builds a trust store containing the certificate in `R.raw.ca`. A public-CA
server certificate still needs the matching CA supplied to this store; the
normal connection path does not use the phone's full system CA store.

If constructing this TLS configuration throws an exception, the app logs
`SSL problem` and still calls `connect()` without the custom socket factory.
Treat that log entry as a configuration failure. The current implementation
does not guarantee that every connection uses the bundled CA.

The app does not load a client certificate and private key. Keep
`require_certificate false` on its broker listener. Enabling mTLS requires
changes to the app and provisioning a client identity.

## Schedule and state

The [MQTT reference](../doc/en/mqtt.md) defines the schedule's units, timezone,
boundaries and interaction with manual switching. The firmware uses CET/CEST.
Android's immediate schedule preview uses the phone's local clock, so a phone
in another timezone can briefly show a different state until device status
arrives. Use the controller's status as the reported device state.
