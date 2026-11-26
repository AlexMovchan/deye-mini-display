// Автономне міні-табло Deye: NiceMCU-32S-DEV 2.8IPS. Див. docs/PLAN.md.
//   Режим налаштування — немає налаштувань або утримали BOOT 5 с: точка доступу Deye-Setup + сторінка для телефона.
//   Робочий режим — плитки: світло, заряд, споживання, батарея; тривога — якщо задано регіон.
#include <Arduino.h>
#include <StreamString.h>
#include <WebServer.h>
#include <esp_attr.h>
#include <esp_task_wdt.h>
#include <lvgl.h>

#include "board.h"
#include "config.h"
#include "event_log.h"
#include "kyiv_time.h"
#include "monitor.h"
#include "net.h"
#include "portal.h"
#include "settings.h"
#include "tiles.h"
#include "ui.h"

namespace {
// Прапорець «після перезапуску — у налаштування» переживає програмний перезапуск
constexpr uint32_t SETUP_MAGIC = 0x5E7AB00D;
RTC_NOINIT_ATTR uint32_t setupFlag;

bool setupMode = false;
bool panelOk = false;
WebServer logServer(LOG_HTTP_PORT);

void enterSetupAfterRestart() {
  setupFlag = SETUP_MAGIC;
  evlog::add("BOOT утримано — перезапуск у режим налаштування");
  delay(200);
  ESP.restart();
}

// BOOT (GPIO0) утримано SETUP_HOLD_MS → режим налаштування
void handleButton() {
  static uint32_t downSince = 0;
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    if (!downSince) downSince = millis();
    if (!setupMode && millis() - downSince > SETUP_HOLD_MS) enterSetupAfterRestart();
  } else {
    downSince = 0;
  }
}

void handleSerial() {
  static char line[32];
  static size_t n = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch != '\n' && ch != '\r') {
      if (n < sizeof line - 1) line[n++] = ch;
      continue;
    }
    line[n] = 0;
    n = 0;
    if (!strcmp(line, "log")) evlog::dump(Serial);
    else if (!strcmp(line, "setup")) enterSetupAfterRestart();
    else if (*line) Serial.println("Команди: log, setup");
  }
}

int brightness() {
  if (!net::timeValid()) return DAY_BRIGHTNESS;
  int h = kt::toLocal(net::now()).hour;
  bool night = NIGHT_START_HOUR <= NIGHT_END_HOUR ? (h >= NIGHT_START_HOUR && h < NIGHT_END_HOUR)
                                                  : (h >= NIGHT_START_HOUR || h < NIGHT_END_HOUR);
  return night ? NIGHT_BRIGHTNESS : DAY_BRIGHTNESS;
}

void startLogServer() {
  logServer.on("/log", HTTP_GET, [] {
    StreamString out;
    evlog::dump(out);
    logServer.sendHeader("Cache-Control", "no-store");
    logServer.send(200, "text/plain; charset=utf-8", static_cast<const String&>(out));
  });
  logServer.on("/", HTTP_GET, [] {
    logServer.send(200, "text/plain; charset=utf-8",
                   "Табло Deye " FW_VERSION ". Журнал: /log. Налаштування: утримайте BOOT 5 с.\n");
  });
  logServer.begin();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  evlog::add("Старт табло %s, перезапуск: %s (%d)", FW_VERSION, evlog::resetReasonText(), (int)esp_reset_reason());
  evlog::crashBegin();
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);

  // Сторожовий таймер головного циклу: зависання → перезапуск
  esp_task_wdt_config_t wdt = {30000, 0, true};
  if (esp_task_wdt_reconfigure(&wdt) != ESP_OK) esp_task_wdt_init(&wdt);
  esp_task_wdt_add(nullptr);

  panelOk = board::begin();
  if (panelOk) {
    ui::begin();
    lv_timer_handler();
    board::backlight(DAY_BRIGHTNESS);
  } else {
    evlog::add("Екран не запустився");
  }

  settings::Config cfg = settings::load();
  bool forced = setupFlag == SETUP_MAGIC;
  setupFlag = 0;
  setupMode = forced || !cfg.complete();
  if (setupMode) {
    evlog::add("Режим налаштування (%s)", forced ? "BOOT" : "налаштувань немає");
    portal::begin(cfg);
  } else {
    evlog::add("Логер %lu, тривоги: %s", (unsigned long)cfg.loggerSn,
               cfg.regionId.length() ? cfg.regionName.c_str() : "вимкнено");
    net::beginStation(cfg.ssid, cfg.pass);
    monitor::begin(cfg);
    startLogServer();
  }
}

void loop() {
  esp_task_wdt_reset();
  handleSerial();
  handleButton();
  static uint32_t lastUi = 0;
  if (setupMode) {
    portal::loop();
    if (panelOk && millis() - lastUi > 250) {
      lastUi = millis();
      ui::showSetup(portal::view());
    }
  } else {
    net::loop();
    logServer.handleClient();
    if (panelOk && millis() - lastUi > 1000) {
      lastUi = millis();
      ui::showTiles(tiles::build(monitor::snapshot()));
      static int lastBl = -1;
      int bl = brightness();
      if (bl != lastBl) {
        board::backlight(bl);
        lastBl = bl;
      }
    }
  }
  if (panelOk) lv_timer_handler();
  delay(5);
}
