#include "monitor.h"

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "event_log.h"
#include "https.h"
#include "inverter.h"
#include "logger.h"
#include "net.h"
#include "siren.h"

namespace monitor {

namespace {
settings::Config cfg;
SemaphoreHandle_t mtx;

// ── Спільний стан (під mtx) ──
struct Shared {
  bool hasData = false;
  uint32_t lastOkMs = 0;
  inv::Reading reading;
  inv::Grid grid = inv::Grid::Unknown;
  int64_t gridSince = 0;
  float hoursLeft = -1;
  bool searching = true;
  bool searchFailed = false;
  bool unsupported = false;
  uint16_t deviceType = 0;
  bool alertEver = false;  // була хоч одна відповідь siren.pp.ua
  uint32_t alertOkMs = 0;
  bool alertFailed = false;
  siren::Alert alert;
} S;

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

int64_t clockNow() { return net::timeValid() ? net::now() : 0; }

// ── Логер ──
IPAddress loggerIp;
int fails = 0;
uint32_t lastPoll = 0, lastSearch = 0;
bool needSearch = true;
inv::Tracker tracker(GRID_ON_VOLTS, GRID_CONFIRM_READS, LOAD_AVG_SECONDS);

void search() {
  {
    Lock l;
    S.searching = true;
  }
  evlog::add("Логер: шукаю (серійник %lu)", (unsigned long)cfg.loggerSn);
  logger::Found f = logger::discover(cfg.loggerSn, loggerIp, nullptr, nullptr);
  lastSearch = millis();
  Lock l;
  S.searching = false;
  if (!f.ok) {
    S.searchFailed = true;
    evlog::add("Логер: не знайдено, повторю за хвилину");
    return;
  }
  S.searchFailed = false;
  S.deviceType = f.deviceType;
  S.unsupported = !inv::supported(f.deviceType);
  if (S.unsupported) evlog::add("Інвертор: тип %d — не підтримується", f.deviceType);
  loggerIp = f.ip;
  needSearch = false;
  fails = 0;
  lastPoll = 0;  // одразу прочитати
  settings::saveLoggerIp((uint32_t)f.ip);
}

void poll() {
  lastPoll = millis();
  logger::ReadResult r = logger::read(loggerIp, cfg.loggerSn);
  if (!r.ok) {
    fails++;
    if (fails == 1 || fails % 10 == 0) evlog::add("Логер: %s (%d раз поспіль)", r.error, fails);
    if (fails >= LOGGER_FAILS_SEARCH) needSearch = true;
    return;
  }
  if (fails >= 1) evlog::add("Логер: знову відповідає");
  fails = 0;
  int64_t now = clockNow();
  inv::Grid before = tracker.shown();
  tracker.update(r.reading, now);
  if (before != inv::Grid::Unknown && tracker.shown() != before)
    evlog::add("Світло: %s", tracker.shown() == inv::Grid::On ? "є" : "зникло");
  Lock l;
  S.hasData = true;
  S.lastOkMs = millis();
  S.reading = r.reading;
  S.grid = tracker.shown();
  S.gridSince = tracker.since();
  S.hoursLeft = tracker.hoursLeft(r.reading);
}

// ── Тривоги ──
uint32_t lastSiren = 0;

void pollSiren() {
  lastSiren = millis();
  String url = String(SIREN_URL_PREFIX) + cfg.regionId;
  static char body[4096];
  size_t len = 0;
  https::Request req;
  req.url = url.c_str();
  https::Response res = https::get(req, [&](Stream& s) {
    len = s.readBytes(body, sizeof body - 1);
    body[len] = 0;
    return len > 0;
  });
  siren::Alert a;
  bool ok = res.status == 200 && res.bodyOk && siren::parse(body, len, &a) == siren::Parse::Ok;
  Lock l;
  if (!ok) {
    if (!S.alertFailed) evlog::add("Тривоги: siren.pp.ua не відповів (%d)", res.status);
    S.alertFailed = true;
    return;
  }
  if (a.active != S.alert.active || a.red != S.alert.red)
    evlog::add("Тривога: %s", a.active ? a.head : "немає");
  S.alertFailed = false;
  S.alertEver = true;
  S.alertOkMs = millis();
  S.alert = a;
}

void task(void*) {
  for (;;) {
    if (!net::online()) {
      delay(500);
      continue;
    }
    if (needSearch && (lastSearch == 0 || millis() - lastSearch > 60000UL)) search();
    if (!needSearch && !S.unsupported && (lastPoll == 0 || millis() - lastPoll >= (LOGGER_POLL_SECONDS - 5) * 1000UL)) {
      // З годинником — у свою секунду циклу (логер погано тримає кілька з'єднань одночасно); без нього — просто раз на 30 с
      int64_t now = clockNow();
      bool due;
      if (lastPoll == 0) {
        due = true;
      } else if (now) {
        int phase = (int)(now % LOGGER_POLL_SECONDS);
        due = phase >= LOGGER_POLL_OFFSET && phase < LOGGER_POLL_OFFSET + 5;
      } else {
        due = millis() - lastPoll >= LOGGER_POLL_SECONDS * 1000UL;
      }
      if (due) poll();
    }
    // Тривоги — лише з регіоном і після синхронізації часу (без годинника не порахувати «з HH:MM»)
    if (cfg.regionId.length() && net::timeValid() &&
        (lastSiren == 0 || millis() - lastSiren >= SIREN_POLL_SECONDS * 1000UL))
      pollSiren();
    delay(200);
  }
}
}  // namespace

void begin(const settings::Config& c) {
  cfg = c;
  mtx = xSemaphoreCreateMutex();
  loggerIp = IPAddress(cfg.loggerIp);
  // Збережена адреса — спершу пробуємо її (пошук почнеться, лише якщо логер на ній не відповість)
  needSearch = cfg.loggerIp == 0;
  if (!needSearch) S.searching = false;
  xTaskCreatePinnedToCore(task, "monitor", 12288, nullptr, 1, nullptr, 0);
}

tiles::Input snapshot() {
  tiles::Input in;
  in.now = clockNow();
  Lock l;
  if (S.unsupported) {
    in.logger = tiles::LoggerState::Unsupported;
    in.deviceType = S.deviceType;
  } else if (S.hasData) {
    uint32_t age = (millis() - S.lastOkMs) / 1000;
    in.logger = age > LOGGER_STALE_SECONDS ? tiles::LoggerState::Stale : tiles::LoggerState::Ok;
    in.staleMinutes = age / 60;
  } else if (S.searching || (fails == 0 && !S.searchFailed)) {
    in.logger = tiles::LoggerState::Searching;
  } else {
    in.logger = tiles::LoggerState::NoData;
  }
  in.reading = S.reading;
  in.grid = S.grid;
  in.gridSince = S.gridSince;
  in.hoursLeft = S.hoursLeft;
  if (!cfg.regionId.length()) {
    in.alertState = tiles::AlertState::Off;
  } else if (S.alertEver && millis() - S.alertOkMs < SIREN_STALE_SECONDS * 1000UL) {
    in.alertState = tiles::AlertState::Ok;
    in.alert = S.alert;
  } else if (S.alertFailed || (S.alertEver && millis() - S.alertOkMs >= SIREN_STALE_SECONDS * 1000UL)) {
    in.alertState = tiles::AlertState::NoData;
  } else {
    in.alertState = tiles::AlertState::Off;  // ще не питали — не лякаємо «немає даних»
  }
  return in;
}

}  // namespace monitor
