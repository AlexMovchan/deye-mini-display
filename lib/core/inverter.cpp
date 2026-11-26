#include "inverter.h"

namespace inv {

bool supported(uint16_t t) { return t == TYPE_SINGLE_PHASE_HYBRID; }

const char* deviceTypeName(uint16_t t) {
  switch (t) {
    case 2: return "мережевий інвертор";
    case 3: return "однофазний гібрид";
    case 4: return "мікроінвертор";
    case 5: return "трифазний гібрид";
    case 6: return "трифазний гібрид (високовольтний)";
    default: return "невідомий тип";
  }
}

static int s16(uint16_t v) { return (int16_t)v; }

Reading decode(uint16_t dayLoad, const uint16_t* live) {
  auto r = [&](uint16_t reg) -> uint16_t { return live[reg - LIVE_FIRST]; };
  Reading d;
  d.soc = r(REG_SOC);
  if (d.soc > 100) d.soc = 100;
  d.battV = r(REG_BATT_V) / 100.0f;
  d.battW = s16(r(REG_BATT_W));
  int load = s16(r(REG_LOAD_W));
  d.loadW = load > 0 ? load : 0;
  d.gridV = r(REG_GRID_V) / 10.0f;
  d.gridW = s16(r(REG_GRID_W));
  d.dayKwh = dayLoad / 10.0f;
  d.battAh = r(REG_BATT_AH);
  d.shutdownSoc = r(REG_SHUTDOWN_SOC);
  return d;
}

float batteryKwh(const Reading& r) {
  float v = r.battV >= 40 && r.battV <= 60 ? r.battV : 51.2f;
  return r.battAh * v / 1000.0f;
}

int cutoffSoc(const Reading& r) { return r.shutdownSoc >= 1 && r.shutdownSoc <= 90 ? r.shutdownSoc : 20; }

void Tracker::update(const Reading& r, int64_t now) {
  histT_[histHead_] = now;
  histW_[histHead_] = r.loadW;
  histHead_ = (histHead_ + 1) % HIST;
  if (histN_ < HIST) histN_++;

  Grid g = r.gridV > onVolts_ ? Grid::On : Grid::Off;
  streak_ = g == last_ ? streak_ + 1 : 1;
  last_ = g;
  if (streak_ >= confirm_ && g != confirmed_) {
    Grid before = confirmed_;
    confirmed_ = g;
    // Момент зміни відомий, лише якщо табло бачило попередній стан і має годинник
    since_ = before != Grid::Unknown ? now : 0;
  }
}

float Tracker::loadAvg() const {
  if (histN_ == 0) return 0;
  int64_t newest = histT_[(histHead_ + HIST - 1) % HIST];
  long sum = 0;
  int n = 0;
  for (int i = 0; i < histN_; i++) {
    int idx = (histHead_ + HIST - 1 - i) % HIST;
    if (newest > 0 && histT_[idx] < newest - avgSeconds_) break;
    sum += histW_[idx];
    n++;
  }
  return n ? (float)sum / n : 0;
}

float Tracker::hoursLeft(const Reading& r) const {
  float avg = loadAvg();
  if (avg <= 30) return -1;
  int usable = r.soc - cutoffSoc(r);
  if (usable < 0) usable = 0;
  return usable / 100.0f * batteryKwh(r) * 1000 / avg;
}

}  // namespace inv
