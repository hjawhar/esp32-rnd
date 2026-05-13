#include <Arduino.h>

constexpr uint8_t LED_PIN = 48;     // onboard WS2812 RGB

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);       // never block on Serial when host isn't reading
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    static const uint8_t colors[][3] = {
        {30,  0,  0},   // red
        { 0, 30,  0},   // green
        { 0,  0, 30},   // blue
    };
    static const char *names[] = {"R", "G", "B"};
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        rgbLedWrite(LED_PIN, colors[i][0], colors[i][1], colors[i][2]);
        Serial.printf("color=%s\n", names[i]);
        delay(500);
    }
}
