#include "HTTPServerUtil.hpp"
#include "DisplayUtil.hpp"
#include "HIDUtil.hpp"
#include "M5Joystick2Util.hpp"
#include <WiFi.h>

// I2C Multiplexer
#include "TCA9548.h"
PCA9546 MP(0x70);
uint8_t channels = 0;
// Connected I2C devices on the multiplexer.
M5Joystick2Util joystickSteer;
M5Joystick2Util joystickDrive;

// --- WiFi Credentials ---
// Please define WIFI_SSID and WIFI_PASSWORD in your build environment.
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif

const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASSWORD;
// --- End WiFi Credentials ---

float centerOffsetSteer = 0.0f;
float centerOffsetDrive = 0.0f;
float minSteerMeasurement = 0.0f;
float minDriveMeasurement = 0.0f;
float maxSteerMeasurement = 0.0f;
float maxDriveMeasurement = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(300);

  DisplayUtil::Init();

  // WiFi init.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  DisplayUtil::Print(0, "Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    DisplayUtil::Print(0, "Connecting.");
    delay(250);
    DisplayUtil::Print(0, "Connecting..");
    delay(250);
    DisplayUtil::Print(0, "Connecting...");
    delay(250);
  }
  DisplayUtil::Print(0, WiFi.localIP().toString().c_str());

  HTTPServerUtil::Init();
  HIDUtil::Init();

  // I2C init.
  Wire.begin();
  //  initialize multiplexer
  if (MP.begin() == false)
  {
    Serial.println("Multiplexer error");
  }

  // Initialize connected I2C devices.
  MP.selectChannel(2); // Joystick Steer
  if (!joystickSteer.Init(0x63)) {
    // Handle initialization failure
  }

  MP.selectChannel(3); // Joystick Drive
  if (!joystickDrive.Init(0x63)) {
    // Handle initialization failure
  }

  { // Set center offset for joysticks.
    DisplayUtil::Print(1, "Calibrating Center...");
    
    // Init float array with 11 values.
    float centerOffsetSteerMeasurementArray[11];
    float centerOffsetDriveMeasurementArray[11];

    // Read the joystick values 11 times and calculate the average.
    for (int i = 0; i < 11; i++) {
      MP.selectChannel(2);
      M5Joystick2Data joystickSteerData = joystickSteer.GetData();
      centerOffsetSteerMeasurementArray[i] = joystickSteerData.y*-1.0f; // Invert steering joystick axis.
      MP.selectChannel(3);
      M5Joystick2Data joystickDriveData = joystickDrive.GetData();
      centerOffsetDriveMeasurementArray[i] = joystickDriveData.y*-1.0f; // Invert driving joystick axis.
      delay(10);
    }

    std::sort(centerOffsetSteerMeasurementArray, centerOffsetSteerMeasurementArray + 11);
    std::sort(centerOffsetDriveMeasurementArray, centerOffsetDriveMeasurementArray + 11);
    centerOffsetSteer = centerOffsetSteerMeasurementArray[5]; // Use median value instead of average for better accuracy.
    centerOffsetDrive = centerOffsetDriveMeasurementArray[5];
  }

  { // Get the minimal values for both joysticks for around 2 seconds.
    DisplayUtil::Print(1, "Calibrating Min...");

    // Read the joystick min values 10 times and use the lowest value.
    for (int i = 0; i < 20; i++) {
      MP.selectChannel(2);
      M5Joystick2Data joystickSteerData = joystickSteer.GetData();
      if (joystickSteerData.y*-1.0f < minSteerMeasurement) {
        minSteerMeasurement = joystickSteerData.y*-1.0f;
      }
      MP.selectChannel(3);
      M5Joystick2Data joystickDriveData = joystickDrive.GetData();
      if (joystickDriveData.y*-1.0f < minDriveMeasurement) {
        minDriveMeasurement = joystickDriveData.y*-1.0f;
      }
      delay(100);
    }
  }

  { // Get the maximal values for both joysticks for around 2 seconds.
    DisplayUtil::Print(1, "Calibrating Max...");
    // Read the joystick max values 10 times and use the highest value.
    for (int i = 0; i < 20; i++) {
      MP.selectChannel(2);
      M5Joystick2Data joystickSteerData = joystickSteer.GetData();
      if (joystickSteerData.y*-1.0f > maxSteerMeasurement) {
        maxSteerMeasurement = joystickSteerData.y*-1.0f;
      }
      MP.selectChannel(3);
      M5Joystick2Data joystickDriveData = joystickDrive.GetData();
      if (joystickDriveData.y*-1.0f > maxDriveMeasurement) {
        maxDriveMeasurement = joystickDriveData.y*-1.0f;
      }
      delay(100);
    }
  }

  // Display calibration results.
  const char* steerCalibrationText = "%.1f/%.1f/%.1f";
  char steerCalibrationBuffer[32];
  snprintf(steerCalibrationBuffer, sizeof(steerCalibrationBuffer), steerCalibrationText, minSteerMeasurement, centerOffsetSteer, maxSteerMeasurement);
  DisplayUtil::Print(1, steerCalibrationBuffer);
  const char* driveCalibrationText = "%.1f/%.1f/%.1f";
  char driveCalibrationBuffer[32];
  snprintf(driveCalibrationBuffer, sizeof(driveCalibrationBuffer), driveCalibrationText, minDriveMeasurement, centerOffsetDrive, maxDriveMeasurement);
  DisplayUtil::Print(2, driveCalibrationBuffer);

  // Define new min / max boundaries based on the center offset to have a symmetric range.
  minSteerMeasurement += centerOffsetSteer; // eg. if center offset is 0.1f -> min limit is 1.1f
  maxSteerMeasurement -= centerOffsetSteer; // eg. if center offset is 0.1f -> max limit is 0.9f
  minDriveMeasurement += centerOffsetDrive;
  maxDriveMeasurement -= centerOffsetDrive;
}

int8_t oldJoystickSteerValue = 0;
int8_t oldJoystickDriveValue = 0;

void loop() {
  int8_t joystickSteerMedianValue = 0;
  int8_t joystickDriveMedianValue = 0;

  { // Read joystick and build median values from five samples for each joystick.
    float joystickSteerSamples[5];
    float joystickDriveSamples[5];

    // Take five samples for each joystick.
    for (int i = 0; i < 5; i++) {
      // 1. Steering Joystick.
      MP.selectChannel(2);
      M5Joystick2Data joystickSteerData = joystickSteer.GetData();
      joystickSteerSamples[i] = joystickSteerData.y*-1.0f;
      
      // 2. Driving Joystick.
      MP.selectChannel(3);
      M5Joystick2Data joystickDriveData = joystickDrive.GetData();
      joystickDriveSamples[i] = joystickDriveData.y*-1.0f;
      
      delay(1);
    }

    // Calculate median for booth joystick.
    std::sort(joystickSteerSamples, joystickSteerSamples + 5);
    std::sort(joystickDriveSamples, joystickDriveSamples + 5);

    // Remove center offset from median samples.
    joystickSteerSamples[2] -= centerOffsetSteer;
    joystickDriveSamples[2] -= centerOffsetDrive;

    {// Scale median samples from -1.0f to +1.0f range based on min / max boundaries which are based on center offset.
      joystickSteerSamples[2] < 0
        ? joystickSteerSamples[2] /= -minSteerMeasurement // Warning: minSteerMeasurement is a negative value!
        : joystickSteerSamples[2] /= maxSteerMeasurement;
      joystickDriveSamples[2] < 0
        ? joystickDriveSamples[2] /= -minDriveMeasurement // Warning: minDriveMeasurement is a negative value!
        : joystickDriveSamples[2] /= maxDriveMeasurement;
    }

    // Ignore values in deadzone (before applying non-linear transformation) around center position (+-5).
    if (std::abs(joystickSteerMedianValue) < 3) {
      joystickSteerMedianValue = 0;
    }
    if (std::abs(joystickDriveMedianValue) < 3) {
      joystickDriveMedianValue = 0;
    }

    // Make both joystick values non-linear for finer control around center position.
    joystickDriveSamples[2] = std::copysign(std::pow(std::abs(joystickDriveSamples[2]), 2.0f), joystickDriveSamples[2]);
    joystickSteerSamples[2] = std::copysign(std::pow(std::abs(joystickSteerSamples[2]), 2.0f), joystickSteerSamples[2]);

    // Map median samples to int8_t range -127 to +127. Clamp values to avoid overflow.
    joystickDriveMedianValue = static_cast<int8_t>(std::clamp((joystickDriveSamples[2] * 127.0f), -127.0f, 127.0f));
    joystickSteerMedianValue = static_cast<int8_t>(std::clamp((joystickSteerSamples[2] * 127.0f), -127.0f, 127.0f));
    

    // Make the steering joystick value non-linear for finer control around center position.
    joystickSteerMedianValue = static_cast<int8_t>(std::copysign(std::pow(std::abs(joystickSteerMedianValue) / 127.0f, 1.5f) * 127.0f, joystickSteerMedianValue));
  }

  // Only update if values have changed.
  if (oldJoystickSteerValue != joystickSteerMedianValue || oldJoystickDriveValue != joystickDriveMedianValue) {
    // Display joystick values.
    const char* steerText = "S: %d";
    char steerBuffer[32];
    snprintf(steerBuffer, sizeof(steerBuffer), steerText, joystickSteerMedianValue);
    DisplayUtil::Print(3, steerBuffer);
    const char* driveText = "D: %d";
    char driveBuffer[32];
    snprintf(driveBuffer, sizeof(driveBuffer), driveText, joystickDriveMedianValue);
    DisplayUtil::Print(4, driveBuffer);

    // Send HID gamepad report.
    HIDUtil::SendGamepad(joystickSteerMedianValue, joystickDriveMedianValue, 0, 0, 0, 0, 0, 0);

    // Update old values.
    oldJoystickSteerValue = joystickSteerMedianValue;
    oldJoystickDriveValue = joystickDriveMedianValue;
  }

  delay(8);
}