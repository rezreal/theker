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
};

// Brings up the GATT server and starts advertising, exposing the Hismith
// service (ffe5) with its write characteristic (ffe9), plus a Device
// Information Service (180a).
void bleStart(const BleIdentity& identity, const BleHandlers& handlers);

}  // namespace hismith
