#include "config.h"

#include <Preferences.h>

// Opcjonalnie: wartości domyślne na etapie kompilacji (dla osób budujących samodzielnie).
#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace config {
namespace {

Config cfg;
Preferences prefs;

String getStr(const char* key, const char* fallback) {
    return prefs.isKey(key) ? prefs.getString(key) : String(fallback);
}

}  // namespace

void applySttPreset(Config& c) {
    if (c.sttProvider == "openai") {
        c.sttUrl = "https://api.openai.com/v1/audio/transcriptions";
        c.sttModel = "gpt-4o-mini-transcribe";
    } else if (c.sttProvider != "custom") {
        c.sttProvider = "groq";
        c.sttUrl = "https://api.groq.com/openai/v1/audio/transcriptions";
        c.sttModel = "whisper-large-v3";
    }
}

void begin() {
    prefs.begin("cfg", false);
#ifdef WIFI_SSID
    cfg.ssid = getStr("ssid", WIFI_SSID);
    cfg.pass = getStr("pass", WIFI_PASSWORD);
#else
    cfg.ssid = getStr("ssid", "");
    cfg.pass = getStr("pass", "");
#endif
#ifdef WEATHER_PLACE
    cfg.place = getStr("place", WEATHER_PLACE);
    cfg.lat = prefs.getDouble("lat", WEATHER_LAT);
    cfg.lon = prefs.getDouble("lon", WEATHER_LON);
#else
    cfg.place = getStr("place", "Warszawa");
    cfg.lat = prefs.getDouble("lat", NAN);
    cfg.lon = prefs.getDouble("lon", NAN);
#endif
#ifdef STT_KEY
    cfg.sttProvider = getStr("sttProv", "custom");
    cfg.sttUrl = getStr("sttUrl", STT_URL);
    cfg.sttModel = getStr("sttModel", STT_MODEL);
    cfg.sttKey = getStr("sttKey", STT_KEY);
#else
    cfg.sttProvider = getStr("sttProv", "groq");
    cfg.sttUrl = getStr("sttUrl", "");
    cfg.sttModel = getStr("sttModel", "");
    cfg.sttKey = getStr("sttKey", "");
#endif
#ifdef TIMEZONE
    cfg.tz = getStr("tz", TIMEZONE);
#else
    cfg.tz = getStr("tz", "CET-1CEST,M3.5.0,M10.5.0/3");
#endif
    if (cfg.sttUrl.isEmpty()) applySttPreset(cfg);
}

Config& get() { return cfg; }

void save() {
    prefs.putString("ssid", cfg.ssid);
    prefs.putString("pass", cfg.pass);
    prefs.putString("place", cfg.place);
    prefs.putDouble("lat", cfg.lat);
    prefs.putDouble("lon", cfg.lon);
    prefs.putString("sttProv", cfg.sttProvider);
    prefs.putString("sttUrl", cfg.sttUrl);
    prefs.putString("sttModel", cfg.sttModel);
    prefs.putString("sttKey", cfg.sttKey);
    prefs.putString("tz", cfg.tz);
}

bool hasWifi() { return cfg.ssid.length() > 0; }

bool hasLocation() { return !isnan(cfg.lat) && !isnan(cfg.lon); }

}  // namespace config
