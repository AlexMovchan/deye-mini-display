#include "outage.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "kyiv_time.h"

namespace outage {

namespace {
const Day* findDay(const Schedule* s, int32_t dn) {
  if (!s) return nullptr;
  for (int i = 0; i < s->dayCount; i++)
    if (s->days[i].dayNumber == dn) return &s->days[i];
  return nullptr;
}

bool parseDate(const char* s, int32_t* day) {
  int y, m, d;
  if (!s || sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return false;
  *day = (int32_t)kt::daysFromCivil(y, m, d);
  return true;
}

void hhmm(char* out, size_t n, int slot) {  // слот 0..96 → "HH:MM" (96 і 48 як 24:00 / 00:00 наступної доби)
  int m = (slot % 48) * 30;
  if (slot == 48 || slot == 96) m = 24 * 60;
  snprintf(out, n, "%02d:%02d", m / 60, m % 60);
}
// Тривалість: «2 год», «1 г 30 хв», «30 хв»
void duration(char* out, size_t n, int minutes) {
  int h = minutes / 60, m = minutes % 60;
  if (h && m) snprintf(out, n, "%d г %d хв", h, m);
  else if (h) snprintf(out, n, "%d год", h);
  else snprintf(out, n, "%d хв", m);
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
  if (now <= 0) return m;  // без годинника — «Даних немає»
  kt::Local l = kt::toLocal(now);
  snprintf(m.clock, sizeof m.clock, "%02d:%02d", l.hour, l.minute);
  if (!s || !s->valid || (s->fetched > 0 && now - s->fetched > staleAfter)) return m;
  const Day* today = findDay(s, l.dayNumber());
  if (today && today->status == Status::Emergency) {
    m.kind = MapKind::Emergency;
    return m;
  }
  if (!today || today->status != Status::Applies) return m;
  // 96 півгодинок: сьогодні й завтра; 2 — невідомо (завтра ще не опубліковане)
  uint8_t t[96];
  const Day* tomorrow = findDay(s, l.dayNumber() + 1);
  for (int i = 0; i < 48; i++) {
    t[i] = today->off[i];
    t[48 + i] = tomorrow && tomorrow->status == Status::Applies ? tomorrow->off[i] : 2;
  }
  int cur = l.hour * 2 + l.minute / 30;
  int start = cur;
  while (start > 0 && t[start - 1] == t[cur]) start--;  // поточний проміжок — від його початку
  while (start < 96 && t[start] != 2 && m.count < ROWS) {
    int end = start;
    while (end < 96 && t[end] == t[start]) end++;
    Row& r = m.rows[m.count++];
    char a[6], b_[6];
    hhmm(a, sizeof a, start);
    hhmm(b_, sizeof b_, end);
    snprintf(r.text, sizeof r.text, "%s–%s", a, b_);
    r.light = t[start] == 0;
    r.tomorrow = start >= 48;
    r.current = start <= cur;
    // Кінець відомий, лише якщо далі йде інший стан (а не «завтра ще не опубліковане» чи кінець даних)
    bool known = end < 96 && t[end] != 2;
    char d[24] = "";
    if (known) duration(d, sizeof d, r.current ? end * 30 - (l.hour * 60 + l.minute) : (end - start) * 30);
    const char* sep = *d ? " · " : "";
    if (r.current) snprintf(r.note, sizeof r.note, "зараз%s%s", *d ? " · ще " : "", d);
    else if (r.tomorrow) snprintf(r.note, sizeof r.note, "завтра%s%s", sep, d);
    else snprintf(r.note, sizeof r.note, "%s", d);
    if (r.current) {
      Bar& b = m.bar;
      b.show = true;
      b.light = r.light;
      snprintf(b.from, sizeof b.from, "%s", a);
      snprintf(b.to, sizeof b.to, "%s", b_);
      int nowMin = l.hour * 60 + l.minute;
      b.frac = (float)(nowMin - start * 30) / ((end - start) * 30);
    }
    start = end;
  }
  m.kind = MapKind::Rows;
  return m;
}

Bar statusBar(const MapModel& m, Fetch f) {
  if (m.bar.show) return m.bar;
  Bar b;
  b.show = b.message = true;
  b.light = true;  // нейтральна (не червона), крім екстрених
  const char* t = "Даних немає";
  if (m.kind == MapKind::Emergency) {
    t = "Екстрені відключення";
    b.light = false;
  } else if (f == Fetch::Pending) {
    t = "Завантаження графіка…";
  } else if (f == Fetch::Failed) {
    t = "Помилка запиту графіка";
  }
  snprintf(b.text, sizeof b.text, "%s", t);
  return b;
}

}  // namespace outage
