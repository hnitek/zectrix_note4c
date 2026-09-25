#pragma once

// Pinout ZecTrix NOTE4C (ESP32-S3 N16R8). Źródło: konfiguracja płytki
// "zectrix-s3-epaper-4.2" z firmware'u społeczności (wakewon/NOTE4C-firmware-Rapid).

// --- Zasilanie ---
#define PIN_VBAT_HOLD   17  // podtrzymanie zasilania z baterii (HIGH = włączone)
#define PIN_EPD_POWER    6  // zasilanie panelu e-paper
#define PIN_AUDIO_POWER 42  // zasilanie kodeka audio + podciągnięcia I2C
#define PIN_AUDIO_PA    46  // wzmacniacz głośnika
#define PIN_LED          3  // zielona dioda, aktywna stanem niskim

// --- Przyciski (aktywne stanem niskim) ---
#define PIN_BTN_CONFIRM  0  // środkowy / BOOT  -> naciśnij i mów
#define PIN_BTN_UP      39  // góra             -> odśwież ekran
#define PIN_BTN_DOWN    18  // dół (także włącznik zasilania)

// --- Wyświetlacz SSD2683, 400x300, 4 kolory (czarny/biały/żółty/czerwony) ---
#define PIN_EPD_DC   10
#define PIN_EPD_CS   11
#define PIN_EPD_SCK  12
#define PIN_EPD_MOSI 13
#define PIN_EPD_RST   9
#define PIN_EPD_BUSY  8  // LOW = zajęty

// --- Kodek audio ES8311 ---
#define PIN_I2S_MCLK 14
#define PIN_I2S_BCLK 15
#define PIN_I2S_WS   38
#define PIN_I2S_DIN  16  // dane z mikrofonu (ES8311 -> ESP32)
#define PIN_I2S_DOUT 45  // dane do głośnika (ESP32 -> ES8311)
#define PIN_I2C_SDA  47
#define PIN_I2C_SCL  48
#define ES8311_ADDR  0x18
