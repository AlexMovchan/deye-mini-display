#include "net.h"

#include <WiFi.h>
#include <esp_sntp.h>
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
int rssi() { return lastRssi; }

// Годинник ESP32 переживає програмне перезавантаження; після вимкнення живлення — з 1970 р., тоді чекаємо NTP.
bool timeValid() { return time(nullptr) > 1767225600; }  // після 01.01.2026
int64_t now() { return (int64_t)time(nullptr); }

}  // namespace net
