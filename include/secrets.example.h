#pragma once

// Skopiuj ten plik do include/secrets.h i uzupełnij. secrets.h jest w .gitignore.

// --- Wi-Fi (tylko 2.4 GHz) ---
#define WIFI_SSID     "MojaSiec"
#define WIFI_PASSWORD "haslo"

// --- Lokalizacja dla pogody (Open-Meteo, bez klucza) ---
#define WEATHER_LAT   52.2297
#define WEATHER_LON   21.0122
#define WEATHER_PLACE "Warszawa"

// Strefa czasowa POSIX (tu: Polska z czasem letnim)
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// --- Rozpoznawanie mowy ---
// Dowolne API zgodne z OpenAI /v1/audio/transcriptions.
// Groq (darmowy limit, dobrze rozumie polski): https://console.groq.com/keys
#define STT_URL   "https://api.groq.com/openai/v1/audio/transcriptions"
#define STT_MODEL "whisper-large-v3"
#define STT_KEY   "gsk_..."
// Alternatywa – OpenAI:
// #define STT_URL   "https://api.openai.com/v1/audio/transcriptions"
// #define STT_MODEL "gpt-4o-mini-transcribe"
// #define STT_KEY   "sk-..."

// Nazwa w sieci lokalnej: panel dostępny pod http://lodowka.local
#define HOSTNAME "lodowka"
