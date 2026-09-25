#include "net.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include "config.h"

namespace net {

bool connect(uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(config::kHostname);
    WiFi.setAutoReconnect(true);
    const Config& c = config::get();
    WiFi.begin(c.ssid.c_str(), c.pass.c_str());
    // SNTP synchronizuje się sam, gdy tylko sieć będzie dostępna.
    configTzTime(c.tz.c_str(), "pool.ntp.org", "time.google.com");
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) return false;
        delay(200);
    }
    startServices();
    return true;
}

void startServices() {
    static bool mdnsStarted = false;
    if (!mdnsStarted && MDNS.begin(config::kHostname)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
    }
    log_i("Wi-Fi OK, IP %s", WiFi.localIP().toString().c_str());
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

String ipAddress() { return WiFi.localIP().toString(); }

namespace {
esp_err_t onEvent(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data) {
        static_cast<String*>(evt->user_data)->concat(static_cast<const char*>(evt->data),
                                                     evt->data_len);
    }
    return ESP_OK;
}
}  // namespace

int request(const char* method, const String& url, const char* contentType, const uint8_t* body,
            size_t bodyLen, String& response, const char* bearerToken, int timeoutMs,
            String* error) {
    response = "";
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.timeout_ms = timeoutMs;
    cfg.event_handler = onEvent;
    cfg.user_data = &response;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = 2048;
    cfg.buffer_size_tx = 2048;
    cfg.method = strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return -1;
    if (contentType) esp_http_client_set_header(client, "Content-Type", contentType);
    String auth;
    if (bearerToken) {
        auth = String("Bearer ") + bearerToken;
        esp_http_client_set_header(client, "Authorization", auth.c_str());
    }
    if (body && bodyLen) {
        esp_http_client_set_post_field(client, reinterpret_cast<const char*>(body), bodyLen);
    }
    const esp_err_t err = esp_http_client_perform(client);
    int status = err == ESP_OK ? esp_http_client_get_status_code(client) : -1;
    // Przy odpowiedzi 401 klient ESP-IDF próbuje sam obsłużyć autoryzację (Basic/Digest)
    // i dla "Bearer" zwraca ESP_ERR_NOT_SUPPORTED – to w rzeczywistości odrzucony klucz.
    if (err == ESP_ERR_NOT_SUPPORTED && esp_http_client_get_status_code(client) == 401) status = 401;
    if (status < 0) {
        // Szczegóły do diagnostyki: kod błędu ESP, errno gniazda i wolna pamięć wewnętrzna
        // (TLS potrzebuje kilkudziesięciu kB).
        const String why = String(esp_err_to_name(err)) + ", errno " +
                           String(esp_http_client_get_errno(client)) + ", wolna pamięć " +
                           String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024) + " kB";
        log_e("HTTP %s %s: %s", method, url.c_str(), why.c_str());
        if (error) *error = why;
    }
    esp_http_client_cleanup(client);
    return status;
}

}  // namespace net
