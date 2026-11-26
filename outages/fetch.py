"""Забрати графіки з усіх джерел і записати в спільному форматі (README.md).

    python outages/fetch.py [тека_результату] [адреса_попередньої_публікації]

Якщо джерело не вдалося, а адресу попередньої публікації задано (GitHub Pages), його файли беруться звідти —
табло й далі отримує вчорашній графік з часом оновлення, а не порожнечу.

Кожне джерело — окремо: збій одного не зупиняє інші, а його старі файли лишаються як були.
Код виходу 1, якщо не вдалося жодне джерело (щоб GitHub Actions показав помилку).
"""
import datetime
import json
import os
import urllib.request
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "adapters"))

from common import UA, write  # noqa: E402

import yasno  # noqa: E402
import ztoe  # noqa: E402

ADAPTERS = [yasno, ztoe]


def keep_previous(provider, out, base):
    """Скопіювати попередньо опубліковані файли постачальника (index.json і всі черги)."""
    def get(path):
        req = urllib.request.Request(f"{base}/{provider}/{path}", headers={"User-Agent": UA})
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.read()
    index = get("index.json")
    os.makedirs(os.path.join(out, provider), exist_ok=True)
    with open(os.path.join(out, provider, "index.json"), "wb") as f:
        f.write(index)
    for g in json.loads(index)["groups"]:
        with open(os.path.join(out, provider, g + ".json"), "wb") as f:
            f.write(get(g + ".json"))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "out")
    previous = sys.argv[2].rstrip("/") if len(sys.argv) > 2 else None
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    ok = 0
    for a in ADAPTERS:
        try:
            s = a.fetch()
            groups = write(s, out, now)
            dates = ", ".join(f"{d.date} {d.status}" for d in s.days)
            print(f"✓ {a.PROVIDER}: {len(groups)} черг; {dates}")
            ok += 1
        except Exception as e:  # noqa: BLE001 — будь-який збій одного джерела не валить інші
            print(f"✗ {a.PROVIDER}: {type(e).__name__}: {e}")
            traceback.print_exc(limit=1)
            if previous:
                try:
                    keep_previous(a.PROVIDER, out, previous)
                    print(f"  {a.PROVIDER}: лишаю попередню публікацію")
                except Exception as e2:  # noqa: BLE001
                    print(f"  {a.PROVIDER}: попередньої публікації немає ({e2})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
