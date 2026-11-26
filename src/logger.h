#pragma once
// Логер Solarman (TCP 8899): читання регістрів Deye і пошук логера в мережі. ЛИШЕ ЧИТАННЯ (Modbus 0x03).
#include <Arduino.h>
#include <IPAddress.h>

#include <functional>

#include "inverter.h"

namespace logger {

struct ReadResult {
  bool ok = false;
  inv::Reading reading;
  char error[64] = "";
};

// Одне з'єднання: регістр 84 і блок 150..217.
ReadResult read(IPAddress ip, uint32_t sn);

// Тип пристрою (регістр 0) або -1, якщо логер не відповів так, як треба (чужий пристрій, інший серійник).
int readDeviceType(IPAddress ip, uint32_t sn);

struct Found {
  bool ok = false;
  IPAddress ip;
  uint32_t sn = 0;
  int deviceType = -1;
};

// Пошук: збережена адреса → широкомовний запит Solarman (UDP 48899) → перебір своєї підмережі /24 по TCP 8899.
// sn == 0 — серійника ще немає: підходить лише логер, що сам відповів на UDP (його серійник і беремо).
// progress(перевірено, усього) — для екрана й сторінки налаштування; cancel — перервати.
Found discover(uint32_t sn, IPAddress saved, std::function<void(int, int)> progress, volatile bool* cancel);

}  // namespace logger
