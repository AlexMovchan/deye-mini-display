#include "yasno_address.h"

#include <ArduinoJson.h>

#include "https.h"

namespace yasno {

namespace {
const char* BASE = "https://app.yasno.ua/api/blackout-service/public/shutdowns/addresses/v2/";
const char* REGION = "regionId=25&dsoId=902";

String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

// Масив [{"id":..,"value":".."}, …] читаємо по одному об'єкту з потоку: відповідь буває на сотні будинків,
// а пам'яті без PSRAM мало — тримаємо лише перші max.
std::vector<Item> list(const String& url, int max, String* error) {
  std::vector<Item> out;
  https::Request req;
  req.url = url.c_str();
  https::Response res = https::get(req, [&](Stream& s) {
    while (true) {
      int c = s.peek();
      if (c < 0) {
        uint32_t t0 = millis();
        while (s.peek() < 0 && millis() - t0 < 2000) delay(5);
        if (s.peek() < 0) break;
        continue;
      }
      if (c == '{') {
        JsonDocument d;
        if (deserializeJson(d, s)) break;
        if ((int)out.size() < max) out.push_back({d["id"] | 0u, String(d["value"] | "")});
      } else {
        s.read();  // '[', ',', ']' і пробіли
        if (c == ']') break;
      }
    }
    return true;
  });
  if (error && res.status != 200) *error = "Yasno: HTTP " + String(res.status);
  return out;
}
}  // namespace

std::vector<Item> streets(const String& query, int max, String* error) {
  return list(String(BASE) + "streets?" + REGION + "&query=" + urlEncode(query), max, error);
}

std::vector<Item> houses(uint32_t streetId, const String& query, int max, String* error) {
  return list(String(BASE) + "houses?" + REGION + "&streetId=" + streetId + "&query=" + urlEncode(query), max, error);
}

String group(uint32_t streetId, uint32_t houseId, String* error) {
  String url = String(BASE) + "group?" + REGION + "&streetId=" + streetId + "&houseId=" + houseId;
  https::Request req;
  req.url = url.c_str();
  String g;
  https::Response res = https::get(req, [&](Stream& s) {
    JsonDocument d;
    if (deserializeJson(d, s)) return false;
    int a = d["group"] | 0, b = d["subgroup"] | 0;
    if (a > 0 && b > 0) g = String(a) + "." + String(b);
    return g.length() > 0;
  });
  if (error && !g.length()) *error = "Yasno: черга не визначилась (HTTP " + String(res.status) + ")";
  return g;
}

}  // namespace yasno
