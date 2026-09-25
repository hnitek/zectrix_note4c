#include "stt.h"

#include <ArduinoJson.h>

#include "net.h"
#include "shopping.h"
#include "config.h"

namespace stt {

namespace {
const char* kBoundary = "----lodowkaFormBoundary7MA4YWxk";

void addField(String& s, const char* name, const char* value) {
    s += "--";
    s += kBoundary;
    s += "\r\nContent-Disposition: form-data; name=\"";
    s += name;
    s += "\"\r\n\r\n";
    s += value;
    s += "\r\n";
}
}  // namespace

String transcribe(const uint8_t* wav, size_t wavLen, const std::vector<std::string>& listItems,
                  String* error) {
    auto fail = [&](const String& why) {
        log_e("STT: %s", why.c_str());
        if (error) *error = why;
        return String();
    };
    const Config& c = config::get();
    if (c.sttKey.isEmpty()) return fail("brak klucza API (ustaw w /ustawienia)");
    String head;
    addField(head, "model", c.sttModel.c_str());
    addField(head, "language", "pl");
    addField(head, "response_format", "json");
    addField(head, "temperature", "0");
    // Słownictwo zakupowe bardzo pomaga przy pojedynczych słowach ("mleko", "masło").
    const std::string prompt = buildSttPrompt(listItems);
    addField(head, "prompt", prompt.c_str());
    head += "--";
    head += kBoundary;
    head += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
            "Content-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--";
    tail += kBoundary;
    tail += "--\r\n";

    const size_t total = head.length() + wavLen + tail.length();
    uint8_t* body = static_cast<uint8_t*>(ps_malloc(total));
    if (!body) return fail("brak pamięci");
    memcpy(body, head.c_str(), head.length());
    memcpy(body + head.length(), wav, wavLen);
    memcpy(body + head.length() + wavLen, tail.c_str(), tail.length());

    String contentType = String("multipart/form-data; boundary=") + kBoundary;
    String response, netError;
    const int status = net::request("POST", c.sttUrl, contentType.c_str(), body, total, response,
                                    c.sttKey.c_str(), 30000, &netError);
    free(body);

    if (status < 0) return fail("brak połączenia z " + c.sttUrl + " (" + netError + ")");
    if (status != 200) {
        // Np. 401 = zły klucz, 429 = limit zapytań; treść błędu z API skracamy.
        return fail("HTTP " + String(status) + ": " + response.substring(0, 200));
    }
    JsonDocument doc;
    if (deserializeJson(doc, response)) return fail("niezrozumiała odpowiedź: " + response.substring(0, 120));
    String text = doc["text"] | "";
    text.trim();
    log_i("Rozpoznano: \"%s\"", text.c_str());
    if (text.isEmpty()) return fail("serwis nie rozpoznał żadnych słów");
    if (isPromptEcho(text.c_str(), prompt)) {
        return fail("odrzucone – serwis zwrócił podpowiedź zamiast mowy (\"" + text.substring(0, 60) + "...\")");
    }
    return text;
}

}  // namespace stt
