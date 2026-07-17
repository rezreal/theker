#include <Arduino.h>

#include "config.h"
#include "relay.h"

#if RELAY_SELFTEST

// Pin/polarity discovery aid. Cycles each relay candidate on the RobotDyn
// ESP32R4 so you can hear which one clicks and confirm the active level before
// committing to RELAY_GPIO / RELAY_ACTIVE_LOW. BLE is not started here.

static const int kCandidatePins[] = {25, 26, 33, 32};
static constexpr size_t kCandidateCount = sizeof(kCandidatePins) / sizeof(kCandidatePins[0]);

void setup() {
    for (size_t i = 0; i < kCandidateCount; ++i) {
        pinMode(kCandidatePins[i], OUTPUT);
        digitalWrite(kCandidatePins[i], LOW);
    }
    Serial.begin(SERIAL_BAUD);
    delay(500);
    Serial.println();
    Serial.println(F("=== relay self-test ==="));
    Serial.println(F("Each pin is driven HIGH for 1.5s, then LOW for 1.5s."));
    Serial.println(F("Note which pin clicks the relay your pump is on, and"));
    Serial.println(F("whether it closes on HIGH (active-high) or on LOW (active-low)."));
}

void loop() {
    for (size_t i = 0; i < kCandidateCount; ++i) {
        const int pin = kCandidatePins[i];
        Serial.printf("GPIO %d -> HIGH\n", pin);
        digitalWrite(pin, HIGH);
        delay(1500);
        Serial.printf("GPIO %d -> LOW\n", pin);
        digitalWrite(pin, LOW);
        delay(1500);
    }
}

#else

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include "ble_peripheral.h"
#include "hismith_protocol.h"

// MAX_ON_MS is the failsafe that stops a hung host running the pump dry.
static_assert(MAX_ON_MS > 0, "MAX_ON_MS must be > 0; it is a safety cap, not a tuning knob");
static_assert(HOLD_MS > 0, "HOLD_MS must be > 0 or the relay can never stay held");

namespace {

constexpr bool kActiveLow = (RELAY_ACTIVE_LOW != 0);

hismith::Parser g_parser;
hismith::RelayController g_relay({HOLD_MS, MAX_ON_MS, COOLDOWN_MS});

// NimBLE callbacks run in the host task while the parser and relay are owned by
// loop(). Rather than lock the state machines, received bytes are handed across
// this queue and a flag, so all logic stays single-threaded in loop().
QueueHandle_t g_rx_queue = nullptr;
std::atomic<bool> g_disconnect_pending{false};
std::atomic<uint32_t> g_dropped_bytes{0};

int g_last_level = -1;

void applyRelay() {
    const bool on = g_relay.energised();
    const int level = (kActiveLow ? !on : on) ? HIGH : LOW;
    if (level != g_last_level) {
        digitalWrite(RELAY_GPIO, level);
        g_last_level = level;
    }
}

void onBleData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (xQueueSend(g_rx_queue, &data[i], 0) != pdTRUE) {
            g_dropped_bytes.fetch_add(1);
        }
    }
}

void onBleConnect() {
    Serial.println(F("[ble] host connected"));
}

void onBleDisconnect() {
    // Failsafe: never leave the pump running because a host vanished.
    g_disconnect_pending.store(true);
}

void onBleSubscribe(uint16_t sub_value) {
    // Notable rather than routine: a subscriber expects responses, and no
    // Piupiu notify payload is documented, so nothing is ever sent.
    Serial.printf("[ble] host %s notify characteristic (cccd=0x%04x); nothing will be sent\n",
                  sub_value == 0 ? "unsubscribed from" : "subscribed to", sub_value);
}

void handleFrame(const hismith::Frame& frame) {
    if (frame.isSquirt()) {
        g_relay.squirt(millis());
        return;
    }
    // Valid framing, unrecognised command. Only the squirt command is publicly
    // documented for the Piupiu, so log these -- they are the raw material for
    // extending the protocol.
    Serial.printf("[proto] unknown frame cmd=0x%02x arg=0x%02x\n", frame.cmd, frame.arg);
}

void drainRxQueue() {
    uint8_t byte = 0;
    while (xQueueReceive(g_rx_queue, &byte, 0) == pdTRUE) {
        hismith::Frame frame;
        switch (g_parser.push(byte, frame)) {
            case hismith::Parser::Result::Frame:
                handleFrame(frame);
                break;
            case hismith::Parser::Result::BadChecksum:
                Serial.println(F("[proto] bad checksum, resyncing"));
                break;
            case hismith::Parser::Result::NeedMore:
                break;
        }
    }
}

}  // namespace

void setup() {
    // Before anything else: make certain the pump is not energised at boot.
    pinMode(RELAY_GPIO, OUTPUT);
    applyRelay();

    Serial.begin(SERIAL_BAUD);
    delay(200);
    Serial.println();
    Serial.printf("Hismith Piupiu emulator | relay GPIO %d (%s) | hold %dms | max-on %dms\n",
                  RELAY_GPIO, kActiveLow ? "active-low" : "active-high", HOLD_MS, MAX_ON_MS);

    g_rx_queue = xQueueCreate(128, sizeof(uint8_t));
    if (g_rx_queue == nullptr) {
        Serial.println(F("[fatal] could not allocate RX queue"));
        ESP.restart();
    }

    hismith::BleHandlers handlers;
    handlers.onData = onBleData;
    handlers.onConnect = onBleConnect;
    handlers.onDisconnect = onBleDisconnect;
    handlers.onSubscribe = onBleSubscribe;

    const hismith::BleIdentity identity{
        HISMITH_BLE_NAME,
        DEVICE_MANUFACTURER,
        DEVICE_MODEL,
        FIRMWARE_REVISION,
    };
    hismith::bleStart(identity, handlers);

    Serial.printf("[ble] advertising as \"%s\" (model \"%s\" %s)\n", HISMITH_BLE_NAME, DEVICE_MODEL,
                  FIRMWARE_REVISION);
}

void loop() {
    const uint32_t now = millis();

    if (g_disconnect_pending.exchange(false)) {
        g_relay.forceOff(now);
        g_parser.reset();
        Serial.println(F("[ble] host disconnected, relay off"));
    }

    drainRxQueue();
    g_relay.tick(now);
    applyRelay();

    if (g_relay.consumeLockoutEvent()) {
        Serial.printf("[relay] max-on %dms exceeded, locked out for %dms\n", MAX_ON_MS, COOLDOWN_MS);
    }

    const uint32_t dropped = g_dropped_bytes.exchange(0);
    if (dropped > 0) {
        Serial.printf("[ble] dropped %lu rx bytes (queue full)\n", static_cast<unsigned long>(dropped));
    }

    delay(2);
}

#endif  // RELAY_SELFTEST
