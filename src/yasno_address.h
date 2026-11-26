#pragma once
// Київ: черга відключень за адресою через публічний пошук Yasno (ДТЕК Київські електромережі), без токена.
//   …/addresses/v2/streets?regionId=25&dsoId=902&query=<вулиця>          → [{"id":911,"value":"вул. Хрещатик"}]
//   …/addresses/v2/houses?regionId=25&dsoId=902&streetId=911&query=<буд> → [{"id":41771,"value":"1"}]
//   …/addresses/v2/group?regionId=25&dsoId=902&streetId=911&houseId=41771 → {"group":19,"subgroup":1}
#include <Arduino.h>

#include <vector>

namespace yasno {

struct Item {
  uint32_t id;
  String value;
};

// Перші max збігів; порожньо — нічого не знайшлось або збій (error — пояснення для журналу).
std::vector<Item> streets(const String& query, int max, String* error = nullptr);
std::vector<Item> houses(uint32_t streetId, const String& query, int max, String* error = nullptr);
// "47.1" або "" при збої
String group(uint32_t streetId, uint32_t houseId, String* error = nullptr);

}  // namespace yasno
