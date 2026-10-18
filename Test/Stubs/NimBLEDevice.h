#pragma once
#include "Arduino.h"
#include <memory>
#include <cassert>
#define BLE_HS_IO_NO_INPUT_OUTPUT 3
#define ESP_OK 0
#define USB_VID 0x2886
#define USB_PID 0x0056
struct NimBLEConnInfo {
    uint16_t conn_handle;
    struct { bool encrypted = false; } sec_state;
    uint16_t mtu = 23;
    uint16_t getConnHandle() const { return conn_handle; }
    bool isEncrypted() const { return sec_state.encrypted; }
    uint16_t getMTU() const { return mtu; }
};
namespace NIMBLE_PROPERTY {
    constexpr uint32_t READ = 1, READ_ENC = 2, WRITE = 4, WRITE_ENC = 8, WRITE_NR = 16, NOTIFY = 32;
}
struct MockAttValue : std::vector<uint8_t> {
    using std::vector<uint8_t>::vector;
    size_t length() const { return size(); }
};
class NimBLEUUID {
public:
    uint16_t value;
    explicit NimBLEUUID(uint16_t value) : value(value) {}
};
class NimBLECharacteristic;
class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onRead(NimBLECharacteristic*, NimBLEConnInfo&) {}
    virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
    virtual void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) {}
};
class NimBLEDescriptor {
public:
    uint16_t uuid;
    std::vector<uint8_t> value;
    explicit NimBLEDescriptor(NimBLEUUID uuid) : uuid(uuid.value) {}
    void setValue(const uint8_t* bytes, size_t length) { value.assign(bytes, bytes + length); }
};
namespace BLEMock {
    struct Packet { uint8_t id; std::vector<uint8_t> data; };
    inline std::vector<Packet> sent;
    inline unsigned failures = 0;
    inline unsigned initialized = 0;
    inline bool bonding = false, mitm = false, secureConnections = false, forceAuthentication = false;
}
class NimBLECharacteristic {
public:
    uint16_t uuid;
    uint32_t properties;
    NimBLECharacteristicCallbacks* callbacks = nullptr;
    std::vector<uint8_t> value;
    std::vector<std::unique_ptr<NimBLEDescriptor>> descriptors;
    NimBLECharacteristic(NimBLEUUID uuid, uint32_t properties) : uuid(uuid.value), properties(properties) {}
    void setValue(const uint8_t* bytes, size_t length) { value.assign(bytes, bytes + length); }
    MockAttValue getValue() { return MockAttValue(value.begin(), value.end()); }
    void setCallbacks(NimBLECharacteristicCallbacks* value) { callbacks = value; }
    void addDescriptor(NimBLEDescriptor* value) { descriptors.emplace_back(value); }
    uint8_t reportId() const { return descriptors.empty() ? 0 : descriptors[0]->value[0]; }
    uint8_t reportType() const { return descriptors.empty() ? 0 : descriptors[0]->value[1]; }
    NimBLEDescriptor* createDescriptor(NimBLEUUID uuid, uint32_t, uint16_t) {
        descriptors.emplace_back(new NimBLEDescriptor(uuid));
        return descriptors.back().get();
    }
    bool notify(const uint8_t* data, size_t length, uint16_t peer) {
        assert(peer == 1);
        if (BLEMock::failures) { --BLEMock::failures; return false; }
        BLEMock::sent.push_back({reportId(), {data, data + length}});
        return true;
    }
};
class NimBLEService {
public:
    uint16_t uuid;
    std::vector<std::unique_ptr<NimBLECharacteristic>> characteristics;
    explicit NimBLEService(NimBLEUUID uuid) : uuid(uuid.value) {}
    NimBLECharacteristic* createCharacteristic(NimBLEUUID uuid, uint32_t properties, uint16_t = 512) {
        characteristics.emplace_back(new NimBLECharacteristic(uuid, properties));
        return characteristics.back().get();
    }
    bool start() { return true; }
};
class NimBLEServer;
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
    virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {}
    virtual void onMTUChange(uint16_t, NimBLEConnInfo&) {}
    virtual void onAuthenticationComplete(NimBLEConnInfo&) {}
};
class NimBLEServer {
public:
    NimBLEServerCallbacks* callbacks = nullptr;
    std::vector<uint16_t> disconnected;
    std::vector<std::unique_ptr<NimBLEService>> services;
    void setCallbacks(NimBLEServerCallbacks* value, bool) { callbacks = value; }
    NimBLEService* createService(NimBLEUUID uuid, uint32_t = 15) {
        services.emplace_back(new NimBLEService(uuid));
        return services.back().get();
    }
    void advertiseOnDisconnect(bool) {}
    bool disconnect(uint16_t peer) { disconnected.push_back(peer); return 0; }
};
class NimBLEAdvertising {
public:
    unsigned starts = 0;
    std::vector<uint16_t> services;
    void addServiceUUID(NimBLEUUID uuid) { services.push_back(uuid.value); }
    void setAppearance(uint16_t) {}
    void setName(const char*) {}
    void enableScanResponse(bool) {}
    bool start() { ++starts; return true; }
};
class NimBLEDevice {
public:
    static inline NimBLEServer server;
    static inline NimBLEAdvertising advertising;
    static bool init(const char*) { ++BLEMock::initialized; return true; }
    static bool setMTU(uint16_t) { return true; }
    static void setSecurityAuth(bool bonding, bool mitm, bool sc) {
        BLEMock::bonding = bonding; BLEMock::mitm = mitm; BLEMock::secureConnections = sc;
    }
    static void setSecurityIOCap(uint8_t) {}
    static bool startSecurity(uint16_t) { BLEMock::forceAuthentication = true; return true; }
    static NimBLEServer* createServer() { return &server; }
    static NimBLEAdvertising* getAdvertising() { return &advertising; }
};
namespace BLEMock {
    inline NimBLECharacteristic* report(uint8_t id, uint8_t type = 1) {
        for (auto& service : NimBLEDevice::server.services)
            for (auto& characteristic : service->characteristics)
                if (characteristic->reportId() == id && characteristic->reportType() == type) return characteristic.get();
        return nullptr;
    }
    inline NimBLECharacteristic* characteristic(uint16_t uuid) {
        for (auto& service : NimBLEDevice::server.services)
            for (auto& characteristic : service->characteristics)
                if (characteristic->uuid == uuid) return characteristic.get();
        return nullptr;
    }
    inline NimBLEConnInfo peer{1, {false}};
    inline void connect() { peer.sec_state.encrypted = false; NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server, peer); }
    inline void authenticate(int status = 0) {
        peer.sec_state.encrypted = status == 0;
        NimBLEDevice::server.callbacks->onAuthenticationComplete(peer);
    }
    inline void subscribe(uint8_t id, bool enabled = true) {
        auto* characteristic = report(id);
        characteristic->callbacks->onSubscribe(characteristic, peer, enabled ? 1 : 0);
    }
    inline void disconnect() { NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server, peer, 0); }
}
