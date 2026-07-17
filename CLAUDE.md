# theker

Firmware making an ESP32 relay board advertise as a `Hismith Piupiu` BLE device
and drive a pump from the documented squirt command. See [README.md](README.md).

## Set up mise before using any tool

This project pins its entire toolchain — Python, Node, PlatformIO — in
[mise.toml](mise.toml). **Run this before the first tool use in a session:**

```bash
mise trust && mise install
```

Then run everything through mise. Do **not** use system Python, system Node, a
`pip install platformio`, or a local `.venv`: those drift from what CI uses.

```bash
mise run test                    # host-native unit tests, no hardware needed
mise run build                   # build the firmware
mise run check                   # everything CI runs: tests, firmware, flasher
mise run selftest                # flash the relay pin/polarity self-test
mise exec -- pio run -e esp32r4  # anything without a task
```

`mise tasks` lists them. Tool versions in `mise.toml` and
`.github/workflows/ci.yml` are kept in step — change both together.

## Things that will bite you

**The relay failsafes are safety features, not tuning knobs.** This drives a
pump. `MAX_ON_MS` cannot be disabled (there is a `static_assert`), the relay
drops on BLE disconnect, and it is driven off before BLE starts at boot. Do not
weaken these to make a test pass.

**The advertised BLE name must stay `Hismith Piupiu`.** Hosts match on that
string to recognise the protocol, and a device can only advertise one name. The
board's own identity lives in the Device Information Service (`180a`) instead —
put identifying info there, not in the GAP name. Likewise only `ffe5` is
advertised: that is what is documented for this device, and advertising the
HM-10 `ffe0` service too would make it *less* like the thing it imitates.

**`ffe0`/`ffe4` is exposed but must stay silent.** It exists so hosts expecting
the full HM-10 layout find it during service discovery. No Piupiu notify payload
is documented, so do not invent one — a host may act on it. `onSubscribe` logs
subscribers instead, which is real evidence about the protocol.

**Keep `hismith_protocol` and `relay` free of Arduino includes.** They are pure
C++ so the only real logic is host-testable without hardware. An `#include
<Arduino.h>` in either breaks `mise run test` and the CI gate. BLE callbacks run
in the NimBLE task, so bytes cross into `loop()` via a FreeRTOS queue — do not
touch the parser or relay from a callback.

**Do not add a firmware merge script.** The platform already emits
`firmware.factory.bin` (flashable at `0x0`). A hand-rolled `esptool merge_bin`
post-action produced a broken 10.8 MB image and used options esptool v5
deprecated.

**Web flasher: no CDN, and no fetching release assets from JS.** GitHub's CORS
rules block a Pages site from reading a release asset
([esp-web-tools#521](https://github.com/esphome/esp-web-tools/issues/521)), so
firmware is served same-origin from `gh-pages` and listed via a generated
`versions.json`. `web/build.mjs` copies esp-web-tools' pre-bundled `dist/web`;
do not try to re-bundle it from source — it depends on `@material/web` `^2.2.0`
and 2.5.0 renamed the `*-styles.js` files it imports.

**The board pinout is inferred, not confirmed.** Relay defaults to GPIO25
active-high on a RobotDyn ESP32R4; the published Tasmota and ESPHome configs for
it disagree and the active level is undocumented. See
[docs/HARDWARE.md](docs/HARDWARE.md). Never assert it works on hardware that has
not been checked with `mise run selftest`.

## Conventions

- Only the squirt command is publicly documented; the parser validates the
  `0xCC | cmd | arg | (cmd+arg)&0xFF` frame rather than matching a byte string,
  and logs well-formed unknown frames for future protocol work.
- Commits in this repo use an anonymous author (repo-local git config).
