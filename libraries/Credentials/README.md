# Credentials

This template supplies the network credentials and device identities used by
TimerNTP. It contains the same API as the author's private library, without
the author's secrets or private encoding code.

## Install the external library

TimerNTP links `../libraries/Credentials/src/cortex-m0plus/libCredentials.a`,
relative to the lights-timer repository. The template inside this repository
is not linked directly.

For a new installation, run from the **lights-timer repository root**:

```bash
./scripts/setup-credentials.sh
```

This copies the template to `../libraries/Credentials` and creates its two
local configuration files. The script refuses to overwrite an existing
directory. If you already have a working private library there, use its own
configuration and build procedure.

The remaining commands run from the **external Credentials directory**. From
the lights-timer repository root, enter it with:

```bash
cd ../libraries/Credentials
```

For a separately copied template, `./scripts/configure.sh` creates missing
local configuration files without overwriting existing ones.

## Configure the deployment

Edit these files in the external copy:

- `config/CredentialsData.local.h`: set Wi-Fi and MQTT credentials, WireGuard
  server details, NTP servers and the OTA password and port. TimerNTP uses
  `CR_MQTT_BROKER_WIREGUARD` with `CR_MQTT_BROKER_PORT`; the secure-broker and CA
  fields belong to the shared API and are not used by its MQTT connection.
- `config/MacHostMapping.local.cpp`: add each controller's factory Wi-Fi MAC,
  unique hostname, relay count from 1 to 4, WireGuard IP and private key. Register
  the matching public key and address on the WireGuard server.

Replace the example values and set `CREDENTIALS_LOCAL_CONFIGURED` to `1`.
MAC matching ignores separators and letter case. Obtain the MAC from the access
point's client table after the Pico W connects to Wi-Fi. A diagnostic using
`hal_wifi_get_mac()` after Wi-Fi initialization is another option; check its
return value before using the output.

Local configuration files and generated archives are ignored by Git. The
template compiles credential strings into the archive and firmware; it does
not encrypt them.

## Build for RP2040

Install Bash, CMake 3.20 or newer, a CMake build tool such as Make, and an Arm GNU
toolchain containing `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-ar`
and `arm-none-eabi-ranlib`. Use the toolchain prepared for TimerNTP by JaszczurHAL.

Set `RP2040_TOOLCHAIN_BIN` explicitly to its `bin` directory. For a compiler
already on `PATH`, run:

```bash
command -v arm-none-eabi-gcc
export RP2040_TOOLCHAIN_BIN="$(dirname "$(command -v arm-none-eabi-gcc)")"
./scripts/build.sh rp2040
```

The first command must print a compiler path. For a toolchain outside `PATH`,
set `RP2040_TOOLCHAIN_BIN` to its absolute `bin` path instead. Without this
variable, the script searches Arduino's RP2040 package under
`${ARDUINO_DATA_DIR:-$HOME/.arduino15}/packages/rp2040/tools/pqt-gcc`; a missing
package can make it exit without a diagnostic.

The expected result is `src/cortex-m0plus/libCredentials.a`. Rebuild this
archive and then TimerNTP whenever credentials or the MAC mapping change.

## Build for STM32G474

The template also supports the Cortex-M4 hard-float ABI:

```bash
./scripts/build.sh stm32g474
```

This produces `build/stm32g474/libCredentials.a`. The script uses
`STM32_TOOLCHAIN_BIN` when set, otherwise it finds `arm-none-eabi-gcc` on `PATH`.
This archive is for STM32 projects; TimerNTP's current assembly uses RP2040.
