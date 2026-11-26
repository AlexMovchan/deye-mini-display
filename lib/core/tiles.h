#pragma once
// Модель основного екрана «плитки 2×2» (варіант C): що й якими кольорами показати. Без LVGL і Arduino —
// тестується на ПК; src/ui.cpp лише малює цю модель. Правила — docs/PLAN.md §1.
#include <stdint.h>

#include "inverter.h"
#include "siren.h"

namespace tiles {

using Rgb = uint32_t;

namespace color {
// 0.1.1: на IPS-матриці під кутом темно-сірі картки на темно-сірому тлі зливались — тло чорне, картки світліші
// з обвідкою, підписи світліші
constexpr Rgb FRAME_CALM = 0x000000, FRAME_YELLOW = 0xF5C518, FRAME_RED = 0xD7263D;
constexpr Rgb CARD = 0x262A31, CARD_EDGE = 0x4A505A, CARD_LIGHT = 0xE6E8EB, CARD_OFF = 0x000000, BORDER = 0x6A707A;
constexpr Rgb TEXT = 0xE6E8EB, TEXT_DARK = 0x15171B, TEXT_DARK2 = 0x555B64;
constexpr Rgb MUTED = 0xCDD2D9, DIM = 0xA0A6AF, STALE = 0x6E747E, UNIT = 0xD8DCE1;
constexpr Rgb ALERT_YELLOW = 0xF5C518, ALERT_RED = 0xFF6B78, WARN = 0xFF8A65;
constexpr Rgb PLUS = 0x6EE7B7, MINUS = 0xFF8A65;
constexpr Rgb SOC_GREEN = 0x4CC38A, SOC_YELLOW = 0xE5B92E, SOC_RED = 0xE5484D;
constexpr Rgb BOLT = 0xF5C518;
}  // namespace color

enum class LoggerState : uint8_t {
  Searching,    // ще шукаємо логер (після старту чи коли пропав)
  Ok,           // свіжі дані
  Stale,        // були дані, але логер мовчить довше LOGGER_STALE_SECONDS
  NoData,       // даних ще не було й логер не відповідає
  Unsupported,  // інвертор не однофазний гібрид
};

enum class AlertState : uint8_t {
  Off,     // регіон не задано — тривоги не показуємо
  Ok,      // свіжі дані
  NoData,  // запит не вдався / давно немає відповіді
};

// Значок біля годинника: є зв'язок / слабкий сигнал (< −75 дБм) / немає (Wi-Fi відпав або дані не ходять)
enum class Wifi : uint8_t { Ok, Weak, Off };
Wifi wifiState(bool online, bool dataFlowing, int rssi);

struct Input {
  Wifi wifi = Wifi::Ok;
  int64_t now = 0;            // unix; 0 — годинника ще немає
  LoggerState logger = LoggerState::Searching;
  int staleMinutes = 0;       // для Stale
  inv::Reading reading;
  inv::Grid grid = inv::Grid::Unknown;
  int64_t gridSince = 0;
  float hoursLeft = -1;       // < 0 — «надовго»
  uint16_t deviceType = 0;    // для Unsupported
  AlertState alertState = AlertState::Off;
  siren::Alert alert;
};

enum class Icon : uint8_t { None, BoltOn, BoltOff };

struct Tile {
  char title[32] = "";
  Rgb titleColor = color::MUTED;
  Rgb bg = color::CARD;
  Rgb border = 0;              // 0 — без рамки
  Icon icon = Icon::None;
  // Значення: big — великими цифрами (лише 0-9, %, —), unit — дрібніше поруч; або text — звичайним шрифтом.
  char big[8] = "";
  char unit[24] = "";
  char text[24] = "";
  Rgb valueColor = color::TEXT;
  Rgb unitColor = color::UNIT;
  char sub[48] = "";
  Rgb subColor = color::MUTED;
  bool bar = false;            // смужка заряду
  float barFrac = 0;
  Rgb barColor = color::SOC_GREEN;
};

struct Model {
  Rgb frame = color::FRAME_CALM;
  char top[64] = "";
  Rgb topColor = color::MUTED;
  char clock[6] = "";          // "HH:MM" або ""
  Wifi wifi = Wifi::Ok;
  Tile light, soc, load, batt;  // зліва направо, згори вниз
  bool message = false;        // замість плиток — повідомлення (немає логера, не той інвертор)
  char msgTitle[64] = "";
  Rgb msgTitleColor = color::TEXT;
  char msgText[256] = "";
};

Model build(const Input& in);

// Допоміжне (тестується окремо)
Rgb socColor(int soc);
void formatHours(float hours, char* big, int bigLen, char* unit, int unitLen);  // 2 + «год 10 хв», 40 + «хв»
void formatWatts(int w, char* big, int bigLen, char* unit, int unitLen);       // 294 + «Вт»; ≥ 10 000 → 12 + «кВт»

}  // namespace tiles
