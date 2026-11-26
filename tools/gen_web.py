"""Вбудовані в прошивку файли сторінки налаштування → src/web_assets.h (gzip-масиви).

    python tools/gen_web.py                 # список регіонів — з siren.pp.ua
    python tools/gen_web.py regions.json    # або з раніше збереженої відповіді /api/v3/regions

- src/web/setup.html → SETUP_HTML_GZ
- дерево регіонів siren.pp.ua (області й громади) → REGIONS_TXT_GZ, рядки:
    S|<id>|<назва області або міста-регіону>
    C|<id>|<назва громади>|<id області>|<назва району>
Список вбудований, бо під час налаштування телефон підключений до табло й інтернету не має.
"""
import gzip
import json
import os
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
URL = "https://siren.pp.ua/api/v3/regions"


def load_regions(arg):
    if arg:
        with open(arg, encoding="utf-8") as f:
            return json.load(f)
    req = urllib.request.Request(URL, headers={"User-Agent": "deye-mini-display build"})
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.load(r)


def regions_text(d):
    rows = []
    for st in d["states"]:
        if "Тестов" in st["regionName"]:
            continue
        rows.append(f"S|{st['regionId']}|{st['regionName']}")
        for ds in st.get("regionChildIds") or []:
            for c in ds.get("regionChildIds") or []:
                if c.get("regionType") == "Community":
                    rows.append(f"C|{c['regionId']}|{c['regionName']}|{st['regionId']}|{ds['regionName']}")
    for r in rows:
        assert "\n" not in r
    return "\n".join(rows) + "\n"


def c_array(name, data):
    lines = [f"static const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(data), 20):
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 20]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    html = open(os.path.join(ROOT, "src", "web", "setup.html"), "rb").read()
    regs = regions_text(load_regions(sys.argv[1] if len(sys.argv) > 1 else None)).encode("utf-8")
    html_gz = gzip.compress(html, 9, mtime=0)
    regs_gz = gzip.compress(regs, 9, mtime=0)
    out = os.path.join(ROOT, "src", "web_assets.h")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("#pragma once\n// Згенеровано tools/gen_web.py — не редагувати вручну.\n#include <Arduino.h>\n\n")
        f.write(c_array("SETUP_HTML_GZ", html_gz) + "\n\n")
        f.write(c_array("REGIONS_TXT_GZ", regs_gz) + "\n")
    lines = regs.count(b"\n")
    print(f"setup.html {len(html)} → {len(html_gz)} Б; регіони {lines} рядків, {len(regs)} → {len(regs_gz)} Б")


if __name__ == "__main__":
    main()
