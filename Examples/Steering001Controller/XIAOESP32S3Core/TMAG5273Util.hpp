/**
 * @file TMAG5273Util.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 19.01.2026
 */
#pragma once

#include <Wire.h>
#include "SparkFun_TMAG5273_Arduino_Library.h"

struct TMAG5273Data {
    float magX;
    float magY;
    float magZ;
};

class TMAG5273Util {
  private:
      static TMAG5273 tmag5273;
      static uint8_t i2cAddress;
  public:
      static bool Init();
      static TMAG5273Data Read();
};
