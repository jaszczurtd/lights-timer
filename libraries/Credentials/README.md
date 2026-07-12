# Credentials template

This is a complete, buildable example for the `Credentials` library with secrets,
used by TimerNTP. It contains no project author's secrets and no 
private encoding/decoding code. You have to fill it with your own data.

Create private configuration files first:

```bash
./scripts/configure.sh
```

Then edit:

- `config/CredentialsData.local.h`: Wi-Fi, MQTT, WireGuard endpoint, NTP and CA
  values; set `CREDENTIALS_LOCAL_CONFIGURED` to `1` after filling it,
- `config/MacHostMapping.local.cpp`: board MAC, hostname, WireGuard local IP
  and the device private key.

Both files are ignored by this template's `.gitignore`.

Build the archive required by an RP2040 project:

```bash
./scripts/build.sh rp2040
```

The result is `src/cortex-m0plus/libCredentials.a`. For STM32G474 use:

```bash
./scripts/build.sh stm32g474
```

The result is `build/stm32g474/libCredentials.a`.

