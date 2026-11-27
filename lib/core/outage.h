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

// ── Екран 2 (рішення власника 26.11.2026): до 4 рядків-проміжків від поточного — «14:00–18:00» білим, коли світло
// є, червоним — коли немає. Замість рядків — лише «Екстрені відключення» або «Даних немає».
constexpr int ROWS = 4;

struct Row {
  char text[24] = "";   // "14:00–18:00"
  bool light = false;   // білим (є) / червоним (немає)
  bool tomorrow = false;
  bool current = false;  // проміжок, що триває зараз (обвідка білим)
  char note[40] = "";    // «зараз · ще 9 г 50 хв» / «завтра · 2 год» / «3 год» (макет A, 27.11.2026)
};

// Смужка внизу основного екрана (27.11.2026): поточний проміжок на всю ширину, зліва початок, справа кінець,
// курсор — де ми зараз. Біла — світло є, червона — немає. Немає графіка чи він не діє — смужки немає.
struct Bar {
  bool show = false;
  bool light = false;
  char from[6] = "", to[6] = "";
  float frac = 0;  // 0..1 — скільки проміжку минуло
};

enum class MapKind : uint8_t { Rows, Emergency, NoData };

struct MapModel {
  tiles::Rgb frame = tiles::color::FRAME_CALM;
  char title[32] = "";  // «Черга 47.1»
  char clock[6] = "";
  MapKind kind = MapKind::NoData;
  Row rows[ROWS];
  int count = 0;
  Bar bar;  // поточний проміжок (rows[0]) для основного екрана
};

// staleAfter — скільки секунд графік ще годиться (далі — «Даних немає»).
MapModel buildMap(const Schedule* s, const char* group, int64_t now, tiles::Rgb frame, int staleAfter);

}  // namespace outage
