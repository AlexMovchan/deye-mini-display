#include "logger.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>

#include "config.h"
#include "discovery.h"
#include "event_log.h"
#include "solarman.h"

namespace logger {

namespace {
uint8_t sequence = 0;

// Надсилає запит і чекає на свою відповідь, пропускаючи службові кадри логера й чужі номери.
solarman::Result request(WiFiClient& c, uint32_t sn, uint16_t reg, uint16_t count, uint16_t* values) {
  if (!sequence) sequence = (uint8_t)(esp_random() % 254 + 1);
  sequence = sequence == 0xFF ? 1 : sequence + 1;
  uint8_t req[solarman::REQUEST_LEN];
  size_t n = solarman::buildReadRequest(req, sn, sequence, MODBUS_SLAVE_ID, reg, count);
  while (c.available()) c.read();
  if (c.write(req, n) != n) return solarman::Result::Incomplete;

  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  while (millis() - start < LOGGER_TIMEOUT_MS) {
    while (c.available() && len < sizeof buf) buf[len++] = c.read();
    if (len && buf[0] != 0xA5) {  // сміття до початку кадру
      size_t i = 1;
      while (i < len && buf[i] != 0xA5) i++;
      memmove(buf, buf + i, len - i);
      len -= i;
      continue;
    }
    size_t flen = solarman::frameLength(buf, len);
    if (flen > sizeof buf) return solarman::Result::BadFrame;
    if (flen && len >= flen) {
      solarman::Result r = solarman::parseReadResponse(buf, flen, sn, sequence, MODBUS_SLAVE_ID, count, values);
      if (r != solarman::Result::CounterFrame && r != solarman::Result::WrongSequence) return r;
      memmove(buf, buf + flen, len - flen);
      len -= flen;
      continue;
    }
    if (!c.connected() && !c.available()) return solarman::Result::Incomplete;
    delay(10);
  }
  return solarman::Result::Incomplete;
}

int deviceTypeOn(WiFiClient& c, uint32_t sn) {
  uint16_t t;
  return request(c, sn, inv::REG_DEVICE_TYPE, 1, &t) == solarman::Result::Ok ? t : -1;
}
}  // namespace

ReadResult read(IPAddress ip, uint32_t sn) {
  ReadResult res;
  WiFiClient c;
  if (!c.connect(ip, LOGGER_PORT, LOGGER_TIMEOUT_MS)) {
    snprintf(res.error, sizeof res.error, "немає з'єднання з %s", ip.toString().c_str());
    return res;
  }
  c.setNoDelay(true);
  uint16_t day = 0;
  static uint16_t live[inv::LIVE_COUNT];
  solarman::Result r = request(c, sn, inv::REG_DAY_LOAD, 1, &day);
  if (r == solarman::Result::Ok) r = request(c, sn, inv::LIVE_FIRST, inv::LIVE_COUNT, live);
  c.stop();
  if (r != solarman::Result::Ok) {
    snprintf(res.error, sizeof res.error, "логер: %s", solarman::resultName(r));
    return res;
  }
  res.ok = true;
  res.reading = inv::decode(day, live);
  return res;
}

int readDeviceType(IPAddress ip, uint32_t sn) {
  WiFiClient c;
  if (!c.connect(ip, LOGGER_PORT, LOGGER_TIMEOUT_MS)) return -1;
  c.setNoDelay(true);
  int t = deviceTypeOn(c, sn);
  c.stop();
  return t;
}

Found discover(uint32_t sn, IPAddress saved, std::function<void(int, int)> progress, volatile bool* cancel) {
  Found f;
  IPAddress own = WiFi.localIP();
  auto stop = [&] { return cancel && *cancel; };

  // 1. Збережена адреса
  if (sn && saved != IPAddress((uint32_t)0)) {
    int t = readDeviceType(saved, sn);
    if (t >= 0) {
      f = {true, saved, sn, t};
      evlog::add("Логер: на збереженій адресі %s", saved.toString().c_str());
      return f;
    }
  }
  if (stop()) return f;

  // 2. Широкомовний запит Solarman: логер може сам назвати свою адресу й серійник
  WiFiUDP udp;
  if (udp.begin(0)) {
    for (int attempt = 0; attempt < 2 && !f.ok && !stop(); attempt++) {
      udp.beginPacket(WiFi.broadcastIP(), DISCOVERY_UDP_PORT);
      udp.print(DISCOVERY_UDP_MSG);
      udp.endPacket();
      uint32_t t0 = millis();
      while (millis() - t0 < 1500 && !f.ok) {
        int n = udp.parsePacket();
        if (n > 0) {
          char buf[96];
          int len = udp.read(buf, sizeof buf - 1);
          disc::Reply r;
          if (len > 0 && disc::parseReply(buf, len, &r) && (!sn || r.serial == sn)) {
            IPAddress ip(r.ip[0], r.ip[1], r.ip[2], r.ip[3]);
            int t = readDeviceType(ip, r.serial);
            if (t >= 0) {
              f = {true, ip, r.serial, t};
              evlog::add("Логер: відповів на UDP, %s", ip.toString().c_str());
            }
          }
        }
        delay(20);
      }
    }
    udp.stop();
  }
  if (f.ok || !sn || stop()) return f;  // без серійника перебір підмережі нічого не перевірить

  // 3. Перебір своєї підмережі /24: хто відкрив TCP 8899 і відповів на наш серійник
  uint8_t hosts[254];
  uint8_t savedHost = (saved[0] == own[0] && saved[1] == own[1] && saved[2] == own[2]) ? saved[3] : 0;
  int total = disc::scanOrder(own[3], savedHost, hosts, 254);
  evlog::add("Логер: перебираю %d.%d.%d.1–254", own[0], own[1], own[2]);
  for (int i = 0; i < total && !stop(); i++) {
    if (progress) progress(i, total);
    IPAddress ip(own[0], own[1], own[2], hosts[i]);
    WiFiClient c;
    if (!c.connect(ip, LOGGER_PORT, SCAN_CONNECT_MS)) continue;
    c.setNoDelay(true);
    int t = deviceTypeOn(c, sn);
    c.stop();
    if (t >= 0) {
      f = {true, ip, sn, t};
      evlog::add("Логер: знайдено перебором, %s", ip.toString().c_str());
      break;
    }
  }
  if (progress) progress(total, total);
  return f;
}

}  // namespace logger
