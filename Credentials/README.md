# Credentials library template

[Polski](README.pl.md)

This template implements the public Credentials API without the author's
secrets or private encoding code. Fill it with your own data and build an
archive for the target firmware. Values are stored without encryption.

TimerNTP links `../Credentials/build/<target>/libCredentials.a` for the target
of its assembly (`rp2040` for `picow`), relative to the lights-timer repository.
This template is not linked directly.

## Configure a private copy

For a new installation, run from the **lights-timer repository root**:

```bash
./scripts/setup-credentials.sh
```

The installer copies this directory to `../Credentials`, creates its two local
configuration files and refuses an existing destination. If you already have a
working private library there, use its own configuration and build procedure.
The remaining commands run from that external Credentials directory:

```bash
cd ../Credentials
```

If you copied the template yourself, run from the Credentials directory:

```bash
./scripts/configure.sh
```

```powershell
py -3 scripts/configure.py
```

Edit both `config/CredentialsData.local.h` and
`config/MacHostMapping.local.cpp`. Replace the example values and set
`CREDENTIALS_LOCAL_CONFIGURED` to `1`. Normal configuration preserves
existing local files. Keep local configuration and generated archives out of Git.

For TimerNTP:

- In `config/CredentialsData.local.h`, set Wi-Fi and MQTT credentials,
  WireGuard server details, NTP servers and the OTA password and port. TimerNTP
  uses `CR_MQTT_BROKER_WIREGUARD` with `CR_MQTT_BROKER_PORT`; the secure-broker,
  CA and SIM fields belong to the shared API and are not used by TimerNTP.
- In `config/MacHostMapping.local.cpp`, add each controller's factory Wi-Fi MAC,
  unique hostname, relay count from 1 to 4, WireGuard IP and private key.
  Register the matching public key and address on the WireGuard server.

MAC matching ignores separators and letter case. Obtain the MAC from the access
point's client table after the Pico W connects to Wi-Fi. A diagnostic using
`hal_wifi_get_mac()` after Wi-Fi initialization is another option; check its
return value before using the output.

## Build for the firmware target

```bash
./scripts/build.sh rp2040
```

```powershell
py -3 scripts/build.py rp2040
```

| Target | Archive |
| --- | --- |
| `rp2040`, `rp2350-arm`, `rp2350-riscv`, `stm32g474`, `esp32`, `esp32s3` | `build/<target>/libCredentials.a` |

The builder locates JaszczurHAL next to the installed Credentials directory.
For another layout, pass `--jaszczurhal-root /path/to/JaszczurHAL`.
It uses HAL-managed CMake, Ninja and compilers where available, otherwise
resolving tools from `PATH`.
For RP and STM32 builds, it prepends the selected compiler directory to the
build processes' `PATH`, so CMake's nested compiler checks can find the same
tools without changing your shell environment.

RP targets use the Pico SDK toolchain selected for their platform; STM32 uses
the HAL toolchain file. `rp2350-riscv` uses the RISC-V toolchain version selected
by HAL. ESP builds use HAL's ESP-IDF and its default configuration, including
picolibc, and prepare the required tools on first use. Build the archive for
the same target and compatible SDK/ABI configuration as the firmware.

Current JaszczurHAL supports precompiled archives through `JH_LINK_LIBRARIES`
and headers through `JH_EXTRA_INCLUDES` for RP, STM32 and ESP. Set those paths
in the consuming project's manifest. TimerNTP's manifest has one assembly,
`picow` (RP2040); the other library targets serve other projects. Rebuild the
archive and then TimerNTP whenever credentials or the MAC mapping change.

## API and buffer lifetime

Use the header and archive from the same library build. The public header is
[src/Credentials.h](src/Credentials.h); the root header forwards to it.

- `getCredential()` returns an allocated string or `NULL` for an invalid ID
  or allocation failure. Erase secret data and `free()` the buffer after use.
- `getWireguardPrivateKey()` also returns an allocated buffer, despite its
  `const char *` return type; the caller must erase and free it. Unknown MACs
  return `NULL`.
- `getFriendlyHostname()` and `getWireguardLocalIP()` return borrowed strings.
  Do not free them. The fallback hostname uses a shared static buffer and is
  replaced by the next fallback lookup; unknown local IPs return `NULL`.
- `getCredentialInt()` converts with `atoi()`, returning `-1` when no string
  is available. It does not validate numeric syntax. `getSwitchesNumber()`
  returns `0` for an unknown MAC. `initCredentials()` is a no-op in this template.

## Public test configuration

`scripts/configure.py --test` uses tracked example fixtures with reserved
addresses and no real secrets. It refuses to run over either existing local
configuration file. Use these values only for automated build tests, not
deployed devices.
