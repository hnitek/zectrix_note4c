#include "web.h"

#include <ArduinoJson.h>
#include <WebServer.h>

#include "state.h"

namespace web {
namespace {

WebServer server(80);
void (*changed)() = nullptr;

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
    server.begin();
}

void loop() { server.handleClient(); }

}  // namespace web
