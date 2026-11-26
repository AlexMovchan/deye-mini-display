#include "net.h"

#include <WiFi.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ping/ping_sock.h>
#include <time.h>

#include "config.h"
#include "event_log.h"

namespace net {

namespace {
String ssid_, pass_;
bool station = false;  // перепідключатися самим (робочий режим)
uint32_t lastAttempt = 0, lastNtp = 0, lastRssiLog = 0;
bool wasOnline = false;
int lastRssi = 0;
bool synced = false;

void onSync(struct timeval*) { synced = true; }

void startTime() {
  sntp_set_time_sync_notification_cb(onSync);
  configTzTime(TZ_INFO, NTP_SERVER, "time.google.com");
  lastNtp = millis();
}
}  // namespace

void beginStation(const String& ssid, const String& pass) {
  ssid_ = ssid;
  pass_ = pass;
  station = true;
  WiFi.setHostname(HOSTNAME);  // до WiFi.mode(), інакше роутер бачить «esp32-xxxxxx»
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);        // енергозбереження давало втрату половини пакетів
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid_.c_str(), pass_.c_str());
  lastAttempt = millis();
  startTime();
  evlog::add("Wi-Fi: під'єднуюсь до «%s»", ssid_.c_str());
}

void beginSetup(int channel) {
  station = false;
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAP(SETUP_AP_SSID, nullptr, channel);
  startTime();
  evlog::add("Налаштування: точка доступу %s, канал %d, %s", SETUP_AP_SSID, channel,
             WiFi.softAPIP().toString().c_str());
}

void connect(const String& ssid, const String& pass) {
  ssid_ = ssid;
  pass_ = pass;
  WiFi.disconnect(false, false);
  WiFi.begin(ssid_.c_str(), pass_.c_str());
  lastAttempt = millis();
}

void loop() {
  bool on = WiFi.status() == WL_CONNECTED;
  if (on) lastRssi = WiFi.RSSI();
  if (on != wasOnline) {
    wasOnline = on;
    if (on) evlog::add("Wi-Fi: є, %s, %d дБм", WiFi.localIP().toString().c_str(), lastRssi);
    else evlog::add("Wi-Fi: зник (останній сигнал %d дБм)", lastRssi);
  }
  if (on && millis() - lastRssiLog > 600000UL) {
    lastRssiLog = millis();
    evlog::add("Wi-Fi: сигнал %d дБм", lastRssi);
  }
  if (on && !synced && millis() - lastNtp > 60000UL) {
    evlog::add("Час: NTP ще не відповів, повторюю");
    startTime();
  }
  if (station && !on && millis() - lastAttempt > 30000UL) {
    lastAttempt = millis();
    WiFi.disconnect();
    WiFi.begin(ssid_.c_str(), pass_.c_str());
  }
}

bool online() { return WiFi.status() == WL_CONNECTED; }

namespace {
struct PingCtx {
  SemaphoreHandle_t done;
  volatile bool ok;
};
void onPingSuccess(esp_ping_handle_t, void* arg) { static_cast<PingCtx*>(arg)->ok = true; }
void onPingEnd(esp_ping_handle_t, void* arg) { xSemaphoreGive(static_cast<PingCtx*>(arg)->done); }
}  // namespace

bool gatewayAlive() {
  if (!online()) return false;
  IPAddress gw = WiFi.gatewayIP();
  if (gw == IPAddress((uint32_t)0)) return false;
  PingCtx ctx{xSemaphoreCreateBinary(), false};
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  ip_addr_t target = {};
  IP_ADDR4(&target, gw[0], gw[1], gw[2], gw[3]);
  cfg.target_addr = target;
  cfg.count = 3;
  cfg.interval_ms = 300;
  cfg.timeout_ms = 600;
  esp_ping_callbacks_t cbs = {};
  cbs.cb_args = &ctx;
  cbs.on_ping_success = onPingSuccess;
  cbs.on_ping_end = onPingEnd;
  esp_ping_handle_t h = nullptr;
  bool ok = false;
  if (esp_ping_new_session(&cfg, &cbs, &h) == ESP_OK) {
    esp_ping_start(h);
    xSemaphoreTake(ctx.done, pdMS_TO_TICKS(4000));
    esp_ping_stop(h);
    esp_ping_delete_session(h);
    ok = ctx.ok;
  }
  vSemaphoreDelete(ctx.done);
  return ok;
}

void reconnect() {
  evlog::add("Wi-Fi: перепідключаюсь (дані не ходили)");
  WiFi.disconnect();
  delay(500);
  WiFi.begin(ssid_.c_str(), pass_.c_str());
  lastAttempt = millis();
}
int rssi() { return lastRssi; }

// Годинник ESP32 переживає програмне перезавантаження; після вимкнення живлення — з 1970 р., тоді чекаємо NTP.
bool timeValid() { return time(nullptr) > 1767225600; }  // після 01.01.2026
int64_t now() { return (int64_t)time(nullptr); }

}  // namespace net
