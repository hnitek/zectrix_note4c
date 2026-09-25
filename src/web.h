#pragma once

// Prosty panel WWW w sieci lokalnej (http://lodowka.local): podgląd i edycja listy z telefonu.
namespace web {

// onChange wywoływane po każdej zmianie listy z poziomu przeglądarki.
void begin(void (*onChange)());
void loop();

}  // namespace web
