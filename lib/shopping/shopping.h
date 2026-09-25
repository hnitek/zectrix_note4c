#pragma once

// Logika listy zakupów i interpretacja poleceń głosowych po polsku.
// Czysty C++ (bez Arduino), dzięki czemu testy działają na komputerze: `pio test -e native`.

#include <string>
#include <vector>

struct VoiceCommand {
    enum class Kind { None, Add, Remove, Clear };
    Kind kind = Kind::None;
    std::vector<std::string> items;  // już przycięte, pierwsza litera wielka
};

// "Dodaj mleko, chleb i masło" -> Add [Mleko, Chleb, Masło]
// "Usuń mleko" / "Kupiłem chleb" -> Remove [...]
// "Wyczyść listę" -> Clear
// Samo "mleko i jajka" (bez czasownika) -> Add
VoiceCommand parseVoiceCommand(const std::string& text);

// Małe litery (ASCII + polskie znaki w UTF-8), bez interpunkcji, pojedyncze spacje.
std::string normalizeText(const std::string& text);

// Podpowiedź słownictwa dla rozpoznawania mowy (parametr "prompt" Whispera):
// typowe produkty + to, co już jest na liście (żeby "usuń X" trafiało w te same nazwy).
std::string buildSttPrompt(const std::vector<std::string>& listItems);

// Whisper przy niewyraźnym nagraniu potrafi zwrócić po prostu treść podpowiedzi.
// Zwraca true, jeśli rozpoznany tekst wygląda na takie "echo".
bool isPromptEcho(const std::string& text, const std::string& prompt);

// Wielka pierwsza litera (obsługuje polskie znaki).
std::string capitalizeFirst(const std::string& text);

// Czy dwie nazwy produktów oznaczają to samo ("jajka" ~ "jajek", "mleko" ~ "Mleko 2l").
bool itemsMatch(const std::string& a, const std::string& b);

class ShoppingList {
public:
    const std::vector<std::string>& items() const { return items_; }
    void setItems(std::vector<std::string> items) { items_ = std::move(items); }

    bool add(const std::string& item);     // false, jeśli już jest na liście
    bool remove(const std::string& item);  // false, jeśli nie znaleziono
    bool removeAt(size_t index);
    void clear() { items_.clear(); }

    // Wykonuje polecenie; zwraca true, jeśli lista się zmieniła.
    bool apply(const VoiceCommand& cmd);

private:
    std::vector<std::string> items_;
};
