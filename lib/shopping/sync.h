#pragma once

// Scalanie listy z lodówki z listą w chmurze (aplikacja w telefonie).
// Czysty C++ – testowane w test/test_shopping.

#include <map>
#include <string>
#include <vector>

struct RemoteItem {
    std::string id;
    std::string name;
};

struct MergeResult {
    std::vector<std::string> items;              // nowa lista na lodówce
    std::map<std::string, std::string> idByKey;  // itemKey(nazwa) -> id w chmurze
};

// remote:      aktywne (niekupione) pozycje z chmury, w kolejności z aplikacji
// local:       bieżąca lista na lodówce
// idByKey:     znane powiązania z poprzedniej synchronizacji
// pendingDone: id oznaczone lokalnie jako kupione, jeszcze niewysłane
//
// Zasady:
//  - pozycje z chmury są na liście (chyba że czekają na oznaczenie jako kupione),
//  - pozycja lokalna, która miała id, a zniknęła z chmury, została kupiona
//    w telefonie – znika też z lodówki,
//  - pozycja lokalna bez id (dodana, gdy nie było internetu) zostaje i poczeka na wysłanie.
MergeResult mergeWithRemote(const std::vector<RemoteItem>& remote,
                            const std::vector<std::string>& local,
                            const std::map<std::string, std::string>& idByKey,
                            const std::vector<std::string>& pendingDone);
