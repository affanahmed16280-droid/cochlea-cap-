#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

/* 
 * MOTOR PINOUT CONFIGURATION
 * FRONT Motor -> GPIO 13
 * BACK  Motor -> GPIO 12
 * LEFT  Motor -> GPIO 14
 * RIGHT Motor -> GPIO 27
 */

const int MOTOR_PINS[] = {13, 12, 14, 27};

typedef struct struct_message {
    uint8_t direction;
} struct_message;

struct_message incomingData;

// Handle different ESP32 Core versions
void onDataRecv(const uint8_t * mac, const uint8_t *incoming, int len) {
    memcpy(&incomingData, incoming, sizeof(incomingData));
    int d = incomingData.direction;

    if (d >= 0 && d <= 3) {
        Serial.printf("VIBRATING: %d\n", d);
        digitalWrite(MOTOR_PINS[d], HIGH);
        delay(600); // Pulse duration
        digitalWrite(MOTOR_PINS[d], LOW);
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    Serial.println("\n=====================================");
    Serial.println("  ARMBAND RECEIVER: MOTORS READY     ");
    Serial.println("  F:13 | B:12 | L:14 | R:27          ");
    Serial.println("=====================================");

    for (int i = 0; i < 4; i++) {
        pinMode(MOTOR_PINS[i], OUTPUT);
        digitalWrite(MOTOR_PINS[i], LOW);
    }

    if (esp_now_init() != ESP_OK) return;

    // Register callback
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
}

void loop() {
    // Logic handled in callback
    delay(100);
}
