#pragma once
// Екрани на LVGL: основний (плитки 2×2 з моделі lib/core/tiles) і налаштування (QR-код мережі Deye-Setup
// і стан кроків). Об'єкти створюються один раз, далі лише оновлюються. Без Arduino — рендериться й на ПК.
#include "tiles.h"

namespace ui {

struct Step {
  enum State : uint8_t { Pending, Active, Done, Error } state = Pending;
  char text[64] = "";
};

struct SetupView {
  Step wifi, logger, region;
  char note[128] = "";  // підказка внизу
};

void begin();                           // після board::begin() (на ПК — після lv_init і дисплея)
void showTiles(const tiles::Model& m);  // оновити й показати основний екран
void showSetup(const SetupView& v);     // оновити й показати екран налаштування

}  // namespace ui
