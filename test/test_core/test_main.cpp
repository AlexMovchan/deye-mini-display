// Тести чистої логіки lib/core на ПК: pio test -e native
// Серійник логера в тестах вигаданий (1234567890); кадри звірено з pysolarmanv5 (_v5_frame_encoder/_decoder).
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "discovery.h"
#include "inverter.h"
#include "kyiv_time.h"
#include "siren.h"
#include "solarman.h"
#include "tiles.h"

void setUp() {}
void tearDown() {}

static const uint32_t SN = 1234567890UL;

static int hexToBytes(const char* hex, uint8_t* out) {
  int n = 0;
  for (; hex[0] && hex[1]; hex += 2) {
    unsigned v;
    sscanf(hex, "%2x", &v);
    out[n++] = (uint8_t)v;
  }
  return n;
}

static int64_t utc(int y, int mo, int d, int h, int mi, int s = 0) {
  return kt::daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}
static int64_t kyiv(int d, int h, int mi) { return utc(2026, 11, d, h - 2, mi); }  // листопад: UTC+2

// ───────────────────────── Solarman V5 ─────────────────────────
static void test_v5_requests_match_pysolarmanv5() {
  struct { uint16_t reg, cnt; uint8_t seq; const char* hex; } v[] = {
      {84, 1, 0x42, "a5170010454200d2029649020000000000000000000000000000010300540001c5da5b15"},
      {150, 68, 0x43, "a5170010454300d2029649020000000000000000000000000000010300960044a5d5bc15"},
      {0, 1, 0x44, "a5170010454400d2029649020000000000000000000000000000010300000001840af815"},
  };
  for (auto& t : v) {
    uint8_t exp[64], got[solarman::REQUEST_LEN];
    TEST_ASSERT_EQUAL(36, hexToBytes(t.hex, exp));
    TEST_ASSERT_EQUAL(36, solarman::buildReadRequest(got, SN, t.seq, 1, t.reg, t.cnt));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, got, 36);
  }
}

static void test_v5_responses() {
  uint8_t f[64];
  uint16_t v;
  int n = hexToBytes("a5150010154200d202964902010000000000000000000000000103020038b996bf15", f);
  TEST_ASSERT_EQUAL(n, solarman::frameLength(f, n));
  TEST_ASSERT_EQUAL(solarman::Result::Ok, solarman::parseReadResponse(f, n, SN, 0x42, 1, 1, &v));
  TEST_ASSERT_EQUAL(56, v);
  // чужий серійник, інший номер запиту, неповний кадр
  TEST_ASSERT_EQUAL(solarman::Result::BadFrame, solarman::parseReadResponse(f, n, 1111111111UL, 0x42, 1, 1, &v));
  TEST_ASSERT_EQUAL(solarman::Result::WrongSequence, solarman::parseReadResponse(f, n, SN, 0x43, 1, 1, &v));
  TEST_ASSERT_EQUAL(solarman::Result::Incomplete, solarman::parseReadResponse(f, n - 1, SN, 0x42, 1, 1, &v));
  // тип пристрою (регістр 0) = 3
  n = hexToBytes("a5150010154400d202964902010000000000000000000000000103020003f8457a15", f);
  TEST_ASSERT_EQUAL(solarman::Result::Ok, solarman::parseReadResponse(f, n, SN, 0x44, 1, 1, &v));
  TEST_ASSERT_EQUAL(3, v);
}

static void test_crc16() {
  uint8_t m[] = {0x01, 0x03, 0x00, 0xB8, 0x00, 0x01};
  TEST_ASSERT_EQUAL_HEX16(0x2F04, solarman::crc16Modbus(m, sizeof m));
}

// ───────────────────────── Інвертор ─────────────────────────
// Справжні значення 150..217 з однофазного Deye (26.11.2026, під час відключення).
static const uint16_t LIVE[inv::LIVE_COUNT] = {
    48, 0, 48, 0, 2301, 0, 2301, 2344, 0, 0, 0, 0, 0, 0, 120, 0, 0, 0, 0, 0, 0, 0, 0, 336, 0, 336, 336, 0, 336, 336,
    0, 85, 1230, 5290, 88, 336, 0, 0, 0, 0, 347, 656, 5000, 5000, 0, 16400, 0, 100, 0, 0, 1, 5760, 5760, 5520, 200,
    4500, 20, 90, 0, 0, 60, 130, 0, 1, 0, 8, 990, 15};

static void test_decode_live() {
  inv::Reading r = inv::decode(56, LIVE);
  TEST_ASSERT_EQUAL(88, r.soc);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.8f, r.gridV);
  TEST_ASSERT_EQUAL(336, r.loadW);
  TEST_ASSERT_EQUAL(347, r.battW);  // розряд
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 52.9f, r.battV);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.6f, r.dayKwh);
  TEST_ASSERT_EQUAL(200, r.battAh);
  TEST_ASSERT_EQUAL(15, r.shutdownSoc);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.58f, inv::batteryKwh(r));
  TEST_ASSERT_EQUAL(15, inv::cutoffSoc(r));
  r.shutdownSoc = 0;
  TEST_ASSERT_EQUAL(20, inv::cutoffSoc(r));
  r.battV = 0;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.24f, inv::batteryKwh(r));  // 200 А·год × 51,2 В
}

static void test_device_type() {
  TEST_ASSERT_TRUE(inv::supported(3));
  TEST_ASSERT_FALSE(inv::supported(5));
  TEST_ASSERT_EQUAL_STRING("трифазний гібрид", inv::deviceTypeName(5));
}

static void test_grid_tracker() {
  inv::Tracker t(180, 2, 600);
  inv::Reading on = inv::decode(56, LIVE);
  on.gridV = 230;
  inv::Reading off = on;
  off.gridV = 4.8f;
  t.update(on, 1000);
  TEST_ASSERT_EQUAL(inv::Grid::On, t.shown());  // до підтвердження — останнє зчитування
  t.update(on, 1030);
  TEST_ASSERT_EQUAL(inv::Grid::On, t.shown());
  TEST_ASSERT_EQUAL_INT64(0, t.since());        // світло було ще до старту — час невідомий
  t.update(off, 1060);
  TEST_ASSERT_EQUAL(inv::Grid::On, t.shown());  // одне зчитування — ще не зміна
  t.update(off, 1090);
  TEST_ASSERT_EQUAL(inv::Grid::Off, t.shown());
  TEST_ASSERT_EQUAL_INT64(1090, t.since());
}

static void test_hours_left() {
  inv::Tracker t(180, 2, 600);
  inv::Reading r = inv::decode(56, LIVE);  // 88 %, поріг 15 %, 10,58 кВт·год
  for (int i = 0; i < 5; i++) {
    r.loadW = 336;
    t.update(r, 1000 + i * 30);
  }
  // (88 − 15) % × 10,58 кВт·год / 336 Вт ≈ 22,99 год
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.99f, t.hoursLeft(r));
  inv::Tracker idle(180, 2, 600);
  r.loadW = 20;
  idle.update(r, 1000);
  TEST_ASSERT_TRUE(idle.hoursLeft(r) < 0);
}

// ───────────────────────── Тривоги ─────────────────────────
static siren::Alert sp(const char* body, siren::Parse expect = siren::Parse::Ok) {
  siren::Alert a;
  TEST_ASSERT_EQUAL(expect, siren::parse(body, strlen(body), &a));
  return a;
}

static void test_siren() {
  siren::Alert a = sp("[{\"regionId\":\"442\",\"activeAlerts\":[]}]");
  TEST_ASSERT_FALSE(a.active);
  // Громада повертає тривогу свого району (regionId 59) — як у справжній відповіді 26.11.2026
  a = sp("[{\"regionId\":\"442\",\"regionType\":\"Community\",\"activeAlerts\":[{\"regionId\":\"59\",\"regionType\":"
         "\"District\",\"type\":\"AIR\",\"lastUpdate\":\"2026-11-26T15:22:49.140682Z\",\"activeAlertLevels\":[]}]}]");
  TEST_ASSERT_TRUE(a.active);
  TEST_ASSERT_TRUE(a.red);
  TEST_ASSERT_EQUAL_STRING("Повітряна тривога", a.head);
  TEST_ASSERT_EQUAL_STRING("17:22", a.since);
  a = sp("{\"activeAlerts\":[{\"type\":\"AIR\",\"activeAlertLevels\":[{\"alertLevel\":\"Yellow\",\"reason\":"
         "\"Дронова загроза\",\"createdAt\":\"2026-11-09T13:55:02Z\"}]}]}");
  TEST_ASSERT_FALSE(a.red);
  TEST_ASSERT_EQUAL_STRING("Дронова небезпека", a.head);
  TEST_ASSERT_EQUAL_STRING("15:55", a.since);
  a = sp("{\"activeAlerts\":[{\"type\":\"AIR\",\"activeAlertLevels\":[{\"alertLevel\":\"Red\",\"reason\":"
         "\"Ракетна загроза\"}]}]}");
  TEST_ASSERT_EQUAL_STRING("Ракетна загроза", a.head);
  a = sp("{\"activeAlerts\":[{\"type\":\"ARTILLERY\",\"activeAlertLevels\":[]}]}");
  TEST_ASSERT_FALSE(a.active);
  sp("\x1f\x8b\x08\x00", siren::Parse::Gzip);
  sp("<html>", siren::Parse::BadJson);
  TEST_ASSERT_TRUE(siren::validRegionId("442"));
  TEST_ASSERT_FALSE(siren::validRegionId(""));
  TEST_ASSERT_FALSE(siren::validRegionId("44a"));
  TEST_ASSERT_FALSE(siren::validRegionId("1234567"));
}

// ───────────────────────── Пошук логера ─────────────────────────
static void test_serial() {
  uint32_t s;
  TEST_ASSERT_TRUE(disc::parseSerial("1234567890", &s));
  TEST_ASSERT_EQUAL_UINT32(1234567890UL, s);
  TEST_ASSERT_TRUE(disc::parseSerial("1234 567-890", &s));
  TEST_ASSERT_EQUAL_UINT32(1234567890UL, s);
  TEST_ASSERT_TRUE(disc::parseSerial("4294967295", &s));
  TEST_ASSERT_FALSE(disc::parseSerial("4294967296", &s));  // не вміщається в uint32
  TEST_ASSERT_FALSE(disc::parseSerial("12345678901", &s));
  TEST_ASSERT_FALSE(disc::parseSerial("", &s));
  TEST_ASSERT_FALSE(disc::parseSerial("0", &s));
  TEST_ASSERT_FALSE(disc::parseSerial("12ab", &s));
}

static void test_udp_reply() {
  disc::Reply r;
  const char* s = "192.168.1.77,A1B2C3D4E5F6,1234567890\r\n";
  TEST_ASSERT_TRUE(disc::parseReply(s, strlen(s), &r));
  TEST_ASSERT_EQUAL(192, r.ip[0]);
  TEST_ASSERT_EQUAL(77, r.ip[3]);
  TEST_ASSERT_EQUAL_UINT32(1234567890UL, r.serial);
  const char* bad[] = {"192.168.1,A1B2C3D4E5F6,1", "192.168.1.300,A1B2C3D4E5F6,1", "192.168.1.7,A1B2,1", "+ok"};
  for (const char* b : bad) TEST_ASSERT_FALSE(disc::parseReply(b, strlen(b), &r));
}

static void test_scan_order() {
  uint8_t h[254];
  int n = disc::scanOrder(10, 77, h, 254);
  TEST_ASSERT_EQUAL(253, n);  // усі 1..254, крім власної
  TEST_ASSERT_EQUAL(77, h[0]);
  TEST_ASSERT_EQUAL(1, h[1]);
  for (int i = 0; i < n; i++) TEST_ASSERT_NOT_EQUAL(10, h[i]);
  n = disc::scanOrder(10, 0, h, 254);
  TEST_ASSERT_EQUAL(253, n);
  TEST_ASSERT_EQUAL(1, h[0]);
}

// ───────────────────────── Плитки ─────────────────────────
static tiles::Input baseInput() {
  tiles::Input in;
  in.now = kyiv(26, 11, 20);
  in.logger = tiles::LoggerState::Ok;
  in.reading = inv::decode(56, LIVE);
  in.reading.gridV = 234;
  in.reading.battW = -42;
  in.reading.soc = 100;
  in.grid = inv::Grid::On;
  in.gridSince = kyiv(26, 9, 30);
  in.alertState = tiles::AlertState::Ok;
  return in;
}

static void test_tiles_calm_light_on() {
  tiles::Model m = tiles::build(baseInput());
  TEST_ASSERT_FALSE(m.message);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::FRAME_CALM, m.frame);
  TEST_ASSERT_EQUAL_STRING("Тривоги немає", m.top);
  TEST_ASSERT_EQUAL_STRING("11:20", m.clock);
  TEST_ASSERT_EQUAL_STRING("Є", m.light.text);
  TEST_ASSERT_EQUAL_STRING("з 09:30 · 234 В", m.light.sub);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::CARD_LIGHT, m.light.bg);
  TEST_ASSERT_EQUAL_STRING("100", m.soc.big);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::SOC_GREEN, m.soc.barColor);
  TEST_ASSERT_EQUAL_STRING("336", m.load.big);
  TEST_ASSERT_EQUAL_STRING("за добу 5,6 кВт·год", m.load.sub);
  TEST_ASSERT_EQUAL_STRING("Батарея", m.batt.title);
  TEST_ASSERT_EQUAL_STRING("42", m.batt.big);
  TEST_ASSERT_EQUAL_STRING("заряджається", m.batt.sub);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::PLUS, m.batt.valueColor);
}

static void test_tiles_red_light_off() {
  tiles::Input in = baseInput();
  in.grid = inv::Grid::Off;
  in.gridSince = kyiv(26, 15, 0);
  in.reading.battW = 410;
  in.reading.soc = 38;
  in.hoursLeft = 2 + 10 / 60.0f;
  in.alert.active = true;
  in.alert.red = true;
  snprintf(in.alert.head, sizeof in.alert.head, "Повітряна тривога");
  snprintf(in.alert.since, sizeof in.alert.since, "15:41");
  tiles::Model m = tiles::build(in);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::FRAME_RED, m.frame);
  TEST_ASSERT_EQUAL_STRING("Повітряна тривога · з 15:41", m.top);
  TEST_ASSERT_EQUAL_STRING("Немає", m.light.text);
  TEST_ASSERT_EQUAL_STRING("з 15:00", m.light.sub);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::SOC_RED, m.soc.barColor);
  TEST_ASSERT_EQUAL_STRING("Вистачить", m.batt.title);
  TEST_ASSERT_EQUAL_STRING("2", m.batt.big);
  TEST_ASSERT_EQUAL_STRING("год 10 хв", m.batt.unit);
  TEST_ASSERT_EQUAL_STRING("розряд 410 Вт", m.batt.sub);
}

static void test_tiles_no_region_and_stale() {
  tiles::Input in = baseInput();
  in.alertState = tiles::AlertState::Off;
  in.logger = tiles::LoggerState::Stale;
  in.staleMinutes = 3;
  tiles::Model m = tiles::build(in);
  TEST_ASSERT_EQUAL_STRING("", m.top);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::FRAME_CALM, m.frame);
  // Збій зв'язку — не «світла немає»
  TEST_ASSERT_EQUAL_STRING("Невідомо", m.light.text);
  TEST_ASSERT_EQUAL_STRING("дані 3 хв тому", m.soc.sub);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::STALE, m.soc.valueColor);
  TEST_ASSERT_EQUAL_STRING("Батарея", m.batt.title);  // «вистачить» лише для свіжих даних
}

static void test_tiles_messages() {
  tiles::Input in = baseInput();
  in.logger = tiles::LoggerState::Searching;
  TEST_ASSERT_TRUE(tiles::build(in).message);
  in.logger = tiles::LoggerState::Unsupported;
  in.deviceType = 5;
  tiles::Model m = tiles::build(in);
  TEST_ASSERT_EQUAL_STRING("Інвертор не підтримується", m.msgTitle);
  TEST_ASSERT_NOT_NULL(strstr(m.msgText, "трифазний гібрид (5)"));
  in.alertState = tiles::AlertState::NoData;
  in.logger = tiles::LoggerState::Ok;
  TEST_ASSERT_EQUAL_STRING("Тривога: немає даних", tiles::build(in).top);
}

static void test_formats() {
  char big[8], unit[24];
  tiles::formatHours(0.66f, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("40", big);
  TEST_ASSERT_EQUAL_STRING("хв", unit);
  tiles::formatHours(3.0f, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("3", big);
  TEST_ASSERT_EQUAL_STRING("год", unit);
  tiles::formatHours(12.6f, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("13", big);
  tiles::formatHours(30, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("24", big);
  TEST_ASSERT_EQUAL_STRING("год+", unit);
  tiles::formatWatts(-1450, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("1450", big);
  tiles::formatWatts(12400, big, sizeof big, unit, sizeof unit);
  TEST_ASSERT_EQUAL_STRING("12", big);
  TEST_ASSERT_EQUAL_STRING("кВт", unit);
  TEST_ASSERT_EQUAL_HEX32(tiles::color::SOC_YELLOW, tiles::socColor(45));
  TEST_ASSERT_EQUAL_HEX32(tiles::color::SOC_RED, tiles::socColor(44));
  TEST_ASSERT_EQUAL_HEX32(tiles::color::SOC_GREEN, tiles::socColor(75));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_v5_requests_match_pysolarmanv5);
  RUN_TEST(test_v5_responses);
  RUN_TEST(test_crc16);
  RUN_TEST(test_decode_live);
  RUN_TEST(test_device_type);
  RUN_TEST(test_grid_tracker);
  RUN_TEST(test_hours_left);
  RUN_TEST(test_siren);
  RUN_TEST(test_serial);
  RUN_TEST(test_udp_reply);
  RUN_TEST(test_scan_order);
  RUN_TEST(test_tiles_calm_light_on);
  RUN_TEST(test_tiles_red_light_off);
  RUN_TEST(test_tiles_no_region_and_stale);
  RUN_TEST(test_tiles_messages);
  RUN_TEST(test_formats);
  return UNITY_END();
}
