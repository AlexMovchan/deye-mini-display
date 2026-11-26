#!/usr/bin/env python3
"""
Кореневі сертифікати для HTTPS-запитів плат (alerts.in.ua, siren.pp.ua, Yasno).

Повний набір Mozilla на класичному ESP32 не вміщується в пам'ять під час рукостискання,
тож беремо лише корені, якими підписані наші сайти, плюс запасні від тих самих центрів
(сайти час від часу змінюють проміжні сертифікати). Джерело — certifi (набір Mozilla).

    python tools/gen_root_ca.py      # пише src/common/root_ca.h

Перевірити ланцюжки сайтів:
    openssl s_client -connect api.alerts.in.ua:443 -servername api.alerts.in.ua -showcerts
"""
import os
import re

import certifi

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "common", "root_ca.h")

# 28.10.2026: alerts.in.ua — GTS Root R4 (сам GlobalSign Root CA у наборі Mozilla вже відсутній);
# siren.pp.ua — ISRG Root X1 (Let's Encrypt); app.yasno.ua — GlobalSign Root R46 (Imperva);
# api.telegram.org — Go Daddy Root G2.
WANTED = [
    "GTS Root R1", "GTS Root R3", "GTS Root R4",
    "GlobalSign Root R46", "GlobalSign Root E46",
    "ISRG Root X1", "ISRG Root X2",
    "DigiCert Global Root G2",
    "USERTrust RSA Certification Authority", "Amazon Root CA 1",
    "Go Daddy Root Certificate Authority - G2",  # api.telegram.org (бот на РК)
]


def main():
    text = open(certifi.where(), encoding="utf-8").read()
    blocks = re.findall(r"# Label: \"([^\"]+)\".*?(-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)",
                        text, re.S)
    found = {label: pem for label, pem in blocks if label in WANTED}
    missing = [w for w in WANTED if w not in found]
    if missing:
        raise SystemExit(f"Немає в certifi: {missing}")
    lines = ["#pragma once",
             "// Згенеровано tools/gen_root_ca.py з набору Mozilla (certifi) — не редагувати вручну.",
             "// Кореневі сертифікати для alerts.in.ua, siren.pp.ua і Yasno.",
             "",
             "static const char ROOT_CA_PEM[] ="]
    for w in WANTED:
        lines.append(f"    // {w}")
        for row in found[w].splitlines():
            lines.append(f'    "{row}\\n"')
    lines[-1] += ";"
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{OUT}: {len(WANTED)} сертифікатів")


if __name__ == "__main__":
    main()
