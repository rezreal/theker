#pragma once

// Build-time configuration. Every value can be overridden with -D in
// platformio.ini without editing this file.

// --- Board ------------------------------------------------------------------
//
// RobotDyn ESP32R4 "Smart Home Controller":
//   Relay 1..4 -> GPIO 25, 26, 33, 32
//   Button 1..4 -> GPIO 34, 35, 36, 39   (input-only pins, not used here)
//
// The published Tasmota template and ESPHome config for this board disagree;
// the mapping above is the one corroborated by the buttons landing on ESP32's
// input-only pins. Confirm on your own unit with the `esp32r4-selftest` env
// before wiring a pump to it.

#ifndef RELAY_GPIO
#define RELAY_GPIO 25
#endif

// 0 = coil energises on a HIGH output, 1 = energises on LOW.
// Not documented for this board -- verify with `esp32r4-selftest`.
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 0
#endif

// --- Behaviour --------------------------------------------------------------

// Release the relay this long after the last squirt command. The host controls
// dose by how long it keeps sending; this is just the gap tolerance.
#ifndef HOLD_MS
#define HOLD_MS 300
#endif

// Hard cap on continuous energised time. Safety, not a tuning knob: it is what
// stops a hung or spamming host from running the pump dry. Deliberately cannot
// be disabled (a 0 here is rejected at compile time in main.cpp).
#ifndef MAX_ON_MS
#define MAX_ON_MS 30000
#endif

// After MAX_ON_MS trips, ignore squirt commands for this long.
#ifndef COOLDOWN_MS
#define COOLDOWN_MS 2000
#endif

// --- BLE identity -----------------------------------------------------------

// Advertised GAP name. Hosts match on this string, so changing it will stop
// them recognising the device. A device can only advertise one name, which is
// why this stays the Hismith one and the real identity lives in the Device
// Information Service below.
#ifndef HISMITH_BLE_NAME
#define HISMITH_BLE_NAME "Hismith Piupiu"
#endif

// Device Information Service (0x180a). Not advertised, so it does not affect
// what a scan list shows or how a host matches the device -- read it after
// connecting to tell this box apart from the real thing.
#ifndef DEVICE_MANUFACTURER
#define DEVICE_MANUFACTURER "the.ker"
#endif

#ifndef DEVICE_MODEL
#define DEVICE_MODEL "the.ker"
#endif

// Overridden from the git tag by the release workflow.
#ifndef FIRMWARE_REVISION
#define FIRMWARE_REVISION "dev"
#endif

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
