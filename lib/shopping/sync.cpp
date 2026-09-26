#include "sync.h"

#include <algorithm>
#include <set>

#include "shopping.h"

MergeResult mergeWithRemote(const std::vector<RemoteItem>& remote,
                            const std::vector<std::string>& local,
                            const std::map<std::string, std::string>& idByKey,
                            const std::vector<std::string>& pendingDone) {
    MergeResult out;
    std::set<std::string> keys;
    auto isPending = [&](const std::string& id) {
        return std::find(pendingDone.begin(), pendingDone.end(), id) != pendingDone.end();
    };

    for (const auto& r : remote) {
        if (isPending(r.id)) continue;
        const std::string k = itemKey(r.name);
        if (k.empty() || !keys.insert(k).second) continue;
        out.items.push_back(r.name);
        out.idByKey[k] = r.id;
    }
    for (const auto& name : local) {
        const std::string k = itemKey(name);
        if (k.empty() || keys.count(k)) continue;
        // Miała id: jest w chmurze (może pod zmienioną nazwą – wtedy już jest wyżej)
        // albo zniknęła z chmury, czyli kupiona/usunięta w telefonie. Tak czy inaczej
        // o jej obecności decyduje chmura.
        if (idByKey.count(k)) continue;
        // Bez id: jeszcze niewysłana – zostaje.
        keys.insert(k);
        out.items.push_back(name);
    }
    return out;
}
