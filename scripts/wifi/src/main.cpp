#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;

static const char *status_name(wl_status_t s) {
    switch (s) {
        case WL_IDLE_STATUS:     return "IDLE";
        case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
        case WL_CONNECTED:       return "CONNECTED";
        case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED:    return "DISCONNECTED";
        default:                 return "?";
    }
}

static void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[wifi] got ip=%s gw=%s rssi=%d dBm\n",
                          WiFi.localIP().toString().c_str(),
                          WiFi.gatewayIP().toString().c_str(),
                          WiFi.RSSI());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("[wifi] disconnected reason=%u, reconnecting...\n",
                          info.wifi_sta_disconnected.reason);
            WiFi.reconnect();
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);                          // give USB-CDC host time to attach
    Serial.println("\n[boot] wifi-hello");
    Serial.printf("[boot] target=\"%s\"\n", WIFI_SSID);

    WiFi.onEvent(on_wifi_event);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT_MS) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[wifi] failed within %ums (status=%d %s)\n",
                      CONNECT_TIMEOUT_MS, WiFi.status(),
                      status_name((wl_status_t)WiFi.status()));
        Serial.println("       check src/secrets.h, remember ESP32 is 2.4 GHz only");
    }
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last >= 10000) {
        last = millis();
        Serial.printf("[status] %s ip=%s rssi=%d\n",
                      status_name((wl_status_t)WiFi.status()),
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
    }
}
