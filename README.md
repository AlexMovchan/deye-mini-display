# deye-mini-display

A standalone mini display for **Deye** hybrid inverters with a **Solarman** Wi-Fi logger.
A small screen for the bedside table or the kitchen that shows what matters during power outages:

- **grid power on / off** (and since when),
- **battery charge %** and whether it is charging or discharging,
- the home's **power consumption**,
- an **air-raid alert** for your community (optional, Ukraine),
- the **power outage schedule**: the current interval as a bar at the bottom and the next ones on a second screen —
  Kyiv (by address) and Zhytomyr region (by group), optional.

No Deye/Solarman cloud, no accounts, no tokens: the display reads the logger directly on your home network
and **never writes anything to the inverter** (read-only Modbus function `0x03`).

The on-device interface is in Ukrainian.

| Grid on | Alert, grid off | Setup |
| --- | --- | --- |
| ![](docs/design/tiles_bar_on.png) | ![](docs/design/tiles_bar_off_alert.png) | ![](docs/design/setup_start.png) |

| Outage schedule (swipe) | No schedule data |
| --- | --- |
| ![](docs/design/map_both.png) | ![](docs/design/map_no_tomorrow.png) |

> Status: **0.2.9, running on real hardware.** Details: [`docs/PLAN.md`](docs/PLAN.md).

## What you need

- A Deye single-phase hybrid inverter (SG0xLP1 series) with a Solarman Wi-Fi logger answering on local port 8899.
  Three-phase models are not supported yet — the display says so instead of showing wrong numbers.
- A **NiceMCU-32S-DEV 2.8IPS** board (ESP32, 2.8″ 240×320 touch screen, USB-C).
- 2.4 GHz Wi-Fi on the same network as the logger.
- The logger serial number (the "SN" sticker on the logger), in case the display cannot find the logger by itself.

## Installation

### From the browser (Chrome or Edge)

1. Download the files from the [latest release](https://github.com/AlexMovchan/deye-mini-display/releases/latest).
2. Open <https://espressif.github.io/esptool-js/>, set Baudrate **460800**, press **Connect** and pick the
   CH340 / USB Serial port.
3. Flash one file:

   | File | Address | When |
   | --- | --- | --- |
   | `deye-mini-display-<version>.bin` | **`0x0`** | first install — settings are **erased** |
   | `deye-mini-display-<version>-update.bin` | **`0x10000`** | update — settings are **kept** |

4. Press **Program**, wait for "Leaving…", press **Disconnect** in the browser and **RST** on the board.

### With PlatformIO

```
pio run -e board -t upload
```

### First setup

1. The display shows a QR code for the **Deye-Setup** Wi-Fi network — point your phone camera at it.
2. The setup page opens by itself (or open `http://192.168.4.1`): choose your home Wi-Fi, enter the logger
   serial number, and optionally a community for alerts and an outage schedule.
3. Press "Зберегти й запустити" (save and start) — the display restarts and shows live data.

To change settings, hold the **BOOT** button for 5 seconds while the display is running. The saved values stay
filled in, and the saved logger is checked again automatically.

To erase everything and start from scratch (for example, before giving the display to someone else): in setup mode,
release BOOT and hold it for 5 seconds again, or tap "Скинути все" at the bottom of the setup page. Flashing the full
image at `0x0` erases the settings too.
Event log: `http://<display address>/log`.

The Wi-Fi password is stored unencrypted in the board's flash (as in most devices of this kind).

## How it works

- **Inverter:** Solarman V5 over TCP 8899, polled every 30 s, one connection at a time.
- **Alerts:** [siren.pp.ua](https://siren.pp.ua), at most one request per minute and only if a region is set.
- **Outage schedules:** a normalizer in [`outages/`](outages/README.md) runs on GitHub Actions every 30 minutes,
  converts each grid operator's site into one common format and publishes it on GitHub Pages. The display fetches
  only the small file for its own group. When an operator changes its site, only the adapter changes —
  no firmware update needed.

## Development

```
pio test -e native      # logic tests on the PC
pio test -e render      # render screens on the PC (images go to the folder in RENDER_DIR)
python tools/gen_web.py # after changing src/web/setup.html — regenerate src/web_assets.h
```

Some boards ship with a broken USB-UART link; check yours with `tools/check_board.bat`.

## License

MIT — see [`LICENSE`](LICENSE). The Inter font is under SIL OFL (`src/fonts/Inter-LICENSE.txt`).
