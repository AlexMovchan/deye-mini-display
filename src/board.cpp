#include "board.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <lvgl.h>

#include "config.h"
#include "event_log.h"

namespace board {

namespace {
constexpr spi_host_device_t SPI_HOST_ID = SPI3_HOST;
constexpr int DRAW_LINES = 32;  // буфер малювання 240×32×2 = 15 КБ (PSRAM немає — малюємо частинами)
constexpr int BL_FREQ = 400, BL_BITS = 8;

esp_lcd_panel_handle_t panel = nullptr;
lv_display_t* display = nullptr;

bool ok(esp_err_t e, const char* what) {
  if (e == ESP_OK) return true;
  evlog::add("Екран: %s — %s", what, esp_err_to_name(e));
  return false;
}

bool onTransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
  lv_display_flush_ready(static_cast<lv_display_t*>(ctx));
  return false;
}

void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  lv_draw_sw_rgb565_swap(px, lv_area_get_size(a));  // ST7789 чекає старший байт першим
  if (esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px) != ESP_OK) lv_display_flush_ready(d);
}
}  // namespace

bool begin() {
  // Підсвітка вимкнена, доки не буде першого кадру
  ledcAttach(LCD_BL, BL_FREQ, BL_BITS);
  ledcWrite(LCD_BL, 0);

  // Спільне скидання екрана й сенсора
  pinMode(LCD_TP_RST, OUTPUT);
  digitalWrite(LCD_TP_RST, LOW);
  delay(20);
  digitalWrite(LCD_TP_RST, HIGH);
  delay(50);

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  display = lv_display_create(W, H);
  if (!display) return false;
  const size_t bufBytes = W * DRAW_LINES * sizeof(uint16_t);
  void* buf = heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf) {
    evlog::add("Екран: немає %u Б під буфер малювання", (unsigned)bufBytes);
    return false;
  }
  lv_display_set_buffers(display, buf, nullptr, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = LCD_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = LCD_SCLK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = (int)bufBytes;
  if (!ok(spi_bus_initialize(SPI_HOST_ID, &bus, SPI_DMA_CH_AUTO), "SPI")) return false;

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = LCD_CS;
  io.dc_gpio_num = LCD_DC;
  io.spi_mode = 0;
  io.pclk_hz = LCD_PCLK_HZ;
  io.trans_queue_depth = 10;
  io.on_color_trans_done = onTransferDone;
  io.user_ctx = display;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  esp_lcd_panel_io_handle_t ioh = nullptr;
  if (!ok(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io, &ioh), "панель IO")) return false;

  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = -1;  // скидання — спільне, вище
  dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  dev.bits_per_pixel = 16;
  if (!ok(esp_lcd_new_panel_st7789(ioh, &dev, &panel), "ST7789") || !ok(esp_lcd_panel_init(panel), "init") ||
      !ok(esp_lcd_panel_invert_color(panel, true), "інверсія") ||  // як у виробника
      !ok(esp_lcd_panel_mirror(panel, LCD_MIRROR_X, LCD_MIRROR_Y), "дзеркало") ||
      !ok(esp_lcd_panel_disp_on_off(panel, true), "увімкнення"))
    return false;
  lv_display_set_flush_cb(display, flush);

  pinMode(TP_INT, INPUT);
  if (!Wire.begin(TP_SDA, TP_SCL, 400000)) evlog::add("Сенсор: I2C не запустився");
  evlog::add("Екран: ST7789 240×320, вільно %u КБ", (unsigned)(ESP.getFreeHeap() / 1024));
  return true;
}

void backlight(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  ledcWrite(LCD_BL, ((1 << BL_BITS) - 1) * percent / 100);
}

bool readTouch(Touch* t) {
  // CST816D: з регістра 0x02 — кількість пальців, X (12 біт), Y (12 біт)
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t d[5];
  if (Wire.requestFrom((int)TP_ADDR, 5) != 5) return false;
  for (uint8_t& b : d) b = Wire.read();
  t->pressed = (d[0] & 0x0F) > 0;
  t->x = ((d[1] & 0x0F) << 8) | d[2];
  t->y = ((d[3] & 0x0F) << 8) | d[4];
  if (TP_FLIP_X) t->x = W - 1 - t->x;
  return true;
}

}  // namespace board
