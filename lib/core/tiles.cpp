#include "tiles.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kyiv_time.h"

namespace tiles {

static void put(char* dst, int n, const char* s) { snprintf(dst, n, "%s", s); }

Rgb socColor(int soc) { return soc >= 75 ? color::SOC_GREEN : soc >= 45 ? color::SOC_YELLOW : color::SOC_RED; }

void formatHours(float hours, char* big, int bigLen, char* unit, int unitLen) {
  int mins = (int)lroundf(hours * 60);
  if (mins < 1) mins = 1;
  if (mins < 60) {
    snprintf(big, bigLen, "%d", mins);
    put(unit, unitLen, "хв");
  } else if (mins >= 24 * 60) {
    put(big, bigLen, "24");  // понад добу — точність не має сенсу
    put(unit, unitLen, "год+");
  } else if (mins >= 10 * 60) {
    snprintf(big, bigLen, "%d", (mins + 30) / 60);  // від 10 год — без хвилин
    put(unit, unitLen, "год");
  } else {
    snprintf(big, bigLen, "%d", mins / 60);
    int m = mins % 60;
    if (m) snprintf(unit, unitLen, "год %d хв", m);
    else put(unit, unitLen, "год");
  }
}

void formatWatts(int w, char* big, int bigLen, char* unit, int unitLen) {
  w = abs(w);
  if (w >= 10000) {
    snprintf(big, bigLen, "%d", (w + 500) / 1000);
    put(unit, unitLen, "кВт");
  } else {
    snprintf(big, bigLen, "%d", w);
    put(unit, unitLen, "Вт");
  }
}

static void hhmm(int64_t t, char* out, int n) {
  kt::Local l = kt::toLocal(t);
  snprintf(out, n, "%02d:%02d", l.hour, l.minute);
}

static void kwh(float v, char* out, int n) {
  // 5,6 кВт·год — кома, як прийнято в Україні
  int tenths = (int)lroundf(v * 10);
  snprintf(out, n, "%d,%d", tenths / 10, tenths % 10);
}

static void message(Model& m, const char* title, Rgb titleColor, const char* text) {
  m.message = true;
  put(m.msgTitle, sizeof m.msgTitle, title);
  m.msgTitleColor = titleColor;
  put(m.msgText, sizeof m.msgText, text);
}

Model build(const Input& in) {
  Model m;
  if (in.now > 0) hhmm(in.now, m.clock, sizeof m.clock);

  // Рядок і рамка тривоги
  switch (in.alertState) {
    case AlertState::Off:
      break;
    case AlertState::NoData:
      put(m.top, sizeof m.top, "Тривога: немає даних");
      m.topColor = color::DIM;
      break;
    case AlertState::Ok:
      if (in.alert.active) {
        if (*in.alert.since) snprintf(m.top, sizeof m.top, "%s · з %s", in.alert.head, in.alert.since);
        else put(m.top, sizeof m.top, in.alert.head);
        m.topColor = in.alert.red ? color::ALERT_RED : color::ALERT_YELLOW;
        m.frame = in.alert.red ? color::FRAME_RED : color::FRAME_YELLOW;
      } else {
        put(m.top, sizeof m.top, "Тривоги немає");
      }
      break;
  }

  // Без даних з логера — повідомлення замість плиток (дані, що вже були, лишаються як «застарілі»)
  if (in.logger == LoggerState::Unsupported) {
    char t[256];
    snprintf(t, sizeof t, "Тип: %s (%u). Табло працює лише з однофазними гібридними Deye (SG0xLP1).",
             inv::deviceTypeName(in.deviceType), (unsigned)in.deviceType);
    message(m, "Інвертор не підтримується", color::WARN, t);
    return m;
  }
  if (in.logger == LoggerState::Searching) {
    message(m, "Шукаю логер…", color::TEXT, "Логер Solarman у вашій мережі. Перший пошук — до хвилини.");
    return m;
  }
  if (in.logger == LoggerState::NoData) {
    message(m, "Логер не відповідає", color::WARN,
            "Перевірте, що логер увімкнений і в тій самій мережі Wi-Fi. Налаштування — утримуйте BOOT 5 с.");
    return m;
  }

  const bool stale = in.logger == LoggerState::Stale;
  const inv::Reading& r = in.reading;

  // ── Світло ──
  Tile& L = m.light;
  put(L.title, sizeof L.title, "Світло");
  if (stale || in.grid == inv::Grid::Unknown) {
    // Збій зв'язку ≠ відсутність світла
    put(L.text, sizeof L.text, "Невідомо");
    L.valueColor = color::MUTED;
    put(L.sub, sizeof L.sub, stale ? "логер мовчить" : "");
    L.subColor = color::WARN;
  } else if (in.grid == inv::Grid::On) {
    L.bg = color::CARD_LIGHT;
    L.titleColor = color::TEXT_DARK2;
    L.icon = Icon::BoltOn;
    put(L.text, sizeof L.text, "Є");
    L.valueColor = color::TEXT_DARK;
    L.subColor = color::TEXT_DARK2;
    int v = (int)lroundf(r.gridV);
    if (in.gridSince > 0) {
      char t[6];
      hhmm(in.gridSince, t, sizeof t);
      snprintf(L.sub, sizeof L.sub, "з %s · %d В", t, v);
    } else {
      snprintf(L.sub, sizeof L.sub, "%d В", v);
    }
  } else {
    L.bg = color::CARD_OFF;
    L.border = color::BORDER;
    L.icon = Icon::BoltOff;
    put(L.text, sizeof L.text, "Немає");
    if (in.gridSince > 0) {
      char t[6];
      hhmm(in.gridSince, t, sizeof t);
      snprintf(L.sub, sizeof L.sub, "з %s", t);
    }
  }

  // ── Заряд ──
  Tile& S = m.soc;
  put(S.title, sizeof S.title, "Заряд");
  snprintf(S.big, sizeof S.big, "%d", r.soc);
  put(S.unit, sizeof S.unit, "%");
  S.bar = true;
  S.barFrac = r.soc / 100.0f;
  S.barColor = stale ? color::STALE : socColor(r.soc);
  if (stale) {
    S.valueColor = color::STALE;
    snprintf(S.sub, sizeof S.sub, "дані %d хв тому", in.staleMinutes);
    S.subColor = color::WARN;
  }

  // ── Споживання ──
  Tile& C = m.load;
  put(C.title, sizeof C.title, "Споживання");
  formatWatts(r.loadW, C.big, sizeof C.big, C.unit, sizeof C.unit);
  char day[12];
  kwh(r.dayKwh, day, sizeof day);
  snprintf(C.sub, sizeof C.sub, "за добу %s кВт·год", day);
  if (stale) C.valueColor = color::STALE;

  // ── Батарея / вистачить ──
  Tile& B = m.batt;
  if (!stale && in.grid == inv::Grid::Off) {
    put(B.title, sizeof B.title, "Вистачить");
    if (in.hoursLeft < 0) {
      put(B.text, sizeof B.text, "надовго");
      B.valueColor = color::ALERT_YELLOW;
    } else {
      formatHours(in.hoursLeft, B.big, sizeof B.big, B.unit, sizeof B.unit);
      B.valueColor = in.hoursLeft < 2 ? color::SOC_RED : color::ALERT_YELLOW;
      B.unitColor = B.valueColor;
    }
    if (r.battW > 20) {
      snprintf(B.sub, sizeof B.sub, "розряд %d Вт", r.battW);
      B.subColor = color::MINUS;
    }
  } else {
    put(B.title, sizeof B.title, "Батарея");
    formatWatts(r.battW, B.big, sizeof B.big, B.unit, sizeof B.unit);
    if (r.battW < -20) {
      B.valueColor = B.unitColor = color::PLUS;
      put(B.sub, sizeof B.sub, "заряджається");
    } else if (r.battW > 20) {
      B.valueColor = B.unitColor = color::MINUS;
      put(B.sub, sizeof B.sub, "розряд");  // «розряджається» не влазить у плитку
    } else {
      put(B.big, sizeof B.big, "0");
      B.valueColor = color::MUTED;
      put(B.sub, sizeof B.sub, "у спокої");
    }
    if (stale) B.valueColor = B.unitColor = color::STALE;
  }
  return m;
}

}  // namespace tiles
