# Lodówka – pogoda i lista zakupów na ZecTrix NOTE4C

Firmware dla **ZecTrix NOTE4C** (ESP32-S3, czterokolorowy e-papier 4,2" 400×300), który zamienia
urządzenie w magnes na lodówkę:

- **pogoda**: bieżąca i na 3 dni, z [Open-Meteo](https://open-meteo.com) (bez klucza API),
- **lista zakupów dodawana głosem**: przytrzymujesz środkowy przycisk i mówisz *„dodaj mleko, chleb i masło”*,
- **panel WWW** w sieci domowej: podgląd i edycja listy z telefonu (np. w sklepie).

![Podgląd ekranu](docs/podglad.png)

*Podgląd wyrenderowany z tego samego kodu UI na PC. Na prawdziwym ekranie kolory są matowe, jak na e-papierze.*

## Jak to działa

```
 [przycisk] ─► mikrofon (ES8311) ─► WAV 16 kHz ─► API rozpoznawania mowy (Whisper)
                                                        │ tekst: "dodaj mleko i chleb"
                                                        ▼
                                  parser poleceń PL ─► lista (pamięć flash) ─► e-papier
```

Rozpoznawanie mowy działa w chmurze, przez dowolne API zgodne z OpenAI `/v1/audio/transcriptions`.
Domyślnie jest to **Groq** (`whisper-large-v3`): ma darmowy limit i dobrze radzi sobie z polskim.
Zamiast niego możesz ustawić OpenAI albo własny serwer Whisper w sieci lokalnej.
Nagranie wysyłane jest tylko wtedy, gdy trzymasz przycisk.

## Obsługa

| Akcja | Co robi |
|---|---|
| **Przytrzymaj środkowy przycisk** i mów, puść po skończeniu | polecenie głosowe (maks. 10 s) |
| Przycisk **w górę** | natychmiastowe odświeżenie pogody i ekranu |
| `http://lodowka.local` lub IP ze stopki ekranu | edycja listy z telefonu |

Sygnały dźwiękowe: krótki pisk = słucham, dwa tony w górę = zrobione, niski ton = nie zrozumiałem
albo błąd. Wynik ostatniego polecenia pojawia się na żółtym pasku u dołu ekranu.

### Przykładowe polecenia

| Powiedz | Efekt |
|---|---|
| „Dodaj mleko, chleb i masło” | dodaje 3 pozycje |
| „Jajka i żółty ser” | bez czasownika też dodaje |
| „Trzeba kupić ketchup” / „Skończyło się masło” | dodaje |
| „Usuń mleko” / „Skreśl chleb” / „Kupiłem jajka” | usuwa (rozumie odmianę: „jajek” pasuje do „Jajka”) |
| „Wyczyść listę” | czyści wszystko |

W polu tekstowym panelu WWW możesz wpisywać te same polecenia.

> E-papier odświeża się w pełnych kolorach ok. **20–25 s**, a ekran w tym czasie mruga. To normalne.
> Dlatego ekran odświeża się tylko po zmianie listy, zmianie pogody (sprawdzanej co 30 min) i o północy.

## Instalacja

### 1. Narzędzia

Zainstaluj [VS Code](https://code.visualstudio.com/) z rozszerzeniem **PlatformIO IDE**
albo samo PlatformIO z linii poleceń: `pip install platformio`.

### 2. Konfiguracja

```bash
cp include/secrets.example.h include/secrets.h
```

W `include/secrets.h` uzupełnij:
- `WIFI_SSID` / `WIFI_PASSWORD`: sieć **2,4 GHz** (ESP32 nie obsługuje 5 GHz),
- `WEATHER_LAT` / `WEATHER_LON` / `WEATHER_PLACE`: Twoja lokalizacja,
- `STT_KEY`: klucz API do rozpoznawania mowy. Dla Groq: załóż konto na
  [console.groq.com](https://console.groq.com/keys) i utwórz klucz (`gsk_...`).

Plik `secrets.h` jest w `.gitignore`, więc nie trafi do repozytorium.

### 3. Wgranie

Podłącz NOTE4C kablem USB-C i włącz urządzenie. Potem:

```bash
pio run -e note4c -t upload
pio device monitor        # logi (115200)
```

Jeśli port się nie pojawia albo wgrywanie nie startuje, wejdź w tryb bootloadera: przytrzymaj
**środkowy przycisk (BOOT)**, włącz zasilanie albo podłącz USB, potem puść.

> **Uwaga:** wgranie tego firmware'u zastępuje fabryczne oprogramowanie. Jeśli chcesz mieć
> możliwość powrotu, zrób najpierw kopię:
> `esptool.py --chip esp32s3 read_flash 0 0x1000000 note4c_backup.bin`
> (przywracanie: `esptool.py --chip esp32s3 write_flash 0 note4c_backup.bin`).

### 4. Testy parsera (opcjonalnie)

Logika poleceń głosowych jest w czystym C++ i ma testy uruchamiane na komputerze:

```bash
pio test -e native
```

## Zasilanie

Wi-Fi jest cały czas włączone, żeby panel WWW i polecenia głosowe działały od razu. Przy takim
trybie wbudowany akumulator 2000 mAh wystarcza na mniej więcej 1–2 dni, więc na lodówce najlepiej
zasilać urządzenie z ładowarki USB-C (cienki kabel płaski dobrze się sprawdza).

## Struktura

| Plik | Zawartość |
|---|---|
| `src/main.cpp` | pętla główna: przyciski, harmonogram pogody, zadanie odświeżania ekranu |
| `src/epd.*` | sterownik e-papieru SSD2683 (BWRY, 2 bity na piksel) jako `Adafruit_GFX` |
| `src/ui.*` | układ ekranu, ikony pogody, polskie czcionki (U8g2) |
| `src/audio.*` | kodek ES8311: nagrywanie z mikrofonu, sygnały dźwiękowe |
| `src/stt.*` | wysyłka nagrania do API rozpoznawania mowy |
| `src/weather.*` | Open-Meteo i opisy pogody po polsku |
| `src/web.*` | panel WWW i REST API (`/api/list`, `/api/add`, `/api/remove`, `/api/clear`) |
| `src/state.*` | lista zakupów zapisywana w NVS |
| `lib/shopping/` | parser polskich poleceń i logika listy (testowane w `test/`) |
| `include/board.h` | pinout NOTE4C |

Pinout i sekwencję sterowania panelem wzięto z firmware'ów społeczności NOTE4C
([wakewon/NOTE4C-firmware-Rapid](https://github.com/wakewon/NOTE4C-firmware-Rapid),
[alexclmy/Paperwake](https://github.com/alexclmy/Paperwake)). Inicjalizacja ES8311 pochodzi
ze sterownika `esp_codec_dev` od Espressif.

## Rozwiązywanie problemów

- **Pisk błędu od razu po puszczeniu przycisku**: nagranie było za ciche (uznane za ciszę) albo
  kodek nie odpowiada. Zajrzyj do logów w `pio device monitor`.
- **„Nie zrozumiałem: …” na ekranie**: rozpoznany tekst nie pasował do żadnego polecenia. Mów bliżej
  urządzenia, zaczynając od „dodaj” albo „usuń”.
- **`STT HTTP 401`** w logach: nieprawidłowy `STT_KEY`.
- **Ekran się nie odświeża / `EPD busy timeout`**: sprawdź, czy masz NOTE4C (czterokolorowy).
  Wersja NOTE4 (czarno-biała) ma inny panel i ten sterownik na niej nie zadziała.
