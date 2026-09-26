#include "state.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include <algorithm>
#include <set>

#include "shopping.h"

namespace state {
namespace {

constexpr size_t kMaxItems = 60;
ShoppingList list;
SemaphoreHandle_t mutex = nullptr;
Preferences prefs;

// Synchronizacja z aplikacją w chmurze.
bool syncEnabled = false;
std::map<std::string, std::string> idByKey;  // itemKey(nazwa) -> id w chmurze
std::vector<std::string> pendingDone;        // kupione na lodówce, jeszcze niewysłane

struct Lock {
    Lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
};

void save() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& item : list.items()) arr.add(item);
    String out;
    serializeJson(doc, out);
    prefs.putString("items", out);

    JsonDocument sync;
    JsonObject ids = sync["ids"].to<JsonObject>();
    for (const auto& kv : idByKey) ids[kv.first] = kv.second;
    JsonArray done = sync["done"].to<JsonArray>();
    for (const auto& id : pendingDone) done.add(id);
    String s;
    serializeJson(sync, s);
    prefs.putString("sync", s);
}

String join(const std::vector<std::string>& items) {
    String s;
    for (const auto& i : items) {
        if (s.length()) s += ", ";
        s += i.c_str();
    }
    return s;
}

// Po każdej lokalnej zmianie: to, co zniknęło z listy, trzeba oznaczyć w chmurze jako kupione;
// to, co doszło, jest nowe i dostanie id przy najbliższej synchronizacji.
void trackChanges(const std::vector<std::string>& before) {
    if (!syncEnabled) return;
    std::set<std::string> beforeKeys, afterKeys;
    for (const auto& n : before) beforeKeys.insert(itemKey(n));
    for (const auto& n : list.items()) afterKeys.insert(itemKey(n));
    for (const auto& k : beforeKeys) {
        if (afterKeys.count(k)) continue;
        const auto it = idByKey.find(k);
        if (it != idByKey.end()) {
            pendingDone.push_back(it->second);
            idByKey.erase(it);
        }
    }
    for (const auto& k : afterKeys) {
        if (!beforeKeys.count(k)) idByKey.erase(k);
    }
}

}  // namespace

void begin() {
    mutex = xSemaphoreCreateMutex();
    prefs.begin("lodowka", false);
    JsonDocument doc;
    if (!deserializeJson(doc, prefs.getString("items", "[]"))) {
        std::vector<std::string> items;
        for (JsonVariant v : doc.as<JsonArray>()) items.push_back(v.as<std::string>());
        list.setItems(std::move(items));
    }
    JsonDocument sync;
    if (!deserializeJson(sync, prefs.getString("sync", "{}"))) {
        for (JsonPair kv : sync["ids"].as<JsonObject>()) idByKey[kv.key().c_str()] = kv.value().as<std::string>();
        for (JsonVariant v : sync["done"].as<JsonArray>()) pendingDone.push_back(v.as<std::string>());
    }
}

void setSyncEnabled(bool enabled) {
    Lock lock;
    syncEnabled = enabled;
}

std::vector<std::string> listItems() {
    Lock lock;
    return list.items();
}

bool applyText(const std::string& text, String* summary) {
    const VoiceCommand cmd = parseVoiceCommand(text);
    Lock lock;
    const std::vector<std::string> before = list.items();
    bool changed = false;
    if (cmd.kind == VoiceCommand::Kind::Add) {
        std::vector<std::string> added;
        for (const auto& item : cmd.items) {
            if (list.items().size() >= kMaxItems) break;
            if (list.add(item)) added.push_back(item);
        }
        changed = !added.empty();
        if (summary) *summary = changed ? "Dodano: " + join(added) : "Już jest na liście: " + join(cmd.items);
    } else if (cmd.kind == VoiceCommand::Kind::Remove) {
        std::vector<std::string> removed;
        for (const auto& item : cmd.items) {
            std::string name;
            if (list.remove(item, &name)) removed.push_back(name);
        }
        changed = !removed.empty();
        if (summary) *summary = changed ? "Usunięto: " + join(removed) : "Nie ma tego na liście";
    } else if (cmd.kind == VoiceCommand::Kind::Undo) {
        const std::string last = list.items().empty() ? "" : list.items().back();
        changed = list.apply(cmd);
        if (summary) *summary = changed ? String("Cofnięto: ") + last.c_str() : "Lista jest pusta";
    } else if (cmd.kind == VoiceCommand::Kind::Clear) {
        changed = list.apply(cmd);
        if (summary) *summary = "Lista wyczyszczona";
    } else if (summary) {
        *summary = String("Nie zrozumiałem: ") + text.c_str();
    }
    if (changed) {
        trackChanges(before);
        save();
    }
    return changed;
}

bool removeAt(size_t index) {
    Lock lock;
    const std::vector<std::string> before = list.items();
    if (!list.removeAt(index)) return false;
    trackChanges(before);
    save();
    return true;
}

bool clear() {
    Lock lock;
    if (list.items().empty()) return false;
    const std::vector<std::string> before = list.items();
    list.clear();
    trackChanges(before);
    save();
    return true;
}

SyncSnapshot syncSnapshot() {
    Lock lock;
    SyncSnapshot s;
    s.items = list.items();
    s.idByKey = idByKey;
    s.pendingDone = pendingDone;
    return s;
}

void resetSync() {
    Lock lock;
    idByKey.clear();
    pendingDone.clear();
    save();
}

void confirmDone(const std::vector<std::string>& ids) {
    if (ids.empty()) return;
    Lock lock;
    for (const auto& id : ids) {
        pendingDone.erase(std::remove(pendingDone.begin(), pendingDone.end(), id), pendingDone.end());
    }
    save();
}

bool applySync(const std::vector<RemoteItem>& remote, const std::vector<std::string>& doneSent,
               const std::vector<RemoteItem>& created) {
    Lock lock;
    // Potwierdzone w chmurze "kupione" nie muszą już czekać w kolejce.
    for (const auto& id : doneSent) {
        pendingDone.erase(std::remove(pendingDone.begin(), pendingDone.end(), id), pendingDone.end());
    }
    // Utworzone w tej rundzie, ale w międzyczasie usunięte z lodówki -> od razu jako kupione.
    std::set<std::string> localKeys;
    for (const auto& n : list.items()) localKeys.insert(itemKey(n));
    for (const auto& c : created) {
        if (!localKeys.count(itemKey(c.name))) pendingDone.push_back(c.id);
    }
    const MergeResult m = mergeWithRemote(remote, list.items(), idByKey, pendingDone);
    const bool listChanged = m.items != list.items();
    const bool idsChanged = m.idByKey != idByKey;
    if (listChanged) list.setItems(m.items);
    idByKey = m.idByKey;
    if (listChanged || idsChanged || !doneSent.empty() || !created.empty()) save();
    return listChanged;
}

}  // namespace state
