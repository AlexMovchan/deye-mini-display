#pragma once
// Київський час без libc-таймзон: однаково на платі й на ПК (тести).
// Правило як у TZ_INFO "EET-2EEST,M3.5.0/3,M10.5.0/4": літній час з останньої неділі березня
// 01:00 UTC до останньої неділі жовтня 01:00 UTC.
#include <stdint.h>

namespace kt {

struct Local {
  int year, month, day;  // month 1..12
  int hour, minute, second;
  int weekday;           // 0 = понеділок … 6 = неділя (як datetime.weekday() у Python)
  int minuteOfDay() const { return hour * 60 + minute; }
  int32_t dayNumber() const;  // дні від 1970-01-01 (для порівняння дат)
};

int64_t daysFromCivil(int y, int m, int d);
int utcOffsetSeconds(int64_t unixUtc);
Local toLocal(int64_t unixUtc);

// "2026-10-10T12:42:52.252Z", "2026-10-09T13:55:02Z", "2026-10-09T00:00:00+03:00" → unix.
// Повертає false, якщо рядок не схожий на ISO 8601.
bool parseIso(const char* s, int64_t* unixOut);

// "HH:MM" київського часу для ISO-рядка; buf ≥ 6 байтів. false — не вдалося розібрати.
bool isoToHhmm(const char* iso, char* buf);

}  // namespace kt
