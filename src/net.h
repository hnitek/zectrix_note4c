#pragma once

#include <Arduino.h>

namespace net {

// Wi-Fi + NTP + mDNS. Blokuje do połączenia (max timeoutMs).
bool connect(uint32_t timeoutMs);
// mDNS (http://lodowka.local); wywołaj po (ponownym) połączeniu.
void startServices();
bool isConnected();
String ipAddress();

// Żądanie HTTP(S) z weryfikacją certyfikatu (wbudowany pakiet CA z ESP-IDF).
// Zwraca kod HTTP (lub <0 przy błędzie), treść odpowiedzi w `response`.
int request(const char* method, const String& url, const char* contentType, const uint8_t* body,
            size_t bodyLen, String& response, const char* bearerToken = nullptr,
            int timeoutMs = 15000, String* error = nullptr);

}  // namespace net
