#include "ui.h"

#include <lvgl.h>
#include <string.h>

#include <initializer_list>

#include "config.h"

LV_FONT_DECLARE(inter_10)
LV_FONT_DECLARE(inter_11)
LV_FONT_DECLARE(inter_12)
LV_FONT_DECLARE(inter_12_semi)
LV_FONT_DECLARE(inter_14_semi)
LV_FONT_DECLARE(inter_18_semi)
LV_FONT_DECLARE(inter_28_semi)

namespace ui {

namespace {
using tiles::Rgb;
namespace col = tiles::color;

// Геометрія варіанта C: рамка 8 px, зазор 6, рядок тривоги 30 px, плитки 109×131
constexpr int M = 8, GAP = 6, TOP_H = 30;
constexpr int TILE_W = (240 - 2 * M - GAP) / 2;               // 109
constexpr int TILE_Y0 = M + TOP_H + GAP;                       // 44
constexpr int TILE_H = (320 - M - TILE_Y0 - GAP) / 2;          // 131
constexpr int TP = 10;                                         // відступ усередині плитки
constexpr Rgb SETUP_FRAME = 0x1E6FB5;

lv_color_t c(Rgb rgb) { return lv_color_hex(rgb); }

int textWidth(const char* t, const lv_font_t* f) {
  lv_point_t p;
  lv_text_get_size(&p, t, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return p.x;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, Rgb bg, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, c(bg), 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* f, Rgb color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c(color), 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_label_set_text(l, "");
  return l;
}

lv_obj_t* wrapLabel(lv_obj_t* parent, const lv_font_t* f, Rgb color, int width) {
  lv_obj_t* l = label(parent, f, color);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(l, width);
  return l;
}

void baseline(lv_obj_t* l, int y) {
  const lv_font_t* f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
  lv_obj_set_y(l, y - (f->line_height - f->base_line));
}

void setText(lv_obj_t* l, const char* t, Rgb color) {
  if (strcmp(lv_label_get_text(l), t)) lv_label_set_text(l, t);
  lv_obj_set_style_text_color(l, c(color), 0);
}

void show(lv_obj_t* o, bool v) {
  if (v) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// ⚡ — ламана 2× (22×28); з рисочкою навскіс — «світла немає»
const lv_point_precise_t BOLT[] = {{16, 0}, {2, 16}, {12, 16}, {10, 26}, {22, 10}, {12, 10}, {16, 0}};
const lv_point_precise_t SLASH[] = {{0, 2}, {22, 28}};
const lv_point_precise_t CHECK[] = {{1, 6}, {4, 9}, {10, 2}};

lv_obj_t* line(lv_obj_t* parent, const lv_point_precise_t* pts, int n, int width, Rgb color) {
  lv_obj_t* l = lv_line_create(parent);
  lv_line_set_points(l, pts, n);
  lv_obj_set_style_line_width(l, width, 0);
  lv_obj_set_style_line_color(l, c(color), 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  return l;
}

// ── Значок Wi-Fi (дві дуги й крапка; без зв'язку — сірий з помаранчевою рискою) ──
struct WifiUi {
  lv_obj_t *box, *outer, *inner, *dot, *slash;
};
const lv_point_precise_t WIFI_SLASH[] = {{1, 1}, {14, 12}};

lv_obj_t* arcPart(lv_obj_t* parent, int size, int x, int y) {
  lv_obj_t* a = lv_arc_create(parent);
  lv_obj_remove_style_all(a);
  lv_obj_set_size(a, size, size);
  lv_obj_set_pos(a, x, y);
  lv_arc_set_bg_angles(a, 0, 0);
  lv_arc_set_angles(a, 225, 315);
  lv_obj_set_style_arc_width(a, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  return a;
}

void createWifi(WifiUi& w, lv_obj_t* parent) {
  w.box = box(parent, 0, 0, 16, 13, col::CARD, 0);
  lv_obj_set_style_bg_opa(w.box, LV_OPA_TRANSP, 0);
  w.outer = arcPart(w.box, 18, -1, 1);
  w.inner = arcPart(w.box, 10, 3, 5);
  w.dot = box(w.box, 6, 9, 4, 4, col::TEXT, 2);
  w.slash = line(w.box, WIFI_SLASH, 2, 2, col::WARN);
}

void applyWifi(WifiUi& w, tiles::Wifi s, int right, int top) {
  Rgb color = s == tiles::Wifi::Ok ? col::TEXT : s == tiles::Wifi::Weak ? col::ALERT_YELLOW : col::STALE;
  lv_obj_set_style_arc_color(w.outer, c(s == tiles::Wifi::Weak ? col::STALE : color), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(w.inner, c(color), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(w.dot, c(color), 0);
  show(w.slash, s == tiles::Wifi::Off);
  lv_obj_set_pos(w.box, right - 16, top);
}

// ── Основний екран ──
struct TileUi {
  lv_obj_t *box, *title, *boltBg, *bolt, *slash, *big, *unit, *text, *sub, *bar, *barFill;
};

struct MainUi {
  lv_obj_t *scr, *topBox, *top, *clock;
  WifiUi wifi;
  TileUi t[4];
  lv_obj_t *msgBox, *msgTitle, *msgText;
} S;

void createTile(TileUi& u, int x, int y) {
  u.box = box(S.scr, x, y, TILE_W, TILE_H, col::CARD, 10);
  u.title = label(u.box, &inter_14_semi, col::MUTED);
  lv_obj_set_x(u.title, TP);
  baseline(u.title, 23);
  u.boltBg = line(u.box, BOLT, 7, 4, col::TEXT_DARK);
  u.bolt = line(u.box, BOLT, 7, 2, col::BOLT);
  u.slash = line(u.box, SLASH, 2, 2, col::UNIT);
  for (lv_obj_t* o : {u.boltBg, u.bolt, u.slash}) lv_obj_set_pos(o, TP, 34);
  u.big = label(u.box, &inter_28_semi, col::TEXT);
  lv_obj_set_x(u.big, TP);
  baseline(u.big, 84);
  u.unit = label(u.box, &inter_14_semi, col::UNIT);
  baseline(u.unit, 84);
  u.text = label(u.box, &inter_18_semi, col::TEXT);
  lv_obj_set_x(u.text, TP);
  baseline(u.text, 84);
  u.bar = box(u.box, TP, 96, TILE_W - 2 * TP, 8, col::CARD_OFF, 4);
  lv_obj_set_style_clip_corner(u.bar, true, 0);
  u.barFill = box(u.bar, 0, 0, 10, 8, col::SOC_GREEN, 0);
  u.sub = wrapLabel(u.box, &inter_12, col::MUTED, TILE_W - 2 * TP);
  lv_obj_set_x(u.sub, TP);
  lv_obj_set_y(u.sub, 106);
}

void applyTile(TileUi& u, const tiles::Tile& t) {
  lv_obj_set_style_bg_color(u.box, c(t.bg), 0);
  // Кожна плитка з обвідкою: на чорному тлі межі плиток видно й під кутом
  lv_obj_set_style_border_width(u.box, 2, 0);  // однакова товщина — вміст плиток не зсувається
  lv_obj_set_style_border_color(u.box, c(t.border ? t.border : t.bg == col::CARD_LIGHT ? col::CARD_LIGHT : col::CARD_EDGE), 0);
  setText(u.title, t.title, t.titleColor);
  show(u.boltBg, t.icon == tiles::Icon::BoltOn);
  show(u.bolt, t.icon != tiles::Icon::None);
  show(u.slash, t.icon == tiles::Icon::BoltOff);
  lv_obj_set_style_line_color(u.bolt, c(t.icon == tiles::Icon::BoltOn ? col::BOLT : col::UNIT), 0);

  bool digits = *t.big;
  show(u.big, digits);
  show(u.unit, digits && *t.unit);
  show(u.text, !digits && *t.text);
  if (digits) {
    setText(u.big, t.big, t.valueColor);
    setText(u.unit, t.unit, t.unitColor);
    lv_obj_set_x(u.unit, TP + textWidth(t.big, &inter_28_semi) + 3);
  } else {
    setText(u.text, t.text, t.valueColor);
  }
  show(u.bar, t.bar);
  if (t.bar) {
    float f = t.barFrac < 0 ? 0 : t.barFrac > 1 ? 1 : t.barFrac;
    int w = (int)((TILE_W - 2 * TP) * f + 0.5f);
    show(u.barFill, w > 0);
    lv_obj_set_width(u.barFill, w > 0 ? w : 1);
    lv_obj_set_style_bg_color(u.barFill, c(t.barColor), 0);
  }
  setText(u.sub, t.sub, t.subColor);
  // Текст під смужкою або під значенням
  lv_obj_set_y(u.sub, t.bar ? 107 : 96);
}

void createMain() {
  S.scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(S.scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(S.scr, c(col::FRAME_CALM), 0);
  S.topBox = box(S.scr, M, M, 240 - 2 * M, TOP_H, col::CARD, 8);
  lv_obj_set_style_border_width(S.topBox, 2, 0);
  lv_obj_set_style_border_color(S.topBox, c(col::CARD_EDGE), 0);
  S.top = label(S.topBox, &inter_14_semi, col::MUTED);
  lv_obj_set_width(S.top, 240 - 2 * M - 26 - 44 - 22);
  lv_obj_set_x(S.top, 10);
  baseline(S.top, 20);
  S.clock = label(S.topBox, &inter_14_semi, col::TEXT);
  lv_obj_set_width(S.clock, 44);
  lv_obj_set_style_text_align(S.clock, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_x(S.clock, 240 - 2 * M - 10 - 44);
  baseline(S.clock, 20);
  createWifi(S.wifi, S.topBox);
  int xs[2] = {M, M + TILE_W + GAP}, ys[2] = {TILE_Y0, TILE_Y0 + TILE_H + GAP};
  for (int i = 0; i < 4; i++) createTile(S.t[i], xs[i % 2], ys[i / 2]);
  S.msgBox = box(S.scr, M, TILE_Y0, 240 - 2 * M, 320 - M - TILE_Y0, col::CARD, 10);
  S.msgTitle = wrapLabel(S.msgBox, &inter_18_semi, col::TEXT, 240 - 2 * M - 28);
  lv_obj_set_pos(S.msgTitle, 14, 20);
  S.msgText = wrapLabel(S.msgBox, &inter_12, col::MUTED, 240 - 2 * M - 28);
  lv_obj_set_pos(S.msgText, 14, 76);
}

// ── Екран 2: графік відключень по годинах ──
// Сьогодні — 24 клітинки (4×6) з половинками по 30 хв: біла — світло є, чорна — немає; поточна година в жовтій
// рамці 3 px із темною смужкою всередині. Завтра — смужка з 48 півгодинок (на 24 клітинки місця немає).
constexpr int CARD_W = 240 - 2 * M;
constexpr int CELL_W = 49, CELL_H = 26, CELL_GAP = 4;
constexpr int STRIP_X = 10, STRIP_W = 200, STRIP_H = 14;

struct CellUi {
  lv_obj_t *box, *left, *right, *pill, *text, *boltBg, *bolt, *slash, *ring;
};
struct MapUi {
  lv_obj_t *scr, *card, *title, *clock, *todayTitle, *todayNote, *sep, *tomorrowTitle, *tomorrowNote, *strip, *updated;
  lv_obj_t* stripSeg[48];
  lv_obj_t* stripLbl[5];
  WifiUi wifi;
  CellUi cells[24];
} G;

const lv_point_precise_t CELL_BOLT[] = {{8, 0}, {1, 8}, {6, 8}, {5, 13}, {11, 5}, {6, 5}, {8, 0}};
const lv_point_precise_t CELL_SLASH[] = {{0, 1}, {10, 12}};

lv_obj_t* noteBox(lv_obj_t* parent, int y, int h) {
  lv_obj_t* b = box(parent, 8, y, CARD_W - 16, h, col::CARD, 6);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, c(col::BORDER), 0);
  lv_obj_t* l = label(b, &inter_12, col::MUTED);
  lv_obj_center(l);
  return b;
}

void createCell(CellUi& u, lv_obj_t* parent, int x, int y) {
  u.box = box(parent, x, y, CELL_W, CELL_H, col::CARD_OFF, 6);
  lv_obj_set_style_border_width(u.box, 1, 0);
  lv_obj_set_style_clip_corner(u.box, true, 0);
  u.left = box(u.box, 0, 0, CELL_W / 2, CELL_H, col::CARD_LIGHT, 0);
  u.right = box(u.box, CELL_W / 2, 0, CELL_W - CELL_W / 2, CELL_H, col::CARD_LIGHT, 0);
  u.pill = box(u.box, 0, 3, 10, CELL_H - 8, col::CARD, 4);
  u.text = label(u.box, &inter_10, col::TEXT);
  u.boltBg = line(u.box, CELL_BOLT, 7, 3, col::TEXT_DARK);
  u.bolt = line(u.box, CELL_BOLT, 7, 1, col::BOLT);
  u.slash = line(u.box, CELL_SLASH, 2, 1, col::UNIT);
  u.ring = box(u.box, 3, 3, CELL_W - 6, CELL_H - 6, col::CARD, 3);
  lv_obj_set_style_bg_opa(u.ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(u.ring, 1, 0);
  lv_obj_set_style_border_color(u.ring, c(col::CARD), 0);
}

void applyCell(CellUi& u, const outage::Cell& cell) {
  bool full = cell.first && cell.second, none = !cell.first && !cell.second, half = !full && !none;
  // Дочірні об'єкти рахуються від внутрішнього краю рамки: рамка «зараз» 3 px замість 1 зсуває їх на 2 px
  const int off = cell.now ? -2 : 0;
  lv_obj_set_style_border_color(u.box, c(cell.now ? col::FRAME_YELLOW : full ? col::CARD_LIGHT : col::CARD_EDGE), 0);
  lv_obj_set_style_border_width(u.box, cell.now ? 3 : 1, 0);
  show(u.ring, cell.now);
  show(u.left, cell.first);
  show(u.right, cell.second);
  lv_obj_set_pos(u.left, off, off);
  lv_obj_set_pos(u.right, CELL_W / 2 + off, off);
  lv_obj_set_pos(u.ring, 3 + off, 3 + off);
  lv_obj_set_style_line_color(u.bolt, c(full ? col::BOLT : col::UNIT), 0);
  show(u.boltBg, full);
  show(u.bolt, full || none);
  show(u.slash, none);
  setText(u.text, cell.hour, full ? col::TEXT_DARK : half ? col::TEXT : col::UNIT);
  int tw = textWidth(cell.hour, &inter_10);
  int iw = (full || none) ? 14 : 0;
  int x0 = (CELL_W - tw - iw) / 2;
  lv_obj_set_pos(u.boltBg, x0 + off, 6 + off);
  lv_obj_set_pos(u.bolt, x0 + off, 6 + off);
  lv_obj_set_pos(u.slash, x0 + off, 6 + off);
  lv_obj_set_x(u.text, x0 + iw + off);
  baseline(u.text, 17 + off);
  show(u.pill, half);
  if (half) {
    lv_obj_set_pos(u.pill, x0 - 3 + off, 3 + off);
    lv_obj_set_width(u.pill, tw + 6);
  }
}

void createMap() {
  G.scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(G.scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(G.scr, c(col::FRAME_CALM), 0);
  G.card = box(G.scr, M, M, CARD_W, 320 - 2 * M, col::CARD, 10);
  lv_obj_set_style_border_width(G.card, 2, 0);
  lv_obj_set_style_border_color(G.card, c(col::CARD_EDGE), 0);
  G.title = label(G.card, &inter_14_semi, col::TEXT);
  lv_obj_set_x(G.title, 10);
  baseline(G.title, 20);
  G.clock = label(G.card, &inter_14_semi, col::TEXT);
  lv_obj_set_width(G.clock, 44);
  lv_obj_set_style_text_align(G.clock, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_x(G.clock, CARD_W - 4 - 10 - 44);
  baseline(G.clock, 20);
  createWifi(G.wifi, G.card);
  G.todayTitle = label(G.card, &inter_12_semi, col::TEXT);
  lv_obj_set_x(G.todayTitle, 10);
  baseline(G.todayTitle, 42);
  for (int h = 0; h < 24; h++)
    createCell(G.cells[h], G.card, 6 + (h % 4) * (CELL_W + CELL_GAP), 48 + (h / 4) * (CELL_H + CELL_GAP));
  G.todayNote = noteBox(G.card, 48, 6 * (CELL_H + CELL_GAP) - CELL_GAP);
  G.sep = box(G.card, 6, 232, CARD_W - 16, 2, col::BORDER, 1);
  G.tomorrowTitle = label(G.card, &inter_12_semi, col::TEXT);
  lv_obj_set_x(G.tomorrowTitle, 10);
  baseline(G.tomorrowTitle, 252);
  G.strip = box(G.card, STRIP_X, 258, STRIP_W, STRIP_H, col::CARD_OFF, 3);
  lv_obj_set_style_clip_corner(G.strip, true, 0);
  for (int i = 0; i < 48; i++) {
    int x0 = i * STRIP_W / 48, x1 = (i + 1) * STRIP_W / 48;
    G.stripSeg[i] = box(G.strip, x0, 0, x1 - x0, STRIP_H, col::CARD_LIGHT, 0);
  }
  const char* ticks[5] = {"00", "06", "12", "18", "24"};
  for (int i = 0; i < 5; i++) {
    G.stripLbl[i] = label(G.card, &inter_10, col::MUTED);
    lv_label_set_text(G.stripLbl[i], ticks[i]);
    int x = STRIP_X + i * STRIP_W / 4 - textWidth(ticks[i], &inter_10) / 2;
    lv_obj_set_x(G.stripLbl[i], x < 4 ? 4 : x);
    baseline(G.stripLbl[i], 284);
  }
  G.tomorrowNote = noteBox(G.card, 258, 30);
  G.updated = label(G.card, &inter_11, col::DIM);
  lv_obj_set_width(G.updated, CARD_W - 24);
  lv_obj_set_style_text_align(G.updated, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_x(G.updated, 10);
  baseline(G.updated, 298);
}

// ── Налаштування ──
struct StepUi {
  lv_obj_t *dot, *mark, *text;
};
struct SetupUi {
  lv_obj_t *scr, *card, *title, *hint, *qr, *net, *note;
  StepUi st[4];
} P;

void createSetup() {
  P.scr = lv_obj_create(nullptr);
  lv_obj_remove_flag(P.scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(P.scr, c(SETUP_FRAME), 0);
  P.card = box(P.scr, M, M, 240 - 2 * M, 320 - 2 * M, col::CARD, 10);
  const int W = 240 - 2 * M - 24;
  P.title = label(P.card, &inter_14_semi, col::TEXT);
  lv_obj_set_x(P.title, 12);
  baseline(P.title, 24);
  lv_label_set_text(P.title, "Налаштування");
  P.hint = wrapLabel(P.card, &inter_11, col::MUTED, W);
  lv_obj_set_pos(P.hint, 12, 32);
  lv_label_set_text(P.hint, "Наведіть камеру телефона на код, щоб підключитися до табло");
  // QR-код мережі: стандартний формат Wi-Fi, телефон підключається сам
  P.qr = lv_qrcode_create(P.card);
  lv_qrcode_set_size(P.qr, 100);
  lv_qrcode_set_dark_color(P.qr, lv_color_black());
  lv_qrcode_set_light_color(P.qr, lv_color_white());
  const char* data = "WIFI:T:nopass;S:" SETUP_AP_SSID ";;";
  lv_qrcode_update(P.qr, data, strlen(data));
  lv_obj_set_style_border_color(P.qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(P.qr, 6, 0);
  lv_obj_set_pos(P.qr, (240 - 2 * M - 112) / 2, 64);
  P.net = label(P.card, &inter_11, col::MUTED);
  lv_label_set_text(P.net, "Мережа " SETUP_AP_SSID " · без пароля");
  lv_obj_set_width(P.net, W);
  lv_obj_set_style_text_align(P.net, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_x(P.net, 12);
  baseline(P.net, 194);
  for (int i = 0; i < 4; i++) {
    StepUi& s = P.st[i];
    int y = 198 + i * 19;
    s.dot = box(P.card, 12, y, 16, 16, col::CARD, 8);
    lv_obj_set_style_border_width(s.dot, 2, 0);
    s.mark = line(s.dot, CHECK, 3, 2, col::TEXT_DARK);  // галочка (у шрифті символу ✓ немає)
    lv_obj_set_pos(s.mark, 0, 0);
    s.text = label(P.card, &inter_12, col::TEXT);
    lv_obj_set_width(s.text, W - 24);
    lv_obj_set_x(s.text, 36);
    baseline(s.text, y + 13);
  }
  P.note = wrapLabel(P.card, &inter_10, col::DIM, W);
  lv_obj_set_pos(P.note, 12, 280);
}

void applyStep(StepUi& s, const Step& st) {
  Rgb ring = col::BORDER, fill = col::CARD, text = col::DIM;
  bool mark = false;
  switch (st.state) {
    case Step::Pending: break;
    case Step::Active: ring = col::ALERT_YELLOW; text = col::TEXT; break;
    case Step::Done: ring = fill = col::SOC_GREEN; text = col::TEXT; mark = true; break;
    case Step::Error: ring = fill = col::WARN; text = col::WARN; break;
  }
  lv_obj_set_style_border_color(s.dot, c(ring), 0);
  lv_obj_set_style_bg_color(s.dot, c(fill), 0);
  show(s.mark, mark);
  setText(s.text, st.text, text);
}
}  // namespace

void begin() {
  createMain();
  createMap();
  createSetup();
  lv_screen_load(S.scr);
}

void showTiles(const tiles::Model& m) {
  lv_obj_set_style_bg_color(S.scr, c(m.frame), 0);
  // Довга назва тривоги («Дронова небезпека · з 10:02») — дрібнішим шрифтом, щоб влізла поруч із годинником
  constexpr int TOP_W = 240 - 2 * M - 26 - 44 - 22;  // ширина мітки (до розкладки LVGL get_width ще 0)
  const lv_font_t* tf = textWidth(m.top, &inter_14_semi) <= TOP_W   ? &inter_14_semi
                        : textWidth(m.top, &inter_12_semi) <= TOP_W ? &inter_12_semi
                                                                    : &inter_11;
  if (lv_obj_get_style_text_font(S.top, LV_PART_MAIN) != tf) {
    lv_obj_set_style_text_font(S.top, tf, 0);
    baseline(S.top, 20);
  }
  setText(S.top, m.top, m.topColor);
  setText(S.clock, m.clock, col::TEXT);
  // Значок — ліворуч від годинника (годинник вирівняний праворуч)
  applyWifi(S.wifi, m.wifi, 240 - 2 * M - 10 - textWidth(m.clock, &inter_14_semi) - 6 - 4, 6);
  const tiles::Tile* ts[4] = {&m.light, &m.soc, &m.load, &m.batt};
  for (int i = 0; i < 4; i++) {
    show(S.t[i].box, !m.message);
    if (!m.message) applyTile(S.t[i], *ts[i]);
  }
  show(S.msgBox, m.message);
  if (m.message) {
    setText(S.msgTitle, m.msgTitle, m.msgTitleColor);
    setText(S.msgText, m.msgText, col::MUTED);
  }
  if (lv_screen_active() != S.scr) lv_screen_load(S.scr);
}

void showMap(const outage::MapModel& m, tiles::Wifi wifi) {
  lv_obj_set_style_bg_color(G.scr, c(m.frame), 0);
  setText(G.title, m.title, col::TEXT);
  setText(G.clock, m.clock, col::TEXT);
  applyWifi(G.wifi, wifi, CARD_W - 4 - 10 - textWidth(m.clock, &inter_14_semi) - 6, 6);
  setText(G.todayTitle, m.today.title, col::TEXT);
  for (int h = 0; h < 24; h++) {
    show(G.cells[h].box, m.today.cells);
    if (m.today.cells) applyCell(G.cells[h], m.today.c[h]);
  }
  show(G.todayNote, !m.today.cells);
  setText(lv_obj_get_child(G.todayNote, 0), m.today.note, col::MUTED);
  setText(G.tomorrowTitle, m.tomorrow.title, col::TEXT);
  show(G.strip, m.tomorrow.cells);
  for (int i = 0; i < 5; i++) show(G.stripLbl[i], m.tomorrow.cells);
  if (m.tomorrow.cells)
    for (int i = 0; i < 48; i++) {
      bool light = i % 2 ? m.tomorrow.c[i / 2].second : m.tomorrow.c[i / 2].first;
      lv_obj_set_style_bg_color(G.stripSeg[i], c(light ? col::CARD_LIGHT : col::CARD_OFF), 0);
    }
  show(G.tomorrowNote, !m.tomorrow.cells);
  setText(lv_obj_get_child(G.tomorrowNote, 0), m.tomorrow.note, col::MUTED);
  setText(G.updated, m.updated, m.updatedColor);
  if (lv_screen_active() != G.scr) lv_screen_load(G.scr);
}

void showSetup(const SetupView& v) {
  applyStep(P.st[0], v.wifi);
  applyStep(P.st[1], v.logger);
  applyStep(P.st[2], v.region);
  applyStep(P.st[3], v.outage);
  setText(P.note, v.note, col::DIM);
  if (lv_screen_active() != P.scr) lv_screen_load(P.scr);
}

}  // namespace ui
