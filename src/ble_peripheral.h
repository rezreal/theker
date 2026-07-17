#pragma once

#include <cstddef>
#include <cstdint>

namespace hismith {

// What the device says it is.
//
// `name` is the advertised GAP name and must stay the Hismith name: it is what
// hosts match on to recognise the protocol, and a device can only advertise
// one. The Device Information Service fields are where this box can identify
// itself honestly without breaking that.
struct BleIdentity {
    const char* name;
    const char* manufacturer;
    const char* model;
    const char* firmware_revision;
};

// Handlers are invoked from the NimBLE host task, NOT from loop(). Keep them
// short and do not touch state that loop() owns without handing it over.
struct BleHandlers {
    void (*onData)(const uint8_t* data, size_t len) = nullptr;
    void (*onConnect)() = nullptr;
    void (*onDisconnect)() = nullptr;
    // A host subscribed to (or unsubscribed from) the notify characteristic.
    // Worth surfacing: it means the host expects responses we cannot send, and
    // no Piupiu notify payload is documented.
    void (*onSubscribe)(uint16_t sub_value) = nullptr;
};

// Brings up the GATT server and starts advertising, exposing the Hismith write
// service (ffe5/ffe9), the HM-10 style notify service (ffe0/ffe4), and a
// Device Information Service (180a).
void bleStart(const BleIdentity& identity, const BleHandlers& handlers);

}  // namespace hismith
