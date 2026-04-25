/**
 * @file M5Joystick2Util.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 23.01.2026
 */
#include "M5Joystick2Util.hpp"

bool M5Joystick2Util::Init(uint8_t iic_address) {
  if(joystick2.begin(&Wire, iic_address, 21, 22) != 1)
  {
    return false;
  }

  joystick2.set_rgb_color(0x00ff00);
  return true;
}

M5Joystick2Data M5Joystick2Util::GetData() {
  M5Joystick2Data joystick2Data = {0.0f, 0.0f};
  uint16_t adc_x, adc_y;
  joystick2.get_joy_adc_16bits_value_xy(&adc_x, &adc_y);

  // Normalize both axis to -1.0 to 1.0.
  joystick2Data.x = static_cast<float>(adc_x) / 65535.0f * 2.0f - 1.0f; 
  joystick2Data.y = static_cast<float>(adc_y) / 65535.0f * 2.0f - 1.0f;

  return joystick2Data;
}
