#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

/* 
 * PINOUT CONFIGURATION (ADC1 ONLY)
 * FRONT Mic -> GPIO 34
 * BACK  Mic -> GPIO 35
 * LEFT  Mic -> GPIO 32
 * RIGHT Mic -> GPIO 33
 */

const int MIC_PINS[] = {34, 35, 32, 33};
const char* NAMES[]  = {"FRONT", "BACK", "LEFT", "RIGHT"};

// --- TUNING CONSTANTS ---
#define THRESHOLD        450    // Increase if too sensitive, decrease if not picking up sound
#define SAMPLE_COUNT     150    // Samples per reading window
#define SAMPLE_DELAY_US  40     // Timing between samples (~20kHz)
#define LOCKOUT_MS       1200   // Wait time after trigger (prevents vibration loop)

// RECEIVER MAC ADDRESS
uint8_t receiverAddress[] = {0x00, 0x70, 0x07, 0x7E, 0x73, 0x98};

typedef struct struct_message {
    uint8_t direction; 
} struct_message;

struct_message dataPacket;

int getLoudness(int pin) {
    int maxVal = 0;
    int minVal = 4095;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int val = analogRead(pin);
        if (val > maxVal) maxVal = val;
        if (val < minVal) minVal = val;
        delayMicroseconds(SAMPLE_DELAY_US);
    }
    return maxVal - minVal;
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    analogReadResolution(12);

    Serial.println("\n=====================================");
    Serial.println("   COCHLEA CAP: SENSOR UNIT READY    ");
    Serial.println("   FRONT:34 | BACK:35 | L:32 | R:33  ");
    Serial.println("=====================================");

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Error");
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void loop() {
    int peakLevels[4];
    int topVolume = 0;
    int bestDir = -1;

    for (int i = 0; i < 4; i++) {
        peakLevels[i] = getLoudness(MIC_PINS[i]);
        if (peakLevels[i] > topVolume) {
            topVolume = peakLevels[i];
            bestDir = i;
        }
    }

    // Monitor levels in Serial Plotter/Monitor
    Serial.printf("F:%d B:%d L:%d R:%d\n", peakLevels[0], peakLevels[1], peakLevels[2], peakLevels[3]);

    if (topVolume > THRESHOLD) {
        Serial.printf(">>> DETECTED: %s (VAL: %d)\n", NAMES[bestDir], topVolume);
        
        dataPacket.direction = (uint8_t)bestDir;
        esp_now_send(receiverAddress, (uint8_t *) &dataPacket, sizeof(dataPacket));
        
        delay(LOCKOUT_MS); 
    }
    delay(20);
}
