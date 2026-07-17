# Hismith Piupiu emulator

Firmware that turns an ESP32 relay board into a BLE peripheral that speaks the
**Hismith Piupiu** (lube launcher) protocol. It advertises under that name, and
when a host writes the documented squirt command it closes a relay — so a pump on
the relay is driven by anything that already talks to a Piupiu, whether that's
your own `bleak`/`noble` script or a Buttplug/Intiface host.

Built for the **RobotDyn ESP32R4 Smart Home Controller**, but any ESP32 board with
a relay works — see [docs/HARDWARE.md](docs/HARDWARE.md).

## Flash it

**From the browser** (Chrome or Edge — Web Serial is required):
open the project's GitHub Pages site, pick a version, click flash.

**From a checkout:**

```bash
pip install platformio
pio run -e esp32r4 -t upload
pio device monitor
```

PlatformIO bundles its own esptool, so there is nothing else to install.

> Confirm the relay pin on your board before wiring a pump to it:
> `pio run -e esp32r4-selftest -t upload`. See [docs/HARDWARE.md](docs/HARDWARE.md).

## How it behaves

The board advertises as `Hismith Piupiu` with service `ffe5` and write
characteristic `ffe9`. Writing the squirt command closes the relay:

```
cc 0b 01 0c
```

The advertised name has to stay the Hismith one — it is what hosts match on to
recognise the protocol, and a device can only advertise one name. So the board
reports what it really is through a **Device Information Service** (`180a`)
instead: manufacturer and model `the.ker`, plus the firmware revision. That is
not advertised, so it changes nothing about how a host finds or matches the
device; read it once connected to tell this box from the real thing:

```bash
./tools/squirt.py --once
# device info:
#   manufacturer: the.ker
#   model: the.ker
#   firmware: v1.0.0
```

The relay is **held while commands repeat** and releases `HOLD_MS` after the last
one, so the host controls dose by how long it keeps sending.

Because this drives a pump, three failsafes are non-negotiable in the firmware:

- **Max-on cap** — after `MAX_ON_MS` of continuous running the relay drops and
  ignores commands for `COOLDOWN_MS`, so a hung or spamming host cannot run the
  pump dry. It cannot be disabled.
- **Relay off on BLE disconnect** — a host that vanishes cannot leave it running.
- **Relay off before BLE starts** — it can never come up energised at boot.

## Configuration

Defaults live in [`src/config.h`](src/config.h); override them with `-D` in
`platformio.ini` rather than editing the file:

| Flag | Default | |
| --- | --- | --- |
| `RELAY_GPIO` | `25` | Relay 1 on the ESP32R4 |
| `RELAY_ACTIVE_LOW` | `0` | Verify with the self-test build |
| `HOLD_MS` | `300` | Release this long after the last command |
| `MAX_ON_MS` | `30000` | Safety cap on continuous run |
| `COOLDOWN_MS` | `2000` | Lockout after the cap trips |
| `HISMITH_BLE_NAME` | `"Hismith Piupiu"` | Advertised name — hosts match on this |
| `DEVICE_MANUFACTURER` | `"the.ker"` | Device Information Service |
| `DEVICE_MODEL` | `"the.ker"` | Device Information Service |
| `FIRMWARE_REVISION` | `"dev"` | Set from the git tag by the release workflow |

```ini
[env:esp32r4]
build_flags = ${common.build_flags} -D RELAY_GPIO=26 -D HOLD_MS=500
```

## The protocol

Frames are four bytes:

```
0xCC | cmd | arg | checksum        checksum = (cmd + arg) & 0xFF
```

Only the squirt command (`cc 0b 01 0c`) is publicly documented for the Piupiu,
but the checksum rule holds across every published Hismith frame — including
`cc 01 00 01` and `cc 01 a1 a2` from the S2 — so the firmware validates frames
instead of matching one magic byte string. Frames that are well-formed but
unrecognised are logged over serial with their `cmd`/`arg`, which makes the board
useful for extending the protocol.

`ffe5`/`ffe9` is the classic HM-10 BLE-serial UUID pair, so writes are a byte
stream rather than discrete packets: they may split one frame across writes or
pack several into one. The parser handles both and resynchronises after garbage.

## Development

```bash
pio test -e native      # protocol + relay logic, no hardware needed
pio run -e esp32r4      # build
```

The protocol parser and relay state machine are pure C++ with no Arduino
includes, so all the real logic is tested on the host in CI.

End-to-end against real hardware:

```bash
pip install bleak                 # only dependency outside the project, dev-only
./tools/squirt.py --scan          # confirm it advertises
./tools/squirt.py --hold 3        # relay closes for ~3s, then releases
./tools/squirt.py --hold 40       # long enough to trip the max-on lockout
```

Interrupting `squirt.py` mid-hold is itself a test: the relay must drop on
disconnect.

## CI and releases

- **CI** runs the host tests, builds both firmware envs, and builds the flasher.
- **Release** — push a `v*` tag. It builds, attaches `firmware.factory.bin` (the
  platform's combined image, flashable at `0x0`) and `firmware.bin` to a GitHub
  release, then publishes the flasher plus that firmware to `gh-pages`.

The flasher serves firmware **same-origin from `gh-pages`** rather than from the
release assets: GitHub's CORS rules stop a Pages site from fetching a release
asset with JavaScript ([esp-web-tools#521](https://github.com/esphome/esp-web-tools/issues/521)).
The version list is generated from what is actually published, so the picker can
never offer a build it cannot flash. `esp-web-tools`' pre-bundled build is copied
into the page, so the deployed site loads no code from a CDN at runtime.

## Caveats

- **Intiface may not recognise it.** The Piupiu protocol is documented by the
  community ([docs.buttplug.io#34](https://github.com/buttplugio/docs.buttplug.io/issues/34)),
  but that issue is still an open *documentation request* — a shipped Buttplug
  protocol implementation for the Piupiu is not confirmed. Any host that writes
  the command yourself (`tools/squirt.py`) works regardless.
- **HM-10 fidelity.** Only `ffe5`/`ffe9` (write) is exposed. Real HM-10 modules
  often also expose `ffe0`/`ffe4` for notify; if a host needs that to discover
  the device, it is a small addition.
- Only the squirt command is implemented, because it is the only one documented
  for this device.
