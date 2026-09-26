#pragma once

#include <Arduino.h>

// Ustawienia zapisywane w pamięci flash (NVS). Podaje się je po wgraniu firmware'u,
// przez stronę konfiguracyjną (tryb "Lodowka-Setup") albo w panelu WWW: /ustawienia.
struct Config {
    String ssid;
    String pass;
    String place;            // nazwa miasta wpisana przez użytkownika
    double lat = NAN;        // NAN = jeszcze nie wyszukano współrzędnych
    double lon = NAN;
    String sttProvider;      // "groq", "openai" lub "custom"
    String sttUrl;
    String sttModel;
    String sttKey;
    String tz;               // strefa czasowa POSIX
    String cloudUrl;         // aplikacja w telefonie (Cloudflare Worker), np. https://lodowka.x.workers.dev
    String cloudPassword;    // hasło (sekret HASLO w Workerze)
};

namespace config {

constexpr const char* kHostname = "lodowka";
constexpr const char* kSetupSsid = "Lodowka-Setup";

void begin();
Config& get();
void save();
bool hasWifi();
bool hasLocation();
bool hasCloud();

// Ustawia URL i model dla wybranego dostawcy (dla "custom" zostawia podane).
void applySttPreset(Config& c);

}  // namespace config
