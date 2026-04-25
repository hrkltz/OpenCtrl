/**
 * @file M5Joystick2Util.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 23.01.2026
 */
#pragma once

#include <Wire.h>
#include "m5_unit_joystick2.hpp"

struct M5Joystick2Data {
    float x;
    float y;
};

class M5Joystick2Util {
  private:
      M5UnitJoystick2 joystick2;
  public:
      bool Init(uint8_t iic_address);
      M5Joystick2Data GetData();
};
