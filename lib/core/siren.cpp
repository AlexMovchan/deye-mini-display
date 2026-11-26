#include "siren.h"

#include <ArduinoJson.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "kyiv_time.h"

namespace siren {

static void copy(char* dst, size_t n, const char* src) {
  snprintf(dst, n, "%s", src);
}

// Нижній регістр для UTF-8 кирилиці (А–Я, Є, І, Ї) і латиниці — для пошуку основ слів у причинах.
static void lowerUtf8(const char* s, char* out, size_t outLen) {
  size_t n = strlen(s), j = 0;
  for (size_t i = 0; i < n && j + 2 < outLen; i++) {
    unsigned char c = s[i], d = i + 1 < n ? s[i + 1] : 0;
    if (c == 0xD0 && d >= 0x90 && d <= 0x9F) { out[j++] = (char)0xD0; out[j++] = (char)(d + 0x20); i++; }
    else if (c == 0xD0 && d >= 0xA0 && d <= 0xAF) { out[j++] = (char)0xD1; out[j++] = (char)(d - 0x20); i++; }
    else if (c == 0xD0 && (d == 0x84 || d == 0x86 || d == 0x87)) { out[j++] = (char)0xD1; out[j++] = (char)(d + 0x10); i++; }
    else if (c < 0x80) out[j++] = (char)tolower(c);
    else out[j++] = (char)c;
  }
  out[j] = 0;
}

Parse parse(const char* body, size_t len, Alert* out) {
  if (len >= 2 && (unsigned char)body[0] == 0x1F && (unsigned char)body[1] == 0x8B) return Parse::Gzip;
  JsonDocument doc;
  if (deserializeJson(doc, body, len)) return Parse::BadJson;
  JsonVariantConst root = doc.as<JsonVariantConst>();
  if (root.is<JsonArrayConst>()) root = root[0];  // відповідь — масив з одного об'єкта
  if (!root.is<JsonObjectConst>()) return Parse::BadJson;

  Alert a;
  bool anyAir = false, red = false;
  int64_t earliest = INT64_MAX;
  char reasons[256] = "";
  auto noteTime = [&](const char* iso) {
    int64_t t;
    if (kt::parseIso(iso, &t) && t < earliest) earliest = t;
  };
  for (JsonObjectConst al : root["activeAlerts"].as<JsonArrayConst>()) {
    if (strcmp(al["type"] | "", "AIR") != 0) continue;
    anyAir = true;
    JsonArrayConst levels = al["activeAlertLevels"].as<JsonArrayConst>();
    if (levels.size() == 0) red = true;  // рівнів немає — вважаємо червоним (обережніше)
    for (JsonObjectConst lv : levels) {
      if (strcmp(lv["alertLevel"] | "Red", "Red") == 0) red = true;
      const char* reason = lv["reason"] | "";
      if (*reason) {
        size_t n = strlen(reasons);
        snprintf(reasons + n, sizeof reasons - n, "%s%s", n ? " " : "", reason);
      }
      noteTime(lv["createdAt"] | "");
    }
    noteTime(al["lastUpdate"] | "");
  }
  if (!anyAir) {
    *out = a;
    return Parse::Ok;
  }
  a.active = true;
  a.red = red;
  char low[256];
  lowerUtf8(reasons, low, sizeof low);
  if (strstr(low, "ракет") || strstr(low, "балісти"))
    copy(a.head, sizeof a.head, "Ракетна загроза");
  else if (strstr(low, "масован"))
    copy(a.head, sizeof a.head, "Масована атака");
  else if (strstr(low, "дрон"))
    copy(a.head, sizeof a.head, "Дронова небезпека");
  else
    copy(a.head, sizeof a.head, red ? "Повітряна тривога" : "Дронова небезпека");
  if (earliest != INT64_MAX) {
    kt::Local l = kt::toLocal(earliest);
    snprintf(a.since, sizeof a.since, "%02d:%02d", l.hour, l.minute);
  }
  *out = a;
  return Parse::Ok;
}

bool validRegionId(const char* id) {
  if (!id || !*id) return false;
  size_t n = 0;
  for (const char* p = id; *p; p++, n++)
    if (*p < '0' || *p > '9' || n >= 6) return false;
  return true;
}

}  // namespace siren
