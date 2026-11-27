#pragma once
// Константи прошивки. Тут НЕМАЄ і не може бути жодних даних користувача (Wi-Fi, серійник, адреси):
// усе це вводиться на платі під час налаштування й лежить у NVS (src/settings.cpp).

#define FW_VERSION "0.2.8"
#define HOSTNAME "deye-mini"

// ── Налаштування: точка доступу й кнопка ──
#define SETUP_AP_SSID        "Deye-Setup"     // без пароля, лише на час налаштування
#define SETUP_BUTTON_PIN     0                // BOOT; після старту безпечно читати
#define SETUP_HOLD_MS        5000             // утримання BOOT → режим налаштування

// ── Логер Solarman ──
#define LOGGER_PORT          8899
#define MODBUS_SLAVE_ID      1
#define LOGGER_TIMEOUT_MS    5000             // на один запит
#define LOGGER_POLL_SECONDS  30
#define LOGGER_POLL_OFFSET   15               // секунда в 30-секундному циклі: :15/:45 — щоб не збігатися з іншими
                                              // пристроями, що теж читають логер (зазвичай :00/:30)
#define LOGGER_FAILS_SEARCH  4                // стільки невдалих циклів поспіль (≈ 2 хв) → шукаємо логер знову
#define LOGGER_STALE_SECONDS 90
#define NET_PROBE_SECONDS    90               // нічого не вдалось стільки → ping роутера; не відповів двічі → перепідключити Wi-Fi
#define NET_RESTART_RECONNECTS 3            // стільки перепідключень без жодного успіху → перезапуск плати               // без свіжих даних стільки → «Логер не відповідає»
#define DISCOVERY_UDP_PORT   48899
#define DISCOVERY_UDP_MSG    "WIFIKIT-214028-READ"
#define SCAN_CONNECT_MS      250              // перебір підмережі: чекати TCP 8899 на одній адресі

// ── Світло ──
#define GRID_ON_VOLTS        180.0f
#define GRID_CONFIRM_READS   2                // світло є/немає змінюється після стількох однакових зчитувань
#define LOAD_AVG_SECONDS     600              // середнє споживання для «вистачить ≈»

// ── Тривоги: siren.pp.ua ──
#define SIREN_URL_PREFIX     "https://siren.pp.ua/api/v3/alerts/"
#define SIREN_POLL_SECONDS   60               // 1 запит на хвилину (ліміт сервісу — 50/хв з IP)
#define SIREN_TIMEOUT_MS     4000             // короткий: зависле HTTPS не має з'їдати час логера
#define SIREN_STALE_SECONDS  300              // без відповіді стільки → «Тривога: немає даних»
#define HTTP_TIMEOUT_MS      8000

// ── Графік відключень: файл черги з GitHub Pages (нормалізатор outages/, оновлюється о :07 і :37) ──
#define OUTAGE_URL_PREFIX    "https://alexmovchan.github.io/deye-mini-display/outages/"
#define OUTAGE_POLL_MINUTES  30
#define OUTAGE_RETRY_MINUTES 5
#define OUTAGE_STALE_HOURS   12               // графік старший (нормалізатор чи інтернет мовчать) → «Даних немає»
#define KYIV_RECHECK_HOURS   24               // Київ: перевірка черги за адресою (Yasno переносить будинки)
#define HTTP_USER_AGENT      "deye-mini-display/" FW_VERSION " (+https://github.com/AlexMovchan/deye-mini-display)"

// ── Час ──
#define NTP_SERVER           "pool.ntp.org"
#define TZ_INFO              "EET-2EEST,M3.5.0/3,M10.5.0/4"   // Київ

// ── Екран ──
#define DAY_BRIGHTNESS       100              // %
#define NIGHT_BRIGHTNESS     30               // % з 00:00 до 07:00
#define NIGHT_START_HOUR     0
#define NIGHT_END_HOUR       7
#define SWIPE_MIN_PX         60               // свайп: мінімальний зсув пальця по горизонталі
#define MAP_RETURN_SECONDS   60               // екран 2 (графік) сам повертається на основний

// ── Журнал ──
#define EVENT_LOG_SIZE       50
#define LOG_HTTP_PORT        80               // у робочому режимі: http://<IP табло>/log

// ── Піни NiceMCU-32S-DEV 2.8IPS (виробник: include/nicemcu/board_config.h) ──
#define LCD_MOSI   13
#define LCD_SCLK   14
#define LCD_CS     15   // strapping (лог завантажувача) — безпечно
#define LCD_DC     12   // ⚠️ strapping: HIGH на старті → флеш 1,8 В. Не тягнути вгору до ініціалізації екрана
#define LCD_TP_RST 2    // спільне скидання екрана й сенсора
#define LCD_BL     25   // підсвітка через AO3401, ШІМ
#define LCD_PCLK_HZ (24 * 1000 * 1000)
#define TP_SDA     33
#define TP_SCL     32
#define TP_INT     36   // лише вхід
#define TP_ADDR    0x15
#define LCD_MIRROR_X 0
#define LCD_MIRROR_Y 0
#define TP_FLIP_X    0
