#include "HIDTransportBackend.hpp"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <algorithm>
#include <atomic>
#include <memory>

namespace {
constexpr uint16_t noConnection = 0xffff;
class BluetoothTransport;
class ReportCallbacks : public NimBLECharacteristicCallbacks {
public:
    BluetoothTransport& owner;
    uint8_t id;
    HIDReportType type;
    size_t length;
    NimBLECharacteristic* characteristic;
    std::atomic<bool> subscribed{false};
    ReportCallbacks(BluetoothTransport& owner, uint8_t id, HIDReportType type, size_t length,
                    NimBLECharacteristic* characteristic)
        : owner(owner), id(id), type(type), length(length), characteristic(characteristic) {}
    void onRead(NimBLECharacteristic*, NimBLEConnInfo&) override;
    void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) override;
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t value) override;
};

class BluetoothTransport : public HIDTransportBackend, public NimBLEServerCallbacks {
    class ControlCallbacks : public NimBLECharacteristicCallbacks {
        BluetoothTransport& owner;
    public:
        explicit ControlCallbacks(BluetoothTransport& owner) : owner(owner) {}
        void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& peer) override {
            const auto value = characteristic->getValue();
            if (!owner.authorized(peer) || value.length() != 1 || uint8_t(value[0]) > 1) return;
            const bool suspended = value[0] == 0;
            if (owner.suspended.exchange(suspended) != suspended) ++owner.generation;
        }
    } control{*this};
    HIDTransportConfiguration config;
    NimBLEServer* server = nullptr;
    std::vector<std::unique_ptr<ReportCallbacks>> reports;
    std::atomic<uint16_t> connection{noConnection};
    std::atomic<uint16_t> mtu{23};
    std::atomic<bool> secured{false};
    std::atomic<bool> suspended{false};
    std::atomic<bool> advertise{false};
    std::atomic<uint32_t> generation{0};
    uint32_t lastAdvertisement = 0;
    ReportCallbacks* input(uint8_t id) const {
        for (const auto& report : reports)
            if (report->id == id && report->type == HIDReportType::Input) return report.get();
        return nullptr;
    }
    void addReport(NimBLEService* service, uint8_t id, HIDReportType type, size_t length) {
        if (!length) return;
        uint32_t properties = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC;
        if (type == HIDReportType::Input) properties |= NIMBLE_PROPERTY::NOTIFY;
        else properties |= NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC;
        if (type == HIDReportType::Output) properties |= NIMBLE_PROPERTY::WRITE_NR;
        auto* characteristic = service->createCharacteristic(NimBLEUUID(uint16_t(0x2a4d)), properties, length);
        auto* reference = characteristic->createDescriptor(NimBLEUUID(uint16_t(0x2908)), NIMBLE_PROPERTY::READ, 2);
        uint8_t value[] = {id, uint8_t(type)};
        reference->setValue(value, sizeof(value));
        std::vector<uint8_t> initial(length, 0);
        if (config.getReport) config.getReport(id, type, initial.data(), initial.size());
        characteristic->setValue(initial.data(), initial.size());
        reports.emplace_back(new ReportCallbacks(*this, id, type, length, characteristic));
        characteristic->setCallbacks(reports.back().get());
    }
    void resetLink() {
        secured = false;
        suspended = false;
        mtu = 23;
        for (auto& report : reports) report->subscribed = false;
        ++generation;
    }
public:
    bool Begin(HIDTransportConfiguration configuration) override {
        config = std::move(configuration);
        if (config.descriptor.empty()) return true; // USB-only sketches do not start the radio.
        if (!NimBLEDevice::init("OpenCtrl")) return false;
        if (!NimBLEDevice::setMTU(128)) return false;
        NimBLEDevice::setSecurityAuth(true, false, true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        server = NimBLEDevice::createServer();
        if (!server) return false;
        server->setCallbacks(this, false);
        server->advertiseOnDisconnect(false);
        auto* deviceInfo = server->createService(NimBLEUUID(uint16_t(0x180a)));
        auto* identity = deviceInfo->createCharacteristic(NimBLEUUID(uint16_t(0x2a50)), NIMBLE_PROPERTY::READ);
        const uint8_t pnp[] = {2, uint8_t(USB_VID), uint8_t(USB_VID >> 8),
            uint8_t(USB_PID), uint8_t(USB_PID >> 8), 0, 1};
        identity->setValue(pnp, sizeof(pnp));
        if (!deviceInfo->start()) return false;
        auto* battery = server->createService(NimBLEUUID(uint16_t(0x180f)));
        auto* level = battery->createCharacteristic(NimBLEUUID(uint16_t(0x2a19)), NIMBLE_PROPERTY::READ);
        const uint8_t powered = 100; // This firmware is externally powered; no battery gauge is installed.
        level->setValue(&powered, 1);
        if (!battery->start()) return false;
        auto* service = server->createService(NimBLEUUID(uint16_t(0x1812)));
        if (!service) return false;
        auto* info = service->createCharacteristic(NimBLEUUID(uint16_t(0x2a4a)), NIMBLE_PROPERTY::READ);
        const uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x02};
        info->setValue(hidInfo, sizeof(hidInfo));
        auto* map = service->createCharacteristic(NimBLEUUID(uint16_t(0x2a4b)),
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC);
        map->setValue(config.descriptor.data(), config.descriptor.size());
        auto* controlPoint = service->createCharacteristic(NimBLEUUID(uint16_t(0x2a4c)),
            NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
        controlPoint->setCallbacks(&control);
        for (const auto& layout : config.reports) {
            addReport(service, layout.id, HIDReportType::Input, layout.input);
            addReport(service, layout.id, HIDReportType::Output, layout.output);
            addReport(service, layout.id, HIDReportType::Feature, layout.feature);
        }
        if (!service->start()) return false;
        auto* advertising = NimBLEDevice::getAdvertising();
        advertising->addServiceUUID(NimBLEUUID(uint16_t(0x1812)));
        advertising->setAppearance(0x03c0);
        advertising->setName("OpenCtrl");
        advertising->enableScanResponse(true);
        return advertising->start();
    }
    bool Connected() const override { return connection != noConnection && secured; }
    bool Active(uint8_t id) const override {
        auto* report = input(id);
        return !suspended && report && report->subscribed;
    }
    bool Ready(uint8_t id = 0) const override {
        if (!Connected() || suspended) return false;
        if (id) {
            auto* report = input(id);
            return report && report->subscribed && report->length + 3 <= mtu;
        }
        for (const auto& report : reports)
            if (report->type == HIDReportType::Input && Ready(report->id)) return true;
        return std::none_of(reports.begin(), reports.end(), [](const auto& report) {
            return report->type == HIDReportType::Input;
        });
    }
    bool Send(uint8_t id, const uint8_t* data, size_t length) override {
        auto* report = input(id);
        if (!report || length != report->length || !Ready(id)) return false;
        const auto peer = connection.load();
        const auto epoch = generation.load();
        report->characteristic->setValue(data, length);
        const bool queued = report->characteristic->notify(data, length, peer);
        // NimBLE reports whether it queued the notification, not host consumption.
        return queued && Ready(id) && connection == peer && generation == epoch;
    }
    uint32_t Generation() const override { return generation; }
    void Service() override {
        if (advertise && connection == noConnection && uint32_t(millis() - lastAdvertisement) >= 1000) {
            lastAdvertisement = millis();
            if (NimBLEDevice::getAdvertising()->start()) advertise = false;
        }
    }
    bool authorized(const NimBLEConnInfo& peer) const {
        return peer.getConnHandle() == connection && peer.isEncrypted();
    }
    void onConnect(NimBLEServer*, NimBLEConnInfo& peer) override {
        if (connection != noConnection) { server->disconnect(peer.getConnHandle()); return; }
        resetLink();
        connection = peer.getConnHandle();
        mtu = peer.getMTU();
        advertise = false;
        if (!NimBLEDevice::startSecurity(peer.getConnHandle())) server->disconnect(peer.getConnHandle());
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo& peer, int) override {
        if (connection != peer.getConnHandle()) return;
        connection = noConnection;
        resetLink();
        advertise = true;
    }
    void onMTUChange(uint16_t value, NimBLEConnInfo& peer) override {
        if (connection == peer.getConnHandle()) mtu = value;
    }
    void onAuthenticationComplete(NimBLEConnInfo& peer) override {
        if (connection != peer.getConnHandle()) return;
        secured = authorized(peer);
        ++generation;
        if (!secured) server->disconnect(peer.getConnHandle());
    }
    friend class ReportCallbacks;
};

void ReportCallbacks::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& peer) {
    if (!owner.authorized(peer) || !owner.config.getReport) return;
    if (type == HIDReportType::Output) return; // Retain the last host-written Output value.
    std::vector<uint8_t> bytes(length, 0);
    const auto count = owner.config.getReport(id, type, bytes.data(), bytes.size());
    characteristic->setValue(bytes.data(), std::min<size_t>(count, length));
}
void ReportCallbacks::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& peer) {
    if (!owner.authorized(peer) || type == HIDReportType::Input || !owner.config.setReport) return;
    const auto bytes = characteristic->getValue();
    if (bytes.length() == length)
        owner.config.setReport(id, type, bytes.data(), length);
}
void ReportCallbacks::onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& peer, uint16_t value) {
    if (peer.getConnHandle() != owner.connection || type != HIDReportType::Input) return;
    const bool enabled = value & 1;
    if (subscribed.exchange(enabled) != enabled) ++owner.generation;
}
}

HIDTransportBackend& HIDBLEBackend() { static BluetoothTransport transport; return transport; }
