#pragma once

#include <Arduino.h>

// Mikrofon i głośnik przez kodek ES8311 (I2S 16 kHz, 16 bit).
namespace audio {

constexpr uint32_t kSampleRate = 16000;

bool begin();

// Nagrywa, dopóki stillRecording() zwraca true (np. przycisk wciśnięty),
// maksymalnie maxMs. Zwraca plik WAV (mono 16 kHz) w PSRAM – zwolnij free().
// nullptr, gdy nagranie to cisza albo zabrakło pamięci.
uint8_t* recordWav(size_t& wavLen, uint32_t maxMs, bool (*stillRecording)());

// Krótkie sygnały dźwiękowe.
void beepCaptured();  // nagranie zakończone, wysyłam do rozpoznania
void beepOk();     // polecenie wykonane
void beepError();  // nie zrozumiałem / błąd

}  // namespace audio
