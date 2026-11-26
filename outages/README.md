# Outage schedule normalizer

The display knows nothing about how grid operators' websites are built. Every 30 minutes GitHub Actions runs
`outages/fetch.py`: each adapter (`adapters/*.py`) fetches the schedule from its source and converts it into
**one common format**, and the result is published on GitHub Pages. The display fetches only the small file
for its own group.

When an operator changes its website, the adapter is fixed here — the firmware does not need an update.

## Format (v1)

`out/<provider>/<group>.json`, for example `out/ztoe/3.2.json`:

```json
{
  "v": 1,
  "provider": "ztoe",
  "name": "Житомирщина (Житомиробленерго)",
  "group": "3.2",
  "fetched": "2026-11-26T17:10:00Z",
  "days": [
    {"date": "2026-11-26", "status": "applies", "off": "000000111111000000…"},
    {"date": "2026-11-27", "status": "waiting", "off": ""}
  ]
}
```

- `off` — 48 characters, one per 30 minutes from 00:00 Kyiv time: `1` — no power, `0` — power on,
  `?` — the source marks it ambiguously (the display treats it as unknown).
- `status`: `applies` — the schedule is in effect; `waiting` — not published yet (`off` is empty);
  `emergency` — emergency outages (the schedule does not apply).
- `out/<provider>/index.json` — the list of groups and the provider name (for the setup page).
- `name` is shown to users as is, so it stays in Ukrainian.

If an adapter cannot parse the response (the site changed), the previous files are **kept** — yesterday's schedule
with a timestamp is better than a wrong one. The display shows "no data" when the file is older than 12 hours.

Published at `https://alexmovchan.github.io/deye-mini-display/outages/<provider>/<group>.json`.

## Providers

| id | Region | Source | Format |
| --- | --- | --- | --- |
| `yasno-kyiv` | Kyiv (DTEK Kyiv Electric Networks) | `app.yasno.ua` — JSON | intervals in minutes |
| `ztoe` | Zhytomyr region (Zhytomyroblenergo) | `www.ztoe.com.ua/unhooking-search.php` — HTML, windows-1251 | 48 × 30 min table, red = outage |

Notes:

- **ztoe.com.ua** serves a stale copy of the page (without recent changes and without tomorrow) to some
  User-Agent strings — e.g. ones containing "outages", "Python-urllib" or a github.com URL. The adapter sends
  `Mozilla/5.0 (compatible; deye-mini-display)`, which gets the same page as a browser (checked 2026-11-26).
- Zhytomyr's official "Svitlochek" API (`api-ztoe-poweron.inneti.net`) only serves the schedule as an image,
  so the HTML table is used instead.

Run locally: `python outages/fetch.py` (output goes to `outages/out/`).
