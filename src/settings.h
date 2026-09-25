#pragma once

#include <WebServer.h>

// Formularz ustawień: w trybie konfiguracji (własna sieć "Lodowka-Setup")
// i w zwykłym panelu WWW pod /ustawienia.
namespace settings {

void registerRoutes(WebServer& server, const char* path);

// Uruchamia punkt dostępowy z portalem konfiguracyjnym. Nie wraca:
// po zapisaniu ustawień (albo gdy zapisana sieć znów zadziała) restartuje urządzenie.
[[noreturn]] void runSetupPortal();

}  // namespace settings
