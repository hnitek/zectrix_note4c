#include "state.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "shopping.h"

namespace state {
namespace {

constexpr size_t kMaxItems = 60;
ShoppingList list;
SemaphoreHandle_t mutex = nullptr;
Preferences prefs;

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
}

String join(const std::vector<std::string>& items) {
    String s;
    for (const auto& i : items) {
        if (s.length()) s += ", ";
        s += i.c_str();
    }
    return s;
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
}

std::vector<std::string> listItems() {
    Lock lock;
    return list.items();
}

bool applyText(const std::string& text, String* summary) {
    const VoiceCommand cmd = parseVoiceCommand(text);
    Lock lock;
    bool changed = false;
    if (cmd.kind == VoiceCommand::Kind::Add) {
        std::vector<std::string> added;
        for (const auto& item : cmd.items) {
            if (list.items().size() >= kMaxItems) break;
            if (list.add(item)) added.push_back(item);
        }
        changed = !added.empty();
        if (summary) *summary = changed ? "Dodano: " + join(added) : "Już jest na liście";
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
    if (changed) save();
    return changed;
}

bool removeAt(size_t index) {
    Lock lock;
    if (!list.removeAt(index)) return false;
    save();
    return true;
}

bool clear() {
    Lock lock;
    if (list.items().empty()) return false;
    list.clear();
    save();
    return true;
}

}  // namespace state
