# Agent instructions

Firmware for a standalone mini display for Deye inverters. Read `docs/PLAN.md` first.

## Hard rules

1. **The inverter is read-only.** Modbus function `0x03` only. Write functions `0x06`/`0x10` are forbidden in the code.
2. **No secrets or personal data in the repository.** The repository is public. Wi-Fi, logger serial number,
   addresses, region — only what the user enters on the board (NVS). Use made-up values in examples and tests
   (serial `1234567890`, network `Home_2G`).
3. **No Deye/Solarman cloud.** The logger is accessed locally only, port 8899.
4. **A connection failure is not a power outage.** Logger did not answer → "no data", never "no power".
5. **Alerts:** siren.pp.ua, at most 1 request per minute, only when a region is set.
6. **Language:** documentation, release notes, commit messages and code identifiers — English.
   On-device UI and setup page texts — Ukrainian (the users are in Ukraine).
7. Pure logic (protocol, parsing, screen rules) has no Arduino dependencies, lives in `lib/core/` and is tested on the PC.
