// Рендер екранів src/ui.cpp на ПК тим самим LVGL — для звірки з макетами без плати.
// Картинки 240×320 (PPM) у теку з RENDER_DIR: pio test -e render
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <string>

#include "kyiv_time.h"
#include "tiles.h"
#include "ui.h"

void setUp() {}
void tearDown() {}

static uint16_t fb[240 * 320];
static uint32_t fakeTick = 0;

static void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  const uint16_t* p = (const uint16_t*)px;
  for (int y = a->y1; y <= a->y2; y++)
    for (int x = a->x1; x <= a->x2; x++) fb[y * 240 + x] = *p++;
  lv_display_flush_ready(d);
}

static void save(const char* name) {
  for (int i = 0; i < 5; i++) {
    fakeTick += 50;
    lv_timer_handler();
  }
  lv_refr_now(nullptr);
  const char* dir = getenv("RENDER_DIR");
  if (!dir) return;
  std::string path = std::string(dir) + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  TEST_ASSERT_NOT_NULL(f);
  fprintf(f, "P6\n240 320\n255\n");
  for (uint16_t c : fb) {
    unsigned char rgb[3] = {(unsigned char)((c >> 11 & 0x1F) * 255 / 31), (unsigned char)((c >> 5 & 0x3F) * 255 / 63),
                            (unsigned char)((c & 0x1F) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static int64_t kyiv(int d, int h, int mi) { return kt::daysFromCivil(2026, 11, d) * 86400 + (h - 2) * 3600 + mi * 60; }

static tiles::Input calm() {
  tiles::Input in;
  in.now = kyiv(26, 11, 20);
  in.logger = tiles::LoggerState::Ok;
  in.reading.soc = 100;
  in.reading.gridV = 234;
  in.reading.battW = -42;
  in.reading.loadW = 294;
  in.reading.dayKwh = 6.9f;
  in.grid = inv::Grid::On;
  in.gridSince = kyiv(26, 9, 30);
  in.alertState = tiles::AlertState::Ok;
  return in;
}

static void test_render_tiles() {
  ui::showTiles(tiles::build(calm()));
  save("tiles_calm");

  tiles::Input in = calm();
  in.grid = inv::Grid::Off;
  in.gridSince = kyiv(26, 15, 0);
  in.reading.soc = 38;
  in.reading.battW = 410;
  in.reading.loadW = 410;
  in.hoursLeft = 2 + 10 / 60.0f;
  in.alert.active = true;
  in.alert.red = true;
  strcpy(in.alert.head, "Повітряна тривога");
  strcpy(in.alert.since, "15:41");
  ui::showTiles(tiles::build(in));
  save("tiles_red_off");

  in = calm();
  in.alert.active = true;
  strcpy(in.alert.head, "Дронова небезпека");
  strcpy(in.alert.since, "10:02");
  in.reading.soc = 61;
  in.reading.battW = 1200;
  ui::showTiles(tiles::build(in));
  save("tiles_yellow_discharge");

  in = calm();
  in.alertState = tiles::AlertState::Off;
  in.logger = tiles::LoggerState::Stale;
  in.staleMinutes = 4;
  ui::showTiles(tiles::build(in));
  save("tiles_stale_no_region");

  in = calm();
  in.logger = tiles::LoggerState::Searching;
  ui::showTiles(tiles::build(in));
  save("msg_searching");

  in.logger = tiles::LoggerState::Unsupported;
  in.deviceType = 5;
  ui::showTiles(tiles::build(in));
  save("msg_unsupported");
}

static void test_render_setup() {
  ui::SetupView v;
  v.wifi.state = ui::Step::Active;
  strcpy(v.wifi.text, "Wi-Fi: чекаю телефон");
  strcpy(v.logger.text, "Логер");
  strcpy(v.region.text, "Тривоги (необов’язково)");
  strcpy(v.note, "Скинути налаштування — утримуйте BOOT 5 с");
  ui::showSetup(v);
  save("setup_start");

  v.wifi.state = ui::Step::Done;
  strcpy(v.wifi.text, "Wi-Fi: Home_2G");
  v.logger.state = ui::Step::Active;
  strcpy(v.logger.text, "Шукаю логер: 107 з 253");
  ui::showSetup(v);
  save("setup_search");

  v.logger.state = ui::Step::Error;
  strcpy(v.logger.text, "Логер не знайдено");
  strcpy(v.note, "Перевірте серійник і що логер у тій самій мережі");
  ui::showSetup(v);
  save("setup_fail");
}

int main() {
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return fakeTick; });
  lv_display_t* d = lv_display_create(240, 320);
  static uint16_t buf[240 * 40];
  lv_display_set_buffers(d, buf, nullptr, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(d, flush);
  ui::begin();
  UNITY_BEGIN();
  RUN_TEST(test_render_tiles);
  RUN_TEST(test_render_setup);
  return UNITY_END();
}
