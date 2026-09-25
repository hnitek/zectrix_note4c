#include "settings.h"

#include <DNSServer.h>
#include <WiFi.h>

#include "config.h"
#include "stt.h"

namespace settings {
namespace {

String networks;  // <option> z wynikami skanowania Wi-Fi

String esc(const String& s) {
    String o;
    for (unsigned i = 0; i < s.length(); ++i) {
        const char c = s[i];
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else o += c;
    }
    return o;
}

void scanNetworks() {
    networks = "";
    const int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() && networks.indexOf(">" + esc(ssid) + "<") < 0) {
            networks += "<option>" + esc(ssid) + "</option>";
        }
    }
    WiFi.scanDelete();
}

String page(bool setupMode, const String& action) {
    const Config& c = config::get();
    auto sel = [&](const char* p) { return c.sttProvider == p ? " selected" : ""; };
    String h = R"(<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ustawienia lodówki</title>
<style>:root{--bg:#fff;--fg:#111;--mut:#666;--line:#ccc;--acc:#c62828}
@media(prefers-color-scheme:dark){:root{--bg:#141414;--fg:#eee;--mut:#999;--line:#444}}
*{box-sizing:border-box}body{margin:0;font:17px system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:480px;margin:0 auto;padding:16px}h1{color:var(--acc);font-size:22px}
h2{font-size:17px;margin:24px 0 8px}label{display:block;margin:12px 0 4px;font-weight:600}
input,select{width:100%;padding:11px;font:inherit;border:2px solid var(--line);border-radius:10px;background:var(--bg);color:var(--fg)}
small{color:var(--mut);display:block;margin-top:4px}button{margin-top:24px;width:100%;padding:14px;font:inherit;
border:0;border-radius:10px;background:var(--acc);color:#fff}a{color:var(--acc)}</style></head><body><main>
<h1>Ustawienia lodówki</h1>)";
    if (setupMode) {
        h += "<p>Podaj sieć domową i klucz do rozpoznawania mowy. Po zapisaniu urządzenie uruchomi się ponownie.</p>";
    } else {
        h += "<p><a href=\"/\">&larr; Lista zakupów</a></p>";
    }
    h += "<form method=\"post\" action=\"" + action + "\"><h2>Wi-Fi (2,4 GHz)</h2>";
    h += "<label>Nazwa sieci</label><input name=\"ssid\" list=\"nets\" required value=\"" + esc(c.ssid) +
         "\"><datalist id=\"nets\">" + networks + "</datalist>";
    h += "<label>Hasło</label><input name=\"pass\" type=\"password\" autocomplete=\"off\"";
    h += c.pass.length() ? " placeholder=\"(bez zmian)\">" : ">";
    h += "<h2>Pogoda</h2><label>Miasto</label><input name=\"place\" required value=\"" + esc(c.place) + "\">";
    if (config::hasLocation()) {
        h += "<small>Współrzędne: " + String(c.lat, 3) + ", " + String(c.lon, 3) + "</small>";
    }
    h += "<h2>Rozpoznawanie mowy</h2><label>Dostawca</label><select name=\"prov\">";
    h += String("<option value=\"groq\"") + sel("groq") + ">Groq – Whisper (darmowy limit)</option>";
    h += String("<option value=\"openai\"") + sel("openai") + ">OpenAI</option>";
    h += String("<option value=\"custom\"") + sel("custom") + ">Inny (zgodny z OpenAI)</option></select>";
    h += "<label>Klucz API</label><input name=\"key\" type=\"password\" autocomplete=\"off\"";
    h += c.sttKey.length() ? " placeholder=\"(bez zmian)\">" : " placeholder=\"gsk_...\">";
    h += "<small>Groq: załóż konto na console.groq.com i utwórz klucz w zakładce API Keys.</small>";
    if (!setupMode) h += "<p><a href=\"/test-klucza\">Sprawdź zapisany klucz</a></p>";
    h += "<details><summary>Zaawansowane (dostawca „Inny”)</summary>";
    h += "<label>Adres API</label><input name=\"url\" value=\"" + esc(c.sttUrl) + "\">";
    h += "<label>Model</label><input name=\"model\" value=\"" + esc(c.sttModel) + "\">";
    h += "<label>Strefa czasowa (POSIX)</label><input name=\"tz\" value=\"" + esc(c.tz) + "\"></details>";
    h += "<button>Zapisz i uruchom ponownie</button></form></main></body></html>";
    return h;
}

void save(WebServer& server) {
    Config& c = config::get();
    c.ssid = server.arg("ssid");
    c.ssid.trim();
    if (server.arg("pass").length()) c.pass = server.arg("pass");
    String place = server.arg("place");
    place.trim();
    if (place != c.place) {
        c.place = place;
        c.lat = NAN;  // współrzędne wyszukamy po połączeniu z internetem
        c.lon = NAN;
    }
    c.sttProvider = server.arg("prov");
    if (c.sttProvider == "custom") {
        c.sttUrl = server.arg("url");
        c.sttModel = server.arg("model");
    }
    config::applySttPreset(c);
    if (server.arg("key").length()) {
        c.sttKey = server.arg("key");
        c.sttKey.trim();
    }
    if (server.arg("tz").length()) c.tz = server.arg("tz");
    config::save();

    server.send(200, "text/html; charset=utf-8",
                "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                "<body style='font:18px system-ui;padding:24px'><h2>Zapisano</h2>"
                "<p>Urządzenie uruchamia się ponownie i łączy z siecią <b>" + esc(c.ssid) +
                "</b>. Ekran odświeży się za ok. minutę.</p></body>");
    delay(1500);
    ESP.restart();
}

}  // namespace

void registerRoutes(WebServer& server, const char* path) {
    const String action = path;
    server.on(path, HTTP_GET, [&server, action] {
        server.send(200, "text/html; charset=utf-8", page(false, action));
    });
    server.on(path, HTTP_POST, [&server] { save(server); });
    server.on("/test-klucza", HTTP_GET, [&server] {
        String r = esc(stt::checkKey());
        server.send(200, "text/html; charset=utf-8",
                    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                    "<body style='font:18px system-ui;padding:16px;max-width:520px;margin:auto'>"
                    "<p><a href='/ustawienia'>&larr; Ustawienia</a></p><h2>Test klucza API</h2><p>" +
                        r + "</p></body>");
    });
}

void runSetupPortal() {
    // AP + STA: portal działa, a w tle nadal próbujemy połączyć się z zapisaną siecią.
    WiFi.mode(WIFI_AP_STA);
    scanNetworks();
    WiFi.softAP(config::kSetupSsid);
    delay(200);
    if (config::hasWifi()) WiFi.begin(config::get().ssid.c_str(), config::get().pass.c_str());

    DNSServer dns;
    dns.start(53, "*", WiFi.softAPIP());  // każda domena -> portal (strona otwiera się sama)
    WebServer server(80);
    server.on("/", HTTP_GET, [&server] { server.send(200, "text/html; charset=utf-8", page(true, "/")); });
    server.on("/", HTTP_POST, [&server] { save(server); });
    server.onNotFound([&server] {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        server.send(302, "text/plain", "");
    });
    server.begin();
    log_i("Portal konfiguracyjny: sieć %s, http://%s", config::kSetupSsid,
          WiFi.softAPIP().toString().c_str());

    const uint32_t start = millis();
    for (;;) {
        dns.processNextRequest();
        server.handleClient();
        // Zapisana sieć jednak działa (np. router wrócił po awarii) i nikt nie konfiguruje
        // – wracamy do normalnej pracy.
        if (config::hasWifi() && WiFi.status() == WL_CONNECTED && WiFi.softAPgetStationNum() == 0 &&
            millis() - start > 60000) {
            ESP.restart();
        }
        delay(5);
    }
}

}  // namespace settings
