#include "epd.h"

#include <Arduino.h>
#include <SPI.h>

#include "board.h"

namespace {
constexpr int kBytesPerRow = Epd4Color::WIDTH_PX / 4;  // 4 piksele na bajt
constexpr size_t kBufSize = kBytesPerRow * Epd4Color::HEIGHT_PX;
SPIClass epdSpi(HSPI);
const SPISettings kSpiSettings(10000000, MSBFIRST, SPI_MODE0);

uint8_t solidByte(uint8_t c) { return (c << 6) | (c << 4) | (c << 2) | c; }
}  // namespace

Epd4Color::Epd4Color() : Adafruit_GFX(WIDTH_PX, HEIGHT_PX) {}

bool Epd4Color::begin() {
    buf_ = static_cast<uint8_t*>(ps_malloc(kBufSize));
    if (!buf_) buf_ = static_cast<uint8_t*>(malloc(kBufSize));
    if (!buf_) return false;
    fillScreen(EPD_WHITE);

    pinMode(PIN_EPD_CS, OUTPUT);
    pinMode(PIN_EPD_DC, OUTPUT);
    pinMode(PIN_EPD_RST, OUTPUT);
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_POWER, OUTPUT);
    digitalWrite(PIN_EPD_CS, HIGH);
    digitalWrite(PIN_EPD_RST, HIGH);
    epdSpi.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, -1);
    return true;
}

void Epd4Color::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!buf_ || x < 0 || y < 0 || x >= width() || y >= height()) return;
    // Obsługa setRotation() z Adafruit_GFX.
    switch (getRotation()) {
        case 1: std::swap(x, y); x = WIDTH_PX - 1 - x; break;
        case 2: x = WIDTH_PX - 1 - x; y = HEIGHT_PX - 1 - y; break;
        case 3: std::swap(x, y); y = HEIGHT_PX - 1 - y; break;
        default: break;
    }
    const size_t idx = y * kBytesPerRow + (x >> 2);
    const uint8_t shift = 6 - ((x & 3) << 1);
    buf_[idx] = (buf_[idx] & ~(0x03 << shift)) | ((color & 0x03) << shift);
}

void Epd4Color::fillScreen(uint16_t color) {
    if (buf_) memset(buf_, solidByte(color & 0x03), kBufSize);
}

void Epd4Color::command(uint8_t c) {
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, LOW);
    epdSpi.transfer(c);
    digitalWrite(PIN_EPD_CS, HIGH);
}

void Epd4Color::data(uint8_t d) {
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    epdSpi.transfer(d);
    digitalWrite(PIN_EPD_CS, HIGH);
}

void Epd4Color::waitBusy(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (digitalRead(PIN_EPD_BUSY) == LOW) {
        if (millis() - start > timeoutMs) {
            log_w("EPD busy timeout");
            return;
        }
        delay(10);
    }
}

void Epd4Color::powerOn() { digitalWrite(PIN_EPD_POWER, HIGH); }
void Epd4Color::powerOff() { digitalWrite(PIN_EPD_POWER, LOW); }

void Epd4Color::reset() {
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(10);
    digitalWrite(PIN_EPD_RST, LOW);
    delay(20);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(10);
    waitBusy(5000);
}

void Epd4Color::display() {
    if (!buf_) return;
    powerOn();
    delay(10);
    epdSpi.beginTransaction(kSpiSettings);
    reset();

    // Sekwencja z referencyjnego kodu producenta panelu (SSD2683, BWRY 400x300).
    command(0xE9);
    data(0x01);

    command(0x10);  // zapis obrazu (2 bpp)
    waitBusy(5000);
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    for (int y = 0; y < HEIGHT_PX; ++y) {
        epdSpi.writeBytes(buf_ + y * kBytesPerRow, kBytesPerRow);
    }
    digitalWrite(PIN_EPD_CS, HIGH);

    command(0x04);  // power on
    waitBusy(10000);
    delay(10);
    command(0x12);  // odświeżenie
    data(0x00);
    delay(10);
    waitBusy(120000);
    command(0x02);  // power off
    data(0x00);
    waitBusy(10000);
    delay(20);
    command(0x07);  // deep sleep
    data(0xA5);
    epdSpi.endTransaction();

    // Reset w stanie niskim, żeby nie zasilać kontrolera przez linię RST.
    digitalWrite(PIN_EPD_RST, LOW);
    powerOff();
}
