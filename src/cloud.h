#pragma once

#include <Arduino.h>

// Synchronizacja listy z aplikacją w telefonie (Cloudflare Worker z cloud/worker.js).
// Działa we własnym zadaniu: co minutę i zaraz po każdej zmianie na lodówce.
namespace cloud {

// onListChanged: wywoływane (z zadania synchronizacji), gdy lista zmieniła się w telefonie.
void begin(void (*onListChanged)());

// Poproś o synchronizację wkrótce (np. po zmianie listy głosem).
void requestSync();

// Opis ostatniej synchronizacji (do panelu WWW).
String status();

// Jednorazowy test połączenia i hasła (do /ustawienia).
String test();

}  // namespace cloud
