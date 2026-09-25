#include <unity.h>

#include "shopping.h"

using Kind = VoiceCommand::Kind;

void setUp() {}
void tearDown() {}

static void assertItems(const VoiceCommand& c, std::vector<std::string> expected) {
    TEST_ASSERT_EQUAL_size_t(expected.size(), c.items.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), c.items[i].c_str());
    }
}

void test_add_with_verb_and_list() {
    auto c = parseVoiceCommand("Dodaj mleko, chleb i masło.");
    TEST_ASSERT_TRUE(c.kind == Kind::Add);
    assertItems(c, {"Mleko", "Chleb", "Masło"});
}

void test_add_without_verb() {
    auto c = parseVoiceCommand("jajka oraz żółty ser");
    TEST_ASSERT_TRUE(c.kind == Kind::Add);
    assertItems(c, {"Jajka", "Żółty ser"});
}

void test_add_to_list_phrase() {
    auto c = parseVoiceCommand("Dopisz do listy zakupów papier toaletowy");
    TEST_ASSERT_TRUE(c.kind == Kind::Add);
    assertItems(c, {"Papier toaletowy"});

    c = parseVoiceCommand("Dodaj pomidory do listy.");
    assertItems(c, {"Pomidory"});
}

void test_add_need_phrases() {
    auto c = parseVoiceCommand("Skończyło się masło");
    TEST_ASSERT_TRUE(c.kind == Kind::Add);
    assertItems(c, {"Masło"});

    c = parseVoiceCommand("Trzeba kupić jeszcze ketchup");
    assertItems(c, {"Ketchup"});
}

void test_remove() {
    auto c = parseVoiceCommand("Usuń z listy mleko i chleb");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Mleko", "Chleb"});

    c = parseVoiceCommand("Kupiłem jajka");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Jajka"});
}

void test_remove_suffix_and_more_verbs() {
    auto c = parseVoiceCommand("Mleko kupione.");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Mleko"});
    c = parseVoiceCommand("Chleb już mam");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Chleb"});
    c = parseVoiceCommand("Masło i jajka usuń");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Masło", "Jajka"});
    c = parseVoiceCommand("Odhacz mleko");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    c = parseVoiceCommand("Nie potrzeba mleka");
    TEST_ASSERT_TRUE(c.kind == Kind::Remove);
    assertItems(c, {"Mleka"});
}

void test_undo() {
    TEST_ASSERT_TRUE(parseVoiceCommand("Cofnij.").kind == Kind::Undo);
    TEST_ASSERT_TRUE(parseVoiceCommand("Usuń ostatnie").kind == Kind::Undo);
    ShoppingList list;
    list.apply(parseVoiceCommand("mleko, chleb"));
    TEST_ASSERT_TRUE(list.apply(parseVoiceCommand("cofnij")));
    TEST_ASSERT_EQUAL_size_t(1, list.items().size());
    TEST_ASSERT_EQUAL_STRING("Mleko", list.items()[0].c_str());
    // "nie potrzeba mleka" usuwa "Mleko" mimo odmiany
    TEST_ASSERT_TRUE(list.apply(parseVoiceCommand("nie potrzeba mleka")));
    TEST_ASSERT_EQUAL_size_t(0, list.items().size());
}

void test_clear() {
    TEST_ASSERT_TRUE(parseVoiceCommand("Wyczyść listę.").kind == Kind::Clear);
    TEST_ASSERT_TRUE(parseVoiceCommand("usuń wszystko z listy").kind == Kind::Clear);
}

void test_empty() {
    TEST_ASSERT_TRUE(parseVoiceCommand("").kind == Kind::None);
    TEST_ASSERT_TRUE(parseVoiceCommand("Dodaj.").kind == Kind::None);
    TEST_ASSERT_TRUE(parseVoiceCommand("Dziękuję.").kind == Kind::None);
    TEST_ASSERT_TRUE(
        parseVoiceCommand("Napisy stworzone przez społeczność Amara.org").kind == Kind::None);
}

void test_bare_item_is_add() {
    auto c = parseVoiceCommand("Mleko.");
    TEST_ASSERT_TRUE(c.kind == Kind::Add);
    assertItems(c, {"Mleko"});
    c = parseVoiceCommand("- Mleko");
    assertItems(c, {"Mleko"});
}

void test_prompt_echo() {
    const std::string p = buildSttPrompt({"Pomidory"});
    TEST_ASSERT_TRUE(p.find("pomidory, mleko") != std::string::npos);  // lista najpierw, bez duplikatu
    TEST_ASSERT_TRUE(isPromptEcho("Lista zakupów. Produkty: pomidory, mleko, chleb.", p));
    TEST_ASSERT_TRUE(isPromptEcho("pomidory, mleko, chleb, masło, jajka, ser żółty, twaróg", p));
    TEST_ASSERT_FALSE(isPromptEcho("Mleko", p));
    TEST_ASSERT_FALSE(isPromptEcho("dodaj mleko, chleb i masło", p));
    TEST_ASSERT_FALSE(isPromptEcho("Wyczyść listę zakupów", p));
}

void test_items_match_inflection() {
    TEST_ASSERT_TRUE(itemsMatch("Jajka", "jajek"));
    TEST_ASSERT_TRUE(itemsMatch("Masło", "masła"));
    TEST_ASSERT_TRUE(itemsMatch("Mleko 2l", "mleko"));
    TEST_ASSERT_FALSE(itemsMatch("Chleb", "mleko"));
    TEST_ASSERT_FALSE(itemsMatch("Ser", "sok"));
}

void test_list_apply() {
    ShoppingList list;
    TEST_ASSERT_TRUE(list.apply(parseVoiceCommand("dodaj mleko i jajka")));
    TEST_ASSERT_FALSE(list.apply(parseVoiceCommand("dodaj Mleko")));  // duplikat
    TEST_ASSERT_EQUAL_size_t(2, list.items().size());
    TEST_ASSERT_TRUE(list.apply(parseVoiceCommand("kupiłam jajek")));
    TEST_ASSERT_EQUAL_size_t(1, list.items().size());
    TEST_ASSERT_EQUAL_STRING("Mleko", list.items()[0].c_str());
    TEST_ASSERT_TRUE(list.apply(parseVoiceCommand("wyczyść listę")));
    TEST_ASSERT_EQUAL_size_t(0, list.items().size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_add_with_verb_and_list);
    RUN_TEST(test_add_without_verb);
    RUN_TEST(test_add_to_list_phrase);
    RUN_TEST(test_add_need_phrases);
    RUN_TEST(test_remove);
    RUN_TEST(test_remove_suffix_and_more_verbs);
    RUN_TEST(test_undo);
    RUN_TEST(test_clear);
    RUN_TEST(test_empty);
    RUN_TEST(test_bare_item_is_add);
    RUN_TEST(test_prompt_echo);
    RUN_TEST(test_items_match_inflection);
    RUN_TEST(test_list_apply);
    return UNITY_END();
}
