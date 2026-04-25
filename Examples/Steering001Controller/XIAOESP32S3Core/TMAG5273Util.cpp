/**
 * @file TMAG5273Util.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 19.01.2026
 */
#include "TMAG5273Util.hpp"

TMAG5273 TMAG5273Util::tmag5273;
uint8_t TMAG5273Util::i2cAddress = TMAG5273_I2C_ADDRESS_INITIAL;

bool TMAG5273Util::Init() {
  if(tmag5273.begin(i2cAddress, Wire) != 1)
  {
    return false;
  }

  return true;
}

TMAG5273Data TMAG5273Util::Read() {
  TMAG5273Data tmag5273Data = {0.0f, 0.0f, 0.0f};

  // Checks if mag channels are on - turns on in setup
  if(tmag5273.getMagneticChannel() == 0) {
    return tmag5273Data;
  }

  tmag5273.setTemperatureEn(false);

  tmag5273Data.magX = tmag5273.getXData();
  tmag5273Data.magY = tmag5273.getYData();
  tmag5273Data.magZ = tmag5273.getZData();

  return tmag5273Data;
}
