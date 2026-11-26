#include "event_log.h"

#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "kyiv_time.h"
#include "net.h"

namespace evlog {

namespace {
struct Entry {
  char stamp[16];
  char text[96];
};
// Журнал у пам'яті, яку не стирає програмний чи аварійний перезапуск (panic, watchdog, OTA): після збою
// в /log видно, що було перед ним (11.11.2026 — РК упав без сліду). Після вмикання живлення — сміття, тому magic.
struct Store {
  uint32_t magic;
  int head, count;
  Entry entries[EVENT_LOG_SIZE];
};
constexpr uint32_t MAGIC = 0x10C5A7E1;
__NOINIT_ATTR Store store;
Entry* const entries = store.entries;
int& head = store.head;
int& count = store.count;

void ensure() {
  // Перевірка лише раз за запуск: причина перезапуску лишається «живлення» до наступного скидання, і повторна
  // перевірка стирала журнал при кожному записі й читанні (15.11.2026: /log табло порожній після вмикання)
  static bool checked = false;
  if (checked) return;
  checked = true;
  if (store.magic == MAGIC && store.head >= 0 && store.head < EVENT_LOG_SIZE && store.count >= 0 &&
      store.count <= EVENT_LOG_SIZE && esp_reset_reason() != ESP_RST_POWERON)
    return;
  memset(&store, 0, sizeof store);
  store.magic = MAGIC;
}

// ── Аварія: причина й адреси в коді переживають перезапуск (12.11.2026 — РК падав з panic без сліду) ──
// Обробник викликається в момент аварії (set_arduino_panic_handler): лише копіювання, без виділення пам'яті.
struct Crash {
  uint32_t magic;
  int core;
  char reason[40];
  uint32_t pc;
  uint32_t bt[24];
  int btLen;
};
constexpr uint32_t CRASH_MAGIC = 0xC4A54ED1;
__NOINIT_ATTR Crash crash;

void onPanic(arduino_panic_info_t* info, void*) {
  crash.magic = CRASH_MAGIC;
  crash.core = info->core;
  const char* r = info->reason ? info->reason : "?";
  size_t i = 0;
  for (; i + 1 < sizeof crash.reason && r[i]; i++) crash.reason[i] = r[i];
  crash.reason[i] = 0;
  crash.pc = (uint32_t)info->pc;
  int n = info->backtrace_len < 24 ? (int)info->backtrace_len : 24;
  for (int k = 0; k < n; k++) crash.bt[k] = info->backtrace[k];
  crash.btLen = n;
}
}  // namespace

void crashBegin() {
  if (crash.magic == CRASH_MAGIC && esp_reset_reason() != ESP_RST_POWERON) {
    add("Збій: %s, ядро %d, PC 0x%08lx", crash.reason, crash.core, (unsigned long)crash.pc);
    // Адреси — по 7 у рядок (запис журналу — 96 байт); розшифровка: tools/decode_crash.py
    char line[96];
    int len = crash.btLen < 0 || crash.btLen > 24 ? 0 : crash.btLen;
    for (int k = 0; k < len; k += 7) {
      int pos = snprintf(line, sizeof line, "Стек:");
      for (int j = k; j < len && j < k + 7; j++) pos += snprintf(line + pos, sizeof line - pos, " %08lx", (unsigned long)crash.bt[j]);
      add("%s", line);
    }
  }
  crash.magic = 0;
  set_arduino_panic_handler(onPanic, nullptr);
}

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "увімкнення живлення";
    case ESP_RST_SW: return "програмний (OTA або перезапуск)";
    case ESP_RST_PANIC: return "АВАРІЙНИЙ (panic)";
    case ESP_RST_INT_WDT: return "watchdog переривань";
    case ESP_RST_TASK_WDT: return "watchdog задачі (зависання)";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "просіла напруга живлення";
    case ESP_RST_DEEPSLEEP: return "вихід зі сну";
    default: return "інша";
  }
}

// Пишуть дві задачі (екран і мережа) — запис по черзі
SemaphoreHandle_t lock() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

void add(const char* fmt, ...) {
  xSemaphoreTake(lock(), portMAX_DELAY);
  ensure();
  Entry& e = entries[head];
  if (net::timeValid()) {
    kt::Local l = kt::toLocal(net::now());
    snprintf(e.stamp, sizeof e.stamp, "%02d.%02d %02d:%02d:%02d", l.day, l.month, l.hour, l.minute, l.second);
  } else {
    snprintf(e.stamp, sizeof e.stamp, "+%lus", (unsigned long)(millis() / 1000));
  }
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.text, sizeof e.text, fmt, ap);
  va_end(ap);
  Serial.printf("[%s] %s\n", e.stamp, e.text);
  head = (head + 1) % EVENT_LOG_SIZE;
  if (count < EVENT_LOG_SIZE) count++;
  xSemaphoreGive(lock());
}

String text(int maxLines) {
  ensure();
  String t;
  int n = count < maxLines ? count : maxLines;
  for (int i = count - n; i < count; i++) {
    const Entry& e = entries[(head - count + i + EVENT_LOG_SIZE) % EVENT_LOG_SIZE];
    t += "[";
    t += e.stamp;
    t += "] ";
    t += e.text;
    t += "\n";
  }
  return t;
}

void dump(Print& out) {
  ensure();
  for (int i = 0; i < count; i++) {
    const Entry& e = entries[(head - count + i + EVENT_LOG_SIZE) % EVENT_LOG_SIZE];
    out.printf("[%s] %s\n", e.stamp, e.text);
  }
}

}  // namespace evlog
