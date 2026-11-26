#pragma once
// Wi-Fi (станція з перепідключенням; у режимі налаштування — ще й точка доступу) і час NTP.
#include <Arduino.h>

namespace net {

// Робочий режим: лише станція, без енергозбереження радіо (docs/PLAN.md §2).
void beginStation(const String& ssid, const String& pass);
// Режим налаштування: точка доступу SETUP_AP_SSID на каналі channel (1..13) + станція (ще не підключена).
void beginSetup(int channel);
// Підключити станцію в режимі налаштування (точка доступу лишається).
void connect(const String& ssid, const String& pass);
void loop();
bool online();
int rssi();
bool timeValid();  // годинник синхронізовано (NTP)
int64_t now();     // unix UTC

}  // namespace net
