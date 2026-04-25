/**
 * @file DisplayUtil.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 19.01.2026
 */
#include "DisplayUtil.hpp"

U8X8_SSD1306_128X64_NONAME_HW_I2C DisplayUtil::_mU8x8(U8X8_PIN_NONE);

void DisplayUtil::Init() {
  _mU8x8.begin();
}

void DisplayUtil::Print(uint8_t pLine, const char* pText) {
  if (pLine > 7) return; // Max 8 lines (0-7)
  _mU8x8.setFont(u8x8_font_chroma48medium8_r);
  _mU8x8.setCursor(0, pLine);
  _mU8x8.clearLine(pLine);
  _mU8x8.print(pText);
}

void DisplayUtil::PrintIP(const char* pIP) {
  _mU8x8.setFont(u8x8_font_chroma48medium8_r);
  _mU8x8.setCursor(0, 1);
  _mU8x8.clearLine(1);
  _mU8x8.print(pIP);
}

void DisplayUtil::PrintConnectionStatus(const char* pConnectionStatus) {
  _mU8x8.setFont(u8x8_font_chroma48medium8_r);
  _mU8x8.setCursor(0, 0);
  _mU8x8.clearLine(0);
  _mU8x8.print(pConnectionStatus);
}

void DisplayUtil::PrintHeap(int pHeap) {
  _mU8x8.setFont(u8x8_font_chroma48medium8_r);
  _mU8x8.setCursor(0, 2);
  _mU8x8.clearLine(2);
  _mU8x8.print("Heap:");
  _mU8x8.print(pHeap);
}

void DisplayUtil::PrintError(const char* pErrorText) {
  _mU8x8.setFont(u8x8_font_chroma48medium8_r);
  _mU8x8.clearLine(0);
  _mU8x8.clearLine(1);
  _mU8x8.clearLine(2);
  _mU8x8.clearLine(3);
  _mU8x8.clearLine(4);
  _mU8x8.clearLine(5);
  _mU8x8.clearLine(6);
  _mU8x8.clearLine(7);
  _mU8x8.setCursor(0, 1);
  _mU8x8.print("ERROR:");
  _mU8x8.setCursor(0, 2);
  _mU8x8.print(pErrorText);
}