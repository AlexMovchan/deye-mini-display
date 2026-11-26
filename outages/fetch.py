"""Забрати графіки з усіх джерел і записати в спільному форматі (README.md).

    python outages/fetch.py [тека_результату]      # за замовчуванням outages/out

Кожне джерело — окремо: збій одного не зупиняє інші, а його старі файли лишаються як були.
Код виходу 1, якщо не вдалося жодне джерело (щоб GitHub Actions показав помилку).
"""
import datetime
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "adapters"))

from common import write  # noqa: E402

import yasno  # noqa: E402
import ztoe  # noqa: E402

ADAPTERS = [yasno, ztoe]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "out")
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
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
