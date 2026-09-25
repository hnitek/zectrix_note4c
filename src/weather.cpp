#include "weather.h"

#include <ArduinoJson.h>

#include "net.h"
#include "secrets.h"

namespace weather {

bool fetch(Weather& out) {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LAT, 4) +
                 "&longitude=" + String(WEATHER_LON, 4) +
                 "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
                 "weather_code,wind_speed_10m"
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
                 "precipitation_probability_max"
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

    JsonObject daily = doc["daily"];
    for (int i = 0; i < 3; ++i) {
        out.days[i].code = daily["weather_code"][i] | -1;
        out.days[i].tMax = daily["temperature_2m_max"][i] | 0.0f;
        out.days[i].tMin = daily["temperature_2m_min"][i] | 0.0f;
        out.days[i].precipProb = daily["precipitation_probability_max"][i] | 0;
    }
    out.ok = true;
    return true;
}

const char* describe(int code) {
    switch (code) {
        case 0: return "Bezchmurnie";
        case 1: return "Przeważnie pogodnie";
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
