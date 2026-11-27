# Plan: standalone Deye mini display

A separate project. Some code comes from the author's private home project (a display next to an LCD panel),
but **none of the author's data** (addresses, serial numbers, passwords, groups) may appear here: everything that
depends on the home is entered by the user on the board during setup.

## 1. What the display shows

### Main screen — 2×2 tiles (variant C) with an alert line on top

| Element | What | Source |
| --- | --- | --- |
| Top line | "Тривоги немає" (no alert) / "Повітряна тривога · з 15:41" (alert since…) + update time + Wi-Fi icon. Without a region — only the time | siren.pp.ua |
| Grid | "Є" (white tile, yellow bolt) / "Немає" (black) + "since 09:30 · 234 V" | grid voltage from the inverter |
| Charge | total % + bar: ≥75 % green, 45–74 % yellow, <45 % red | inverter |
| Consumption | W now + "today … kWh" | inverter |
| Battery | grid on: "+42 W" (green, charging) / "−… W" (red, discharging). Grid off: "lasts ≈ 2 h" | inverter |

The screen frame shows the alert colour: grey (calm or no region), yellow, red. High contrast: black background,
2 px tile borders. Individual batteries are **not** shown — only what the inverter reports for the whole bank.

Wi-Fi icon next to the clock: white — connected, yellow — weak (< −75 dBm), grey with a slash — no data for over 2 min.

### Screen 2 (swipe right to left) — outage schedule

Up to 4 text rows starting from the current interval, e.g. "14:00–18:00": white — power on, red — power off,
"завтра" (tomorrow) note for rows on the next day. Instead of rows — only "Екстрені відключення" (emergency
outages) or "Даних немає" (no data: no schedule, no clock, or the file is older than 12 h). Nothing else.
Returns to the main screen by swipe or after 60 s.

Rules (from the home project):
- **Logger did not answer ≠ no power.** A connection failure → "no data" in grey, never "no power".
- "Grid on/off" changes only after 2 identical readings in a row (voltage spikes do not cause flicker).
- When part of the data is missing, only that part is grey.

## 2. Hardware

**NiceMCU-32S-DEV 2.8IPS** (board revision DEV2.0): ESP32-D0WD-V3, 4 MB flash, **no PSRAM**, CH340C (USB-C),
ST7789 240×320 IPS screen (SPI), CST816D touch (I²C), PWM backlight. Vendor documentation:
<https://github.com/NiceMCU-01/NiceMCU-32S-DEV_2.8IPS>.

| What | Pins |
| --- | --- |
| Screen SPI | MOSI 13, SCLK 14, CS 15, DC 12 (⚠️ strapping pin: must not be pulled high before init), 24 MHz |
| Screen and touch reset | GPIO2 (shared) |
| Backlight | GPIO25, PWM |
| Touch | SDA 33, SCL 32, INT 36, address 0x15 |
| BOOT button | GPIO0 — after start: hold 5 s = setup mode |

Known from experience:
- ESP32 Wi-Fi power saving on this board loses half the packets and adds up to 400 ms latency — it is **disabled**
  (`WiFi.setSleep(false)`).
- Some boards ship with a dead USB-UART link (no RX data at all) — check with `tools/check_board`.

## 3. Inverter: what is read

Modbus function **0x03** only (read). Writes (0x06/0x10) are forbidden in the code. The logger speaks
**Solarman V5** over TCP port **8899**; the frame carries the logger serial number.

Single-phase Deye hybrids (SG0xLP1), checked on SUN-…-SG0xLP1:

| Register | What | Units |
| --- | --- | --- |
| 0 | device type (3 = single-phase hybrid) | — |
| 84 | consumed today | ×0.1 kWh |
| 150 | grid voltage | ×0.1 V |
| 178 | home consumption | W, int16 |
| 183 | battery voltage | ×0.01 V |
| 184 | battery charge (total) | % |
| 190 | battery power, negative = charging | W, int16 |
| 204 | battery capacity from inverter settings | Ah |
| 217 | battery shutdown SOC | % |

"Grid on" — grid voltage ≥ 180 V. "Lasts ≈" = (charge − shutdown SOC) × capacity × battery voltage / average
consumption over 10 min.

⚠️ **Three-phase Deye (SG0xLP3) use a different register map** — not supported until a tester shows up.
The display detects this from register 0 and says "not supported" instead of showing nonsense.

Polling: every 30 s from :15 of each cycle, one connection at a time (loggers handle parallel clients badly);
a forced poll if more than 45 s passed since the last one; one retry after 2 s before counting a failure.

## 4. Alerts: siren.pp.ua

No token (a wrapper around Ukraine Alarm), limit 50 requests/min per IP. The display makes **1 request per minute**
and only if a region is set. Sends a `User-Agent`.

- Region tree: `GET https://siren.pp.ua/api/v3/regions` → `states[]` (29 oblasts) → `regionChildIds` (122 raions)
  → communities (1455). The list is built into the firmware (16 KB). Setup page: oblast → community search;
  a whole oblast can be chosen too.
- Alert: `GET https://siren.pp.ua/api/v3/alerts/<regionId>`. **Checked 2026-11-26:** a community request also
  returns alerts declared for its raion, i.e. parent levels are included — one request is enough.
- Request failure → "Тривога: немає даних" (no data), grey frame.

## 5. Setup on the board

First start (no saved settings), holding BOOT for 5 s, or the `setup` serial command:

1. The board starts the **Deye-Setup** access point (no password) with a captive portal; the screen shows a QR
   code for the network.
2. The phone connects and the page opens by itself. Steps:
   1. **Wi-Fi:** list of found networks (2.4 GHz only) + password; the board checks the connection.
   2. **Logger:** serial number from the sticker (10 digits). Search — §6.
   3. **Alerts:** oblast → community, or "no alerts".
   4. **Outage schedule:** "Київ" (by address: street → house → group via Yasno, requests go through the board
      since the phone has no internet in setup mode) / "Житомирщина" (group 1.1…6.2) / "Без графіка" (none).
      The Kyiv group can also be typed manually (a comma is accepted as the decimal separator).
3. "Готово" → saved to NVS → restart → normal operation.

Entering setup via BOOT keeps the saved values; once Wi-Fi connects, a saved logger is re-checked automatically so the
board shows a real check mark. Holding BOOT for 5 s again in setup mode (after releasing it) erases everything and
restarts as a new display; the setup page has "Скинути все" for the same.

Stored in NVS: Wi-Fi SSID and password, logger serial, last working logger IP, `regionId` and name, outage provider
and group, Kyiv street/house IDs and address. Nothing secret is in the repository. The Wi-Fi password in NVS is not
encrypted (as in most such devices) — the README says so.

## 6. Finding the logger

- First a Solarman UDP broadcast (port 48899, `WIFIKIT-214028-READ`): the logger may reply "IP,MAC,SN".
- Otherwise — scan the /24 subnet for hosts with TCP 8899 open (≈ 30–60 s, once), and check each candidate with
  one register read using the entered serial number.
- The found address is saved. If the logger stops answering for several cycles in a row, the search repeats
  (the router may have given it another address). During the search — "no data", never "no power".

## 7. Networking and memory

- **HTTPS** (mbedTLS, no PSRAM) needs a large contiguous heap block. Only one HTTPS connection at a time, only in
  the :20–:27 window of each 30 s cycle, one request per pass: siren → outage file → Kyiv group recheck.
- **Root certificates per source** (`src/root_ca.h`): parsing all 13 roots at once failed with -10368 (X509 alloc
  failed) and left the board with no roots at all. Now: siren.pp.ua and github.io → ISRG Root X1 + ISRG Root YR
  (Let's Encrypt); app.yasno.ua → GlobalSign Root R46.
- **LVGL 9.5** with its own pool (48 KB on the board), allocated once; screens are created lazily and only one work
  screen exists at a time — this keeps the heap unfragmented for TLS.
- **Connection watchdog:** nothing succeeded for 90 s → ping the gateway; two failed pings → Wi-Fi reconnect;
  three reconnects without success → restart.
- On a TLS verification failure (-9984) the board logs a diagnostic (chain, verify flags, time) at most 3 times
  per boot.

## 8. Outage schedules

The normalizer in `outages/` (see [`outages/README.md`](../outages/README.md)) runs on GitHub Actions at :07 and :37
and publishes `https://alexmovchan.github.io/deye-mini-display/outages/<provider>/<group>.json`. The display fetches
its file every 30 min (retry in 5 min on failure); for Kyiv it rechecks the group by address once a day.

Providers: Yasno (Kyiv, public JSON API) and Zhytomyroblenergo (HTML table on ztoe.com.ua). Other regions — when a
specific city and a source appear.

## 9. Status (2026-11-26)

| # | Stage | Status |
| --- | --- | --- |
| 0 | Skeleton | ✅ PlatformIO, `lib/core` (Solarman V5 frames, registers, confirmed grid state, "lasts ≈", siren.pp.ua, logger discovery, tile model, outage schedule), 19 PC tests (`pio test -e native`), frames checked against pysolarmanv5 |
| 1 | Screens | ✅ tiles, messages ("searching for logger", "not supported"), setup screen with QR, schedule screen; PC render (`pio test -e render`, `docs/design/`) |
| 2 | Setup | ✅ on hardware: Deye-Setup + captive portal, 4 steps, NVS |
| 3 | Logger | ✅ on hardware |
| 4 | Alerts | ✅ on hardware (since 0.2.6 — per-source root certificates) |
| 5 | Installation | ✅ releases with full and update images, flashing from the browser via esptool-js |
| 6 | Outage schedules | ✅ normalizer on GitHub Pages; Zhytomyr checked against the site on 2026-11-26 |
| 7 | Reliability | in progress: a day unattended; logger and Wi-Fi disappearing and coming back |

## 10. Open questions

1. **Scheduled GitHub Actions runs are sparse.** The `schedule` trigger (every 30 min) did not fire on the first day,
   then fired only twice in the next ~24 h (2026-11-27 02:23 and 11:22 UTC). On hold — monitoring with
   `gh run list --workflow outages.yml` (the event column shows `schedule` vs `workflow_dispatch`).
   If it stays this sparse, the planned fix is a Cloudflare Worker with a Cron Trigger that only calls
   `workflow_dispatch` every 30 min (a fine-grained token limited to Actions on this repo, stored as a Worker
   secret). The Python adapters, GitHub Pages URLs and the firmware stay unchanged; the GitHub `schedule` stays as
   a fallback.
2. **Kyiv address lookup on the board** — the Yasno certificate (Imperva, 3.8 KB, 111 SANs, RSA-4096 root) may not
   fit in memory; verify with per-source roots.
3. **Loggers with port 8899 closed.** Some new Solarman firmware disables local access — the display cannot work
   then; describe in the README how to check in advance.
4. **Three-phase inverters** — need a tester and a register map.
5. **Bigger font** for the schedule rows (needs Inter TTF and `lv_font_conv`).
6. **Networks:** 2.4 GHz only; a logger in another subnet (repeater) is not found by the scan.
7. **Firmware updates:** USB only for now; auto-update from GitHub Releases — later.

## 11. Out of scope (for now)

Individual batteries, three-phase inverters, a Telegram bot, an alert map.

## 12. Design

Screens are rendered on the PC from the same LVGL code into `docs/design/` (`pio test -e render`).
