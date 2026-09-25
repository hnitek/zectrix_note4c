#pragma once

#include <Adafruit_GFX.h>

// Kolory panelu BWRY (wartości 2-bitowe wysyłane do SSD2683).
enum EpdColor : uint16_t {
    EPD_BLACK = 0,
    EPD_WHITE = 1,
    EPD_YELLOW = 2,
    EPD_RED = 3,
    // Kolory mieszane: szachownica dwóch kolorów panelu (z odległości wygląda jak kolor pośredni).
    EPD_GRAY = 4,          // czarny + biały
    EPD_ORANGE = 5,        // czerwony + żółty
    EPD_LIGHT_YELLOW = 6,  // żółty + biały
    EPD_PINK = 7,          // czerwony + biały
};

// Kolor panelu (0-3) dla danego piksela, z uwzględnieniem kolorów mieszanych.
inline uint8_t epdResolveColor(uint16_t color, int x, int y) {
    static const uint8_t mix[4][2] = {
        {EPD_BLACK, EPD_WHITE}, {EPD_RED, EPD_YELLOW}, {EPD_YELLOW, EPD_WHITE}, {EPD_RED, EPD_WHITE}};
    if (color < 4) return color;
    return mix[(color - 4) & 3][(x ^ y) & 1];
}

// Czterokolorowy e-paper 400x300 z NOTE4C (kontroler SSD2683).
// Rysowanie przez Adafruit_GFX do bufora 2 bpp (30 kB), potem display().
// Pełne odświeżenie trwa ok. 20-25 s – odświeżaj tylko przy zmianach.
class Epd4Color : public Adafruit_GFX {
public:
    static constexpr int WIDTH_PX = 400;
    static constexpr int HEIGHT_PX = 300;

    Epd4Color();
    bool begin();
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillScreen(uint16_t color) override;
    void display();  // wysyła bufor i blokuje do końca odświeżania

private:
    void reset();
    void waitBusy(uint32_t timeoutMs);
    void command(uint8_t c);
    void data(uint8_t d);
    void powerOn();
    void powerOff();

    uint8_t* buf_ = nullptr;
};
