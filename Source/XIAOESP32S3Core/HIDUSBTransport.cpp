#include "HIDTransportBackend.hpp"
#include <USB.h>
#include <USBCDC.h>
#include <tusb.h>
#include <esp32-hal-tinyusb.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <atomic>

#if ARDUINO_USB_ON_BOOT
#error Disable USB CDC/MSC/DFU on boot so profiles can be registered before USB starts.
#endif

namespace {
class USBTransport : public HIDTransportBackend {
public:
    HIDTransportConfiguration config;
    SemaphoreHandle_t completion = nullptr;
    std::atomic<bool> success{false};
    std::atomic<uint32_t> generation{0}, idle{0};
    bool started = false;

    bool BootMode() const { return config.bootKeyboard && tud_hid_n_get_protocol(0) == HID_PROTOCOL_BOOT; }
    bool Begin(HIDTransportConfiguration configuration) override;
    bool Connected() const override { return started && !config.descriptor.empty() && tud_mounted(); }
    bool Ready(uint8_t id = 0) const override { return Connected() && tud_hid_n_ready(0) && (!id || Active(id)); }
    bool Active(uint8_t id) const override { return !BootMode() || id == 1; }
    bool Send(uint8_t id, const uint8_t* data, size_t length) override {
        if (!Ready(id)) return false;
        xSemaphoreTake(completion, 0);
        success = false;
        if (!tud_hid_n_report(0, BootMode() ? 0 : id, data, length)) return false;
        return xSemaphoreTake(completion, pdMS_TO_TICKS(100)) == pdTRUE && success;
    }
    uint32_t Generation() const override { return generation; }
    uint32_t IdlePeriod() const override { return idle; }
};
USBTransport& usb() { static USBTransport instance; return instance; }

uint16_t loadInterface(uint8_t* destination, uint8_t* interface) {
    const bool boot = usb().config.bootKeyboard;
    const uint8_t input = boot ? 1 : tinyusb_get_free_in_endpoint();
    const uint8_t output = boot ? 1 : tinyusb_get_free_out_endpoint();
    if (!input || !output) return 0;
    const uint8_t descriptor[] = {
        TUD_HID_INOUT_DESCRIPTOR(*interface, tinyusb_add_string_descriptor("OpenCtrl HID"),
            boot ? HID_ITF_PROTOCOL_KEYBOARD : HID_ITF_PROTOCOL_NONE, usb().config.descriptor.size(), output,
            uint8_t(0x80 | input), CFG_TUD_ENDPOINT_SIZE, 1)
    };
    ++*interface;
    memcpy(destination, descriptor, sizeof(descriptor));
    return sizeof(descriptor);
}
bool USBTransport::Begin(HIDTransportConfiguration configuration) {
    if (started) return false;
    config = std::move(configuration);
    if (!config.descriptor.empty()) {
        completion = xSemaphoreCreateBinary();
        if (!completion || tinyusb_enable_interface2(USB_INTERFACE_HID, TUD_HID_INOUT_DESC_LEN,
            loadInterface, config.bootKeyboard) != ESP_OK) return false;
    }
    HIDUSBConsole().begin(115200);
    started = USB.begin(); // CDC console is also available in Bluetooth-only mode.
    return started;
}
}
HIDTransportBackend& HIDUSBBackend() { return usb(); }
USBCDC& HIDUSBConsole() { static USBCDC instance(0); return instance; }

extern "C" const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) {
    return instance == 0 ? usb().config.descriptor.data() : nullptr;
}
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t id, hid_report_type_t type,
                                         uint8_t* data, uint16_t length) {
    if (instance || !data || !length || !usb().config.getReport) return 0;
    if (usb().BootMode() && !id) id = 1;
    return usb().config.getReport(id, static_cast<HIDReportType>(type), data, length);
}
extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t id, hid_report_type_t type,
                                      const uint8_t* data, uint16_t length) {
    if (instance || !data || !usb().config.setReport) return;
    if (usb().BootMode() && !id && (type == HID_REPORT_TYPE_OUTPUT || type == 0)) {
        id = 1;
        type = HID_REPORT_TYPE_OUTPUT;
    } else if (!id && (type == HID_REPORT_TYPE_OUTPUT || type == 0)) {
        if (!length) return;
        id = *data++; --length;
        type = HID_REPORT_TYPE_OUTPUT;
    }
    usb().config.setReport(id, static_cast<HIDReportType>(type), data, length);
}
extern "C" void tud_hid_report_complete_cb(uint8_t instance, const uint8_t*, uint16_t) {
    if (!instance && usb().completion) { usb().success = true; xSemaphoreGive(usb().completion); }
}
extern "C" void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t type, const uint8_t*, uint16_t) {
    if (!instance && type == HID_REPORT_TYPE_INPUT && usb().completion) {
        usb().success = false;
        xSemaphoreGive(usb().completion);
    }
}
extern "C" bool tud_hid_set_idle_cb(uint8_t instance, uint8_t rate) {
    if (instance) return false;
    usb().idle = uint32_t(rate) * 4;
    return true;
}
extern "C" void tud_hid_set_protocol_cb(uint8_t instance, uint8_t) {
    if (!instance) ++usb().generation;
}
