#include "cloud.h"

#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "net.h"
#include "shopping.h"
#include "state.h"

namespace cloud {
namespace {

constexpr uint32_t kIntervalMs = 60 * 1000;
constexpr uint32_t kDebounceMs = 2000;  // zbiera kilka szybkich zmian w jedną rundę

TaskHandle_t task = nullptr;
void (*changedCb)() = nullptr;
SemaphoreHandle_t statusMutex = nullptr;
String lastStatus = "jeszcze nie synchronizowano";

void setStatus(const String& s) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    lastStatus = s;
    xSemaphoreGive(statusMutex);
}

String baseUrl() {
    String u = config::get().cloudUrl;
    u.trim();
    while (u.endsWith("/")) u.remove(u.length() - 1);
    if (!u.startsWith("http")) u = "https://" + u;
    return u;
}

int call(const char* method, const String& path, const String& body, String& response, String* err) {
    const Config& c = config::get();
    return net::request(method, baseUrl() + path, body.length() ? "application/json" : nullptr,
                        reinterpret_cast<const uint8_t*>(body.c_str()), body.length(), response,
                        c.cloudPassword.c_str(), 15000, err);
}

String describe(int status, const String& netErr) {
    if (status == 401) return "złe hasło (sprawdź sekret HASLO)";
    if (status < 0) return "brak połączenia (" + netErr + ")";
    return "HTTP " + String(status);
}

bool fetchRemote(std::vector<RemoteItem>& out, String& err) {
    String resp, netErr;
    const int st = call("GET", "/api/list", "", resp, &netErr);
    if (st != 200) {
        err = describe(st, netErr);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.c_str(), resp.length())) {
        err = "niezrozumiała odpowiedź";
        return false;
    }
    out.clear();
    for (JsonObject it : doc["items"].as<JsonArray>()) {
        if (it["done"] | false) continue;
        out.push_back({it["id"].as<std::string>(), it["name"].as<std::string>()});
    }
    return true;
}

void syncOnce() {
    const state::SyncSnapshot snap = state::syncSnapshot();
    String err;

    // 1. Kupione na lodówce -> "kupione" w telefonie.
    std::vector<std::string> doneSent;
    for (const auto& id : snap.pendingDone) {
        JsonDocument b;
        b["id"] = id;
        b["done"] = true;
        String body, resp, netErr;
        serializeJson(b, body);
        const int st = call("POST", "/api/done", body, resp, &netErr);
        if (st == 200) {
            doneSent.push_back(id);
        } else {
            err = describe(st, netErr);
            break;
        }
    }

    // 2. Stan w chmurze.
    std::vector<RemoteItem> remote;
    if (err.isEmpty()) fetchRemote(remote, err);
    if (!err.isEmpty()) {
        // Bez aktualnego stanu z chmury nie scalamy (pusta lista skasowałaby pozycje);
        // zapisujemy tylko potwierdzone "kupione".
        state::confirmDone(doneSent);
        setStatus("błąd: " + err);
        return;
    }

    // 3. Nowe na lodówce (bez id) -> do chmury.
    std::vector<RemoteItem> created;
    for (const auto& name : snap.items) {
        const std::string key = itemKey(name);
        if (snap.idByKey.count(key)) continue;
        bool inRemote = false;
        for (const auto& r : remote) inRemote |= itemKey(r.name) == key;
        if (inRemote) continue;  // ktoś dodał to samo w telefonie – merge połączy
        JsonDocument b;
        b["name"] = name;
        String body, resp, netErr;
        serializeJson(b, body);
        const int st = call("POST", "/api/add", body, resp, &netErr);
        JsonDocument r;
        if (st != 200 || deserializeJson(r, resp.c_str(), resp.length())) {
            err = describe(st, netErr);
            break;
        }
        RemoteItem item{r["id"].as<std::string>(), r["name"].as<std::string>()};
        created.push_back(item);
        remote.push_back(item);
    }

    const bool changed = state::applySync(remote, doneSent, created);
    struct tm t;
    char hhmm[8] = "";
    if (getLocalTime(&t, 10)) strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    setStatus(err.isEmpty() ? String("OK, ") + hhmm : "częściowo: " + err);
    if (changed && changedCb) changedCb();
}

void taskFn(void*) {
    for (;;) {
        // Czekaj na prośbę o synchronizację albo upływ interwału.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kIntervalMs))) {
            vTaskDelay(pdMS_TO_TICKS(kDebounceMs));
            ulTaskNotifyTake(pdTRUE, 0);
        }
        const bool enabled = config::hasCloud();
        state::setSyncEnabled(enabled);
        if (!enabled || !net::isConnected()) continue;
        syncOnce();
    }
}

}  // namespace

void begin(void (*onListChanged)()) {
    changedCb = onListChanged;
    statusMutex = xSemaphoreCreateMutex();
    state::setSyncEnabled(config::hasCloud());
    // Stos 8 kB: TLS + ArduinoJson.
    xTaskCreatePinnedToCore(taskFn, "cloud", 8192, nullptr, 1, &task, 0);
    requestSync();
}

void requestSync() {
    if (task) xTaskNotifyGive(task);
}

String status() {
    if (!config::hasCloud()) return "wyłączona";
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    const String s = lastStatus;
    xSemaphoreGive(statusMutex);
    return s;
}

String test() {
    if (!config::hasCloud()) return "Podaj adres aplikacji i hasło, potem zapisz.";
    std::vector<RemoteItem> remote;
    String err;
    if (!fetchRemote(remote, err)) return "Nie działa: " + err + " – adres: " + baseUrl();
    return "Działa ✓ – w aplikacji jest " + String(remote.size()) + " pozycji do kupienia.";
}

}  // namespace cloud
