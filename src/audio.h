#pragma once

#include <Arduino.h>

// Mikrofon i głośnik przez kodek ES8311 (I2S 16 kHz, 16 bit).
namespace audio {

constexpr uint32_t kSampleRate = 16000;

bool begin();

struct RecordStats {
    int32_t rawPeak = 0;    // największa próbka przed normalizacją (0-32767)
    int32_t level = 0;      // 99,9 percentyl przed normalizacją
    float gain = 1;         // zastosowane wzmocnienie programowe
    bool rightChannel = false;
    bool silent = false;    // za cicho, żeby wysyłać do rozpoznania
    uint32_t ms = 0;
};

// Nagrywa, dopóki stillRecording() zwraca true (np. przycisk wciśnięty),
// maksymalnie maxMs. Zwraca plik WAV (mono 16 kHz) w PSRAM – zwolnij free().
// nullptr tylko przy braku pamięci; cisza jest zwracana z stats.silent = true.
uint8_t* recordWav(size_t& wavLen, uint32_t maxMs, bool (*stillRecording)(), RecordStats& stats);

// Krótkie sygnały dźwiękowe.
void beepCaptured();  // nagranie zakończone, wysyłam do rozpoznania
void beepOk();     // polecenie wykonane
void beepError();  // nie zrozumiałem / błąd

}  // namespace audio
