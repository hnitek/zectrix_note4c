#include "net.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include "secrets.h"

namespace net {

bool connect(uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // SNTP synchronizuje się sam, gdy tylko sieć będzie dostępna.
    configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com");
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
    if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
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
            size_t bodyLen, String& response, const char* bearerToken, int timeoutMs) {
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
    const int status = err == ESP_OK ? esp_http_client_get_status_code(client) : -1;
    if (err != ESP_OK) log_e("HTTP %s %s: %s", method, url.c_str(), esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return status;
}

}  // namespace net
