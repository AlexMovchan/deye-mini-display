#pragma once
// Пошук логера Solarman у мережі: розбір відповіді на широкомовний запит і серійного номера.
#include <stddef.h>
#include <stdint.h>

namespace disc {

// Серійник логера з наклейки: 1..10 цифр, що вміщаються в uint32 (пробіли й дефіси ігноруються).
bool parseSerial(const char* s, uint32_t* out);

// Відповідь логера на "WIFIKIT-214028-READ": "192.168.1.77,AABBCCDDEEFF,1234567890".
struct Reply {
  uint8_t ip[4];
  uint32_t serial;
};
bool parseReply(const char* s, size_t len, Reply* out);

// Порядок перебору підмережі /24: спершу збережена адреса (якщо вона в цій підмережі), далі решта 1..254,
// крім власної. Повертає кількість кандидатів у hosts (останні октети).
int scanOrder(uint8_t ownHost, uint8_t savedHost, uint8_t* hosts, int maxHosts);

}  // namespace disc
