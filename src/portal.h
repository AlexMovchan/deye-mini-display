#pragma once
// Режим налаштування: точка доступу Deye-Setup, captive portal і сторінка для телефона (docs/PLAN.md §5).
// Кроки: Wi-Fi → логер (пошук і перевірка) → тривоги (необов'язково) → збереження в NVS і перезапуск.
#include "settings.h"
#include "ui.h"

namespace portal {

void begin(const settings::Config& current);  // попередні налаштування — як підказки на сторінці
void loop();                                  // DNS, вебсервер, стан Wi-Fi, перезапуск після «Зберегти»
ui::SetupView view();                         // що показати на екрані табло

}  // namespace portal
