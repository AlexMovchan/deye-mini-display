"""Київ — Yasno (ДТЕК Київські електромережі): публічний JSON без токена.

GET https://app.yasno.ua/api/blackout-service/public/shutdowns/regions/25/dsos/902/planned-outages
→ {"47.1": {"today": {"date": "...", "status": "ScheduleApplies", "slots": [{"start": 0, "end": 30,
  "type": "Definite"}, ...]}, "tomorrow": {...}, "updatedOn": "..."}, ...}
"Definite" — світла немає; start/end — хвилини від півночі. Перевірено на домашньому РК з жовтня 2026.
"""
import json

from common import Day, ParseError, Schedule, http_get, slots_from_minutes

PROVIDER = "yasno-kyiv"
NAME = "Київ (Yasno, ДТЕК Київські електромережі)"
URL = "https://app.yasno.ua/api/blackout-service/public/shutdowns/regions/25/dsos/902/planned-outages"

STATUS = {"ScheduleApplies": "applies", "WaitingForSchedule": "waiting", "EmergencyShutdowns": "emergency"}


def parse(body):
    data = json.loads(body)
    if not isinstance(data, dict) or not data:
        raise ParseError("не об'єкт черг")
    days = {}
    for group, rec in data.items():
        if not isinstance(rec, dict):
            continue
        for key in ("today", "tomorrow"):
            d = rec.get(key)
            if not isinstance(d, dict) or "date" not in d:
                continue
            date = d["date"][:10]
            status = STATUS.get(d.get("status"), "applies" if d.get("slots") else "waiting")
            day = days.setdefault((key, date), Day(date=date, status=status))
            if status == "applies":
                off = [(s["start"], s["end"]) for s in d.get("slots", []) if s.get("type") == "Definite"]
                day.groups[group] = slots_from_minutes(off)
    if not days:
        raise ParseError("немає жодного дня")
    order = sorted(days, key=lambda k: (k[1], k[0] != "today"))
    return Schedule(PROVIDER, NAME, [days[k] for k in order])


def fetch():
    return parse(http_get(URL))
