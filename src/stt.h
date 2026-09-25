#pragma once

#include <Arduino.h>

// Zamiana mowy na tekst przez API zgodne z OpenAI (/v1/audio/transcriptions).
namespace stt {

// Zwraca rozpoznany tekst; pusty String przy błędzie (szczegóły w logu).
String transcribe(const uint8_t* wav, size_t wavLen);

}  // namespace stt
