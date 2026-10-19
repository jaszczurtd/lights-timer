# TimerNTP firmware

TimerNTP runs the relay controller on Pico W. It handles local buttons, a daily
schedule, the OLED display and temperature measurements, and exchanges commands
and status through MQTT. See the [MQTT reference](../doc/en/mqtt.md) for message
formats and schedule behavior.

## Prepare the workstation

Keep this checkout layout:

```text
projects_git/
  lights-timer/
    TimerNTP/
  libraries/
    JaszczurHAL/
    Credentials/
```

Clone [JaszczurHAL](https://github.com/jaszczurtd/JaszczurHAL) into the indicated
directory. From the **lights-timer repository root**, prepare its dependencies:

```bash
(cd ../libraries/JaszczurHAL && ./runmefirst.sh)
```

This prepares the Pico SDK, Arm toolchain, upload tools and host dependencies.
Python 3 is required by `jh-vscode`. Then follow the
[Credentials guide](../libraries/Credentials/README.md), including the explicit
`RP2040_TOOLCHAIN_BIN` setting for the template. Before building TimerNTP, the
external `libraries/Credentials/src/cortex-m0plus/libCredentials.a` must exist.

## Hardware and configuration

[device_tree.json](device_tree.json) defines the `picow` assembly, its `rp2040`
target, `picow` board, GPIO assignments, I2C bus and OLED address. Use the values
under `assemblies.picow` when wiring:

- Connect the SSD1306 128×32 OLED to the SDA/SCL pins of bus `i2c`, using its
  configured address. This application requires I2C bus index 0.
- Connect relay control inputs to `relay1` through `relay4`, property `pins.out`.
  Set `activeLow` to match the relay interface; the supplied assembly uses
  active-high relays. Outputs start inactive. The active relay count comes
  from the device's Credentials entry.
- Connect each button between its `button1` through `button4` input and ground.
  The supplied assembly uses `pull: up` and `activeLow: true`; firmware reads
  these properties for each button. Other input circuits need matching settings.
- Connect the DS18B20 data line to `thermometer.pins.data`, with the pull-up
  required by the sensor circuit. Use the modules' electrical specifications
  for power and signal levels, and share ground with the controller.

To display the assembly's current wiring without maintaining a second pin table,
run from **TimerNTP**:

```bash
python3 - <<'PY'
import json
from pathlib import Path
tree = json.loads(Path("device_tree.json").read_text())
print(json.dumps(tree["assemblies"]["picow"], indent=2))
PY
```

[The project manifest](.vscode/jaszczurhal.project.json) selects `assembly: picow`
and contains CMake, USB identity, output and OTA settings.
[hal_project_config.h](hal_project_config.h) selects HAL features. Change wiring
in the device tree and rebuild; do not edit the generated `jh_hardware.h`.
Local settings can store an assembly and upload port. Target/board overrides
from the older selection model are not used with this device tree.

## Build and upload

Open **TimerNTP** as the VS Code workspace. The tasks in
[tasks.json](.vscode/tasks.json) provide build, debug build, serial/UF2/OTA upload,
OTA discovery, serial monitor, IntelliSense refresh and USB identity reset.
`Project: Select board` selects an assembly for projects with a device tree.

Run the following CLI commands from **TimerNTP**. The explicit assembly also
avoids ambiguity from old local target/board settings:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode config-dump --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode check-hardware --project .
../../libraries/JaszczurHAL/vscode/entry/jh-vscode build --project . --assembly picow
```

Expect target `rp2040`, board and assembly `picow`, no hardware configuration
errors and a successful link. Main output files are in `.build/rp2040/picow/`:
`firmware.elf`, `firmware.bin`, `firmware.uf2` and `firmware.ota`.
`compile_commands_patched.json` in the same directory supplies IntelliSense.
CMake output and `firmware.elf.map` remain below `.build/cmake/`.

For the first installation, connect the Pico W by USB while holding BOOTSEL,
then upload the UF2 image:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload-uf2 --project . --assembly picow
```

For a controller already running compatible firmware, use the selected serial
port. List ports and select yours with `Project: Change port` if needed:

```bash
../../libraries/JaszczurHAL/vscode/entry/jh-vscode list-ports --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload --project . --assembly picow
../../libraries/JaszczurHAL/vscode/entry/jh-vscode monitor --project . --assembly picow
```

Use `build-debug` in place of `build` for a debug build, and
`refresh-intellisense` to refresh the editor database. If no serial port matches,
check the cable, selected port and USB identity before retrying.

## Startup and local controls

Firmware configures CET/CEST before restoring a valid RP AON clock. It then
connects Wi-Fi and requires a fresh NTP result before starting WireGuard and
MQTT. Validated NTP time is written back to AON. This clock can survive warm
resets while powered; it has no battery backup.

The built-in LED blinks while connecting to Wi-Fi and synchronizing NTP, and
stays on in `STATE_CONNECTED`. Check the serial log to distinguish NTP,
WireGuard and MQTT failures. Discovery can respond once Wi-Fi is available,
before the complete MQTT startup succeeds.

Relay states and the schedule are saved in flash. On a restart other than a
watchdog reset, firmware initially suppresses a saved ON state for the first
relay; schedule evaluation can restore it once time is available. A watchdog
reset also restores that relay's saved state. Other relay states are restored
normally.

The KV store uses the full EEPROM area reserved by HAL; the Pico W build
reserves 8192 bytes for two independently erasable banks. Startup reports
`KV ready: 8192 bytes`. Initialization, schedule writes and commit failures
include the HAL error in the serial log. After upgrading from firmware using
the old 512-byte layout, check the schedule and relay states and set them
again if needed; that layout has no migration path.

When the OLED is asleep, the first press and release only wakes it. With the
display awake, releasing a button toggles its relay and saves the state.
MQTT commands and relay changes also wake the display. See
[Config.h](Config.h) for the display's active interval.

## OTA updates

Perform the first installation over USB. Subsequent updates use OTA over the
LAN once Wi-Fi is available, even if the MQTT startup has not completed.

Review the manifest's `ota` settings before uploading: it contains a deployment
host and broadcast address. `--host` selects another device for one invocation.
With a host set, discovery queries that address directly; clear the configured
host to use the configured LAN broadcast. This discovery is separate from the
provider's device-list discovery.

The device receives UDP on `ota.port`, which must match `CR_OTA_UPDATE_PORT`.
It then opens a TCP connection back to the uploading computer on
`ota.listenPort`. Both default to 8266 here; the uploader also receives discovery
UDP replies on `listenPort`. Permit these LAN flows in the device network and
uploader firewall. The broker needs this TCP port only if it is also the uploader.

From **TimerNTP**, use the device's LAN address and enter the same password as
`CR_OTA_UPDATE_PASSWORD`:

```bash
read -r -p 'Device LAN IP: ' OTA_HOST
../../libraries/JaszczurHAL/vscode/entry/jh-vscode ota-discover --project . --assembly picow --host "$OTA_HOST"
read -r -s -p 'OTA password: ' TIMER_NTP_OTA_PASSWORD
echo
export TIMER_NTP_OTA_PASSWORD
../../libraries/JaszczurHAL/vscode/entry/jh-vscode upload-ota --project . --assembly picow --host "$OTA_HOST"
unset TIMER_NTP_OTA_PASSWORD
```

Upload creates `firmware.signed.ota` under the assembly output directory.
Authentication does not encrypt the firmware container. `JH_OTA_GENERATION`
and `JH_OTA_VERSION` in the manifest describe the release; generation is metadata,
not an enforced anti-rollback counter.

The trial image is confirmed only after `STATE_CONNECTED`, after Wi-Fi, NTP,
WireGuard and MQTT startup. Failure to reach that state leaves it unconfirmed
and can cause rollback after the configured boot-attempt limit. Inspect startup
logs after upload; a completed transfer alone does not confirm a healthy image.
If network recovery is unavailable, use USB/BOOTSEL.

## Host storage and GPIO regression tests

On Linux with GCC or Clang, run from the **repository root**:

```bash
cmake -S ../libraries/JaszczurHAL -B ../libraries/JaszczurHAL/.build/lights-timer-kv
cmake --build ../libraries/JaszczurHAL/.build/lights-timer-kv --target hal_mock -j4
cmake -S tests -B .build/tests
cmake --build .build/tests -j4
ctest --test-dir .build/tests --output-on-failure
```

The test compiles TimerNTP's MQTT handler and hardware code with the production
RP flash/KV implementation over a RAM buffer. It checks schedule updates,
published status, persistence after reinitialization and relay state storage.
The same code runs with the supplied assembly and altered GPIO properties,
checking input pulls, button release, relay polarity and boot restoration.
It uses test credentials and mock peripherals; no board or broker is needed.
It does not check physical flash timing or the mobile app.

## Implementation references

[TimerNTP.cpp](TimerNTP.cpp) provides `app_start()` and `app_task0()`;
JaszczurHAL supplies the entry point and calls the application loop. HAL provides
CYW43/lwIP networking, WireGuard, time, OTA, watchdog support, KV/EEPROM,
LittleFS, I2C, SSD1306, DS18B20 and cJSON. Detailed behavior is described in the
[HAL API](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/en/JaszczurHAL_API.md),
[firmware workflow](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/en/FwProjectWorkflow.md)
and [OTA guide](https://github.com/jaszczurtd/JaszczurHAL/blob/main/doc/en/OTAWorkflow.md).
