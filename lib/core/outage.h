#pragma once
// Графік відключень у спільному форматі нормалізатора (outages/README.md) і модель екрана 2 (карта по годинах).
// Файл береться з GitHub Pages: https://alexmovchan.github.io/deye-mini-display/outages/<provider>/<черга>.json
#include <stddef.h>
#include <stdint.h>

#include "tiles.h"

namespace outage {

constexpr int SLOTS = 48;  // півгодинки

enum class Status : uint8_t { None, Applies, Waiting, Emergency };

struct Day {
  int32_t dayNumber = -1;  // kt::daysFromCivil; -1 — дня немає
  Status status = Status::None;
  uint8_t off[SLOTS] = {};  // 1 — світла немає (і «?» з джерела — обережніше вважати, що немає)
};

struct Schedule {
  bool valid = false;
  char group[8] = "";
  int64_t fetched = 0;  // unix, коли нормалізатор забрав графік
  Day days[3];
  int dayCount = 0;
};

bool parse(const char* body, size_t len, Schedule* out);

// Черга з назви: "47.1", "3.2" (цифри, крапка, цифра)
bool validGroup(const char* g);

// ── Екран 2 ──
struct Cell {
  char hour[6];        // "07:00"
  bool first, second;  // світло є в першу / другу половину години
  bool now;
};

struct DayView {
  bool cells = false;  // показувати 24 клітинки; інакше — note
  char title[64] = "";
  char note[48] = "";
  Cell c[24];
};

struct MapModel {
  tiles::Rgb frame = tiles::color::FRAME_CALM;
  char title[32] = "";    // «Черга 47.1»
  char clock[6] = "";
  char updated[40] = "";  // «графік на 21:37» / «графік недоступний»
  tiles::Rgb updatedColor = tiles::color::DIM;
  DayView today, tomorrow;
};

// staleAfter — скільки секунд графік вважається свіжим (далі — сірим «графік на …»).
MapModel buildMap(const Schedule* s, const char* group, int64_t now, tiles::Rgb frame, int staleAfter);

}  // namespace outage
