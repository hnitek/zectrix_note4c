#pragma once

#include <Arduino.h>

#include <map>
#include <string>
#include <vector>

#include "sync.h"

// Lista zakupów zapisywana w pamięci flash (NVS), bezpieczna wątkowo.
namespace state {

void begin();
std::vector<std::string> listItems();  // kopia

// Interpretuje tekst jak polecenie głosowe. summary (opcjonalnie) dostaje opis
// wyniku do wyświetlenia, np. "Dodano: Mleko, Chleb". Zwraca true, jeśli lista się zmieniła.
bool applyText(const std::string& text, String* summary);
bool removeAt(size_t index);
bool clear();

// --- Synchronizacja z aplikacją w chmurze ---
void setSyncEnabled(bool enabled);

struct SyncSnapshot {
    std::vector<std::string> items;
    std::map<std::string, std::string> idByKey;
    std::vector<std::string> pendingDone;
};
SyncSnapshot syncSnapshot();

// Zapomina powiązania z chmurą (np. po zmianie adresu aplikacji).
void resetSync();

// Usuwa z kolejki id, które chmura już potwierdziła jako kupione.
void confirmDone(const std::vector<std::string>& ids);

// Wynik rundy synchronizacji: stan z chmury, id potwierdzone jako kupione i pozycje
// utworzone w chmurze w tej rundzie. Zwraca true, jeśli lista na lodówce się zmieniła.
bool applySync(const std::vector<RemoteItem>& remote, const std::vector<std::string>& doneSent,
               const std::vector<RemoteItem>& created);

}  // namespace state
