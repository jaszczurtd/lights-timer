# MQTT diagnostics

This document describes the current firmware diagnostics stream and the requirements for a simple, durable Raspberry Pi backend: ingest over MQTT, persist to storage, and expose it over the web behind nginx.

## What the firmware publishes

Diagnostics are implemented in [MQTTDiagnostics.cpp](MQTTDiagnostics.cpp) and triggered from [MQTTClient.cpp](MQTTClient.cpp). This is not a continuously streamed telemetry feed. The firmware publishes only events that are useful for diagnostics:

1. `boot_cause` once per boot (reset reason + optional fault snapshot).
2. `watchdog` after boot, but only when the previous boot was a watchdog reset.
3. `ping_health_transition` when broker connectivity quality crosses a threshold and changes state.

The messages are published on hierarchical topics rooted at `diagnostics/<hostname>`, where `<hostname>` comes from `hardware.getMyHostname()`.

Important:
- messages are published with `retain=true`,
- the Raspberry Pi backend must consume and store them itself,
- retained messages keep the newest payload per topic in the broker for late subscribers,
- retained messages are not auto-deleted on read; clear them by publishing an empty retained payload,
- the current firmware topic prefix in [Config.h](Config.h) is `diagnostics/`, which is a proper MQTT namespace.

The important distinction is this: MQTT wildcards operate on topic levels separated by `/`. A topic like `diagnostics/<hostname>/watchdog` is fully wildcard-friendly, so a backend can subscribe to all diagnostics, to one device, or to one event type without prior knowledge of exact topic strings.

For a real collector, the better design is hierarchical topics, for example:

- `diagnostics/<hostname>/boot_cause`
- `diagnostics/<hostname>/watchdog`
- `diagnostics/<hostname>/ping_health`

With that structure, a consumer can subscribe to:

- `diagnostics/+`
- `diagnostics/+/boot_cause`
- `diagnostics/+/watchdog`
- `diagnostics/+/ping_health`

## When it runs

The diagnostics flow is tied to the MQTT client lifecycle:

- `onMqttSessionStart()` starts the internal ping timer,
- `onMqttSessionStop()` stops the timer and clears state,
- `processPingHealthProbe()` performs a single broker ping test,
- `prepareWatchdogEventIfNeeded()` queues the watchdog event only once per boot,
- `publishPendingIfConnected()` drains the queue when MQTT is active.

In practice this means the backend receives event-driven messages, not a constant firehose. That is a good fit for Raspberry Pi, because storage writes only happen when a new diagnostic event is actually produced.

## MQTT topics

### Device topic

Recommended format:

`diagnostics/<hostname>/watchdog`

Example:

`diagnostics/pico-08dde4/watchdog`

If you keep the firmware in sync with this document, the collector can subscribe to `diagnostics/+/#`, `diagnostics/+/boot_cause`, `diagnostics/+/watchdog`, or `diagnostics/+/ping_health`.

### Subscription pattern

Recommended backend subscription for a hierarchical scheme:

`diagnostics/+/#`

This gives the collector three useful routing levels:
- all diagnostics: `diagnostics/+/#`
- one device: `diagnostics/<hostname>/#`
- one event type across devices: `diagnostics/+/watchdog`

## Event types

### 1. Boot cause

Published once per boot.

Semantics:
- the event is deduplicated on the firmware side by a one-shot `bootCauseEventPrepared` flag,
- includes HAL reset-cause classification,
- includes optional captured fault registers when available (`pc`, `lr`, `psr`).

Key JSON fields:
- `reason = boot_cause`
- `build`
- `hostname`
- `mac`
- `bootMillis`
- `resetReason`
- `resetReasonCode`
- `watchdogResetOnBoot`
- `brownoutSuspected`
- `lastFaultValid`
- `lastFaultPc`
- `lastFaultLr`
- `lastFaultPsr`
- `stackGuardArmed`
- `ds18b20TemperatureAvailable`
- `ds18b20TemperatureC`
- `rp2040TemperatureC`
- `wifiStrength`
- `wdtBootCount`
- `phaseAtPublish`
- `queuedEvents`
- `droppedEvents`

### 2. Watchdog reboot

Published once per boot, and only when the boot was caused by the watchdog.

Semantics:
- the event is deduplicated on the firmware side by `watchdogEventPrepared`,
- if the boot was not watchdog-related, nothing is published,
- if MQTT is not yet connected, the event remains queued until the connection is available.

Key JSON fields:
- `reason = watchdog`
- `build`
- `hostname`
- `mac`
- `bootMillis`
- `watchdogTimeoutMs`
- `freeHeap`
- `wdtBootCount`
- `lastStateBeforeReset`
- `lastStateBeforeResetKnown`
- `lastStateBeforeResetName`
- `lastUptimeBeforeResetMs`
- `lastPhaseBeforeResetKnown`
- `lastPhaseBeforeResetRaw`
- `lastPhaseBeforeReset`
- `phaseAtPublish`
- `ds18b20TemperatureAvailable`
- `ds18b20TemperatureC`
- `rp2040TemperatureC`
- `wifiStrength`
- `queuedEvents`
- `droppedEvents`

Why these fields matter:
- `wdtBootCount` separates one-off events from repeated resets,
- `lastStateBeforeReset` and `lastPhaseBeforeReset` show what the firmware was doing right before the reset,
- `lastUptimeBeforeResetMs` shows whether the reset happened shortly after boot or after a longer runtime,
- `queuedEvents` and `droppedEvents` show whether the backend or the MQTT link fell behind.

### 3. Ping health transition

This event describes MQTT broker connectivity quality from the device side. The firmware pings the broker, tracks failed attempts, and publishes only when the health state changes.

Health states:
- `healthy`
- `degraded`
- `unreachable`
- `unknown`

Thresholds are derived from `MAX_FAILED_PINGS` in [Config.h](Config.h):
- `healthy` means the number of failed pings is below the degradation threshold,
- `degraded` means roughly half or more failed pings, but not yet total loss,
- `unreachable` means the failure counter reached the maximum.

Key JSON fields:
- `reason = ping_health_transition`
- `build`
- `hostname`
- `mac`
- `localMillis`
- `transitionLocalMillis`
- `from`
- `to`
- `failedPings`
- `maxFailedPings`
- `degradedThreshold`
- `brokerAvailable`
- `brokerPingMs`
- `ds18b20TemperatureAvailable`
- `ds18b20TemperatureC`
- `rp2040TemperatureC`
- `wifiStrength`
- `queuedEvents`
- `droppedEvents`

Why these fields matter:
- they distinguish transient network noise from a real degradation,
- they provide a simple broker connectivity metric,
- they can drive alerts in the backend without extra device polling.

## Event queue

The firmware keeps a small queue of diagnostic events on the device:

- queue size: 8 entries,
- overflow strategy: drop oldest,
- `droppedEvents` is included in the JSON payload.

That means the backend must not assume a lossless queue. If the device stays disconnected from MQTT for a long time, older events can be overwritten by newer ones. Treat these messages as operational signals, not as an immutable audit log.

## Frequency and load

`processPingHealthProbe()` runs periodically, but it publishes only when the state changes. In practice MQTT load is low.

That matters for the backend because:
- there is no need for aggressive polling,
- there is no need to handle large volumes of telemetry,
- a subscriber that stores messages on arrival is enough.

## Recommended Raspberry Pi backend

### Minimal architecture

1. `mosquitto` as the broker, or an external broker if one already exists.
2. A small MQTT consumer process on the Raspberry Pi.
3. A database for event history.
4. A web application behind nginx that reads from the database and renders a dashboard.

### Suggested flow

`devices -> MQTT broker -> collector -> database -> API -> nginx -> browser`

### What to store

At minimum, store:
- backend receive timestamp,
- topic,
- `hostname`,
- `mac`,
- `reason`,
- raw JSON payload,
- optionally normalized fields for filtering and charts.

This keeps the system flexible: the raw JSON preserves compatibility, while normalized columns make filtering and reporting easier.

### Minimal data model

One table named `diagnostic_events` is enough to start:

- `id`
- `received_at`
- `topic`
- `hostname`
- `mac`
- `reason`
- `payload_json`
- `event_time` or `boot_millis`, depending on event type

If you want basic trend analysis, add fields such as:
- `wdt_boot_count`
- `ping_state_from`
- `ping_state_to`
- `failed_pings`
- `broker_available`
- `broker_ping_ms`
- `wifiStrength`

## Exposing it over the web

nginx should act as the frontend and reverse proxy, not as the MQTT processor.

The most practical setup is:
- the collector writes to SQLite, PostgreSQL, or InfluxDB,
- an HTTP backend exposes a JSON API,
- nginx serves the static frontend and proxies API requests,
- the dashboard reads both history and current state from the API.

### Useful HTTP endpoints

- `GET /api/devices`
- `GET /api/devices/<hostname>/events`
- `GET /api/devices/<hostname>/latest`
- `GET /api/events?reason=watchdog`
- `GET /api/health`

### What to show in the UI

- device list with the latest event,
- watchdog reboot count per device,
- latest `ping_health` state,
- time since the last event,
- charts for `brokerPingMs` and failed ping counts,
- raw JSON for debugging.

## MQTT subscription examples

On Raspberry Pi, you can use:

```bash
mosquitto_sub -h localhost -u piuser -P Password -t 'diagnostics/+/#' -v
```

If you want pretty-printed JSON:

```bash
mosquitto_sub -h localhost -u piuser -P Password -t 'diagnostics/+/#' | jq .
```

For a single device:

```bash
mosquitto_sub -h localhost -u piuser -P Password -t 'diagnostics/pico-08dde4/#' -v
```

For a single event type across all devices:

```bash
mosquitto_sub -h localhost -u piuser -P Password -t 'diagnostics/+/watchdog' -v
```

## Important limitations

1. Messages are not retained, so the backend must run continuously.
2. The firmware publishes only selected events, not full telemetry.
3. The queue is small, so events can be dropped during prolonged MQTT outages.
4. The topic is based on hostname, so the backend should treat `hostname` as the logical key and `mac` as the stable hardware identifier.
5. The current slash-separated topic format is a proper MQTT namespace and should be kept consistent across firmware, Raspberry Pi, and any future consumer.

## Implementation guidance for the backend

- Subscribe to `diagnostics/+/#` for all diagnostics, or to `diagnostics/<hostname>/#` for a single device.
- Use `reason` as the primary payload discriminator.
- Treat `hostname` as a UI label and `mac` as the device identifier.
- Keep the raw JSON payload unchanged.
- Add indexes on `received_at`, `hostname`, and `reason`.
- In the UI, show both the latest state and the history, because a single watchdog event without ping history is often not enough context.

## Short recommendation

The best starting point for Raspberry Pi is a small MQTT collector written in Python or Go, persistence to SQLite or PostgreSQL, and nginx as a reverse proxy to a simple dashboard. That gives you a light, maintainable setup with enough room for alerts, charts, and per-device filtering later.