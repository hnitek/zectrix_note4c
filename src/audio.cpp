#include "audio.h"

#include <ESP_I2S.h>
#include <Wire.h>
#include <math.h>

#include "board.h"

namespace audio {
namespace {

I2SClass i2s;
bool ready = false;
constexpr int32_t kSilencePeak = 300;   // poniżej tego szczytu uznajemy nagranie za ciszę
constexpr uint32_t kMinRecordMs = 300;  // krótkie kliknięcie i tak nagra tyle
constexpr uint32_t kPostRollMs = 350;   // dograwanie po puszczeniu przycisku

bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

// Sekwencja jak w esp_codec_dev (Espressif) dla ES8311 w trybie slave,
// MCLK = 256 * 16 kHz = 4.096 MHz, 16 bit, mikrofon analogowy.
bool initCodec() {
    struct { uint8_t reg, val; } seq[] = {
        {0x44, 0x08}, {0x44, 0x08},  // odporność I2C na zakłócenia (zapis 2x wg Espressif)
        {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10},
        {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F},
        {0x00, 0x80},                // power on, slave
        {0x01, 0x3F},                // zegary z pinu MCLK
        {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A},
        // Dzielniki zegara dla 4.096 MHz / 16 kHz
        {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10},
        {0x07, 0x00}, {0x08, 0xFF}, {0x06, 0x03},
        {0x09, 0x0C}, {0x0A, 0x0C},  // I2S, 16 bit
        // Start ADC + DAC
        {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A}, {0x0D, 0x01},
        {0x15, 0x40}, {0x37, 0x08}, {0x45, 0x00},
        {0x16, 0x05},                // wzmocnienie mikrofonu 30 dB
        {0x32, 0xB4},                // głośność DAC
        {0x31, 0x00},                // bez wyciszenia
    };
    for (const auto& r : seq) {
        if (!writeReg(r.reg, r.val)) {
            log_e("ES8311: brak odpowiedzi (reg 0x%02X)", r.reg);
            return false;
        }
    }
    return true;
}

void writeWavHeader(uint8_t* h, uint32_t dataLen) {
    auto put32 = [](uint8_t* p, uint32_t v) { memcpy(p, &v, 4); };
    auto put16 = [](uint8_t* p, uint16_t v) { memcpy(p, &v, 2); };
    memcpy(h, "RIFF", 4);
    put32(h + 4, 36 + dataLen);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);
    put16(h + 20, 1);  // PCM
    put16(h + 22, 1);  // mono
    put32(h + 24, kSampleRate);
    put32(h + 28, kSampleRate * 2);
    put16(h + 32, 2);
    put16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    put32(h + 40, dataLen);
}

void tone(float freq, uint32_t ms, float amp = 0.35f) {
    if (!ready) return;
    digitalWrite(PIN_AUDIO_PA, HIGH);
    const size_t frames = kSampleRate * ms / 1000;
    int16_t block[256 * 2];
    size_t done = 0;
    while (done < frames) {
        size_t n = std::min<size_t>(256, frames - done);
        for (size_t i = 0; i < n; ++i) {
            // Łagodne narastanie/opadanie, żeby nie trzaskało.
            float env = 1.0f;
            const size_t pos = done + i, fade = kSampleRate / 200;
            if (pos < fade) env = float(pos) / fade;
            if (frames - pos < fade) env = float(frames - pos) / fade;
            int16_t s = int16_t(sinf(2 * PI * freq * pos / kSampleRate) * 32767 * amp * env);
            block[2 * i] = s;
            block[2 * i + 1] = s;
        }
        i2s.write(reinterpret_cast<uint8_t*>(block), n * 4);
        done += n;
    }
    // Cisza na opróżnienie bufora DMA przed wyłączeniem wzmacniacza.
    memset(block, 0, sizeof(block));
    for (int i = 0; i < 8; ++i) i2s.write(reinterpret_cast<uint8_t*>(block), sizeof(block));
    digitalWrite(PIN_AUDIO_PA, LOW);
}

}  // namespace

bool begin() {
    pinMode(PIN_AUDIO_POWER, OUTPUT);
    digitalWrite(PIN_AUDIO_POWER, HIGH);
    pinMode(PIN_AUDIO_PA, OUTPUT);
    digitalWrite(PIN_AUDIO_PA, LOW);
    delay(50);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

    // Zegar MCLK musi działać, zanim skonfigurujemy kodek.
    i2s.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_I2S_MCLK);
    if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        log_e("I2S begin failed");
        return false;
    }
    delay(10);
    ready = initCodec();
    return ready;
}

uint8_t* recordWav(size_t& wavLen, uint32_t maxMs, bool (*stillRecording)()) {
    wavLen = 0;
    if (!ready) return nullptr;
    const size_t maxFrames = kSampleRate * maxMs / 1000;
    uint8_t* wav = static_cast<uint8_t*>(ps_malloc(44 + maxFrames * 2));
    // Czytamy stereo; później wybieramy kanał, na którym faktycznie jest mikrofon.
    int16_t* right = static_cast<int16_t*>(ps_malloc(maxFrames * 2));
    if (!wav || !right) {
        free(wav);
        free(right);
        return nullptr;
    }
    int16_t* mono = reinterpret_cast<int16_t*>(wav + 44);

    // Nagrywamy od razu po wciśnięciu, bez piknięcia na start: wcześniej sygnał dźwiękowy
    // i jego wybrzmiewanie zjadały ~0,35 s, czyli początek krótkich słów ("mleko" -> "ko").
    // Bufor DMA zawiera też ułamek sekundy sprzed wciśnięcia – to nie przeszkadza.
    int16_t frame[256 * 2];
    double energyL = 0, energyR = 0;
    size_t frames = 0;
    uint32_t releasedAt = 0;
    const uint32_t start = millis();
    while (frames < maxFrames) {
        const uint32_t now = millis();
        if (!releasedAt && now - start > kMinRecordMs && !stillRecording()) releasedAt = now;
        // Po puszczeniu przycisku nagrywamy jeszcze chwilę, żeby nie uciąć końcówki słowa.
        if (releasedAt && now - releasedAt > kPostRollMs) break;
        const size_t got = i2s.readBytes(reinterpret_cast<char*>(frame), sizeof(frame)) / 4;
        for (size_t i = 0; i < got && frames < maxFrames; ++i, ++frames) {
            mono[frames] = frame[2 * i];
            right[frames] = frame[2 * i + 1];
            energyL += double(frame[2 * i]) * frame[2 * i];
            energyR += double(frame[2 * i + 1]) * frame[2 * i + 1];
        }
    }
    if (energyR > energyL) memcpy(mono, right, frames * 2);
    free(right);

    // Szczyt liczony jako 99,9 percentyl, żeby pojedynczy trzask przycisku
    // nie zaniżał wzmocnienia.
    static uint32_t hist[128];
    memset(hist, 0, sizeof(hist));
    for (size_t i = 0; i < frames; ++i) hist[std::min(127, abs(mono[i]) >> 8)]++;
    int32_t peak = 1;
    size_t above = 0;
    for (int b = 127; b >= 0; --b) {
        above += hist[b];
        if (above > frames / 1000) {
            peak = std::max<int32_t>(1, (b + 1) << 8);
            break;
        }
    }
    if (peak < kSilencePeak) {
        // Whisper na ciszy potrafi "zmyślić" tekst – nie wysyłamy jej wcale.
        log_w("Cisza (szczyt %d) – pomijam", int(peak));
        free(wav);
        return nullptr;
    }
    // Normalizacja do ok. -6 dBFS, z miękkim ograniczeniem pojedynczych szczytów.
    const float gain = std::min(10.0f, 16000.0f / peak);
    for (size_t i = 0; i < frames; ++i) {
        float v = mono[i] * gain;
        if (v > 30000.0f) v = 30000.0f + (v - 30000.0f) * 0.1f;
        if (v < -30000.0f) v = -30000.0f + (v + 30000.0f) * 0.1f;
        mono[i] = int16_t(std::max(-32767.0f, std::min(32767.0f, v)));
    }
    log_i("Nagrano %u ms, szczyt %d, wzmocnienie %.1f", unsigned(frames * 1000 / kSampleRate),
          int(peak), gain);

    writeWavHeader(wav, frames * 2);
    wavLen = 44 + frames * 2;
    return wav;
}

void beepCaptured() { tone(1200, 60, 0.25f); }
void beepOk() {
    tone(660, 90);
    tone(990, 140);
}
void beepError() { tone(300, 350); }

}  // namespace audio
