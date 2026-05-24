/*
  Cochlea Cap — Receiver Unit
  Hardware: ESP32 NodeMCU + 4x vibration motors (via 2N2222 transistors)
  Role: Receives direction from sender via ESP-NOW, fires matching motor

  Wiring (per motor):
    GPIO → 1kΩ → 2N2222 base
    2N2222 collector → motor − terminal
    Motor + terminal → 3.3V
    1N4148 flyback diode across motor (cathode to 3.3V, anode to collector)
    2N2222 emitter → GND
*/

#include <esp_now.h>
#include <WiFi.h>

// ── Motor GPIO pins ───────────────────────────────────────────────────────────
#define MOTOR_FRONT 16
#define MOTOR_RIGHT 17
#define MOTOR_BACK  18
#define MOTOR_LEFT  19

// ── Direction constants ───────────────────────────────────────────────────────
#define DIR_FRONT 0
#define DIR_RIGHT 1
#define DIR_BACK  2
#define DIR_LEFT  3

// ── Buzz duration (should match sender LOCKOUT_MS) ────────────────────────────
#define BUZZ_MS 600

// ── Motor pin lookup ──────────────────────────────────────────────────────────
int motorPins[4] = {MOTOR_FRONT, MOTOR_RIGHT, MOTOR_BACK, MOTOR_LEFT};
const char* dirNames[4] = {"FRONT", "RIGHT", "BACK", "LEFT"};

// ── Buzz task — runs on a separate FreeRTOS task so loop() stays free ─────────
struct BuzzParams {
  int pin;
};

void buzzTask(void *pvParams) {
  BuzzParams *p = (BuzzParams *)pvParams;
  digitalWrite(p->pin, HIGH);
  vTaskDelay(pdMS_TO_TICKS(BUZZ_MS));
  digitalWrite(p->pin, LOW);
  delete p;
  vTaskDelete(NULL);
}

// ── ESP-NOW receive callback ──────────────────────────────────────────────────
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != 1) return;

  uint8_t direction = data[0];
  if (direction > 3) return;

  Serial.print("Received → ");
  Serial.println(dirNames[direction]);

  // Stop all motors first (safety — shouldn't be needed, but clean)
  for (int i = 0; i < 4; i++) {
    digitalWrite(motorPins[i], LOW);
  }

  // Spin up buzz on a task so callback returns immediately
  BuzzParams *p = new BuzzParams{motorPins[direction]};
  xTaskCreate(buzzTask, "buzz", 1024, p, 1, NULL);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_FRONT, OUTPUT);
  pinMode(MOTOR_RIGHT, OUTPUT);
  pinMode(MOTOR_BACK,  OUTPUT);
  pinMode(MOTOR_LEFT,  OUTPUT);

  // All motors off at boot
  digitalWrite(MOTOR_FRONT, LOW);
  digitalWrite(MOTOR_RIGHT, LOW);
  digitalWrite(MOTOR_BACK,  LOW);
  digitalWrite(MOTOR_LEFT,  LOW);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  Serial.println("Cochlea Cap receiver ready.");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  // Nothing to do — all work happens in the ESP-NOW callback + buzz task
  delay(100);
}
