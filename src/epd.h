#pragma once

#include <Adafruit_GFX.h>

// Kolory panelu BWRY (wartości 2-bitowe wysyłane do SSD2683).
enum EpdColor : uint16_t {
    EPD_BLACK = 0,
    EPD_WHITE = 1,
    EPD_YELLOW = 2,
    EPD_RED = 3,
};

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
