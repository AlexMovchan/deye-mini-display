#pragma once
// Регістри однофазного гібридного Deye (SG0xLP1) і похідні значення. ЛИШЕ ЧИТАННЯ (Modbus 0x03).
// Карта регістрів — docs/PLAN.md §3. Трифазні моделі (LP3/HP3) мають іншу карту — не підтримуються.
#include <stdint.h>

namespace inv {

// Тип пристрою — регістр 0 (читаємо при налаштуванні й після кожного пошуку логера).
constexpr uint16_t REG_DEVICE_TYPE = 0;
constexpr uint16_t TYPE_SINGLE_PHASE_HYBRID = 3;
bool supported(uint16_t deviceType);
const char* deviceTypeName(uint16_t deviceType);  // «однофазний гібрид», «трифазний гібрид» …

// Що читаємо кожен цикл (одне TCP-з'єднання, два запити).
constexpr uint16_t REG_DAY_LOAD = 84;                        // ×0,1 кВт·год, спожито за сьогодні
constexpr uint16_t LIVE_FIRST = 150, LIVE_COUNT = 68;         // 150..217
constexpr uint16_t REG_GRID_V = 150;                         // ×0,1 В
constexpr uint16_t REG_GRID_W = 169;                         // Вт, int16
constexpr uint16_t REG_LOAD_W = 178;                         // Вт, int16
constexpr uint16_t REG_BATT_V = 183;                         // ×0,01 В
constexpr uint16_t REG_SOC = 184;                            // %
constexpr uint16_t REG_BATT_W = 190;                         // Вт, int16; мінус = заряджається
constexpr uint16_t REG_BATT_AH = 204;                        // А·год, з налаштувань інвертора
constexpr uint16_t REG_SHUTDOWN_SOC = 217;                   // %, поріг вимкнення від батареї (⚠️ звірити)

struct Reading {
  int soc = 0;            // %
  float battV = 0;        // В
  int battW = 0;          // мінус = заряджається
  int loadW = 0;          // ≥ 0
  float gridV = 0;        // В
  int gridW = 0;
  float dayKwh = 0;
  int battAh = 0;
  int shutdownSoc = 0;
};

// dayLoad — значення регістра 84; live — LIVE_COUNT значень з 150.
Reading decode(uint16_t dayLoad, const uint16_t* live);

// Ємність банку батарей, кВт·год: А·год × напруга (виміряна, якщо правдоподібна, інакше 51,2 В LiFePO4).
float batteryKwh(const Reading& r);
// Поріг, нижче якого інвертор батарею не розряджає: регістр 217, якщо правдоподібний (1..90), інакше 20 %.
int cutoffSoc(const Reading& r);

enum class Grid : uint8_t { Unknown, On, Off };

// Світло є/немає з підтвердженням confirmReads зчитуваннями поспіль + середнє споживання за avgSeconds.
class Tracker {
 public:
  Tracker(float onVolts, int confirmReads, int avgSeconds)
      : onVolts_(onVolts), confirm_(confirmReads), avgSeconds_(avgSeconds) {}

  // now — unix-час (або 0, якщо годинника ще немає: тоді «з HH:MM» не запам'ятовується).
  void update(const Reading& r, int64_t now);
  // Що показувати: підтверджений стан, а до першого підтвердження — останнє зчитування.
  Grid shown() const { return confirmed_ != Grid::Unknown ? confirmed_ : last_; }
  // Відколи поточний стан (unix), 0 — невідомо (стан був ще до запуску табло).
  int64_t since() const { return since_; }
  float loadAvg() const;
  // Годин до відсічки або < 0, якщо споживання ≤ 30 Вт («надовго»).
  float hoursLeft(const Reading& r) const;

 private:
  static constexpr int HIST = 40;
  float onVolts_;
  int confirm_, avgSeconds_;
  Grid last_ = Grid::Unknown, confirmed_ = Grid::Unknown;
  int streak_ = 0;
  int64_t since_ = 0;
  int64_t histT_[HIST] = {};
  int histW_[HIST] = {};
  int histN_ = 0, histHead_ = 0;
};

}  // namespace inv
