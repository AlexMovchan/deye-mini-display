#include "settings.h"

#include <Preferences.h>

namespace settings {

namespace {
constexpr const char* NS = "cfg";
}

Config load() {
  Preferences p;
  Config c;
  if (!p.begin(NS, true)) return c;  // простору ще немає — перший запуск
  c.ssid = p.getString("ssid", "");
  c.pass = p.getString("pass", "");
  c.loggerSn = p.getULong("sn", 0);
  c.loggerIp = p.getULong("lip", 0);
  c.regionId = p.getString("rid", "");
  c.regionName = p.getString("rname", "");
  c.outProvider = p.getString("oprov", "");
  c.outGroup = p.getString("ogrp", "");
  c.yStreet = p.getULong("yst", 0);
  c.yHouse = p.getULong("yhs", 0);
  c.outAddress = p.getString("oaddr", "");
  p.end();
  return c;
}

void save(const Config& c) {
  Preferences p;
  p.begin(NS, false);
  p.putString("ssid", c.ssid);
  p.putString("pass", c.pass);
  p.putULong("sn", c.loggerSn);
  p.putULong("lip", c.loggerIp);
  p.putString("rid", c.regionId);
  p.putString("rname", c.regionName);
  p.putString("oprov", c.outProvider);
  p.putString("ogrp", c.outGroup);
  p.putULong("yst", c.yStreet);
  p.putULong("yhs", c.yHouse);
  p.putString("oaddr", c.outAddress);
  p.end();
}

void saveLoggerIp(uint32_t ip) {
  Preferences p;
  p.begin(NS, false);
  if (p.getULong("lip", 0) != ip) p.putULong("lip", ip);
  p.end();
}

void saveOutGroup(const String& group) {
  Preferences p;
  p.begin(NS, false);
  if (p.getString("ogrp", "") != group) p.putString("ogrp", group);
  p.end();
}

void erase() {
  Preferences p;
  p.begin(NS, false);
  p.clear();
  p.end();
}

}  // namespace settings
