#pragma once
// NiceMCU-32S-DEV_2.8IPS: екран ST7789 (SPI, esp_lcd) для LVGL 9, сенсор CST816D (I²C), підсвітка (ШІМ).
// Драйвер — за заводською прошивкою виробника (NiceMCU-32S-DEV_2.8IPS, src/nicemcu_display.cpp),
// перенесений на Arduino-ESP32 3.x. Піни — include/config_mini.h.
#include <stdint.h>

namespace board {

constexpr int W = 240, H = 320;

// Порядок як у виробника: спільне скидання → SPI/ST7789 → LVGL-дисплей → I²C сенсора. Підсвітка вимкнена.
bool begin();
void backlight(int percent);  // 0..100, плавно (ШІМ)

// Сенсор: є дотик зараз і де (координати екрана після TP_FLIP_X).
struct Touch {
  bool pressed;
  int x, y;
};
bool readTouch(Touch* t);  // false — I²C не відповів

}  // namespace board
