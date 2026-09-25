#include "ui.h"

#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

namespace ui {
namespace {

Epd4Color* gfx = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2;

constexpr int W = Epd4Color::WIDTH_PX;
constexpr int H = Epd4Color::HEIGHT_PX;
constexpr int kHeaderH = 32;
constexpr int kSplitX = 158;  // granica pogoda | lista
constexpr int kFooterH = 22;

const char* kDays[] = {"Niedziela", "Poniedziałek", "Wtorek", "Środa",
                       "Czwartek",  "Piątek",       "Sobota"};
const char* kDaysShort[] = {"Nd", "Pn", "Wt", "Śr", "Cz", "Pt", "So"};
const char* kMonths[] = {"stycznia", "lutego",   "marca",    "kwietnia",  "maja",     "czerwca",
                         "lipca",    "sierpnia", "września", "października", "listopada", "grudnia"};

void text(int x, int y, const String& s, const uint8_t* font, uint16_t fg, uint16_t bg = EPD_WHITE) {
    u8g2.setFont(font);
    u8g2.setForegroundColor(fg);
    u8g2.setBackgroundColor(bg);
    u8g2.setCursor(x, y);
    u8g2.print(s);
}

int textWidth(const String& s, const uint8_t* font) {
    u8g2.setFont(font);
    return u8g2.getUTF8Width(s.c_str());
}

// Przycina tekst do szerokości, dodając "..." (z poszanowaniem UTF-8).
String fit(const String& s, const uint8_t* font, int maxW) {
    if (textWidth(s, font) <= maxW) return s;
    String t = s;
    while (t.length() > 0) {
        // Usuń ostatni znak UTF-8.
        int i = t.length() - 1;
        while (i > 0 && (uint8_t(t[i]) & 0xC0) == 0x80) --i;
        t.remove(i);
        if (textWidth(t + "...", font) <= maxW) return t + "...";
    }
    return "...";
}

String formatTemp(float t) { return String(int(lroundf(t))) + "\xC2\xB0"; }  // "°" w UTF-8

// ---------- Ikony pogody (prymitywy GFX w kolorach panelu + kolory mieszane) ----------

void cloudShape(int x, int y, int s, int grow, uint16_t color) {
    gfx->fillCircle(x + s * 30 / 100, y + s * 42 / 100, s * 19 / 100 + grow, color);
    gfx->fillCircle(x + s * 55 / 100, y + s * 32 / 100, s * 25 / 100 + grow, color);
    gfx->fillCircle(x + s * 77 / 100, y + s * 46 / 100, s * 15 / 100 + grow, color);
    gfx->fillRoundRect(x + s * 10 / 100 - grow, y + s * 42 / 100 - grow, s * 80 / 100 + 2 * grow,
                       s * 20 / 100 + 2 * grow, s * 10 / 100 + grow, color);
}

// Chmura z konturem i szarym cieniem, co daje wrażenie głębi.
void cloud(int x, int y, int s, uint16_t fill = EPD_WHITE, bool shadow = true) {
    const int off = std::max(2, s / 16);
    if (shadow) cloudShape(x + off, y + off, s, 2, EPD_GRAY);
    cloudShape(x, y, s, 2, EPD_BLACK);
    cloudShape(x, y, s, 0, fill);
}

void sun(int cx, int cy, int r) {
    // Czerwone promienie, pomarańczowa tarcza z żółtym środkiem.
    const int thick = std::max(1, r / 6);
    for (int a = 0; a < 360; a += 45) {
        const float rad = a * DEG_TO_RAD;
        const int x0 = cx + cosf(rad) * (r + 3), y0 = cy + sinf(rad) * (r + 3);
        const int x1 = cx + cosf(rad) * (r * 17 / 10), y1 = cy + sinf(rad) * (r * 17 / 10);
        for (int d = -thick / 2; d <= thick / 2; ++d) {
            gfx->drawLine(x0 + d, y0, x1 + d, y1, EPD_RED);
            gfx->drawLine(x0, y0 + d, x1, y1 + d, EPD_RED);
        }
    }
    gfx->fillCircle(cx, cy, r + 1, EPD_BLACK);
    gfx->fillCircle(cx, cy, r - 1, EPD_ORANGE);
    gfx->fillCircle(cx - r / 5, cy - r / 5, r * 55 / 100, EPD_YELLOW);
}

void moon(int cx, int cy, int r, uint16_t bg) {
    gfx->fillCircle(cx, cy, r, EPD_YELLOW);
    gfx->fillCircle(cx + r * 45 / 100, cy - r * 30 / 100, r * 85 / 100, bg);
    // Kilka gwiazdek.
    const int st[3][2] = {{r * 12 / 10, r * 8 / 10}, {-r * 12 / 10, -r}, {r * 14 / 10, -r * 12 / 10}};
    for (auto& p : st) {
        gfx->drawFastHLine(cx + p[0] - 2, cy + p[1], 5, EPD_YELLOW);
        gfx->drawFastVLine(cx + p[0], cy + p[1] - 2, 5, EPD_YELLOW);
    }
}

void drops(int x, int y, int s, int n, bool snow) {
    for (int i = 0; i < n; ++i) {
        const int dx = x + s * (22 + i * 56 / (n > 1 ? n - 1 : 1)) / 100;
        const int dy = y + s * 72 / 100 + (i % 2) * s / 10;
        if (snow) {
            const int r = std::max(2, s / 12);
            for (int t = -1; t <= 0; ++t) {
                gfx->drawLine(dx - r, dy + t, dx + r, dy + t, EPD_BLACK);
                gfx->drawLine(dx + t, dy - r, dx + t, dy + r, EPD_BLACK);
            }
            gfx->drawLine(dx - r + 1, dy - r + 1, dx + r - 1, dy + r - 1, EPD_BLACK);
            gfx->drawLine(dx - r + 1, dy + r - 1, dx + r - 1, dy - r + 1, EPD_BLACK);
        } else {
            // Kropla: kółko + trójkąt w górę.
            const int r = std::max(2, s / 18);
            gfx->fillCircle(dx, dy + 2 * r, r, EPD_BLACK);
            gfx->fillTriangle(dx - r, dy + 2 * r, dx + r, dy + 2 * r, dx, dy - r, EPD_BLACK);
        }
    }
}

void bolt(int x, int y, int s) {
    const int cx = x + s / 2, top = y + s * 50 / 100;
    const int16_t px[] = {int16_t(cx + s / 10), int16_t(cx - s / 7), int16_t(cx + s / 40),
                          int16_t(cx - s / 12), int16_t(cx + s / 7), int16_t(cx - s / 40)};
    const int16_t py[] = {int16_t(top), int16_t(top + s * 26 / 100), int16_t(top + s * 24 / 100),
                          int16_t(top + s * 48 / 100), int16_t(top + s * 18 / 100),
                          int16_t(top + s * 20 / 100)};
    // Kontur czarny, wypełnienie żółte.
    for (int o = 1; o >= 0; --o) {
        const uint16_t c = o ? EPD_BLACK : EPD_YELLOW;
        gfx->fillTriangle(px[0] + o, py[0] - o, px[1] - o, py[1] + o, px[2] + o, py[2] + o, c);
        gfx->fillTriangle(px[5] - o, py[5] - o, px[4] + o, py[4] - o, px[3] - o, py[3] + o, c);
    }
}

void weatherIcon(weather::Icon icon, int x, int y, int s, bool night = false,
                 uint16_t bg = EPD_WHITE) {
    using weather::Icon;
    const bool shadow = s >= 40;
    switch (icon) {
        case Icon::Sun:
            if (night) moon(x + s / 2, y + s / 2, s * 26 / 100, bg);
            else sun(x + s / 2, y + s / 2, s * 22 / 100);
            break;
        case Icon::PartlyCloudy:
            if (night) moon(x + s * 62 / 100, y + s * 32 / 100, s * 18 / 100, bg);
            else sun(x + s * 62 / 100, y + s * 34 / 100, s * 17 / 100);
            cloud(x - s / 20, y + s * 25 / 100, s * 80 / 100, EPD_WHITE, shadow);
            break;
        case Icon::Cloud:
            cloud(x + s / 5, y, s * 75 / 100, EPD_GRAY, false);  // druga, dalsza chmura
            cloud(x, y + s / 6, s, EPD_WHITE, shadow);
            break;
        case Icon::Fog:
            cloud(x, y, s, EPD_WHITE, false);
            for (int i = 0; i < 3; ++i) {
                gfx->fillRoundRect(x + s / 10 + (i % 2) * s / 10, y + s * 68 / 100 + i * s / 9,
                                   s * 70 / 100, std::max(2, s / 18), 2, EPD_GRAY);
            }
            break;
        case Icon::Drizzle:
            cloud(x, y, s, EPD_WHITE, shadow);
            drops(x, y, s, 2, false);
            break;
        case Icon::Rain:
            cloud(x, y, s, EPD_GRAY, shadow);
            drops(x, y, s, 3, false);
            break;
        case Icon::Snow:
            cloud(x, y, s, EPD_WHITE, shadow);
            drops(x, y, s, 3, true);
            break;
        case Icon::Storm:
            cloud(x, y, s, EPD_GRAY, shadow);
            bolt(x, y, s);
            break;
        default:
            text(x + s / 3, y + s * 2 / 3, "?", u8g2_font_helvB24_te, EPD_BLACK);
    }
}

// ---------- Sekcje ekranu ----------

void header() {
    gfx->fillRect(0, 0, W, kHeaderH, EPD_BLACK);
    struct tm t;
    if (getLocalTime(&t, 100)) {
        String date = String(kDays[t.tm_wday]) + ", " + t.tm_mday + " " + kMonths[t.tm_mon];
        text(10, 23, date, u8g2_font_helvB14_te, EPD_WHITE, EPD_BLACK);
        char hhmm[16];
        strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
        const String upd = String("akt. ") + hhmm;
        text(W - 10 - textWidth(upd, u8g2_font_helvR10_te), 21, upd, u8g2_font_helvR10_te,
             EPD_YELLOW, EPD_BLACK);
    } else {
        text(10, 23, "Lodówka", u8g2_font_helvB14_te, EPD_WHITE, EPD_BLACK);
    }
}

// Kolor tła karty pogody i tekstu na niej.
struct CardStyle {
    uint16_t bg, fg, border;
};

CardStyle cardStyle(weather::Icon icon, bool night) {
    using weather::Icon;
    if (night) return {EPD_BLACK, EPD_WHITE, EPD_BLACK};
    switch (icon) {
        case Icon::Sun:
        case Icon::PartlyCloudy: return {EPD_YELLOW, EPD_BLACK, EPD_BLACK};
        case Icon::Storm: return {EPD_RED, EPD_WHITE, EPD_BLACK};
        default: return {EPD_WHITE, EPD_BLACK, EPD_BLACK};
    }
}

uint16_t tempColor(float t, uint16_t normal) { return lroundf(t) >= 25 ? EPD_RED : normal; }

void hourlyChart(const Weather& w, int x0, int y0, int wdt, int hgt) {
    if (w.hourCount < 2) return;
    float tMin = 1e9, tMax = -1e9;
    for (int i = 0; i < w.hourCount; ++i) {
        tMin = std::min(tMin, w.hours[i].temp);
        tMax = std::max(tMax, w.hours[i].temp);
    }
    if (tMax - tMin < 4) {  // płaski przebieg nie ma wyglądać jak huśtawka
        const float mid = (tMax + tMin) / 2;
        tMin = mid - 2;
        tMax = mid + 2;
    }
    const int labelW = 22;
    const int px0 = x0 + labelW, pw = wdt - labelW;
    const int plotTop = y0 + 4, plotBottom = y0 + hgt - 14;
    const int step = pw / w.hourCount;

    // Szansa opadu: słupki od dołu (szare, powyżej 50% czerwone).
    for (int i = 0; i < w.hourCount; ++i) {
        const int p = w.hours[i].precipProb;
        if (p < 10) continue;
        const int bh = (plotBottom - plotTop) * p / 100;
        gfx->fillRect(px0 + i * step + 1, plotBottom - bh, step - 2, bh,
                      p >= 50 ? EPD_PINK : EPD_GRAY);
    }
    gfx->drawFastHLine(px0, plotBottom, pw, EPD_BLACK);

    // Temperatura: gruba czerwona linia z kropkami.
    auto yOf = [&](float t) {
        return int(plotBottom - 3 - (t - tMin) / (tMax - tMin) * (plotBottom - plotTop - 6));
    };
    for (int i = 0; i < w.hourCount; ++i) {
        const int x = px0 + i * step + step / 2, y = yOf(w.hours[i].temp);
        if (i > 0) {
            const int xp = px0 + (i - 1) * step + step / 2, yp = yOf(w.hours[i - 1].temp);
            for (int d = -1; d <= 1; ++d) gfx->drawLine(xp, yp + d, x, y + d, EPD_RED);
        }
        if (i % 3 == 0) {
            gfx->fillCircle(x, y, 2, EPD_BLACK);
            char hh[4];
            snprintf(hh, sizeof(hh), "%d", w.hours[i].hour);
            text(x - textWidth(hh, u8g2_font_helvR08_te) / 2, y0 + hgt, hh, u8g2_font_helvR08_te,
                 EPD_BLACK);
        }
    }
    text(x0, plotTop + 8, formatTemp(tMax), u8g2_font_helvB08_te, EPD_RED);
    text(x0, plotBottom, formatTemp(tMin), u8g2_font_helvB08_te, EPD_BLACK);
}

void weatherPanel(const Weather& w) {
    const int x0 = 4, cardW = kSplitX - 8;
    if (!w.ok) {
        text(x0 + 2, 70, "Pogoda", u8g2_font_helvB14_te, EPD_BLACK);
        text(x0 + 2, 95, "niedostępna", u8g2_font_helvR12_te, EPD_BLACK);
        text(x0 + 2, 120, "Sprawdź miasto", u8g2_font_helvR10_te, EPD_BLACK);
        text(x0 + 2, 135, "w /ustawienia", u8g2_font_helvR10_te, EPD_BLACK);
        return;
    }
    const weather::Icon icon = weather::iconFor(w.code);
    const bool night = !w.isDay && (icon == weather::Icon::Sun || icon == weather::Icon::PartlyCloudy);
    const CardStyle st = cardStyle(icon, night);

    // Karta z bieżącą pogodą.
    const int cardY = kHeaderH + 4, cardH = 96;
    gfx->fillRoundRect(x0, cardY, cardW, cardH, 8, st.border);
    gfx->fillRoundRect(x0 + 2, cardY + 2, cardW - 4, cardH - 4, 7, st.bg);
    weatherIcon(icon, x0 + 4, cardY + 4, 64, night, st.bg);
    const String temp = formatTemp(w.temp);
    text(x0 + cardW - 6 - textWidth(temp, u8g2_font_fub30_tf), cardY + 50, temp, u8g2_font_fub30_tf,
         st.bg == EPD_WHITE || st.bg == EPD_YELLOW ? tempColor(w.temp, st.fg) : st.fg, st.bg);
    const String desc = weather::describe(w.code);
    const uint8_t* descFont =
        textWidth(desc, u8g2_font_helvB12_te) <= cardW - 12 ? u8g2_font_helvB12_te : u8g2_font_helvB10_te;
    text(x0 + 6, cardY + cardH - 10, fit(desc, descFont, cardW - 12), descFont, st.fg, st.bg);

    // Szczegóły pod kartą.
    int y = cardY + cardH + 13;
    text(x0 + 2, y, "Odczuwalna " + formatTemp(w.feelsLike) + ", wiatr " + String(int(lroundf(w.wind))) +
                        " km/h",
         u8g2_font_helvR08_te, EPD_BLACK);
    if (w.sunrise[0]) {
        y += 12;
        text(x0 + 2, y, String("Wschód ") + w.sunrise + ", zachód " + w.sunset, u8g2_font_helvR08_te,
             EPD_BLACK);
    }

    // Wykres na najbliższe 12 godzin.
    hourlyChart(w, x0, y + 4, cardW, 56);

    // Prognoza na 3 dni w kolumnach.
    const int fy = H - 64;
    gfx->drawFastHLine(x0, fy - 3, cardW, EPD_BLACK);
    struct tm t;
    const bool haveTime = getLocalTime(&t, 50);
    const int colW = cardW / 3;
    for (int i = 0; i < 3; ++i) {
        const DayForecast& d = w.days[i];
        const int cx = x0 + i * colW;
        const String name =
            i == 0 ? "Dziś" : i == 1 ? "Jutro" : (haveTime ? kDaysShort[(t.tm_wday + i) % 7] : "");
        text(cx + (colW - textWidth(name, u8g2_font_helvB10_te)) / 2, fy + 10, name,
             u8g2_font_helvB10_te, EPD_BLACK);
        weatherIcon(weather::iconFor(d.code), cx + (colW - 32) / 2, fy + 13, 32);
        const String hi = formatTemp(d.tMax), lo = "/" + formatTemp(d.tMin);
        const int tw = textWidth(hi, u8g2_font_helvB10_te) + textWidth(lo, u8g2_font_helvR10_te);
        const int tx = cx + (colW - tw) / 2;
        text(tx, fy + 60, hi, u8g2_font_helvB10_te, EPD_RED);
        text(tx + textWidth(hi, u8g2_font_helvB10_te), fy + 60, lo, u8g2_font_helvR10_te, EPD_BLACK);
    }
}

void listPanel(const std::vector<std::string>& items) {
    const int x0 = kSplitX + 10;
    const int right = W - 8;
    const int top = kHeaderH + 28;
    const int bottom = H - kFooterH - 4;

    text(x0, kHeaderH + 22, "Lista zakupów", u8g2_font_helvB18_te, EPD_RED);
    if (!items.empty()) {
        const String n = String(items.size());
        text(right - textWidth(n, u8g2_font_helvB14_te), kHeaderH + 22, n, u8g2_font_helvB14_te,
             EPD_BLACK);
    }

    if (items.empty()) {
        text(x0, top + 40, "Lista jest pusta", u8g2_font_helvB14_te, EPD_BLACK);
        text(x0, top + 66, "Przytrzymaj środkowy", u8g2_font_helvR12_te, EPD_BLACK);
        text(x0, top + 84, "przycisk i powiedz:", u8g2_font_helvR12_te, EPD_BLACK);
        text(x0, top + 108, "\"mleko, chleb i masło\"", u8g2_font_helvB12_te, EPD_RED);
        text(x0, top + 134, "Usuwanie: \"kupiłem mleko\"", u8g2_font_helvR10_te, EPD_BLACK);
        return;
    }

    // Jedna kolumna dużą czcionką albo dwie mniejszą, gdy pozycji jest dużo.
    const bool big = items.size() <= 8;
    const int lineH = big ? 26 : 21;
    const uint8_t* font = big ? u8g2_font_helvR14_te : u8g2_font_helvR12_te;
    const int rows = (bottom - top) / lineH;
    const int cols = big ? 1 : 2;
    const int colW = (right - x0) / cols;
    const size_t capacity = size_t(rows * cols);
    const bool overflow = items.size() > capacity;
    const size_t shown = overflow ? capacity - 1 : items.size();

    for (size_t i = 0; i < shown; ++i) {
        const int col = i / rows, row = i % rows;
        const int x = x0 + col * colW;
        const int baseline = top + row * lineH + lineH - 7;
        const int box = big ? 12 : 10;
        gfx->drawRect(x, baseline - box, box, box, EPD_BLACK);
        gfx->drawRect(x + 1, baseline - box + 1, box - 2, box - 2, EPD_BLACK);
        text(x + box + 6, baseline, fit(String(items[i].c_str()), font, colW - box - 10), font,
             EPD_BLACK);
    }
    if (overflow) {
        const int col = cols - 1, row = rows - 1;
        text(x0 + col * colW, top + row * lineH + lineH - 7,
             "+" + String(items.size() - shown) + " więcej", u8g2_font_helvB12_te, EPD_RED);
    }
}

void footer(const String& msg) {
    const int y = H - kFooterH;
    gfx->fillRect(kSplitX + 1, y, W - kSplitX - 1, kFooterH, EPD_YELLOW);
    const String s = msg.length() ? msg : String("Przytrzymaj przycisk i mów");
    text(kSplitX + 8, y + 16, fit(s, u8g2_font_helvB10_te, W - kSplitX - 16), u8g2_font_helvB10_te,
         EPD_BLACK, EPD_YELLOW);
}

}  // namespace

void begin(Epd4Color& epd) {
    gfx = &epd;
    u8g2.begin(epd);
    u8g2.setFontMode(1);  // przezroczyste tło
}

void render(const Weather& w, const std::vector<std::string>& items, const String& footerMsg) {
    gfx->fillScreen(EPD_WHITE);
    header();
    gfx->fillRect(kSplitX, kHeaderH, 2, H - kHeaderH, EPD_BLACK);
    weatherPanel(w);
    listPanel(items);
    footer(footerMsg);
}

void renderMessage(const String& title, const String& msg) {
    gfx->fillScreen(EPD_WHITE);
    gfx->fillRect(0, 0, W, 60, EPD_RED);
    text(16, 40, title, u8g2_font_helvB24_te, EPD_WHITE, EPD_RED);
    int y = 100;
    // Proste zawijanie po słowach.
    String line, word;
    const String s = msg + " ";
    for (unsigned i = 0; i < s.length(); ++i) {
        const char c = s[i];
        if (c == ' ' || c == '\n') {
            if (textWidth(line + " " + word, u8g2_font_helvR14_te) > W - 32 && line.length()) {
                text(16, y, line, u8g2_font_helvR14_te, EPD_BLACK);
                y += 26;
                line = word;
            } else {
                line = line.length() ? line + " " + word : word;
            }
            word = "";
            if (c == '\n') {
                text(16, y, line, u8g2_font_helvR14_te, EPD_BLACK);
                y += 26;
                line = "";
            }
        } else {
            word += c;
        }
    }
    if (line.length()) text(16, y, line, u8g2_font_helvR14_te, EPD_BLACK);
}

}  // namespace ui
