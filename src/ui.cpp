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

// ---------- Ikony pogody (rysowane prymitywami, w kolorach panelu) ----------

void cloudShape(int x, int y, int s, int grow, uint16_t color) {
    gfx->fillCircle(x + s * 30 / 100, y + s * 40 / 100, s * 20 / 100 + grow, color);
    gfx->fillCircle(x + s * 55 / 100, y + s * 30 / 100, s * 26 / 100 + grow, color);
    gfx->fillCircle(x + s * 78 / 100, y + s * 44 / 100, s * 16 / 100 + grow, color);
    gfx->fillRoundRect(x + s * 10 / 100 - grow, y + s * 40 / 100 - grow, s * 80 / 100 + 2 * grow,
                       s * 20 / 100 + 2 * grow, s * 10 / 100 + grow, color);
}

void cloud(int x, int y, int s, uint16_t fill = EPD_WHITE) {
    cloudShape(x, y, s, 2, EPD_BLACK);
    cloudShape(x, y, s, 0, fill);
}

void sun(int cx, int cy, int r) {
    for (int a = 0; a < 360; a += 45) {
        const float rad = a * DEG_TO_RAD;
        const int x0 = cx + cosf(rad) * (r + 4), y0 = cy + sinf(rad) * (r + 4);
        const int x1 = cx + cosf(rad) * (r * 17 / 10), y1 = cy + sinf(rad) * (r * 17 / 10);
        for (int d = -1; d <= 1; ++d) {
            gfx->drawLine(x0 + d, y0, x1 + d, y1, EPD_BLACK);
            gfx->drawLine(x0, y0 + d, x1, y1 + d, EPD_BLACK);
        }
    }
    gfx->fillCircle(cx, cy, r + 2, EPD_BLACK);
    gfx->fillCircle(cx, cy, r, EPD_YELLOW);
}

void drops(int x, int y, int s, int n, bool snow) {
    for (int i = 0; i < n; ++i) {
        const int dx = x + s * (20 + i * 60 / (n > 1 ? n - 1 : 1)) / 100;
        const int dy = y + s * 70 / 100 + (i % 2) * s / 12;
        if (snow) {
            const int r = std::max(2, s / 14);
            gfx->drawLine(dx - r, dy, dx + r, dy, EPD_BLACK);
            gfx->drawLine(dx, dy - r, dx, dy + r, EPD_BLACK);
            gfx->drawLine(dx - r, dy - r, dx + r, dy + r, EPD_BLACK);
            gfx->drawLine(dx - r, dy + r, dx + r, dy - r, EPD_BLACK);
        } else {
            const int len = std::max(4, s / 6);
            for (int t = 0; t < 2; ++t) gfx->drawLine(dx + t, dy, dx - len / 3 + t, dy + len, EPD_BLACK);
        }
    }
}

void bolt(int x, int y, int s) {
    const int cx = x + s / 2, top = y + s * 52 / 100;
    gfx->fillTriangle(cx + s / 10, top, cx - s / 8, top + s * 28 / 100, cx + s / 50, top + s * 25 / 100,
                      EPD_RED);
    gfx->fillTriangle(cx + s / 50, top + s * 20 / 100, cx + s / 8, top + s * 20 / 100,
                      cx - s / 12, top + s * 48 / 100, EPD_RED);
}

void weatherIcon(weather::Icon icon, int x, int y, int s) {
    using weather::Icon;
    switch (icon) {
        case Icon::Sun:
            sun(x + s / 2, y + s / 2, s * 22 / 100);
            break;
        case Icon::PartlyCloudy:
            sun(x + s * 62 / 100, y + s * 35 / 100, s * 17 / 100);
            cloud(x - s / 20, y + s * 25 / 100, s * 80 / 100);
            break;
        case Icon::Cloud:
            cloud(x, y + s / 10, s);
            break;
        case Icon::Fog:
            cloud(x, y, s);
            for (int i = 0; i < 3; ++i) {
                gfx->fillRect(x + s / 10 + (i % 2) * s / 10, y + s * 68 / 100 + i * s / 9,
                              s * 70 / 100, std::max(2, s / 24), EPD_BLACK);
            }
            break;
        case Icon::Drizzle:
            cloud(x, y, s);
            drops(x, y, s, 2, false);
            break;
        case Icon::Rain:
            cloud(x, y, s);
            drops(x, y, s, 4, false);
            break;
        case Icon::Snow:
            cloud(x, y, s);
            drops(x, y, s, 3, true);
            break;
        case Icon::Storm:
            cloud(x, y, s, EPD_WHITE);
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

void weatherPanel(const Weather& w) {
    const int x0 = 6;
    if (!w.ok) {
        text(x0, 70, "Pogoda", u8g2_font_helvB14_te, EPD_BLACK);
        text(x0, 95, "niedostępna", u8g2_font_helvR12_te, EPD_BLACK);
        return;
    }
    weatherIcon(weather::iconFor(w.code), x0, kHeaderH + 6, 70);
    const String temp = formatTemp(w.temp);
    text(kSplitX - 8 - textWidth(temp, u8g2_font_fub30_tf), kHeaderH + 58, temp, u8g2_font_fub30_tf,
         EPD_BLACK);

    int y = kHeaderH + 100;
    // Dłuższe opisy ("Marznąca mżawka") mniejszą czcionką.
    const String desc = weather::describe(w.code);
    const uint8_t* descFont =
        textWidth(desc, u8g2_font_helvB12_te) <= kSplitX - 12 ? u8g2_font_helvB12_te : u8g2_font_helvB10_te;
    text(x0, y, fit(desc, descFont, kSplitX - 12), descFont, EPD_BLACK);
    y += 18;
    text(x0, y, "Odczuwalna " + formatTemp(w.feelsLike), u8g2_font_helvR10_te, EPD_BLACK);
    y += 15;
    text(x0, y, "Wiatr " + String(int(lroundf(w.wind))) + " km/h, " + w.humidity + "%",
         u8g2_font_helvR10_te, EPD_BLACK);

    // Prognoza na 3 dni.
    y += 10;
    gfx->drawFastHLine(x0, y, kSplitX - 12, EPD_BLACK);
    struct tm t;
    const bool haveTime = getLocalTime(&t, 50);
    for (int i = 0; i < 3; ++i) {
        const DayForecast& d = w.days[i];
        const int rowY = y + 6 + i * 30;
        String name = i == 0 ? "Dziś" : i == 1 ? "Jutro" : (haveTime ? kDaysShort[(t.tm_wday + i) % 7] : "");
        text(x0, rowY + 19, name, u8g2_font_helvB10_te, EPD_BLACK);
        weatherIcon(weather::iconFor(d.code), x0 + 42, rowY, 28);
        const String range = formatTemp(d.tMax) + "/" + formatTemp(d.tMin);
        text(x0 + 74, rowY + 14, range, u8g2_font_helvR10_te, EPD_BLACK);
        if (d.precipProb >= 20) {
            text(x0 + 74, rowY + 27, String(d.precipProb) + "% opadu", u8g2_font_helvR08_te,
                 d.precipProb >= 50 ? EPD_RED : EPD_BLACK);
        }
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
        text(x0, top + 108, "\"dodaj mleko i chleb\"", u8g2_font_helvB12_te, EPD_RED);
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
