#pragma once

// Prosty panel WWW w sieci lokalnej (http://lodowka.local): podgląd i edycja listy z telefonu.
#include <Arduino.h>

namespace web {

// onChange wywoływane po każdej zmianie listy z poziomu przeglądarki.
void begin(void (*onChange)());
void loop();

// Przejmuje bufor nagrania (zwalnia poprzednie); dostępne pod /ostatnie.wav.
void setLastRecording(uint8_t* wav, size_t len, const String& recognized, const String& diagnostics);

}  // namespace web
