// Lodówkowy wyświetlacz na ZecTrix NOTE4C:
//  - pogoda z Open-Meteo,
//  - lista zakupów dodawana głosem (przytrzymaj środkowy przycisk i mów),
//  - podgląd/edycja listy z telefonu: http://lodowka.local

#include <Arduino.h>

#include "audio.h"
#include "board.h"
#include "epd.h"
#include "net.h"
#include "config.h"
#include "settings.h"
#include "state.h"
#include "stt.h"
#include "ui.h"
#include "weather.h"
#include "web.h"

namespace {

constexpr uint32_t kWeatherIntervalMs = 30 * 60 * 1000;
constexpr uint32_t kMaxRecordMs = 10000;
constexpr uint32_t kListChangeDebounceMs = 4000;  // zbiera kilka zmian z WWW w jedno odświeżenie

Epd4Color epd;
Weather currentWeather;
SemaphoreHandle_t dataMutex;
String footerMsg;
TaskHandle_t displayTask = nullptr;

int lastDay = -1;
volatile uint32_t listChangedAt = 0;
volatile bool listDirty = false;

void led(bool on) { digitalWrite(PIN_LED, on ? LOW : HIGH); }

bool buttonHeld(int pin) { return digitalRead(pin) == LOW; }
bool talkButtonHeld() { return buttonHeld(PIN_BTN_CONFIRM); }

// Odświeżanie e-papieru trwa ~20 s, więc robi je osobne zadanie,
// a pętla główna w tym czasie dalej obsługuje przyciski i WWW.
void displayTaskFn(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const auto items = state::listItems();
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        const Weather w = currentWeather;
        const String footer = footerMsg;
        xSemaphoreGive(dataMutex);

        ui::render(w, items, footer);
        const uint32_t t0 = millis();
        epd.display();
        log_i("Ekran odświeżony w %u ms", unsigned(millis() - t0));
    }
}

void requestRefresh() { xTaskNotifyGive(displayTask); }

void setFooter(const String& msg) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    footerMsg = msg;
    xSemaphoreGive(dataMutex);
}

bool sameWeather(const Weather& a, const Weather& b) {
    if (a.ok != b.ok || a.code != b.code || lroundf(a.temp) != lroundf(b.temp)) return false;
    for (int i = 0; i < 3; ++i) {
        if (a.days[i].code != b.days[i].code || lroundf(a.days[i].tMax) != lroundf(b.days[i].tMax) ||
            lroundf(a.days[i].tMin) != lroundf(b.days[i].tMin) ||
            a.days[i].precipProb / 10 != b.days[i].precipProb / 10) {
            return false;
        }
    }
    return true;
}

// Zwraca true, jeśli dane się zmieniły.
bool updateWeather() {
    Weather w;
    if (!weather::fetch(w)) return false;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    const bool changed = !sameWeather(w, currentWeather);
    currentWeather = w;
    xSemaphoreGive(dataMutex);
    return changed;
}

// Po zmianie miasta współrzędne trzeba wyszukać (wymaga internetu).
void ensureLocation() {
    if (config::hasLocation()) return;
    Config& c = config::get();
    double lat, lon;
    if (weather::geocode(c.place, lat, lon)) {
        c.lat = lat;
        c.lon = lon;
        config::save();
    }
}

void handleVoice() {
    if (config::get().sttKey.isEmpty()) {
        audio::beepError();
        setFooter("Brak klucza API: http://" + net::ipAddress() + "/ustawienia");
        requestRefresh();
        while (talkButtonHeld()) delay(10);
        return;
    }
    led(true);
    audio::beepStart();
    size_t wavLen = 0;
    uint8_t* wav = audio::recordWav(wavLen, kMaxRecordMs, talkButtonHeld);
    led(false);
    if (!wav) {
        audio::beepError();
        return;
    }
    if (!net::isConnected()) {
        free(wav);
        audio::beepError();
        return;
    }
    // Miganie diodą podczas rozpoznawania.
    led(true);
    const String text = stt::transcribe(wav, wavLen);
    free(wav);
    led(false);
    if (text.isEmpty()) {
        audio::beepError();
        return;
    }

    String summary;
    const bool changed = state::applyText(text.c_str(), &summary);
    log_i("%s", summary.c_str());
    setFooter(summary);
    if (changed) {
        audio::beepOk();
    } else {
        audio::beepError();
    }
    requestRefresh();
    // Poczekaj na puszczenie przycisku, żeby nie nagrywać od razu ponownie.
    while (talkButtonHeld()) delay(10);
}

void onWebChange() {
    listChangedAt = millis();
    listDirty = true;
}

}  // namespace

void setup() {
    // Najpierw podtrzymanie zasilania z baterii, inaczej płytka zgaśnie po puszczeniu włącznika.
    pinMode(PIN_VBAT_HOLD, OUTPUT);
    digitalWrite(PIN_VBAT_HOLD, HIGH);
    pinMode(PIN_LED, OUTPUT);
    led(true);
    pinMode(PIN_BTN_CONFIRM, INPUT_PULLUP);
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(300);
    log_i("Start lodówki");

    dataMutex = xSemaphoreCreateMutex();
    config::begin();
    state::begin();
    epd.begin();
    ui::begin(epd);
    if (!audio::begin()) log_e("Kodek audio nie odpowiada – sterowanie głosem niedostępne");

    xTaskCreatePinnedToCore(displayTaskFn, "display", 8192, nullptr, 1, &displayTask, 0);

    // Tryb konfiguracji: brak ustawień, przycisk "w górę" trzymany przy starcie
    // albo zapisana sieć nie odpowiada.
    const bool forceSetup = buttonHeld(PIN_BTN_UP);
    if (!config::hasWifi() || forceSetup || !net::connect(30000)) {
        const String reason =
            !config::hasWifi() || forceSetup
                ? String("")
                : String("Nie mogę połączyć się z siecią \"") + config::get().ssid + "\".\n";
        ui::renderMessage("Konfiguracja",
                          reason + "1. Połącz telefon z siecią Wi-Fi \"" + config::kSetupSsid +
                              "\".\n2. Otwórz http://192.168.4.1 (zwykle otworzy się samo).\n"
                              "3. Podaj sieć Wi-Fi, miasto i klucz API.");
        epd.display();
        led(false);
        settings::runSetupPortal();  // nie wraca – restart po zapisaniu
    } else {
        // Czekamy chwilę na czas z NTP, żeby nagłówek miał datę.
        struct tm t;
        if (getLocalTime(&t, 5000)) lastDay = t.tm_mday;
        ensureLocation();
        updateWeather();
        setFooter("Telefon: http://" + net::ipAddress());
        requestRefresh();
    }
    web::begin(onWebChange);
    led(false);
}

void loop() {
    static uint32_t lastWeather = millis();
    static bool wasConnected = net::isConnected();
    static bool redrawAfterReconnect = false;

    web::loop();

    if (talkButtonHeld()) handleVoice();

    if (buttonHeld(PIN_BTN_UP)) {
        led(true);
        updateWeather();
        lastWeather = millis();
        setFooter("");
        requestRefresh();
        while (buttonHeld(PIN_BTN_UP)) delay(10);
        led(false);
    }

    // Zmiany z telefonu: jedno odświeżenie po krótkiej przerwie.
    if (listDirty && millis() - listChangedAt > kListChangeDebounceMs) {
        listDirty = false;
        setFooter("Zmieniono z telefonu");
        requestRefresh();
    }

    const bool connected = net::isConnected();
    if (connected && !wasConnected) {
        net::startServices();
        ensureLocation();
        lastWeather = millis() - kWeatherIntervalMs;  // pogoda od razu po powrocie sieci
        redrawAfterReconnect = true;                  // m.in. zdejmuje ekran "Brak Wi-Fi"
    }
    wasConnected = connected;

    if (connected && millis() - lastWeather > kWeatherIntervalMs) {
        lastWeather = millis();
        struct tm t;
        const bool dayChanged = getLocalTime(&t, 10) && t.tm_mday != lastDay;
        if (dayChanged) lastDay = t.tm_mday;
        if (updateWeather() || dayChanged || redrawAfterReconnect) requestRefresh();
        redrawAfterReconnect = false;
    }

    delay(10);
}
