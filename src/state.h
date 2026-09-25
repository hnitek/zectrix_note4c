#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

// Lista zakupów zapisywana w pamięci flash (NVS), bezpieczna wątkowo.
namespace state {

void begin();
std::vector<std::string> listItems();  // kopia

// Interpretuje tekst jak polecenie głosowe. summary (opcjonalnie) dostaje opis
// wyniku do wyświetlenia, np. "Dodano: Mleko, Chleb". Zwraca true, jeśli lista się zmieniła.
bool applyText(const std::string& text, String* summary);
bool removeAt(size_t index);
bool clear();

}  // namespace state
