# Hardware

## Target board

**RobotDyn ESP32R4 "Smart Home Controller"** — ESP32-WROOM-32, 4 relays, 4 buttons.
Any ESP32 board with a relay works; only the pin numbers change.

| Function | GPIO |
| --- | --- |
| Relay 1 | 25 |
| Relay 2 | 26 |
| Relay 3 | 33 |
| Relay 4 | 32 |
| Button 1–4 | 34, 35, 36, 39 |

The firmware defaults to **Relay 1 (GPIO25), active-high**.

## Confirm the pinout before wiring a pump

The published sources for this board disagree: the Tasmota template JSON and the
ESPHome config decode to different pins, and neither documents the relay's active
level. The table above is the mapping corroborated by the buttons landing on
GPIO34–39, which are input-only on the ESP32 and therefore can only be inputs.

That is good evidence, not proof, so confirm it on your own unit:

```bash
mise run selftest
mise run monitor
```

Each candidate pin is driven HIGH for 1.5s then LOW for 1.5s, logging as it goes.
Note which pin clicks the relay you care about, and whether it closes on HIGH
(active-high) or on LOW (active-low). BLE is not started in this mode.

If your board differs, override in `platformio.ini`:

```ini
[env:esp32r4]
build_flags = ${common.build_flags} -D RELAY_GPIO=26 -D RELAY_ACTIVE_LOW=1
```

## Flashing: you need a USB-TTL adapter

**The ESP32R4 has no USB port and no USB-serial bridge.** No cable will flash it
directly. You need a USB-to-TTL serial adapter — a CP2102 is what the board's
write-ups recommend — **switched to 3.3V, not 5V**.

| Adapter | Board |
| --- | --- |
| `3.3V` | `3.3V` |
| `GND` | `GND` |
| `TX` | `RX` |
| `RX` | `TX` |

TX and RX cross over. Two rules that matter more than the wiring:

- **Remove the jumper next to the ESP32** before flashing, and put it back after.
- **Never connect the DC/mains supply and the adapter's 3.3V at the same time.**
  You are powering the ESP32 directly, bypassing the onboard regulator.

### Entering bootloader mode

A 4-wire hookup carries no DTR/RTS, so nothing can auto-reset the board into the
bootloader — not `pio run -t upload`, not the web flasher. Do it by hand each
time, *before* starting the flash:

1. Hold **Prog**
2. Press and release **RST**
3. Release **Prog**

The board is now waiting for a firmware image.

### Which route to flash

The web flasher (Chrome/Edge) talks to the adapter's serial port directly, so on
Windows it needs no driver plumbing. `mise run upload` from WSL2 does: WSL2 has
no USB access, so the adapter has to be attached with
[usbipd-win](https://github.com/dorssel/usbipd-win) first. The web flasher is
the path of least resistance there.

### Running the self-test afterwards

`mise run selftest` has to *hear* relays click, and relay coils run off the main
supply — which must not be connected while the adapter's 3.3V is. So flash
first, then swap power over:

1. Flash the self-test build with the adapter powering the board (jumper out).
2. Disconnect the adapter's **3.3V** wire only; leave `GND`, `TX`, `RX` on.
3. Refit the jumper and power the board from its normal DC input.
4. Open the serial monitor — a shared ground means it still reads — and listen
   for which relay clicks.

Steps 2–4 follow from the "never both power sources" rule rather than from the
vendor docs; if in doubt, power the board normally and read the serial log with
only `GND`/`TX`/`RX` connected.

## Pump wiring

Out of scope for this firmware, but two things matter for the failsafes to mean
anything:

- Respect the relay's contact rating (10A) and use an appropriate supply for the
  pump. Do not switch mains through a pump circuit you have not verified.
- `MAX_ON_MS` caps how long the firmware will energise the relay, which protects
  against a hung host — it does not protect against a pump that should not run
  dry for even that long. Set it to something your pump tolerates.
