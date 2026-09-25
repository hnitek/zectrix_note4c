#include "web.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WebServer.h>

#include "settings.h"
#include "state.h"

namespace web {
namespace {

WebServer server(80);
void (*changed)() = nullptr;
uint8_t* lastWav = nullptr;
size_t lastWavLen = 0;
String lastText;
String lastDiag;
String otaError;

const char kOtaPage[] PROGMEM = R"HTML(<!doctype html><html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Aktualizacja</title>
<style>body{font:17px system-ui,sans-serif;max-width:520px;margin:auto;padding:16px}
button{padding:12px 16px;font:inherit;border:0;border-radius:10px;background:#c62828;color:#fff;width:100%;margin-top:16px}
input{font:inherit;width:100%}progress{width:100%;height:20px;margin-top:16px}</style></head><body>
<p><a href="/">&larr; Lista</a></p><h2>Aktualizacja oprogramowania</h2>
<p>Wybierz plik <b>lodowka-note4c-app.bin</b>. Ustawienia i lista zakupów zostaną zachowane.</p>
<form id="f"><input type="file" id="p" accept=".bin" required><button>Wgraj</button></form>
<progress id="g" max="100" value="0" hidden></progress><p id="m"></p>
<script>
document.getElementById('f').onsubmit=e=>{e.preventDefault();const file=document.getElementById('p').files[0];if(!file)return;
 const g=document.getElementById('g'),m=document.getElementById('m');g.hidden=false;
 const x=new XMLHttpRequest();x.open('POST','/aktualizacja');
 x.upload.onprogress=ev=>{if(ev.lengthComputable)g.value=ev.loaded*100/ev.total};
 x.onload=()=>{m.textContent=x.responseText};x.onerror=()=>{m.textContent='Błąd połączenia'};
 const d=new FormData();d.append('firmware',file);x.send(d);m.textContent='Wgrywanie...'};
</script></body></html>)HTML";

// Aktualizacja przez przeglądarkę: zapis do drugiego slotu aplikacji (OTA),
// więc ustawienia (NVS) i lista zakupów zostają nietknięte.
void handleOtaUpload() {
    HTTPUpload& up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        otaError = "";
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) otaError = Update.errorString();
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!otaError.isEmpty()) return;
        if (up.totalSize == 0) {
            // Pełny obraz od 0x0 zaczyna się od bootloadera, a nie od aplikacji:
            // aplikacja ma opis (esp_app_desc) z magiczną liczbą 0xABCD5432 pod offsetem 32.
            const bool isApp = up.currentSize > 36 && up.buf[0] == 0xE9 && up.buf[32] == 0x32 &&
                               up.buf[33] == 0x54 && up.buf[34] == 0xCD && up.buf[35] == 0xAB;
            if (!isApp) {
                otaError = "To nie jest plik aplikacji. Użyj lodowka-note4c-app.bin "
                           "(pełny obraz lodowka-note4c.bin wgrywa się tylko kablem).";
                Update.abort();
                return;
            }
        }
        if (Update.write(up.buf, up.currentSize) != up.currentSize) otaError = Update.errorString();
    } else if (up.status == UPLOAD_FILE_END) {
        if (otaError.isEmpty() && !Update.end(true)) otaError = Update.errorString();
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaError = "Przerwano wysyłanie";
    }
}

const char kPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lista zakupów</title>
<style>
:root{--bg:#fff;--fg:#111;--mut:#666;--line:#ddd;--acc:#c62828;--hl:#fbc02d}
@media(prefers-color-scheme:dark){:root{--bg:#141414;--fg:#eee;--mut:#999;--line:#333}}
*{box-sizing:border-box}body{margin:0;font:17px system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:520px;margin:0 auto;padding:16px}
h1{font-size:22px;color:var(--acc);margin:4px 0 16px}
form{display:flex;gap:8px;margin-bottom:16px}
input{flex:1;min-width:0;padding:12px;font:inherit;border:2px solid var(--line);border-radius:10px;background:var(--bg);color:var(--fg)}
button{padding:12px 16px;font:inherit;border:0;border-radius:10px;background:var(--fg);color:var(--bg);cursor:pointer}
ul{list-style:none;padding:0;margin:0}
li{display:flex;align-items:center;gap:12px;padding:12px 4px;border-bottom:1px solid var(--line)}
li span{flex:1;overflow-wrap:anywhere}
li button{background:none;color:var(--acc);padding:6px 10px;font-size:20px}
.empty{color:var(--mut);padding:24px 0;text-align:center}
.foot{margin-top:20px;display:flex;justify-content:space-between;color:var(--mut);font-size:14px;align-items:center}
.foot button{background:none;color:var(--acc);border:1px solid var(--acc);padding:8px 12px}
</style></head><body><main>
<h1>Lista zakupów</h1>
<form id="f"><input id="i" placeholder="np. mleko, chleb i masło" autocomplete="off"><button>Dodaj</button></form>
<ul id="l"></ul>
<div class="foot"><span id="c"></span><button id="x">Wyczyść listę</button></div>
<p class="foot"><a href="/ustawienia" style="color:inherit">Ustawienia</a>
<a href="/nagranie" style="color:inherit">Ostatnie nagranie</a>
<a href="/aktualizacja" style="color:inherit">Aktualizacja</a></p>
</main><script>
const l=document.getElementById('l'),c=document.getElementById('c');
async function api(p,b){const r=await fetch(p,{method:b?'POST':'GET',body:b});render(await r.json())}
function render(items){l.innerHTML='';
 if(!items.length){l.innerHTML='<li class="empty">Lista jest pusta</li>'}
 items.forEach((t,i)=>{const li=document.createElement('li');const s=document.createElement('span');s.textContent=t;
  const b=document.createElement('button');b.textContent='✕';b.title='Usuń';
  b.onclick=()=>{const d=new FormData();d.append('index',i);api('/api/remove',d)};li.append(s,b);l.append(li)});
 c.textContent=items.length?('Pozycji: '+items.length):''}
document.getElementById('f').onsubmit=e=>{e.preventDefault();const i=document.getElementById('i');
 if(!i.value.trim())return;const d=new FormData();d.append('text',i.value);i.value='';api('/api/add',d)};
document.getElementById('x').onclick=()=>{if(confirm('Wyczyścić całą listę?'))api('/api/clear',new FormData())};
api('/api/list');setInterval(()=>api('/api/list'),15000);
</script></body></html>)HTML";

void sendList() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& item : state::listItems()) arr.add(item);
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json; charset=utf-8", out);
}

void notifyChanged() {
    if (changed) changed();
}

}  // namespace

void begin(void (*onChange)()) {
    changed = onChange;
    server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", kPage); });
    server.on("/api/list", HTTP_GET, sendList);
    server.on("/api/add", HTTP_POST, [] {
        // Pole tekstowe rozumie to samo co polecenia głosowe ("mleko i chleb", "usuń mleko").
        if (state::applyText(server.arg("text").c_str(), nullptr)) notifyChanged();
        sendList();
    });
    server.on("/api/remove", HTTP_POST, [] {
        if (state::removeAt(server.arg("index").toInt())) notifyChanged();
        sendList();
    });
    server.on("/api/clear", HTTP_POST, [] {
        if (state::clear()) notifyChanged();
        sendList();
    });
    settings::registerRoutes(server, "/ustawienia");
    server.on("/aktualizacja", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", kOtaPage); });
    server.on(
        "/aktualizacja", HTTP_POST,
        [] {
            if (!otaError.isEmpty() || Update.hasError()) {
                server.send(500, "text/plain; charset=utf-8",
                            "Błąd: " + (otaError.length() ? otaError : String(Update.errorString())));
                return;
            }
            server.send(200, "text/plain; charset=utf-8",
                        "Gotowe. Urządzenie uruchamia się ponownie – ekran odświeży się za ok. minutę.");
            delay(1000);
            ESP.restart();
        },
        handleOtaUpload);
    server.on("/nagranie", HTTP_GET, [] {
        String h = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                   "<body style='font:17px system-ui;padding:16px;max-width:520px;margin:auto'>"
                   "<p><a href='/'>&larr; Lista</a></p><h2>Ostatnie nagranie</h2>";
        if (!lastWav) {
            h += "<p>Brak nagrań od uruchomienia.</p>";
        } else {
            String t = lastText, d = lastDiag;
            t.replace("&", "&amp;");
            t.replace("<", "&lt;");
            d.replace("&", "&amp;");
            d.replace("<", "&lt;");
            h += "<audio controls src='/ostatnie.wav' style='width:100%'></audio>"
                 "<p>Rozpoznany tekst:</p><p style='font-size:22px'><b>" +
                 (t.length() ? t : String("(nic)")) + "</b></p><p style='color:#777;font-size:14px'>" +
                 d + "</p>";
        }
        server.send(200, "text/html; charset=utf-8", h);
    });
    server.on("/ostatnie.wav", HTTP_GET, [] {
        if (!lastWav) {
            server.send(404, "text/plain", "brak");
            return;
        }
        server.setContentLength(lastWavLen);
        server.send(200, "audio/wav", "");
        server.sendContent(reinterpret_cast<const char*>(lastWav), lastWavLen);
    });
    server.begin();
}

void loop() { server.handleClient(); }

void setLastRecording(uint8_t* wav, size_t len, const String& recognized, const String& diagnostics) {
    free(lastWav);
    lastWav = wav;
    lastWavLen = len;
    lastText = recognized;
    lastDiag = diagnostics;
}

}  // namespace web
