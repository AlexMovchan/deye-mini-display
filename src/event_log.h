#pragma once
// Кільцевий журнал останніх EVENT_LOG_SIZE подій у RAM + вивід у Serial (docs/SCOPE.md, «Надійність»).
#include <Arduino.h>

namespace evlog {

// Причина останнього перезапуску словами («АВАРІЙНИЙ (panic)», «watchdog задачі (зависання)» …)
const char* resetReasonText();
// Після аварійного перезапуску — записати в журнал причину й адреси (збережені обробником аварії), і поставити
// обробник для наступної. Викликати в setup() одразу після рядка «Старт …».
void crashBegin();

void add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void dump(Print& out);
// Останні maxLines подій одним текстом (для /log у боті).
String text(int maxLines);

}  // namespace evlog
