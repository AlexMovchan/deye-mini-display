#pragma once
// Тривога для обраного регіону: siren.pp.ua (обгортка над Ukraine Alarm, без токена).
// GET https://siren.pp.ua/api/v3/alerts/<regionId> → [{"regionId","activeAlerts":[...]}].
// Запит по громаді повертає й тривоги її району та області (перевірено 26.11.2026, docs/PLAN.md §4).
#include <stddef.h>
#include <stdint.h>

namespace siren {

struct Alert {
  bool active = false;
  bool red = false;       // червоний рівень; інакше — жовтий
  char head[48] = "";     // «Повітряна тривога», «Ракетна загроза», «Дронова небезпека», «Масована атака»
  char since[6] = "";     // "HH:MM" київського часу або ""
};

enum class Parse { Ok, BadJson, Gzip };

// Враховуємо лише повітряні тривоги (type == "AIR"), як домашній проєкт.
Parse parse(const char* body, size_t len, Alert* out);

// Ідентифікатор регіону — лише цифри, 1..6 знаків (захист від сміття з налаштувань).
bool validRegionId(const char* id);

}  // namespace siren
