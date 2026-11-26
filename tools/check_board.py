"""Перевірка USB-UART плати ESP32 з CH340 (доказ браку для продавця). Нічого не записує на плату.

    1) знаходить порт CH340;
    2) перезапускає плату через лінію RTS (екран згасне й засвітиться знову) і 4 с слухає, що плата шле;
    3) просить у завантажувача чипа відповідь (esptool flash-id, лише читання);
    4) пише великий підсумок і додає все в журнал check_board.log поруч зі скриптом.

Запуск: двічі натиснути check_board.bat (або: python tools/check_board.py [COMx]).
"""
import datetime
import os
import re
import subprocess
import sys
import time

import serial
from serial.tools import list_ports

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = os.path.join(HERE, 'check_board.log')
def find_esptool():
    """esptool з PlatformIO (PLATFORMIO_CORE_DIR або ~/.platformio), інакше — встановлений модуль esptool."""
    for core in (os.environ.get('PLATFORMIO_CORE_DIR'), os.path.join(os.path.expanduser('~'), '.platformio')):
        if core:
            path = os.path.join(core, 'packages', 'tool-esptoolpy', 'esptool.py')
            if os.path.exists(path):
                return [sys.executable, path]
    return [sys.executable, '-m', 'esptool']


ESPTOOL = find_esptool()

lines = []


def say(s=''):
    print(s)
    lines.append(s)


def find_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = [p for p in list_ports.comports() if p.vid == 0x1A86]   # WCH: CH340/CH343
    return ports[0].device if ports else None


def main():
    now = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    say('=' * 64)
    say(f'ESP32 board USB-UART check / Перевірка плати      {now}')
    say('=' * 64)
    port = find_port()
    if not port:
        say('USB-UART (CH340) not found / CH340 не знайдено')
        return
    desc = next((p.description for p in list_ports.comports() if p.device == port), '')
    say(f'Port / Порт: {port}  ({desc})')

    # 1. Перезапуск через RTS (EN → 0 → 1) і прослуховування старту
    say('')
    say('[1] Reset via RTS — the screen goes dark and restarts')
    say('    Перезапуск через RTS — екран згасне й засвітиться')
    s = serial.Serial(None, 115200, timeout=0.2)
    s.port = port
    s.dtr = False
    s.rts = False
    s.open()
    s.rts = True
    time.sleep(0.15)
    s.rts = False
    t = time.time()
    buf = b''
    while time.time() - t < 4:
        buf += s.read(1024)
    s.close()
    say(f'    Bytes received from ESP32 after reset / Отримано байтів: {len(buf)}')
    boot = buf.decode('utf-8', 'replace')
    first = next((l.strip() for l in boot.splitlines() if l.strip()), '')
    if first:
        say(f'    First line / Перший рядок: {first[:60]}')

    # 2. Відповідь завантажувача чипа (лише читання)
    say('')
    say('[2] esptool flash-id (read only / лише читання)')
    r = subprocess.run(ESPTOOL + ['--chip','esp32', '--port', port, '--baud', '115200',
                        '--connect-attempts', '3', 'flash-id'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = r.stdout + r.stderr
    keep = [l.strip() for l in out.splitlines()
            if re.search(r'Chip type|Features|MAC|flash size|fatal error|Failed to connect', l)]
    for l in keep:
        say('    ' + l)
    ok = bool(re.search(r'Chip type', out))

    say('')
    say('#' * 64)
    if ok:
        say('#   RESULT: BOARD RESPONDS — OK')
        say('#   ПІДСУМОК: ПЛАТА ВІДПОВІДАЄ — СПРАВНА')
    else:
        say('#   RESULT: NO DATA FROM ESP32 — CANNOT BE FLASHED')
        say('#   ПІДСУМОК: ПЛАТА НЕ ВІДПОВІДАЄ — НЕ ПРОШИВАЄТЬСЯ')
    say('#' * 64)
    say('')


if __name__ == '__main__':
    try:
        main()
    finally:
        with open(LOG, 'a', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
        print(f'Log / Журнал: {LOG}')
