#include "outage.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "kyiv_time.h"

namespace outage {

namespace {
const char* const WEEKDAYS[7] = {"пн", "вт", "ср", "чт", "пт", "сб", "нд"};
const char* const MONTHS[12] = {"січня", "лютого", "березня", "квітня", "травня", "червня",
                                "липня", "серпня", "вересня", "жовтня", "листопада", "грудня"};

bool parseDate(const char* s, int32_t* day) {
  int y, m, d;
  if (!s || sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return false;
  *day = (int32_t)kt::daysFromCivil(y, m, d);
  return true;
}

const Day* findDay(const Schedule* s, int32_t dn) {
  if (!s) return nullptr;
  for (int i = 0; i < s->dayCount; i++)
    if (s->days[i].dayNumber == dn) return &s->days[i];
  return nullptr;
}

void dayTitle(char* out, size_t n, const char* prefix, int64_t t) {
  kt::Local l = kt::toLocal(t);
  snprintf(out, n, "%s · %s %d %s", prefix, WEEKDAYS[l.weekday], l.day, MONTHS[l.month - 1]);
}

void fillDay(DayView& v, const Day* d, int nowHour) {
  if (!d || d->status == Status::Waiting || d->status == Status::None) {
    snprintf(v.note, sizeof v.note, "ще не опубліковано");
    return;
  }
  if (d->status == Status::Emergency) {
    snprintf(v.note, sizeof v.note, "екстрені відключення");
    return;
  }
  v.cells = true;
  for (int h = 0; h < 24; h++) {
    Cell& c = v.c[h];
    snprintf(c.hour, sizeof c.hour, "%02d:00", h);
    c.first = !d->off[2 * h];
    c.second = !d->off[2 * h + 1];
    c.now = h == nowHour;
  }
}
}  // namespace

bool validGroup(const char* g) {
  if (!g) return false;
  int digits = 0, dots = 0, after = 0;
  for (const char* p = g; *p; p++) {
    if (*p >= '0' && *p <= '9') {
      digits++;
      if (dots) after++;
    } else if (*p == '.' && digits && !dots) {
      dots = 1;
    } else {
      return false;
    }
  }
  return dots == 1 && after == 1 && digits <= 3;
}

bool parse(const char* body, size_t len, Schedule* out) {
  JsonDocument doc;
  if (deserializeJson(doc, body, len)) return false;
  if ((doc["v"] | 0) != 1) return false;
  Schedule s;
  snprintf(s.group, sizeof s.group, "%s", doc["group"] | "");
  if (!kt::parseIso(doc["fetched"] | "", &s.fetched)) s.fetched = 0;
  for (JsonObjectConst d : doc["days"].as<JsonArrayConst>()) {
    if (s.dayCount >= 3) break;
    Day& day = s.days[s.dayCount];
    if (!parseDate(d["date"] | "", &day.dayNumber)) return false;
    const char* st = d["status"] | "";
    day.status = !strcmp(st, "applies") ? Status::Applies
                 : !strcmp(st, "waiting") ? Status::Waiting
                 : !strcmp(st, "emergency") ? Status::Emergency
                                            : Status::None;
    if (day.status == Status::Applies) {
      const char* off = d["off"] | "";
      if (strlen(off) != SLOTS) return false;
      for (int i = 0; i < SLOTS; i++) {
        if (off[i] != '0' && off[i] != '1' && off[i] != '?') return false;
        day.off[i] = off[i] != '0';
      }
    }
    s.dayCount++;
  }
  if (!s.dayCount) return false;
  s.valid = true;
  *out = s;
  return true;
}

MapModel buildMap(const Schedule* s, const char* group, int64_t now, tiles::Rgb frame, int staleAfter) {
  MapModel m;
  m.frame = frame;
  snprintf(m.title, sizeof m.title, "Черга %s", group && *group ? group : "—");
  if (now <= 0) {
    snprintf(m.updated, sizeof m.updated, "чекаю час…");
    snprintf(m.today.note, sizeof m.today.note, "немає годинника");
    snprintf(m.tomorrow.note, sizeof m.tomorrow.note, "немає годинника");
    return m;
  }
  kt::Local l = kt::toLocal(now);
  snprintf(m.clock, sizeof m.clock, "%02d:%02d", l.hour, l.minute);
  int32_t today = l.dayNumber();
  dayTitle(m.today.title, sizeof m.today.title, "Сьогодні", now);
  dayTitle(m.tomorrow.title, sizeof m.tomorrow.title, "Завтра", now + 86400);
  if (!s || !s->valid) {
    snprintf(m.updated, sizeof m.updated, "графік недоступний");
    m.updatedColor = tiles::color::WARN;
    snprintf(m.today.note, sizeof m.today.note, "немає даних");
    snprintf(m.tomorrow.note, sizeof m.tomorrow.note, "немає даних");
    return m;
  }
  if (s->fetched > 0) {
    kt::Local f = kt::toLocal(s->fetched);
    bool stale = now - s->fetched > staleAfter;
    snprintf(m.updated, sizeof m.updated, "графік на %02d:%02d%s", f.hour, f.minute, stale ? " (давно)" : "");
    m.updatedColor = stale ? tiles::color::WARN : tiles::color::DIM;
  }
  fillDay(m.today, findDay(s, today), l.hour);
  fillDay(m.tomorrow, findDay(s, today + 1), -1);
  return m;
}

}  // namespace outage
