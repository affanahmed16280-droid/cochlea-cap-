/* =============================================================
 *  COCHLEA CAP — SENDER ESP32
 *  MAX4466 Microphone Layout:
 *    FRONT -> GPIO 32
 *    BACK  -> GPIO 33
 *    LEFT  -> GPIO 35
 *    RIGHT -> GPIO 34
 *
 *  Motor direction mapping sent to receiver:
 *    0 = FRONT  (receiver buzzes GPIO 14)
 *    1 = BACK   (receiver buzzes GPIO 27)
 *    2 = LEFT   (receiver buzzes GPIO 26)
 *    3 = RIGHT  (receiver buzzes GPIO 25)
 * ============================================================= */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ── Pin assignments ──────────────────────────────────────────
const int MIC_PINS[]  = {32, 33, 35, 34};          // FRONT, BACK, LEFT, RIGHT
const char* NAMES[]   = {"FRONT", "BACK", "LEFT", "RIGHT"};

// ── Tuning constants ─────────────────────────────────────────
#define THRESHOLD        4000   // Fire only when peak-to-peak >= 4000 (out of 4095)
#define SAMPLE_COUNT     200    // Samples per loudness window
#define SAMPLE_DELAY_US  40     // ~20 kHz sample rate
#define LOCKOUT_MS       450    // After a trigger, ignore all mics for 450 ms
                                // (prevents double-firing from the same sound)

// ── Receiver MAC address — update to match your receiver ─────
uint8_t receiverAddress[] = {0x00, 0x70, 0x07, 0x7E, 0x73, 0x98};

// ── Shared data structure (must match receiver) ──────────────
typedef struct struct_message {
    uint8_t direction;   // 0=FRONT 1=BACK 2=LEFT 3=RIGHT
} struct_message;

struct_message dataPacket;

// ── Measure peak-to-peak amplitude on one mic ────────────────
int getLoudness(int pin) {
    int maxVal = 0;
    int minVal = 4095;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int val = analogRead(pin);
        if (val > maxVal) maxVal = val;
        if (val < minVal) minVal = val;
        delayMicroseconds(SAMPLE_DELAY_US);
    }
    return maxVal - minVal;   // Peak-to-peak swing
}

// ── Send callback — compatible with ESP32 core 3.x ───────────
// Core 3.x changed the first argument from (const uint8_t*)
// to (const wifi_tx_info_t*).  Cast to the correct type so it
// compiles on both old and new toolchains.
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    Serial.print(">>> SEND STATUS: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    analogReadResolution(12);   // 0–4095

    Serial.println("\n=====================================");
    Serial.println("   COCHLEA CAP: SENSOR UNIT READY    ");
    Serial.println("   FRONT:32 | BACK:33 | L:35 | R:34  ");
    Serial.println("   THRESHOLD: 4000 / 4095             ");
    Serial.println("=====================================\n");

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed");
        return;
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Failed to add peer");
        return;
    }
}

// ── Main loop ─────────────────────────────────────────────────
void loop() {
    int peakLevels[4];
    int topVolume = 0;
    int bestDir   = -1;

    // Sample all four microphones
    for (int i = 0; i < 4; i++) {
        peakLevels[i] = getLoudness(MIC_PINS[i]);
        if (peakLevels[i] > topVolume) {
            topVolume = peakLevels[i];
            bestDir   = i;
        }
    }

    // Serial plotter / monitor output
    Serial.printf("FRONT:%d BACK:%d LEFT:%d RIGHT:%d\n",
                  peakLevels[0], peakLevels[1],
                  peakLevels[2], peakLevels[3]);

    // Only fire when the LOUDEST mic exceeds 4000
    if (topVolume >= THRESHOLD && bestDir >= 0) {
        Serial.printf(">>> DETECTED: %s  (peak-to-peak: %d)\n",
                      NAMES[bestDir], topVolume);

        dataPacket.direction = (uint8_t)bestDir;
        esp_now_send(receiverAddress, (uint8_t *)&dataPacket, sizeof(dataPacket));

        // ── 450 ms lockout ───────────────────────────────────
        // Silences all mics so a single sound doesn't
        // re-trigger on its echo or ringing
        delay(LOCKOUT_MS);
    }

    delay(50);   // Short breathing room between scan windows
}
