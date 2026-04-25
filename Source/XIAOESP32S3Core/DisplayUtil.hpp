/**
 * @file DisplayUtil.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 17.01.2026
 */
#pragma once

#include <U8x8lib.h>
#include <Wire.h>

class DisplayUtil {
  private:
      static U8X8_SSD1306_128X64_NONAME_HW_I2C _mU8x8;
  public:
      static void Init();
      static void Print(uint8_t pLine, const char* pText);
      static void PrintIP(const char* pIP);
      static void PrintConnectionStatus(const char* pConnectionStatus);
      static void PrintHeap(int pHeap);
};
