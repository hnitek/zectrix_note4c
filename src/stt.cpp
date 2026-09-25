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

String transcribe(const uint8_t* wav, size_t wavLen, const std::vector<std::string>& listItems) {
    const Config& c = config::get();
    if (c.sttKey.isEmpty()) {
        log_e("Brak klucza API do rozpoznawania mowy (ustaw w /ustawienia)");
        return "";
    }
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
    if (!body) return "";
    memcpy(body, head.c_str(), head.length());
    memcpy(body + head.length(), wav, wavLen);
    memcpy(body + head.length() + wavLen, tail.c_str(), tail.length());

    String contentType = String("multipart/form-data; boundary=") + kBoundary;
    String response;
    const int status = net::request("POST", c.sttUrl, contentType.c_str(), body, total, response,
                                    c.sttKey.c_str(), 30000);
    free(body);

    if (status != 200) {
        log_e("STT HTTP %d: %s", status, response.c_str());
        return "";
    }
    JsonDocument doc;
    if (deserializeJson(doc, response)) return "";
    String text = doc["text"] | "";
    text.trim();
    log_i("Rozpoznano: \"%s\"", text.c_str());
    if (isPromptEcho(text.c_str(), prompt)) {
        log_w("Odrzucam: to powtórzona podpowiedź, nie mowa");
        return "";
    }
    return text;
}

}  // namespace stt
