"""Спільний формат графіка (див. README.md) і допоміжне."""
import json
import os
import urllib.request
from dataclasses import dataclass, field

UA = "deye-mini-display outages (+https://github.com/AlexMovchan/deye-mini-display)"
SLOTS = 48  # півгодинки за добу


@dataclass
class Day:
    date: str            # "YYYY-MM-DD" (київська дата)
    status: str          # applies | waiting | emergency
    groups: dict = field(default_factory=dict)  # "3.2" -> рядок з 48 символів 0/1/?


@dataclass
class Schedule:
    provider: str
    name: str
    days: list           # [Day], сьогодні й (якщо є) завтра


class ParseError(Exception):
    """Відповідь не схожа на очікувану — сайт, мабуть, змінився. Старі файли не чіпаємо."""


def http_get(url, timeout=30):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept-Encoding": "identity"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def slots_from_minutes(ranges):
    """[(start, end)] у хвилинах від півночі (світла немає) → 48 символів. Півгодинку вважаємо без світла,
    якщо відключення займає її хоча б наполовину."""
    out = []
    for i in range(SLOTS):
        a, b = i * 30, i * 30 + 30
        covered = sum(max(0, min(b, e) - max(a, s)) for s, e in ranges)
        out.append("1" if covered >= 15 else "0")
    return "".join(out)


def check_day(day):
    for g, off in day.groups.items():
        if day.status == "applies" and (len(off) != SLOTS or set(off) - set("01?")):
            raise ParseError(f"{day.date} {g}: рядок «{off[:60]}»")


def write(schedule, out_dir, fetched_iso):
    for d in schedule.days:
        check_day(d)
    base = os.path.join(out_dir, schedule.provider)
    os.makedirs(base, exist_ok=True)
    groups = sorted({g for d in schedule.days for g in d.groups}, key=lambda s: [int(x) for x in s.split(".")])
    if not groups:
        raise ParseError("жодної черги")
    for g in groups:
        doc = {
            "v": 1, "provider": schedule.provider, "name": schedule.name, "group": g, "fetched": fetched_iso,
            "days": [{"date": d.date, "status": d.status, "off": d.groups.get(g, "") if d.status == "applies" else ""}
                     for d in schedule.days],
        }
        with open(os.path.join(base, g + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
    with open(os.path.join(base, "index.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"v": 1, "provider": schedule.provider, "name": schedule.name, "fetched": fetched_iso,
                   "groups": groups, "dates": [d.date for d in schedule.days]}, f, ensure_ascii=False, indent=1)
    return groups
