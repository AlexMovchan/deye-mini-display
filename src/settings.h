#pragma once
// Налаштування користувача в NVS (простір «cfg»). Пишемо лише при справжніх змінах.
#include <Arduino.h>

namespace settings {

struct Config {
  String ssid, pass;      // домашній Wi-Fi
  uint32_t loggerSn = 0;  // серійник логера (з наклейки)
  uint32_t loggerIp = 0;  // остання робоча адреса логера (IPv4 у порядку WiFi IPAddress), 0 — невідома
  String regionId;        // siren.pp.ua; "" — тривоги не відстежуємо
  String regionName;
  bool complete() const { return ssid.length() && loggerSn; }
};

Config load();
void save(const Config& c);
void saveLoggerIp(uint32_t ip);  // лише якщо змінилась
void erase();                    // «Скинути все»

}  // namespace settings
