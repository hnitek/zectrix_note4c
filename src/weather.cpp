#include "weather.h"

#include <ArduinoJson.h>

#include "net.h"
#include "config.h"

namespace weather {

namespace {
String urlEncode(const String& s) {
    String o;
    const char* hex = "0123456789ABCDEF";
    for (unsigned i = 0; i < s.length(); ++i) {
        const uint8_t c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.') {
            o += char(c);
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 15];
        }
    }
    return o;
}
}  // namespace

bool geocode(const String& place, double& lat, double& lon) {
    String body;
    const String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=pl&name=" +
                       urlEncode(place);
    if (net::request("GET", url, nullptr, nullptr, 0, body) != 200) return false;
    JsonDocument doc;
    if (deserializeJson(doc, body.c_str(), body.length())) return false;
    JsonObject r = doc["results"][0];
    if (r.isNull()) {
        log_e("Nie znaleziono miejscowości \"%s\"", place.c_str());
        return false;
    }
    lat = r["latitude"];
    lon = r["longitude"];
    log_i("%s -> %.4f, %.4f", place.c_str(), lat, lon);
    return true;
}

bool fetch(Weather& out) {
    const Config& c = config::get();
    if (!config::hasLocation()) return false;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(c.lat, 4) +
                 "&longitude=" + String(c.lon, 4) +
                 "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
                 "weather_code,wind_speed_10m,is_day"
                 "&hourly=temperature_2m,precipitation_probability"
                 "&forecast_hours=" + String(Weather::kHours + 1) +
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
                 "precipitation_probability_max,sunrise,sunset"
                 "&timezone=auto&forecast_days=3";
    String body;
    const int status = net::request("GET", url, nullptr, nullptr, 0, body);
    if (status != 200) {
        log_e("Open-Meteo HTTP %d", status);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body.c_str(), body.length())) return false;

    JsonObject cur = doc["current"];
    out.temp = cur["temperature_2m"] | 0.0f;
    out.feelsLike = cur["apparent_temperature"] | 0.0f;
    out.humidity = cur["relative_humidity_2m"] | 0;
    out.code = cur["weather_code"] | -1;
    out.wind = cur["wind_speed_10m"] | 0.0f;
    out.isDay = (cur["is_day"] | 1) != 0;

    // Godziny: "2026-09-25T14:00" -> 14. Pierwsza pozycja to bieżąca (pełna) godzina.
    JsonObject hourly = doc["hourly"];
    out.hourCount = 0;
    for (int i = 0; i < Weather::kHours; ++i) {
        const char* t = hourly["time"][i] | "";
        if (strlen(t) < 13) break;
        HourForecast& h = out.hours[out.hourCount++];
        h.hour = atoi(t + 11);
        h.temp = hourly["temperature_2m"][i] | 0.0f;
        h.precipProb = hourly["precipitation_probability"][i] | 0;
    }

    JsonObject daily = doc["daily"];
    for (int i = 0; i < 3; ++i) {
        out.days[i].code = daily["weather_code"][i] | -1;
        out.days[i].tMax = daily["temperature_2m_max"][i] | 0.0f;
        out.days[i].tMin = daily["temperature_2m_min"][i] | 0.0f;
        out.days[i].precipProb = daily["precipitation_probability_max"][i] | 0;
    }
    // "2026-09-25T06:42" -> "06:42"
    const char* rise = daily["sunrise"][0] | "";
    const char* set = daily["sunset"][0] | "";
    if (strlen(rise) >= 16) strlcpy(out.sunrise, rise + 11, sizeof(out.sunrise));
    if (strlen(set) >= 16) strlcpy(out.sunset, set + 11, sizeof(out.sunset));
    out.ok = true;
    return true;
}

const char* describe(int code) {
    switch (code) {
        case 0: return "Bezchmurnie";
        case 1: return "Pogodnie";
        case 2: return "Przejaśnienia";
        case 3: return "Pochmurno";
        case 45: case 48: return "Mgła";
        case 51: case 53: case 55: return "Mżawka";
        case 56: case 57: return "Marznąca mżawka";
        case 61: return "Lekki deszcz";
        case 63: return "Deszcz";
        case 65: return "Ulewa";
        case 66: case 67: return "Marznący deszcz";
        case 71: return "Lekki śnieg";
        case 73: return "Śnieg";
        case 75: return "Intensywny śnieg";
        case 77: return "Krupa śnieżna";
        case 80: case 81: return "Przelotny deszcz";
        case 82: return "Nawałnica";
        case 85: case 86: return "Przelotny śnieg";
        case 95: return "Burza";
        case 96: case 99: return "Burza z gradem";
        default: return "Brak danych";
    }
}

Icon iconFor(int code) {
    if (code == 0) return Icon::Sun;
    if (code == 1 || code == 2) return Icon::PartlyCloudy;
    if (code == 3) return Icon::Cloud;
    if (code == 45 || code == 48) return Icon::Fog;
    if (code >= 51 && code <= 57) return Icon::Drizzle;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Icon::Rain;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Icon::Snow;
    if (code >= 95) return Icon::Storm;
    return Icon::Unknown;
}

}  // namespace weather
