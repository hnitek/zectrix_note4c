#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

// Zamiana mowy na tekst przez API zgodne z OpenAI (/v1/audio/transcriptions).
namespace stt {

// Zwraca rozpoznany tekst; pusty String przy błędzie (szczegóły w logu).
// listItems: bieżąca lista – trafia do podpowiedzi słownictwa.
// error (opcjonalnie) dostaje opis problemu do pokazania w /nagranie.
String transcribe(const uint8_t* wav, size_t wavLen, const std::vector<std::string>& listItems,
                  String* error = nullptr);

}  // namespace stt
