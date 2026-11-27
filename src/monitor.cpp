#include "monitor.h"

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "event_log.h"
#include "https.h"
#include "inverter.h"
#include "logger.h"
#include "net.h"
#include "siren.h"
#include "yasno_address.h"

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
  outage::Schedule sched;  // останній вдалий графік
  bool outFailed = false;  // останній запит графіка не вдався
  String outGroup;
} S;

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

int64_t clockNow() { return net::timeValid() ? net::now() : 0; }

// ── Сторож зв'язку: Wi-Fi буває «підключений», а дані не ходять (табло годинами шукає логер) ──
uint32_t netOkMs = 0, lastProbe = 0;
int deadProbes = 0, reconnects = 0;

void netOk() {
  netOkMs = millis();
  deadProbes = 0;
  reconnects = 0;
}

void watchdog() {
  if (millis() - netOkMs < NET_PROBE_SECONDS * 1000UL || millis() - lastProbe < 30000UL) return;
  lastProbe = millis();
  if (net::gatewayAlive()) {
    // Роутер відповідає — мережа жива, проблема в логері чи інтернеті; не чіпаємо
    netOkMs = millis();
    deadProbes = 0;
    return;
  }
  if (++deadProbes < 2) return;
  deadProbes = 0;
  if (++reconnects > NET_RESTART_RECONNECTS) {
    evlog::add("Wi-Fi: %d перепідключення не допомогли — перезапуск", NET_RESTART_RECONNECTS);
    delay(300);
    ESP.restart();
  }
  evlog::add("Wi-Fi: роутер не відповідає на ping (спроба %d)", reconnects);
  net::reconnect();
  netOkMs = millis();  // дати час на підключення
}

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
  netOk();
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
    // Поодинокі збої — логер саме відповідав іншому клієнту (РК, застосунок). Ще раз за 2 с, а тоді вже збій.
    delay(2000);
    r = logger::read(loggerIp, cfg.loggerSn);
  }
  if (!r.ok) {
    fails++;
    if (fails == 1 || fails % 10 == 0) evlog::add("Логер: %s (%d раз поспіль)", r.error, fails);
    if (fails >= LOGGER_FAILS_SEARCH) needSearch = true;
    return;
  }
  if (fails >= 1) evlog::add("Логер: знову відповідає");
  fails = 0;
  netOk();
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

// ── Графік відключень ──
uint32_t lastOutage = 0, outageInterval = 0, lastRecheck = 0;

void pollOutage() {
  lastOutage = millis();
  String group;
  {
    Lock l;
    group = S.outGroup;
  }
  String url = String(OUTAGE_URL_PREFIX) + cfg.outProvider + "/" + group + ".json";
  static char body[2048];
  size_t len = 0;
  https::Request req;
  req.url = url.c_str();
  req.timeoutMs = SIREN_TIMEOUT_MS;
  https::Response res = https::get(req, [&](Stream& s) {
    len = s.readBytes(body, sizeof body - 1);
    body[len] = 0;
    return len > 0;
  });
  outage::Schedule sch;
  bool ok = res.status == 200 && res.bodyOk && outage::parse(body, len, &sch);
  static bool failed = false;
  if (!ok) {
    if (!failed) evlog::add("Графік: не отримано (%d), повтор за %d хв", res.status, OUTAGE_RETRY_MINUTES);
    failed = true;
    outageInterval = OUTAGE_RETRY_MINUTES * 60000UL;
    Lock l;
    S.outFailed = true;
    return;
  }
  if (failed) evlog::add("Графік: знову отримано");
  failed = false;
  outageInterval = OUTAGE_POLL_MINUTES * 60000UL;
  netOk();
  Lock l;
  S.sched = sch;
  S.outFailed = false;
}

// Київ: раз на добу — чи не перенесли будинок в іншу чергу
void recheckKyiv() {
  lastRecheck = millis();
  String err;
  String g = yasno::group(cfg.yStreet, cfg.yHouse, &err);
  if (!g.length()) {
    evlog::add("Графік: перевірка черги не вдалася (%s)", err.c_str());
    return;
  }
  bool changed;
  {
    Lock l;
    changed = g != S.outGroup;
    if (changed) evlog::add("Графік: черга змінилась %s → %s", S.outGroup.c_str(), g.c_str());
    S.outGroup = g;
  }
  if (changed) {
    settings::saveOutGroup(g);
    lastOutage = 0;  // одразу забрати графік нової черги
  }
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
  req.timeoutMs = SIREN_TIMEOUT_MS;
  https::Response res = https::get(req, [&](Stream& s) {
    len = s.readBytes(body, sizeof body - 1);
    body[len] = 0;
    return len > 0;
  });
  siren::Alert a;
  bool ok = res.status == 200 && res.bodyOk && siren::parse(body, len, &a) == siren::Parse::Ok;
  Lock l;
  static int sirenFails = 0;
  if (!ok) {
    // Пам'ять у журнал: без PSRAM TLS потребує ~40 КБ суцільним блоком
    if (sirenFails++ % 10 == 0)
      evlog::add("Тривоги: siren.pp.ua не відповів (%d), вільно %u КБ, блок %u КБ; %s", res.status,
                 (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024),
                 res.error);
    S.alertFailed = true;
    return;
  }
  netOk();
  if (sirenFails) evlog::add("Тривоги: siren.pp.ua знову відповідає");
  sirenFails = 0;
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
    watchdog();
    int64_t now = clockNow();
    int phase = now ? (int)(now % LOGGER_POLL_SECONDS) : -1;
    // Логер — першим. Своя половина циклу — з :15 (інші клієнти логера зазвичай на :00/:30); пропустили її
    // (щось затрималось) — опитуємо все одно, щоб між зчитуваннями не було більше ~45 с.
    if (!needSearch && !S.unsupported) {
      uint32_t since = millis() - lastPoll;
      bool due = lastPoll == 0 || since >= (LOGGER_POLL_SECONDS + 15) * 1000UL ||
                 (since >= (LOGGER_POLL_SECONDS - 5) * 1000UL && (phase < 0 || phase >= LOGGER_POLL_OFFSET));
      if (due) poll();
    }
    // Тривоги — лише з регіоном і після синхронізації часу; і лише в :20–:27 кожного півхвилинного циклу,
    // щоб навіть зависле з'єднання (до SIREN_TIMEOUT_MS) не налізло на час логера
    // Одне HTTPS-з'єднання за прохід (пам'яті без PSRAM на два немає): тривоги → графік → перевірка черги
    bool httpsWindow = net::timeValid() && phase >= LOGGER_POLL_OFFSET + 5 && phase < LOGGER_POLL_OFFSET + 12;
    if (httpsWindow && cfg.regionId.length() && (lastSiren == 0 || millis() - lastSiren >= SIREN_POLL_SECONDS * 1000UL))
      pollSiren();
    else if (httpsWindow && cfg.outProvider.length() && (lastOutage == 0 || millis() - lastOutage >= outageInterval))
      pollOutage();
    else if (httpsWindow && cfg.outProvider == "yasno-kyiv" && cfg.yHouse && millis() > 120000UL &&
             (lastRecheck == 0 || millis() - lastRecheck >= KYIV_RECHECK_HOURS * 3600000UL))
      recheckKyiv();
    delay(200);
  }
}
}  // namespace

void begin(const settings::Config& c) {
  cfg = c;
  mtx = xSemaphoreCreateMutex();
  S.outGroup = cfg.outGroup;
  outageInterval = OUTAGE_POLL_MINUTES * 60000UL;
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
  // Значок Wi-Fi: «дані не ходять», якщо понад 2 хв жодного вдалого обміну (на старті — 2 хв на підключення)
  bool flowing = netOkMs ? millis() - netOkMs < 120000UL : millis() < 120000UL;
  in.wifi = tiles::wifiState(net::online(), flowing, net::rssi());
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

bool outageSnapshot(outage::Schedule* out, String* group, outage::Fetch* fetch) {
  if (!cfg.outProvider.length()) return false;
  Lock l;
  *out = S.sched;
  *group = S.outGroup;
  if (fetch) *fetch = S.outFailed ? outage::Fetch::Failed : S.sched.valid ? outage::Fetch::Ok : outage::Fetch::Pending;
  return true;
}

}  // namespace monitor
