#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "epd.h"
#include "weather.h"

namespace ui {

void begin(Epd4Color& epd);

// Rysuje cały ekran do bufora (bez odświeżania panelu).
// footer: ostatnia akcja lub komunikat (pusty = podpowiedź, jak dodawać).
void render(const Weather& w, const std::vector<std::string>& items, const String& footer);

// Pełnoekranowy komunikat (np. brak Wi-Fi).
void renderMessage(const String& title, const String& text);

}  // namespace ui
