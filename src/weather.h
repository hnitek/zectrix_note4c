#pragma once

#include <Arduino.h>

struct DayForecast {
    int code = -1;  // kod pogody WMO
    float tMin = 0, tMax = 0;
    int precipProb = 0;  // %
};

struct Weather {
    bool ok = false;
    float temp = 0;
    float feelsLike = 0;
    int code = -1;
    float wind = 0;  // km/h
    int humidity = 0;
    DayForecast days[3];
};

namespace weather {

bool fetch(Weather& out);

// Wyszukuje współrzędne miejscowości (Open-Meteo Geocoding).
bool geocode(const String& place, double& lat, double& lon);

// Opis po polsku dla kodu WMO.
const char* describe(int code);

enum class Icon { Sun, PartlyCloudy, Cloud, Fog, Drizzle, Rain, Snow, Storm, Unknown };
Icon iconFor(int code);

}  // namespace weather
