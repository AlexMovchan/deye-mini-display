"""Житомирщина — Житомиробленерго: HTML-сторінка, кодування windows-1251.

GET https://www.ztoe.com.ua/unhooking-search.php → після «Графіки погодинних відключень» — по таблиці на день:
заголовок дня `<td ... colspan="48"><b>26.11.2026</b></td>`, далі рядки черг: клітинка «1» (rowspan=2),
підчерга «1.1», порожня розділова клітинка й 48 клітинок по 30 хв. Колір фону: #ff3333 — світла немає,
#ffffff / white — є. Інший колір — «?» (табло покаже як невідоме). Розібрано 26.11.2026.
"""
import re

from common import SLOTS, Day, ParseError, Schedule, http_get

PROVIDER = "ztoe"
NAME = "Житомирщина (Житомиробленерго)"
URL = "https://www.ztoe.com.ua/unhooking-search.php"

OFF = {"#ff3333"}
ON = {"#ffffff", "white", "#fff"}

DAY_RE = re.compile(r'colspan="48"[^>]*>\s*<b[^>]*>\s*(\d{2})\.(\d{2})\.(\d{4})\s*<', re.I)
ROW_RE = re.compile(r"<tr[^>]*>(.*?)</tr>", re.S | re.I)
TD_RE = re.compile(r"<td([^>]*)>(.*?)</td>", re.S | re.I)
BG_RE = re.compile(r"background(?:-color)?\s*:\s*([^;\"']+)", re.I)
GROUP_RE = re.compile(r"^\s*(\d{1,2}\.\d)\s*$")


def _text(html):
    return re.sub(r"<[^>]+>", "", html).replace("&nbsp;", " ").strip()


def parse(body):
    html = body.decode("cp1251", errors="replace") if isinstance(body, bytes) else body
    start = html.find("Графіки погодинних відключень")
    if start < 0:
        raise ParseError("немає розділу «Графіки погодинних відключень»")
    html = html[start:]
    heads = list(DAY_RE.finditer(html))
    if not heads:
        raise ParseError("немає жодної дати дня")
    days = []
    for i, m in enumerate(heads):
        date = f"{m.group(3)}-{m.group(2)}-{m.group(1)}"
        end = heads[i + 1].start() if i + 1 < len(heads) else html.find("</table>", m.end())
        chunk = html[m.end(): end if end > 0 else None]
        day = Day(date=date, status="applies")
        for row in ROW_RE.findall(chunk):
            tds = TD_RE.findall(row)
            group = next((GROUP_RE.match(_text(c)).group(1) for _, c in tds if GROUP_RE.match(_text(c))), None)
            if not group:
                continue
            cells = [BG_RE.search(a) for a, _ in tds]
            colors = [c.group(1).strip().lower() for c in cells if c]
            slots = colors[-SLOTS:]
            if len(slots) != SLOTS:
                raise ParseError(f"{date} {group}: {len(slots)} клітинок замість {SLOTS}")
            day.groups[group] = "".join("1" if c in OFF else "0" if c in ON else "?" for c in slots)
        if not day.groups:
            raise ParseError(f"{date}: немає жодної черги")
        days.append(day)
    return Schedule(PROVIDER, NAME, days)


def fetch():
    return parse(http_get(URL))
