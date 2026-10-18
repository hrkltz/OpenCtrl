#include <DisplayUtil.hpp>
#include <HIDUtil.hpp>
#include <M5StackInputs.hpp>
#include <ControllerCalibration.hpp>
#include "Config.hpp"
#include "Controls.hpp"

M5Stack::Joystick2 primary(Wire, {0x63, Config::primaryChannel, Config::hubAddress});
M5Stack::Joystick2 secondary(Wire, {0x63, Config::secondaryChannel, Config::hubAddress});
ControllerCalibration calibration;
bool started = false;
bool inputHealthy = true;

void setup() {
  Serial.begin(115200);
  if (!Wire.begin(SDA, SCL)) return;
  Wire.setClock(100000);
  Wire.setTimeOut(20);
  DisplayUtil::Init();
  DisplayUtil::Print(0, "DriveController");
  DisplayUtil::Print(2, "Center sticks");
  delay(500);

  // Configure measured endpoints/deadzone here if the unit needs adjustment.
  secondary.y.inverted = true;
  if (!calibration.Configure("drive-cal", {
      {"primary.x", "Primary stick · X", &primary.x}, {"primary.y", "Primary stick · Y", &primary.y},
      {"secondary.x", "Auxiliary stick · X", &secondary.x}, {"secondary.y", "Auxiliary stick · Y", &secondary.y}})) {
    DisplayUtil::PrintError("Calibration init");
    return;
  }
  // Saved values take precedence; absent/invalid settings use startup calibration.
  if (!calibration.Load()) {
    primary.CalibrateCenter();
    secondary.CalibrateCenter();
  }
  if (!HIDUtil::Init(HIDUtil::Gamepad, {}, Config::transport)) {
    DisplayUtil::PrintError("HID init");
    return;
  }
  DisplayUtil::Print(1, Config::transport == HIDTransport::USB ? "USB gamepad" :
      Config::transport == HIDTransport::Bluetooth ? "BLE: OpenCtrl" : "USB + BLE");
  if (calibration.Begin("DriveCtrl", "DriveController")) {
    DisplayUtil::Print(2, calibration.Address().c_str());
    DisplayUtil::Print(3, calibration.SSID());
    DisplayUtil::Print(4, "WiFi password:");
    DisplayUtil::Print(5, calibration.Password());
    DisplayUtil::Print(6, "Open page to");
    DisplayUtil::Print(7, "calibrate");
  } else DisplayUtil::Print(2, "Setup WiFi failed");
  started = true;
}

void loop() {
  HIDUtil::Service();
  if (!started) { delay(Config::pollMilliseconds); return; }
  M5Stack::JoystickRawState firstRaw, secondRaw;
  const bool firstOnline = primary.ReadRaw(firstRaw);
  const bool secondOnline = secondary.ReadRaw(secondRaw);
  calibration.UpdateRaw(0, firstRaw.x, firstOnline);
  calibration.UpdateRaw(1, firstRaw.y, firstOnline);
  calibration.UpdateRaw(2, secondRaw.x, secondOnline);
  calibration.UpdateRaw(3, secondRaw.y, secondOnline);
  calibration.Service();
  const bool healthy = firstOnline && secondOnline;
  M5Stack::JoystickState first{primary.x.Normalize(firstRaw.x), primary.y.Normalize(firstRaw.y), firstRaw.pressed};
  M5Stack::JoystickState second{secondary.x.Normalize(secondRaw.x), secondary.y.Normalize(secondRaw.y), secondRaw.pressed};
  if (healthy != inputHealthy) {
    DisplayUtil::Print(7, healthy ? "Inputs ready" : "Input lost");
    inputHealthy = healthy;
  }
  if (!healthy || calibration.Active()) {
    HIDUtil::ReleaseAll();
  } else {
    const auto controls = DriveControls::Map(first, second);
    // Periodic absolute state also restores unchanged controls after reconnect.
    HIDUtil::SendGamepad(controls.steering, 0, controls.brake, controls.throttle, 0, 0, 0, controls.buttons);
  }
  delay(Config::pollMilliseconds);
}
