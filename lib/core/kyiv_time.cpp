#include "kyiv_time.h"

#include <ctype.h>
#include <stdio.h>

namespace kt {

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

static void civilFromDays(int64_t z, int* y, int* m, int* d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *m = mp < 10 ? mp + 3 : mp - 9;
  *y = (int)(yoe + era * 400) + (*m <= 2);
}

// 0 = понеділок
static int weekdayOf(int64_t days) { return (int)(((days % 7) + 7 + 3) % 7); }

static int64_t lastSundayUtc1(int year, int month) {
  int64_t last = daysFromCivil(year, month, 31);
  last -= (weekdayOf(last) + 1) % 7;  // назад до неділі
  return last * 86400 + 3600;
}

int utcOffsetSeconds(int64_t t) {
  int y, m, d;
  civilFromDays(t >= 0 ? t / 86400 : (t - 86399) / 86400, &y, &m, &d);
  bool summer = t >= lastSundayUtc1(y, 3) && t < lastSundayUtc1(y, 10);
  return summer ? 3 * 3600 : 2 * 3600;
}

int32_t Local::dayNumber() const { return (int32_t)daysFromCivil(year, month, day); }

Local toLocal(int64_t t) {
  int64_t lt = t + utcOffsetSeconds(t);
  int64_t days = lt >= 0 ? lt / 86400 : (lt - 86399) / 86400;
  int64_t sec = lt - days * 86400;
  Local l;
  civilFromDays(days, &l.year, &l.month, &l.day);
  l.hour = (int)(sec / 3600);
  l.minute = (int)(sec / 60 % 60);
  l.second = (int)(sec % 60);
  l.weekday = weekdayOf(days);
  return l;
}

static bool digits(const char*& p, int n, int* out) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (!isdigit((unsigned char)*p)) return false;
    v = v * 10 + (*p++ - '0');
  }
  *out = v;
  return true;
}

bool parseIso(const char* s, int64_t* out) {
  if (!s) return false;
  const char* p = s;
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (!digits(p, 4, &y) || *p++ != '-' || !digits(p, 2, &mo) || *p++ != '-' || !digits(p, 2, &d)) return false;
  if (*p == 'T' || *p == ' ') {
    p++;
    if (!digits(p, 2, &h) || *p++ != ':' || !digits(p, 2, &mi)) return false;
    if (*p == ':') { p++; if (!digits(p, 2, &se)) return false; }
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
  }
  int offset = 0;  // без позначки пояса вважаємо UTC (так роблять API)
  if (*p == '+' || *p == '-') {
    int sign = *p++ == '-' ? -1 : 1, oh, om = 0;
    if (!digits(p, 2, &oh)) return false;
    if (*p == ':') p++;
    digits(p, 2, &om);
    offset = sign * (oh * 3600 + om * 60);
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 60) return false;
  *out = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se - offset;
  return true;
}

bool isoToHhmm(const char* iso, char* buf) {
  int64_t t;
  if (!parseIso(iso, &t)) return false;
  Local l = toLocal(t);
  snprintf(buf, 6, "%02d:%02d", l.hour, l.minute);
  return true;
}

}  // namespace kt
