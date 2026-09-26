// Lista zakupów w chmurze dla lodówki (ZecTrix NOTE4C).
// Cloudflare Worker + baza D1. Wdrożenie: patrz cloud/README.md.
//
// Wymagane w ustawieniach Workera:
//   - powiązanie bazy D1 o nazwie  DB
//   - sekret (Secret)             HASLO   – hasło do aplikacji i dla lodówki
//
// Autoryzacja: telefon loguje się hasłem (ciasteczko na rok), lodówka wysyła
// nagłówek "Authorization: Bearer <HASLO>".

const SCHEMA = `CREATE TABLE IF NOT EXISTS items (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  key TEXT NOT NULL,
  done INTEGER NOT NULL DEFAULT 0,
  created INTEGER NOT NULL,
  updated INTEGER NOT NULL
)`;

let schemaReady = false;

async function ensureSchema(env) {
  if (schemaReady) return;
  await env.DB.prepare(SCHEMA).run();
  schemaReady = true;
}

// Klucz do porównywania nazw: małe litery, bez polskich znaków i interpunkcji.
function itemKey(name) {
  return name
    .toLowerCase()
    .replace(/ł/g, "l")
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/[^\p{L}\p{N} ]/gu, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function capitalize(s) {
  return s.charAt(0).toLocaleUpperCase("pl") + s.slice(1);
}

async function sha256Hex(text) {
  const data = new TextEncoder().encode(text);
  const hash = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(hash)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function timingSafeEqual(a, b) {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

async function sessionToken(env) {
  return sha256Hex("lodowka-sesja:" + env.HASLO);
}

async function isAuthorized(request, env) {
  if (!env.HASLO) return false;
  const auth = request.headers.get("Authorization") || "";
  if (auth.startsWith("Bearer ") && timingSafeEqual(auth.slice(7), env.HASLO)) return true;
  const cookie = request.headers.get("Cookie") || "";
  const m = cookie.match(/(?:^|;\s*)sesja=([a-f0-9]{64})/);
  return !!m && timingSafeEqual(m[1], await sessionToken(env));
}

function json(data, status = 200, headers = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store", ...headers },
  });
}

async function listItems(env) {
  const { results } = await env.DB.prepare(
    "SELECT id, name, done FROM items ORDER BY done ASC, CASE WHEN done = 1 THEN -updated ELSE created END ASC"
  ).all();
  return results.map((r) => ({ id: r.id, name: r.name, done: !!r.done }));
}

async function addItem(env, rawName) {
  const name = capitalize(String(rawName || "").trim().slice(0, 100));
  if (!name) return null;
  const key = itemKey(name);
  const now = Date.now();
  // Ta sama rzecz już jest: aktywna -> zwracamy ją, kupiona -> przywracamy na listę.
  const existing = await env.DB.prepare("SELECT id, name, done FROM items WHERE key = ?").bind(key).first();
  if (existing) {
    if (existing.done) {
      await env.DB.prepare("UPDATE items SET done = 0, created = ?, updated = ? WHERE id = ?")
        .bind(now, now, existing.id)
        .run();
    }
    return { id: existing.id, name: existing.name, done: false };
  }
  const id = crypto.randomUUID();
  await env.DB.prepare("INSERT INTO items (id, name, key, done, created, updated) VALUES (?, ?, ?, 0, ?, ?)")
    .bind(id, name, key, now, now)
    .run();
  return { id, name, done: false };
}

async function handleApi(request, env, path) {
  if (path === "/api/login" && request.method === "POST") {
    const body = await request.json().catch(() => ({}));
    if (!env.HASLO || !timingSafeEqual(String(body.password || ""), env.HASLO)) {
      // Małe opóźnienie utrudnia zgadywanie hasła.
      await new Promise((r) => setTimeout(r, 800));
      return json({ error: "Złe hasło" }, 401);
    }
    const token = await sessionToken(env);
    return json({ ok: true }, 200, {
      "Set-Cookie": `sesja=${token}; Path=/; Max-Age=31536000; HttpOnly; Secure; SameSite=Lax`,
    });
  }
  if (path === "/api/logout" && request.method === "POST") {
    return json({ ok: true }, 200, { "Set-Cookie": "sesja=; Path=/; Max-Age=0; HttpOnly; Secure; SameSite=Lax" });
  }

  if (!(await isAuthorized(request, env))) return json({ error: "Brak autoryzacji" }, 401);
  await ensureSchema(env);

  if (path === "/api/list" && request.method === "GET") {
    return json({ items: await listItems(env) });
  }
  const body = request.method === "POST" ? await request.json().catch(() => ({})) : {};
  if (path === "/api/add" && request.method === "POST") {
    const item = await addItem(env, body.name);
    return item ? json(item) : json({ error: "Pusta nazwa" }, 400);
  }
  if (path === "/api/done" && request.method === "POST") {
    await env.DB.prepare("UPDATE items SET done = ?, updated = ? WHERE id = ?")
      .bind(body.done ? 1 : 0, Date.now(), String(body.id || ""))
      .run();
    return json({ ok: true });
  }
  if (path === "/api/delete" && request.method === "POST") {
    await env.DB.prepare("DELETE FROM items WHERE id = ?").bind(String(body.id || "")).run();
    return json({ ok: true });
  }
  if (path === "/api/clear-done" && request.method === "POST") {
    await env.DB.prepare("DELETE FROM items WHERE done = 1").run();
    return json({ ok: true });
  }
  return json({ error: "Nie ma takiej ścieżki" }, 404);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname.startsWith("/api/")) {
      try {
        return await handleApi(request, env, url.pathname);
      } catch (e) {
        return json({ error: String(e && e.message ? e.message : e) }, 500);
      }
    }
    if (url.pathname === "/manifest.webmanifest") {
      return new Response(JSON.stringify(MANIFEST), {
        headers: { "Content-Type": "application/manifest+json" },
      });
    }
    return new Response(PAGE, {
      headers: {
        "Content-Type": "text/html; charset=utf-8",
        "Cache-Control": "no-store",
        "Content-Security-Policy":
          "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data:",
      },
    });
  },
};

const ICON =
  "data:image/svg+xml," +
  encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96"><rect width="96" height="96" rx="20" fill="#c62828"/>' +
      '<path d="M26 50l14 14 30-32" fill="none" stroke="#fbc02d" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/></svg>'
  );

const MANIFEST = {
  name: "Lista zakupów",
  short_name: "Zakupy",
  start_url: "/",
  display: "standalone",
  background_color: "#ffffff",
  theme_color: "#c62828",
  icons: [{ src: ICON, sizes: "any", type: "image/svg+xml" }],
};

const PAGE = `<!doctype html>
<html lang="pl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#c62828">
<meta name="apple-mobile-web-app-capable" content="yes">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="${ICON}"><link rel="apple-touch-icon" href="${ICON}">
<title>Lista zakupów</title>
<style>
:root{--bg:#fff;--fg:#141414;--mut:#6b6b6b;--line:#e4e4e4;--card:#f6f6f6;--acc:#c62828;--hl:#fbc02d}
@media(prefers-color-scheme:dark){:root{--bg:#121212;--fg:#f1f1f1;--mut:#9a9a9a;--line:#2a2a2a;--card:#1c1c1c}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;font:18px/1.35 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:560px;margin:0 auto;padding:16px 16px calc(24px + env(safe-area-inset-bottom))}
header{display:flex;align-items:baseline;justify-content:space-between;gap:12px;margin:8px 0 16px}
h1{font-size:26px;margin:0;color:var(--acc)}
.count{color:var(--mut);font-size:15px}
form.add{display:flex;gap:8px;margin-bottom:12px}
input{flex:1;min-width:0;padding:14px;font:inherit;border:2px solid var(--line);border-radius:12px;background:var(--bg);color:var(--fg)}
input:focus{outline:none;border-color:var(--acc)}
button{font:inherit;border:0;border-radius:12px;padding:14px 16px;cursor:pointer}
.primary{background:var(--acc);color:#fff}
ul{list-style:none;margin:0;padding:0}
li{display:flex;align-items:center;gap:14px;padding:14px 8px;border-bottom:1px solid var(--line);cursor:pointer;user-select:none}
li .box{flex:none;width:26px;height:26px;border:2.5px solid var(--fg);border-radius:7px;display:grid;place-items:center}
li.done .box{background:var(--hl);border-color:var(--hl)}
li.done .box::after{content:"";width:12px;height:7px;border:3px solid #141414;border-top:0;border-right:0;transform:translateY(-2px) rotate(-45deg)}
li .name{flex:1;overflow-wrap:anywhere}
li.done .name{text-decoration:line-through;color:var(--mut)}
li .del{flex:none;background:none;color:var(--mut);padding:4px 8px;font-size:22px;line-height:1}
h2{font-size:15px;color:var(--mut);text-transform:uppercase;letter-spacing:.05em;margin:24px 0 4px}
.empty{color:var(--mut);text-align:center;padding:32px 0}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:20px}
.actions button{background:var(--card);color:var(--fg)}
.ghost{background:none;color:var(--acc);border:1.5px solid var(--acc)}
.status{color:var(--mut);font-size:13px;text-align:center;margin-top:16px;min-height:1em}
#login{display:none;margin-top:40px}
#login p{color:var(--mut)}
#login form{display:flex;flex-direction:column;gap:12px}
.err{color:var(--acc)}
</style></head><body><main>
<section id="app" hidden>
  <header><h1>Lista zakupów</h1><span class="count" id="count"></span></header>
  <form class="add" id="addForm"><input id="addInput" placeholder="Dodaj, np. mleko" autocomplete="off" enterkeyhint="done"><button class="primary">Dodaj</button></form>
  <ul id="todo"></ul>
  <div id="doneWrap" hidden><h2>Kupione</h2><ul id="done"></ul></div>
  <div class="actions">
    <button id="sms">Wyślij SMS-em</button>
    <button id="share">Udostępnij</button>
    <button id="clearDone" class="ghost">Wyczyść kupione</button>
    <button id="logout" class="ghost">Wyloguj</button>
  </div>
  <p class="status" id="status"></p>
</section>
<section id="login">
  <h1>Lista zakupów</h1>
  <p>Podaj hasło do listy.</p>
  <form id="loginForm"><input id="password" type="password" placeholder="Hasło" autocomplete="current-password"><button class="primary">Zaloguj</button></form>
  <p class="err" id="loginErr"></p>
</section>
</main>
<script>
let items = [];
const $ = (id) => document.getElementById(id);

async function api(path, body) {
  const r = await fetch(path, body === undefined ? {} : {
    method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)});
  if (r.status === 401) { showLogin(); throw new Error("401"); }
  if (!r.ok) throw new Error("HTTP " + r.status);
  return r.json();
}

function showLogin() { $("app").hidden = true; $("login").style.display = "block"; }
function showApp() { $("login").style.display = "none"; $("app").hidden = false; }

function row(item) {
  const li = document.createElement("li");
  if (item.done) li.className = "done";
  const box = document.createElement("span"); box.className = "box";
  const name = document.createElement("span"); name.className = "name"; name.textContent = item.name;
  li.append(box, name);
  if (item.done) {
    const del = document.createElement("button"); del.className = "del"; del.textContent = "\\u2715";
    del.title = "Usuń na stałe";
    del.onclick = (e) => { e.stopPropagation(); items = items.filter((i) => i.id !== item.id); render();
      api("/api/delete", {id: item.id}).catch(sync); };
    li.append(del);
  }
  li.onclick = () => { item.done = !item.done; render(); api("/api/done", {id: item.id, done: item.done}).catch(sync); };
  return li;
}

function render() {
  const todo = items.filter((i) => !i.done), done = items.filter((i) => i.done);
  $("todo").replaceChildren(...todo.map(row));
  if (!todo.length) $("todo").innerHTML = '<li class="empty">Wszystko kupione 🎉</li>';
  $("done").replaceChildren(...done.map(row));
  $("doneWrap").hidden = !done.length;
  $("count").textContent = todo.length ? "do kupienia: " + todo.length : "";
}

function listText() {
  return "Lista zakupów:\\n" + items.filter((i) => !i.done).map((i) => "- " + i.name).join("\\n");
}

async function sync() {
  try {
    const data = await api("/api/list");
    items = data.items; showApp(); render();
    $("status").textContent = "Zsynchronizowano " + new Date().toLocaleTimeString("pl", {hour: "2-digit", minute: "2-digit"});
  } catch (e) {
    if (e.message !== "401") $("status").textContent = "Brak połączenia – spróbuję ponownie";
  }
}

$("addForm").onsubmit = async (e) => {
  e.preventDefault();
  const names = $("addInput").value.split(/[,;]| i /).map((s) => s.trim()).filter(Boolean);
  $("addInput").value = "";
  for (const n of names) await api("/api/add", {name: n}).catch(() => {});
  sync();
};
$("sms").onclick = () => {
  // "sms:?&body=" działa na Androidzie i iPhonie.
  location.href = "sms:?&body=" + encodeURIComponent(listText());
};
if (!navigator.share) $("share").hidden = true;
$("share").onclick = () => navigator.share({title: "Lista zakupów", text: listText()}).catch(() => {});
$("clearDone").onclick = async () => {
  items = items.filter((i) => !i.done); render();
  await api("/api/clear-done", {}).catch(() => {}); sync();
};
$("logout").onclick = async () => { await api("/api/logout", {}).catch(() => {}); showLogin(); };
$("loginForm").onsubmit = async (e) => {
  e.preventDefault(); $("loginErr").textContent = "";
  const r = await fetch("/api/login", {method: "POST", headers: {"Content-Type": "application/json"},
    body: JSON.stringify({password: $("password").value})});
  if (r.ok) { $("password").value = ""; sync(); } else { $("loginErr").textContent = "Złe hasło"; }
};

sync();
setInterval(() => { if (document.visibilityState === "visible") sync(); }, 5000);
document.addEventListener("visibilitychange", () => { if (document.visibilityState === "visible") sync(); });
</script></body></html>`;
