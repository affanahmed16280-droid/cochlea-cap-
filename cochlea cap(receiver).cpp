/* =============================================================
 *  HAPTIC ARMBAND — RECEIVER ESP32
 *  Vibration Motor Layout:
 *    FRONT motor -> GPIO 14   (direction 0)
 *    BACK  motor -> GPIO 27   (direction 1)
 *    LEFT  motor -> GPIO 26   (direction 2)
 *    RIGHT motor -> GPIO 25   (direction 3)
 *
 *  Each motor fires a single 500 ms pulse when the sender
 *  detects sound louder than 4000 / 4095 on the matching mic.
 * ============================================================= */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ── Motor pin assignments ────────────────────────────────────
//   Index:  0=FRONT  1=BACK  2=LEFT  3=RIGHT
const int MOTOR_PINS[] = {14, 27, 26, 25};
const char* NAMES[]    = {"FRONT", "BACK", "LEFT", "RIGHT"};

// ── Vibration pulse duration ─────────────────────────────────
#define VIBRATION_MS  500   // How long the motor buzzes (ms)

// ── Shared data structure (must match sender) ────────────────
typedef struct struct_message {
    uint8_t direction;   // 0=FRONT 1=BACK 2=LEFT 3=RIGHT
} struct_message;

struct_message incomingData;

// ── ESP-NOW receive callback ─────────────────────────────────
void onDataRecv(const uint8_t *mac, const uint8_t *incoming, int len) {
    // Safety check: ignore malformed packets
    if (len < (int)sizeof(incomingData)) {
        Serial.println("[WARN] Short packet ignored");
        return;
    }

    memcpy(&incomingData, incoming, sizeof(incomingData));
    int d = incomingData.direction;

    // Validate direction index
    if (d < 0 || d > 3) {
        Serial.printf("[WARN] Unknown direction: %d\n", d);
        return;
    }

    Serial.printf(">>> RECEIVED: %s  -> GPIO %d  BUZZING for %d ms\n",
                  NAMES[d], MOTOR_PINS[d], VIBRATION_MS);

    // ── Single 500 ms vibration pulse ────────────────────────
    digitalWrite(MOTOR_PINS[d], HIGH);
    delay(VIBRATION_MS);
    digitalWrite(MOTOR_PINS[d], LOW);

    Serial.printf("    Motor %s OFF\n", NAMES[d]);
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    Serial.println("\n=====================================");
    Serial.println("  ARMBAND RECEIVER: MOTORS READY     ");
    Serial.println("  FRONT:14 | BACK:27 | L:26 | R:25   ");
    Serial.println("  PULSE: 500 ms per vibration         ");
    Serial.println("=====================================\n");

    // Initialise all motor pins as outputs, default LOW
    for (int i = 0; i < 4; i++) {
        pinMode(MOTOR_PINS[i], OUTPUT);
        digitalWrite(MOTOR_PINS[i], LOW);
    }

    // ── Quick startup test — each motor fires for 150 ms ─────
    Serial.println("[ STARTUP TEST ] Cycling all motors...");
    for (int i = 0; i < 4; i++) {
        Serial.printf("  Testing %s motor (GPIO %d)\n", NAMES[i], MOTOR_PINS[i]);
        digitalWrite(MOTOR_PINS[i], HIGH);
        delay(150);
        digitalWrite(MOTOR_PINS[i], LOW);
        delay(100);
    }
    Serial.println("[ STARTUP TEST ] Done.\n");

    // ── ESP-NOW init ─────────────────────────────────────────
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
    Serial.println("Waiting for direction signals...\n");
}

// ── Main loop ─────────────────────────────────────────────────
void loop() {
    // All work is done in the ESP-NOW callback.
    // Keep the loop alive with a minimal delay.
    delay(500);
}
