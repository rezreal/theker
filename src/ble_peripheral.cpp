#include "ble_peripheral.h"

#include <NimBLEDevice.h>

#include "hismith_protocol.h"

namespace hismith {
namespace {

// Device Information Service, as assigned by the Bluetooth SIG.
constexpr uint16_t kDeviceInfoServiceUuid = 0x180A;
constexpr uint16_t kManufacturerNameUuid = 0x2A29;
constexpr uint16_t kModelNumberUuid = 0x2A24;
constexpr uint16_t kFirmwareRevisionUuid = 0x2A26;

BleHandlers g_handlers;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        (void)server;
        (void)info;
        if (g_handlers.onConnect != nullptr) {
            g_handlers.onConnect();
        }
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override {
        (void)server;
        (void)info;
        (void)reason;
        if (g_handlers.onDisconnect != nullptr) {
            g_handlers.onDisconnect();
        }
        // Real devices stay discoverable, and a host that drops out mid-session
        // must be able to come straight back.
        NimBLEDevice::startAdvertising();
    }
};

class WriteCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
        (void)info;
        const NimBLEAttValue value = characteristic->getValue();
        if (value.length() > 0 && g_handlers.onData != nullptr) {
            g_handlers.onData(value.data(), value.length());
        }
    }
};

ServerCallbacks g_server_callbacks;
WriteCallbacks g_write_callbacks;

void addReadOnlyString(NimBLEService* service, uint16_t uuid, const char* value) {
    NimBLECharacteristic* characteristic =
        service->createCharacteristic(NimBLEUUID(uuid), NIMBLE_PROPERTY::READ);
    characteristic->setValue(value);
}

}  // namespace

void bleStart(const BleIdentity& identity, const BleHandlers& handlers) {
    g_handlers = handlers;

    NimBLEDevice::init(identity.name);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&g_server_callbacks);

    NimBLEService* service = server->createService(kServiceUuid);

    // WRITE_NR as well as WRITE: HM-10 style hosts commonly use write-without-
    // response, and the documented squirt command is fire-and-forget.
    NimBLECharacteristic* characteristic = service->createCharacteristic(
        kWriteCharUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    characteristic->setCallbacks(&g_write_callbacks);

    // The advertised name has to stay the Hismith one for hosts to match it, so
    // this is where the device reports what it really is. Not advertised, so it
    // costs nothing in the scan list -- read it once connected.
    NimBLEService* device_info = server->createService(NimBLEUUID(kDeviceInfoServiceUuid));
    addReadOnlyString(device_info, kManufacturerNameUuid, identity.manufacturer);
    addReadOnlyString(device_info, kModelNumberUuid, identity.model);
    addReadOnlyString(device_info, kFirmwareRevisionUuid, identity.firmware_revision);

    // No service->start() here: in NimBLE 2.x it is a deprecated no-op, as
    // services are started along with the server when advertising begins.
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setName(identity.name);
    advertising->addServiceUUID(kServiceUuid);
    advertising->enableScanResponse(true);
    advertising->start();
}

}  // namespace hismith
