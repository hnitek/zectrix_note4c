#include "shopping.h"

#include <algorithm>
#include <cctype>

namespace {

// Polskie wielkie litery w UTF-8 -> małe. Pierwszy bajt 0xC3/0xC4/0xC5.
struct PlCase { const char* upper; const char* lower; };
const PlCase kPolish[] = {
    {"Ą", "ą"}, {"Ć", "ć"}, {"Ę", "ę"}, {"Ł", "ł"}, {"Ń", "ń"},
    {"Ó", "ó"}, {"Ś", "ś"}, {"Ź", "ź"}, {"Ż", "ż"},
};

std::string toLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        if (c < 0x80) {
            out += static_cast<char>(std::tolower(c));
            ++i;
            continue;
        }
        bool mapped = false;
        if (i + 1 < s.size()) {
            for (const auto& p : kPolish) {
                if (s.compare(i, 2, p.upper) == 0) {
                    out += p.lower;
                    mapped = true;
                    break;
                }
            }
        }
        if (mapped) {
            i += 2;
        } else {
            out += s[i];
            ++i;
        }
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(' ');
    return s.substr(b, e - b + 1);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Zdejmuje najdłuższy pasujący prefiks (całe słowa). Zwraca true, jeśli coś zdjęto.
bool stripPrefix(std::string& s, const std::vector<std::string>& prefixes) {
    const std::string* best = nullptr;
    for (const auto& p : prefixes) {
        if ((s == p || startsWith(s, p + " ")) && (!best || p.size() > best->size())) {
            best = &p;
        }
    }
    if (!best) return false;
    s = trim(s.substr(best->size()));
    return true;
}

bool stripSuffix(std::string& s, const std::vector<std::string>& suffixes) {
    for (const auto& p : suffixes) {
        if (s == p) {
            s.clear();
            return true;
        }
        if (endsWith(s, " " + p)) {
            s = trim(s.substr(0, s.size() - p.size() - 1));
            return true;
        }
    }
    return false;
}

const std::vector<std::string> kClearPhrases = {
    "wyczyść listę", "wyczyść całą listę", "wyczyść wszystko", "wyczyść",
    "usuń wszystko", "skasuj wszystko", "skreśl wszystko", "usuń całą listę",
    "skasuj listę", "nowa lista",
};

const std::vector<std::string> kRemoveVerbs = {
    "usuń", "usuń z listy", "skreśl", "skreśl z listy", "wykreśl", "wykreśl z listy",
    "skasuj", "kupiłem", "kupiłam", "kupiliśmy", "mam już", "mamy już", "już mam",
    "już mamy", "kupione",
};

const std::vector<std::string> kAddVerbs = {
    "dodaj", "dopisz", "zapisz", "kup", "kupić", "trzeba kupić", "muszę kupić",
    "musimy kupić", "potrzebuję", "potrzebujemy", "potrzeba", "brakuje", "brakuje nam",
    "skończył się", "skończyła się", "skończyło się", "skończyły się", "wpisz",
};

const std::vector<std::string> kListWords = {
    "do listy zakupów", "do listy", "na listę zakupów", "na listę", "z listy zakupów",
    "z listy", "z listy zakupów", "listy zakupów",
};

// Typowe "halucynacje" Whispera na szumie – nie są poleceniami.
const std::vector<std::string> kHallucinations = {
    "napisy stworzone", "napisy wykonane", "dziękuję", "dzięki za oglądanie", "subskrybuj",
    "do zobaczenia",
};

const std::vector<std::string> kFillers = {"jeszcze", "też", "także", "proszę", "również"};

std::vector<std::string> splitItems(const std::string& s) {
    // Separatory: przecinek (już zamieniony na " , "), " i ", " oraz ", " a także ".
    std::vector<std::string> words;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            if (!cur.empty()) words.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(cur);

    std::vector<std::string> items;
    std::string item;
    auto flush = [&]() {
        std::string t = trim(item);
        while (!t.empty() && (t.front() == '-' || t.front() == '*')) t = trim(t.substr(1));
        while (!t.empty() && t.back() == '-') t = trim(t.substr(0, t.size() - 1));
        while (stripPrefix(t, kFillers)) {}
        while (stripSuffix(t, kFillers)) {}
        if (!t.empty()) items.push_back(t);
        item.clear();
    };
    for (size_t i = 0; i < words.size(); ++i) {
        const std::string& w = words[i];
        if (w == "," || w == "i" || w == "oraz" || w == "plus") {
            flush();
        } else if (w == "a" && i + 1 < words.size() && words[i + 1] == "także") {
            flush();
            ++i;
        } else {
            if (!item.empty()) item += ' ';
            item += w;
        }
    }
    flush();
    return items;
}

size_t utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    return 4;
}

size_t utf8Length(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); i += utf8CharLen(s[i])) ++n;
    return n;
}

// Pierwsze n znaków (nie bajtów).
std::string utf8Prefix(const std::string& s, size_t n) {
    size_t i = 0;
    while (i < s.size() && n > 0) {
        i += utf8CharLen(s[i]);
        --n;
    }
    return s.substr(0, i);
}

}  // namespace

std::string normalizeText(const std::string& text) {
    std::string lower = toLower(text);
    std::string out;
    for (char ch : lower) {
        unsigned char c = ch;
        if (c == ',' || c == ';') {
            out += " , ";
        } else if (c == '.' || c == '!' || c == '?' || c == ':' || c == '"' || c == '\n' ||
                   c == '\r' || c == '\t') {
            out += ' ';
        } else {
            out += ch;
        }
    }
    // Zwijanie wielu spacji.
    std::string collapsed;
    bool space = false;
    for (char c : out) {
        if (c == ' ') {
            if (!space && !collapsed.empty()) collapsed += ' ';
            space = true;
        } else {
            collapsed += c;
            space = false;
        }
    }
    return trim(collapsed);
}

std::string capitalizeFirst(const std::string& text) {
    if (text.empty()) return text;
    unsigned char c = text[0];
    if (c < 0x80) {
        std::string out = text;
        out[0] = static_cast<char>(std::toupper(c));
        return out;
    }
    for (const auto& p : kPolish) {
        if (text.compare(0, 2, p.lower) == 0) return std::string(p.upper) + text.substr(2);
    }
    return text;
}

namespace {
// Krótko: Whisper bierze pod uwagę tylko ~224 tokeny podpowiedzi.
const char* kGroceries[] = {
    "mleko", "chleb", "masło", "jajka", "ser żółty", "twaróg", "jogurt", "śmietana", "szynka",
    "kiełbasa", "kurczak", "pomidory", "ogórki", "ziemniaki", "cebula", "czosnek", "marchew",
    "jabłka", "banany", "makaron", "ryż", "mąka", "cukier", "kawa", "herbata", "bułki",
    "papier toaletowy", "płyn do naczyń",
};
constexpr size_t kMaxPromptListItems = 15;

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
}  // namespace

std::string buildSttPrompt(const std::vector<std::string>& listItems) {
    std::string p = "Lista zakupów. Produkty: ";
    bool first = true;
    auto append = [&](const std::string& item) {
        if (!first) p += ", ";
        p += normalizeText(item);
        first = false;
    };
    for (size_t i = 0; i < listItems.size() && i < kMaxPromptListItems; ++i) append(listItems[i]);
    for (const char* g : kGroceries) {
        bool dup = false;
        for (const auto& i : listItems) dup |= normalizeText(i) == g;
        if (!dup) append(g);
    }
    return p + ".";
}

bool isPromptEcho(const std::string& text, const std::string& prompt) {
    const std::string t = normalizeText(text);
    // Nikt nie mówi do lodówki "lista zakupów, produkty: ..." w mianowniku.
    if (t.find("lista zakupów") != std::string::npos || t.find("produkty") == 0) return true;
    // Długi ciąg słów w tej samej kolejności co w podpowiedzi = przepisana podpowiedź.
    const auto tw = words(t);
    const auto pw = words(normalizeText(prompt));
    if (tw.size() < 6) return false;
    size_t j = 0, matched = 0;
    for (const auto& w : tw) {
        while (j < pw.size() && pw[j] != w) ++j;
        if (j == pw.size()) break;
        ++matched;
        ++j;
    }
    return matched * 10 >= tw.size() * 8;
}

bool itemsMatch(const std::string& a, const std::string& b) {
    const std::string na = normalizeText(a);
    const std::string nb = normalizeText(b);
    if (na.empty() || nb.empty()) return false;
    if (na == nb) return true;
    // "mleko" pasuje do "mleko 2l"
    if (startsWith(na, nb + " ") || startsWith(nb, na + " ")) return true;
    // Odmiana: wspólny rdzeń ("jajka"/"jajek", "masło"/"masła")
    if (na.find(' ') == std::string::npos && nb.find(' ') == std::string::npos) {
        const size_t la = utf8Length(na), lb = utf8Length(nb);
        const size_t shorter = std::min(la, lb);
        if (shorter < 4) return false;
        const size_t stem = std::max<size_t>(3, shorter - 2);
        return utf8Prefix(na, stem) == utf8Prefix(nb, stem);
    }
    return false;
}

VoiceCommand parseVoiceCommand(const std::string& text) {
    VoiceCommand cmd;
    std::string s = normalizeText(text);
    if (s.empty()) return cmd;
    for (const auto& h : kHallucinations) {
        if (s == h || startsWith(s, h + " ")) return cmd;
    }
    if (s.find("amara org") != std::string::npos) return cmd;

    // Uprzejmości na początku.
    stripPrefix(s, {"proszę", "hej", "okej", "ok"});
    if (!s.empty() && s[0] == ',') s = trim(s.substr(1));

    for (const auto& phrase : kClearPhrases) {
        std::string t = s;
        stripSuffix(t, kListWords);
        if (t == phrase) {
            cmd.kind = VoiceCommand::Kind::Clear;
            return cmd;
        }
    }

    if (stripPrefix(s, kRemoveVerbs)) {
        cmd.kind = VoiceCommand::Kind::Remove;
    } else {
        stripPrefix(s, kAddVerbs);
        cmd.kind = VoiceCommand::Kind::Add;
    }
    stripPrefix(s, kListWords);
    stripSuffix(s, kListWords);

    for (auto& item : splitItems(s)) {
        cmd.items.push_back(capitalizeFirst(item));
    }
    if (cmd.items.empty()) cmd.kind = VoiceCommand::Kind::None;
    return cmd;
}

bool ShoppingList::add(const std::string& item) {
    const std::string t = trim(item);
    if (t.empty()) return false;
    for (const auto& existing : items_) {
        if (normalizeText(existing) == normalizeText(t)) return false;
    }
    items_.push_back(capitalizeFirst(t));
    return true;
}

bool ShoppingList::remove(const std::string& item) {
    // Najpierw dokładne dopasowanie, potem przybliżone.
    const std::string n = normalizeText(item);
    for (size_t i = 0; i < items_.size(); ++i) {
        if (normalizeText(items_[i]) == n) return removeAt(i);
    }
    for (size_t i = 0; i < items_.size(); ++i) {
        if (itemsMatch(items_[i], item)) return removeAt(i);
    }
    return false;
}

bool ShoppingList::removeAt(size_t index) {
    if (index >= items_.size()) return false;
    items_.erase(items_.begin() + static_cast<long>(index));
    return true;
}

bool ShoppingList::apply(const VoiceCommand& cmd) {
    bool changed = false;
    switch (cmd.kind) {
        case VoiceCommand::Kind::Add:
            for (const auto& i : cmd.items) changed |= add(i);
            break;
        case VoiceCommand::Kind::Remove:
            for (const auto& i : cmd.items) changed |= remove(i);
            break;
        case VoiceCommand::Kind::Clear:
            changed = !items_.empty();
            clear();
            break;
        case VoiceCommand::Kind::None:
            break;
    }
    return changed;
}
