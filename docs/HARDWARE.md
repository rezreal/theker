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
pio run -e esp32r4-selftest -t upload
pio device monitor
```

Each candidate pin is driven HIGH for 1.5s then LOW for 1.5s, logging as it goes.
Note which pin clicks the relay you care about, and whether it closes on HIGH
(active-high) or on LOW (active-low). BLE is not started in this mode.

If your board differs, override in `platformio.ini`:

```ini
[env:esp32r4]
build_flags = ${common.build_flags} -D RELAY_GPIO=26 -D RELAY_ACTIVE_LOW=1
```

## Flashing

The ESP32R4 has no USB-serial bridge. Connect a USB-TTL adapter (e.g. CP2102) to
3.3V, GND, TX, RX, and **remove the jumper next to the ESP32** before flashing.
Hold **IO0** while powering up to force bootloader mode if the board is not
detected.

## Pump wiring

Out of scope for this firmware, but two things matter for the failsafes to mean
anything:

- Respect the relay's contact rating (10A) and use an appropriate supply for the
  pump. Do not switch mains through a pump circuit you have not verified.
- `MAX_ON_MS` caps how long the firmware will energise the relay, which protects
  against a hung host — it does not protect against a pump that should not run
  dry for even that long. Set it to something your pump tolerates.
