#include "discovery.h"

#include <stdlib.h>
#include <string.h>

namespace disc {

bool parseSerial(const char* s, uint32_t* out) {
  if (!s) return false;
  uint64_t v = 0;
  int digits = 0;
  for (const char* p = s; *p; p++) {
    if (*p == ' ' || *p == '-') continue;
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (*p - '0');
    if (++digits > 10 || v > 0xFFFFFFFFULL) return false;
  }
  if (!digits || !v) return false;
  *out = (uint32_t)v;
  return true;
}

bool parseReply(const char* s, size_t len, Reply* out) {
  char buf[96];
  if (!s || len == 0 || len >= sizeof buf) return false;
  memcpy(buf, s, len);
  buf[len] = 0;
  // IP
  char* p = buf;
  for (int i = 0; i < 4; i++) {
    char* end;
    long v = strtol(p, &end, 10);
    if (end == p || v < 0 || v > 255) return false;
    out->ip[i] = (uint8_t)v;
    if (i < 3) {
      if (*end != '.') return false;
      p = end + 1;
    } else {
      if (*end != ',') return false;
      p = end + 1;
    }
  }
  // MAC (12 hex) — пропускаємо
  char* comma = strchr(p, ',');
  if (!comma || comma - p != 12) return false;
  for (char* q = p; q < comma; q++) {
    char c = *q;
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
  }
  // Серійник до кінця рядка (можуть бути \r\n)
  char* sn = comma + 1;
  size_t n = strcspn(sn, "\r\n ,");
  sn[n] = 0;
  return parseSerial(sn, &out->serial);
}

int scanOrder(uint8_t ownHost, uint8_t savedHost, uint8_t* hosts, int maxHosts) {
  int n = 0;
  if (savedHost >= 1 && savedHost <= 254 && savedHost != ownHost && n < maxHosts) hosts[n++] = savedHost;
  for (int h = 1; h <= 254 && n < maxHosts; h++) {
    if (h == ownHost || h == savedHost) continue;
    hosts[n++] = (uint8_t)h;
  }
  return n;
}

}  // namespace disc
