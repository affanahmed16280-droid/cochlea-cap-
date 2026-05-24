/*
  Cochlea Cap — Sender Unit
  Hardware: ESP32 WROOM-32U + 4x MAX4466 microphones (analog)
  Role: Reads mic levels via ADC1, finds loudest direction, sends via ESP-NOW

  Wiring:
    MIC FRONT → GPIO 36 (VP)
    MIC RIGHT → GPIO 39 (VN)
    MIC BACK  → GPIO 34
    MIC LEFT  → GPIO 35
    All mics: VCC → 3.3V, GND → GND

  ADC1 only — ADC2 is disabled when WiFi/ESP-NOW is active.
*/

#include <esp_now.h>
#include <WiFi.h>

// ── Receiver MAC address ────────────────────────────────────────────────────
// Replace with your actual receiver ESP32 MAC
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── Direction constants ──────────────────────────────────────────────────────
#define DIR_FRONT 0
#define DIR_RIGHT 1
#define DIR_BACK  2
#define DIR_LEFT  3

// ── ADC1 pins only ───────────────────────────────────────────────────────────
#define MIC_FRONT 36
#define MIC_RIGHT 39
#define MIC_BACK  34
#define MIC_LEFT  35

// ── Tuning ───────────────────────────────────────────────────────────────────
#define SOUND_THRESHOLD  300   // 0–4095 scale; raise if false triggers, lower if missing sounds
#define LOCKOUT_MS       600   // Block period after one direction fires (ms)
#define SAMPLE_COUNT     64    // Samples averaged per mic per scan

// ── Globals ──────────────────────────────────────────────────────────────────
unsigned long lockoutUntil = 0;

// ── ESP-NOW send callback ─────────────────────────────────────────────────────
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
#else
void onSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  // Intentionally silent
}

// ── Read peak deviation from one analog mic ───────────────────────────────────
// Reads SAMPLE_COUNT samples, returns how far the peak swings from midpoint (2048)
int readMicPeak(int pin) {
  int peak = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int raw = analogRead(pin);
    int deviation = abs(raw - 2048);  // 2048 = midpoint of 12-bit ADC
    if (deviation > peak) peak = deviation;
  }
  return peak;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // ADC resolution: 12-bit (0–4095)
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Peer add failed");
  }

  Serial.println("Cochlea Cap sender ready (MAX4466).");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  // Skip scan entirely during lockout window
  if (millis() < lockoutUntil) {
    delay(10);
    return;
  }

  int pins[4]          = {MIC_FRONT, MIC_RIGHT, MIC_BACK, MIC_LEFT};
  const char* names[4] = {"FRONT",   "RIGHT",   "BACK",   "LEFT"};

  int maxPeak = 0;
  int loudest = -1;

  for (int i = 0; i < 4; i++) {
    int peak = readMicPeak(pins[i]);

    if (peak > maxPeak) {
      maxPeak = peak;
      loudest = i;
    }
  }

  if (maxPeak >= SOUND_THRESHOLD && loudest >= 0) {
    uint8_t direction = (uint8_t)loudest;
    esp_now_send(receiverMAC, &direction, sizeof(direction));

    Serial.print("Direction → ");
    Serial.print(names[loudest]);
    Serial.print("  peak=");
    Serial.println(maxPeak);

    // Engage lockout — no re-triggering until buzz finishes
    lockoutUntil = millis() + LOCKOUT_MS;
  }
}
