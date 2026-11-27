#include "portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "discovery.h"
#include "event_log.h"
#include "inverter.h"
#include "logger.h"
#include "net.h"
#include "outage.h"
#include "siren.h"
#include "web_assets.h"
#include "yasno_address.h"

namespace portal {

namespace {
WebServer server(80);
DNSServer dns;
settings::Config cfg;  // те, що буде збережено

// ── Wi-Fi ──
enum class WifiState : uint8_t { Idle, Connecting, Ok, Fail };
WifiState wifiState = WifiState::Idle;
uint32_t wifiStarted = 0;
bool scanning = false;
struct Net {
  String ssid;
  int rssi;
  bool lock;
};
std::vector<Net> networks;

// ── Логер (пошук — у окремій задачі, щоб сторінка відповідала) ──
enum class LogState : uint8_t { Idle, Searching, Found, NotFound, Unsupported };
volatile LogState logState = LogState::Idle;
volatile int logDone = 0, logTotal = 0;
volatile bool logCancel = false;
IPAddress logIp;
uint32_t logSn = 0;
int logType = -1, logSoc = -1;
TaskHandle_t logTask = nullptr;

uint32_t restartAt = 0;

const char* wifiStateName(WifiState s) {
  switch (s) {
    case WifiState::Connecting: return "connecting";
    case WifiState::Ok: return "ok";
    case WifiState::Fail: return "fail";
    default: return "idle";
  }
}
const char* logStateName(LogState s) {
  switch (s) {
    case LogState::Searching: return "searching";
    case LogState::Found: return "found";
    case LogState::NotFound: return "notfound";
    case LogState::Unsupported: return "unsupported";
    default: return "idle";
  }
}

void collectScan(int n) {
  networks.clear();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || s == SETUP_AP_SSID) continue;
    bool dup = false;
    for (auto& e : networks)
      if (e.ssid == s) {
        dup = true;
        if (WiFi.RSSI(i) > e.rssi) e.rssi = WiFi.RSSI(i);
      }
    if (!dup) networks.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  std::sort(networks.begin(), networks.end(), [](const Net& a, const Net& b) { return a.rssi > b.rssi; });
  if (networks.size() > 20) networks.resize(20);
  WiFi.scanDelete();
}

void searchTask(void*) {
  uint32_t sn = cfg.loggerSn;
  logger::Found f = logger::discover(
      sn, IPAddress(cfg.loggerIp),
      [](int done, int total) {
        logDone = done;
        logTotal = total;
      },
      &logCancel);
  if (f.ok) {
    logIp = f.ip;
    logSn = f.sn;
    logType = f.deviceType;
    if (inv::supported(f.deviceType)) {
      logger::ReadResult r = logger::read(f.ip, f.sn);
      logSoc = r.ok ? r.reading.soc : -1;
      logState = LogState::Found;
      cfg.loggerSn = f.sn;
      cfg.loggerIp = (uint32_t)f.ip;
    } else {
      logState = LogState::Unsupported;
    }
  } else {
    logState = LogState::NotFound;
  }
  evlog::add("Налаштування: логер — %s", logStateName(logState));
  logTask = nullptr;
  vTaskDelete(nullptr);
}

void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}

void ok() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", "ok");
}

void handleState() {
  JsonDocument d;
  d["fw"] = FW_VERSION;
  d["scanning"] = scanning;
  JsonArray nets = d["networks"].to<JsonArray>();
  for (auto& n : networks) {
    JsonObject o = nets.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["rssi"] = n.rssi;
    o["lock"] = n.lock;
  }
  JsonObject w = d["wifi"].to<JsonObject>();
  w["state"] = wifiStateName(wifiState);
  w["ssid"] = cfg.ssid;
  if (wifiState == WifiState::Ok) w["ip"] = WiFi.localIP().toString();
  JsonObject l = d["logger"].to<JsonObject>();
  l["state"] = logStateName(logState);
  l["done"] = (int)logDone;
  l["total"] = (int)logTotal;
  if (cfg.loggerSn) l["sn"] = String(cfg.loggerSn);
  l["known"] = cfg.loggerIp != 0;  // логер уже знайдено раніше
  if (logState == LogState::Idle && cfg.loggerIp) l["ip"] = IPAddress(cfg.loggerIp).toString();
  if (logState == LogState::Found || logState == LogState::Unsupported) {
    l["ip"] = logIp.toString();
    l["type"] = logType;
    l["typeName"] = inv::deviceTypeName(logType);
    if (logSoc >= 0) l["soc"] = logSoc;
  }
  JsonObject r = d["region"].to<JsonObject>();
  r["id"] = cfg.regionId;
  r["name"] = cfg.regionName;
  JsonObject o = d["outage"].to<JsonObject>();
  o["provider"] = cfg.outProvider;
  o["group"] = cfg.outGroup;
  o["address"] = cfg.outAddress;
  sendJson(d);
}

void handleScan() {
  if (!scanning) {
    WiFi.scanNetworks(true);
    scanning = true;
  }
  ok();
}

void handleWifi() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) {
    server.send(400, "text/plain", "bad");
    return;
  }
  cfg.ssid = ssid;
  cfg.pass = pass;
  wifiState = WifiState::Connecting;
  wifiStarted = millis();
  net::connect(ssid, pass);
  evlog::add("Налаштування: під'єднуюсь до «%s»", ssid.c_str());
  ok();
}

void startSearch() {
  logState = LogState::Searching;
  logDone = logTotal = 0;
  logCancel = false;
  logType = logSoc = -1;
  xTaskCreate(searchTask, "search", 8192, nullptr, 1, &logTask);
}

void handleLogger() {
  if (wifiState != WifiState::Ok) {
    server.send(409, "text/plain", "wifi");
    return;
  }
  if (logState == LogState::Searching) {
    ok();
    return;
  }
  String sn = server.arg("sn");
  uint32_t v = 0;
  if (sn.length() && !disc::parseSerial(sn.c_str(), &v)) {
    server.send(400, "text/plain", "sn");
    return;
  }
  if (v != cfg.loggerSn) cfg.loggerIp = 0;  // інший логер — стара адреса не підходить
  cfg.loggerSn = v;
  startSearch();
  ok();
}

void handleRegion() {
  String id = server.arg("id"), name = server.arg("name");
  if (id.length() && (!siren::validRegionId(id.c_str()) || name.length() > 120)) {
    server.send(400, "text/plain", "bad");
    return;
  }
  cfg.regionId = id;
  cfg.regionName = id.length() ? name : "";
  ok();
}

// ── Графік відключень ──
// Пошук адреси в Києві робить табло (телефон у мережі Deye-Setup без інтернету): вулиці → будинки → черга.
void sendItems(const std::vector<yasno::Item>& items, const String& error) {
  JsonDocument d;
  JsonArray a = d["items"].to<JsonArray>();
  for (auto& it : items) {
    JsonObject o = a.add<JsonObject>();
    o["id"] = it.id;
    o["name"] = it.value;
  }
  if (error.length()) d["error"] = error;
  sendJson(d);
}

bool needInternet() {
  if (wifiState == WifiState::Ok) return true;
  server.send(409, "text/plain", "wifi");
  return false;
}

void handleStreets() {
  if (!needInternet()) return;
  String err;
  auto items = yasno::streets(server.arg("q"), 20, &err);
  sendItems(items, err);
}

void handleHouses() {
  if (!needInternet()) return;
  String err;
  auto items = yasno::houses(strtoul(server.arg("street").c_str(), nullptr, 10), server.arg("q"), 40, &err);
  sendItems(items, err);
}

void handleGroup() {
  if (!needInternet()) return;
  String err;
  String g = yasno::group(strtoul(server.arg("street").c_str(), nullptr, 10),
                          strtoul(server.arg("house").c_str(), nullptr, 10), &err);
  JsonDocument d;
  d["group"] = g;
  if (err.length()) d["error"] = err;
  sendJson(d);
}

void handleOutage() {
  String prov = server.arg("provider"), group = server.arg("group");
  if (!prov.length()) {
    cfg.outProvider = cfg.outGroup = cfg.outAddress = "";
    cfg.yStreet = cfg.yHouse = 0;
    ok();
    return;
  }
  if ((prov != "yasno-kyiv" && prov != "ztoe") || !outage::validGroup(group.c_str()) ||
      server.arg("address").length() > 120) {
    server.send(400, "text/plain", "bad");
    return;
  }
  cfg.outProvider = prov;
  cfg.outGroup = group;
  cfg.yStreet = prov == "yasno-kyiv" ? strtoul(server.arg("street").c_str(), nullptr, 10) : 0;
  cfg.yHouse = prov == "yasno-kyiv" ? strtoul(server.arg("house").c_str(), nullptr, 10) : 0;
  cfg.outAddress = prov == "yasno-kyiv" ? server.arg("address") : "";
  ok();
}

void handleSave() {
  // Логер має бути знайдений зараз або раніше (вхід через BOOT без зміни логера)
  bool loggerOk = logState == LogState::Found || (logState == LogState::Idle && cfg.loggerIp);
  if (!cfg.complete() || !loggerOk) {
    server.send(409, "text/plain", "incomplete");
    return;
  }
  settings::save(cfg);
  evlog::add("Налаштування збережено, перезапуск");
  restartAt = millis() + 1500;
  ok();
}

bool erased = false;

void handleReset() {
  settings::erase();
  cfg = settings::Config();  // і в пам'яті — щоб «Зберегти» до перезапуску не записало старе назад
  erased = true;
  evlog::add("Налаштування стерто, перезапуск");
  restartAt = millis() + 1500;
  ok();
}

void sendAsset(const uint8_t* data, size_t len, const char* type) {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, type, (const char*)data, len);
}

// Будь-яка інша адреса (перевірки captive portal Android/iOS/Windows) → сторінка налаштування
void redirect() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}
}  // namespace

void begin(const settings::Config& current) {
  cfg = current;
  // Канал точки доступу — як у найсильнішої мережі поруч (найімовірніше, домашньої): тоді підключення табло до
  // неї не змусить точку доступу перескочити на інший канал і телефон не відвалиться.
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  int channel = 1, best = -1000;
  for (int i = 0; i < n; i++) {
    if (current.ssid.length() && WiFi.SSID(i) == current.ssid) {
      channel = WiFi.channel(i);
      best = 1000;
    } else if (WiFi.RSSI(i) > best) {
      best = WiFi.RSSI(i);
      channel = WiFi.channel(i);
    }
  }
  collectScan(n < 0 ? 0 : n);
  net::beginSetup(channel);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [] { sendAsset(SETUP_HTML_GZ, sizeof SETUP_HTML_GZ, "text/html; charset=utf-8"); });
  server.on("/regions.txt", HTTP_GET,
            [] { sendAsset(REGIONS_TXT_GZ, sizeof REGIONS_TXT_GZ, "text/plain; charset=utf-8"); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/scan", HTTP_POST, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/logger", HTTP_POST, handleLogger);
  server.on("/api/region", HTTP_POST, handleRegion);
  server.on("/api/yasno/streets", HTTP_GET, handleStreets);
  server.on("/api/yasno/houses", HTTP_GET, handleHouses);
  server.on("/api/yasno/group", HTTP_GET, handleGroup);
  server.on("/api/outage", HTTP_POST, handleOutage);
  server.on("/api/save", HTTP_POST, handleSave);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.onNotFound(redirect);
  server.begin();

  // Wi-Fi вже був налаштований (вхід через BOOT) — одразу пробуємо під'єднатися, щоб не вводити заново
  if (cfg.ssid.length()) {
    wifiState = WifiState::Connecting;
    wifiStarted = millis();
    net::connect(cfg.ssid, cfg.pass);
  }
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  net::loop();
  if (scanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      collectScan(n);
      scanning = false;
    } else if (n == WIFI_SCAN_FAILED) {
      scanning = false;
    }
  }
  if (wifiState == WifiState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WifiState::Ok;
      evlog::add("Налаштування: Wi-Fi є, %s", WiFi.localIP().toString().c_str());
      // Логер уже збережений (вхід через BOOT) — одразу перевіряємо його (спершу за збереженою адресою, це швидко),
      // щоб і на табло, і на сторінці була справжня галочка, а не лише «Далі» зі старими даними
      if (cfg.loggerSn && logState == LogState::Idle) startSearch();
    } else if (millis() - wifiStarted > 20000) {
      wifiState = WifiState::Fail;
      evlog::add("Налаштування: Wi-Fi не під'єднався (status %d)", (int)WiFi.status());
    }
  } else if (wifiState == WifiState::Ok && WiFi.status() != WL_CONNECTED) {
    wifiState = WifiState::Connecting;
    wifiStarted = millis();
  }
  if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
}

ui::SetupView view() {
  ui::SetupView v;
  switch (wifiState) {
    case WifiState::Idle:
      v.wifi.state = ui::Step::Active;
      snprintf(v.wifi.text, sizeof v.wifi.text, "Wi-Fi: чекаю телефон");
      break;
    case WifiState::Connecting:
      v.wifi.state = ui::Step::Active;
      snprintf(v.wifi.text, sizeof v.wifi.text, "Wi-Fi: під'єднуюсь…");
      break;
    case WifiState::Ok:
      v.wifi.state = ui::Step::Done;
      snprintf(v.wifi.text, sizeof v.wifi.text, "Wi-Fi: %s", cfg.ssid.c_str());
      break;
    case WifiState::Fail:
      v.wifi.state = ui::Step::Error;
      snprintf(v.wifi.text, sizeof v.wifi.text, "Wi-Fi: не під'єднався");
      break;
  }
  switch (logState) {
    case LogState::Idle:
      snprintf(v.logger.text, sizeof v.logger.text, "Логер");
      if (wifiState == WifiState::Ok) v.logger.state = ui::Step::Active;
      break;
    case LogState::Searching:
      v.logger.state = ui::Step::Active;
      if (logTotal) snprintf(v.logger.text, sizeof v.logger.text, "Шукаю логер: %d з %d", (int)logDone, (int)logTotal);
      else snprintf(v.logger.text, sizeof v.logger.text, "Шукаю логер…");
      break;
    case LogState::Found:
      v.logger.state = ui::Step::Done;
      if (logSoc >= 0) snprintf(v.logger.text, sizeof v.logger.text, "Логер: заряд %d %%", logSoc);
      else snprintf(v.logger.text, sizeof v.logger.text, "Логер: %s", logIp.toString().c_str());
      break;
    case LogState::NotFound:
      v.logger.state = ui::Step::Error;
      snprintf(v.logger.text, sizeof v.logger.text, "Логер не знайдено");
      break;
    case LogState::Unsupported:
      v.logger.state = ui::Step::Error;
      snprintf(v.logger.text, sizeof v.logger.text, "Інвертор не підтримується");
      break;
  }
  if (cfg.regionId.length()) {
    v.region.state = ui::Step::Done;
    snprintf(v.region.text, sizeof v.region.text, "Тривоги: %s", cfg.regionName.c_str());
  } else {
    v.region.state = logState == LogState::Found ? ui::Step::Active : ui::Step::Pending;
    snprintf(v.region.text, sizeof v.region.text, "Тривоги (необов’язково)");
  }
  if (cfg.outProvider.length()) {
    v.outage.state = ui::Step::Done;
    snprintf(v.outage.text, sizeof v.outage.text, "Графік: черга %s", cfg.outGroup.c_str());
  } else {
    v.outage.state = cfg.regionId.length() ? ui::Step::Active : ui::Step::Pending;
    snprintf(v.outage.text, sizeof v.outage.text, "Графік (необов’язково)");
  }
  if (restartAt) snprintf(v.note, sizeof v.note, erased ? "Усе стерто, перезапускаюсь…" : "Збережено, перезапускаюсь…");
  else if (logState == LogState::NotFound)
    snprintf(v.note, sizeof v.note, "Перевірте серійник і що логер у тій самій мережі Wi-Fi");
  else snprintf(v.note, sizeof v.note, "Стерти все й почати з нуля — утримуйте BOOT 5 с");
  return v;
}

}  // namespace portal
