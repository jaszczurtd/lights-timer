# Credentials template

This directory is a complete, buildable, secret-free template for the
`Credentials` library used by TimerNTP. It contains no project author secrets
or private encoding/decoding code. Supply your own deployment data in ignored
local configuration files.

Create private configuration files first:

```bash
./scripts/configure.sh
```

Then edit:

- `config/CredentialsData.local.h`: Wi-Fi, MQTT, WireGuard endpoint, native OTA,
  NTP, and CA values; set `CREDENTIALS_LOCAL_CONFIGURED` to `1` after filling
  every placeholder,
- `config/MacHostMapping.local.cpp`: board MAC, hostname, WireGuard local IP
  and private key, plus the relay count for each device.

Both files are ignored by this template's `.gitignore`.

Build the archive required by an RP2040 project:

```bash
./scripts/build.sh rp2040
```

The result is `src/cortex-m0plus/libCredentials.a`, which matches the path in
the TimerNTP manifest. For STM32G474 use:

```bash
./scripts/build.sh stm32g474
```

The result is `build/stm32g474/libCredentials.a`.
