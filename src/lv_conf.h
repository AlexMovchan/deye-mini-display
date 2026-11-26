// Налаштування LVGL 9.5 для міні-табло (ESP32 без PSRAM). Решта — значення за замовчуванням LVGL.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// Пам'ять LVGL — зі звичайної купи (malloc): ~300 об'єктів двох екранів у фіксовані 40 КБ не влазили
// (LVGL при нестачі зависає в LV_ASSERT). PSRAM немає — усе у внутрішній; буфер малювання — board.cpp
// Свій запас пам'яті для LVGL (0.2.1): з системної купи LVGL брала сотні дрібних шматків, і купа дробилась —
// найбільший вільний блок падав до 12 КБ, а HTTPS (siren.pp.ua, Yasno, GitHub) потребує ~40 КБ суцільно.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#if defined(NATIVE_TEST)
#define LV_MEM_SIZE (256 * 1024)  // на ПК вказівники 8 байт — об'єкти вдвічі більші
#else
#define LV_MEM_SIZE (48 * 1024)   // плата: в пам'яті одночасно лише один екран (плитки або графік); див. ui.cpp
#endif
// Запас — одним шматком із купи на старті (поки вона ще не подрібнена), а не статичним масивом: статично 72 КБ
// не вміщаються в сегмент DRAM разом з рештою прошивки
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC malloc
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB

#if defined(NATIVE_TEST) && !defined(__ASSEMBLY__)  // lv_conf.h підключається і в асемблерні файли LVGL
#include <stdlib.h>
#define LV_ASSERT_HANDLER abort();  // рендер на ПК: помилка — одразу видно, а не нескінченний цикл
#endif

#define LV_DEF_REFR_PERIOD 33
#define LV_USE_LOG 0
#define LV_USE_OS LV_OS_NONE

// Шрифти — власні Inter з кирилицею (src/mini/fonts); вбудований — лише запасний
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// Без анімацій і непотрібних віджетів — менше флешу
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_TABLE 0
#define LV_USE_TEXTAREA 0
#define LV_USE_ROLLER 0
#define LV_USE_DROPDOWN 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_LED 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SWITCH 0
#define LV_USE_CHECKBOX 0
#define LV_USE_SPAN 0
#define LV_USE_QRCODE 1   // QR-код мережі Deye-Setup на екрані налаштування

#endif
