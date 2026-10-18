/**
 * @file HIDPen.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 15.10.2026
 */
#include "HIDPen.hpp"


// The ID is assigned when this profile is registered.
static const uint8_t HID_REPORT_ID_PEN = 7;

static const uint8_t report_descriptor[] = {
  0x05, 0x0D,              // Usage Page (Digitizer)
  0x09, 0x02,              // Usage (Pen)
  0xA1, 0x01,              // Collection (Application)
  0x85, HID_REPORT_ID_PEN, //   Report ID
  0x09, 0x20,              //   Usage (Stylus)
  0xA1, 0x00,              //   Collection (Physical)
  0x09, 0x42,              //     Usage (Tip Switch)
  0x09, 0x44,              //     Usage (Barrel Switch)
  0x09, 0x45,              //     Usage (Eraser)
  0x09, 0x32,              //     Usage (In Range)
  0x15, 0x00,              //     Logical Minimum (0)
  0x25, 0x01,              //     Logical Maximum (1)
  0x75, 0x01,              //     Report Size (1)
  0x95, 0x04,              //     Report Count (4)
  0x81, 0x02,              //     Input (Data, Variable, Absolute)
  0x95, 0x04,              //     Report Count (4)
  0x81, 0x03,              //     Input (Constant) padding
  0x05, 0x01,              //     Usage Page (Generic Desktop)
  0x26, 0xFF, 0x7F,        //     Logical Maximum (32767)
  0x35, 0x00,              //     Physical Minimum (0)
  0x46, 0xE8, 0x03,        //     Physical Maximum (1000)
  0x55, 0x0E,              //     Unit Exponent (-2)
  0x65, 0x13,              //     Unit (Inch) -> 10 x 10 inch surface
  0x75, 0x10,              //     Report Size (16)
  0x95, 0x01,              //     Report Count (1)
  0x09, 0x30,              //     Usage (X)
  0x81, 0x02,              //     Input (Data, Variable, Absolute)
  0x09, 0x31,              //     Usage (Y)
  0x81, 0x02,              //     Input (Data, Variable, Absolute)
  0x05, 0x0D,              //     Usage Page (Digitizer)
  0x09, 0x30,              //     Usage (Tip Pressure)
  0x26, 0xFF, 0x03,        //     Logical Maximum (1023)
  0x45, 0x00,              //     Physical Maximum (0)
  0x55, 0x00,              //     Unit Exponent (0)
  0x65, 0x00,              //     Unit (None)
  0x81, 0x02,              //     Input (Data, Variable, Absolute)
  0xC0,                    //   End Collection
  0xC0,                    // End Collection
};

struct __attribute__((packed)) PenReport {
  uint8_t buttons; // Bit 0: tip, 1: barrel, 2: eraser, 3: in range.
  uint16_t x;
  uint16_t y;
  uint16_t pressure;
};

bool HIDPen::begin(HIDTransport transport) {
  if (reportId) return false;
  const uint8_t id = HIDUtil::NextReportId();
  if (!id) return false;
  HIDProfile profile{{report_descriptor, report_descriptor + sizeof(report_descriptor)},
                     {{HID_REPORT_ID_PEN, std::vector<uint8_t>(7)}}, {}};
  if (!HIDUtil::RegisterProfile(profile, transport, id)) return false;
  reportId = id;
  return true;
}

bool HIDPen::send(bool tip, bool inRange, bool barrel, bool eraser, int x, int y, int pressure) {
  if (!reportId || x < 0 || x > 32767 || y < 0 || y > 32767 || pressure < 0 || pressure > 1023) return false;
  PenReport report = {
    static_cast<uint8_t>(tip | barrel << 1 | eraser << 2 | inRange << 3),
    static_cast<uint16_t>(x),
    static_cast<uint16_t>(y),
    static_cast<uint16_t>(pressure),
  };
  return HIDUtil::SendInput(reportId, reinterpret_cast<const uint8_t*>(&report), sizeof(report));
}
