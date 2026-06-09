#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// PIN DEFINITIONS
#define MIC_FRONT  34
#define MIC_BACK   35
#define MIC_LEFT   32
#define MIC_RIGHT  33

// SETTINGS
#define SAMPLE_WINDOW    50    // Microseconds between samples
#define SAMPLE_COUNT     100   // Samples per reading
#define THRESHOLD        300   // Sensitivity (Adjust this based on Serial Monitor)
#define LOCKOUT_TIME     1000  // Delay after trigger (ms)

// RECEIVER MAC ADDRESS
uint8_t receiverAddress[] = {0x00, 0x70, 0x07, 0x7E, 0x73, 0x98};

typedef struct struct_message {
    uint8_t direction; // 0:FRONT, 1:BACK, 2:LEFT, 3:RIGHT
} struct_message;

struct_message myData;
const char* labels[] = {"FRONT", "BACK", "LEFT", "RIGHT"};
const int pins[] = {MIC_FRONT, MIC_BACK, MIC_LEFT, MIC_RIGHT};

int getPeakToPeak(int pin) {
    int maxVal = 0;
    int minVal = 4095;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int read = analogRead(pin);
        if (read > maxVal) maxVal = read;
        if (read < minVal) minVal = read;
        delayMicroseconds(SAMPLE_WINDOW);
    }
    return maxVal - minVal;
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    // Print Pinout for verification
    Serial.println("\n--- COCHLEA CAP SENDER INITIALIZED ---");
    Serial.println("PINOUT CONFIGURATION:");
    Serial.println("FRONT Mic -> GPIO 34");
    Serial.println("BACK  Mic -> GPIO 35");
    Serial.println("LEFT  Mic -> GPIO 32");
    Serial.println("RIGHT Mic -> GPIO 33");
    Serial.println("---------------------------------------");

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }

    esp_now_register_send_cb(onDataSent);
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void loop() {
    int amplitudes[4];
    int loudestVal = 0;
    int directionIndex = -1;

    // Scan all 4 microphones
    for (int i = 0; i < 4; i++) {
        amplitudes[i] = getPeakToPeak(pins[i]);
        if (amplitudes[i] > loudestVal) {
            loudestVal = amplitudes[i];
            directionIndex = i;
        }
    }

    // Print levels for tuning
    Serial.printf("F:%d B:%d L:%d R:%d\n", amplitudes[0], amplitudes[1], amplitudes[2], amplitudes[3]);

    // Check if loudest sound crosses threshold
    if (loudestVal > THRESHOLD) {
        Serial.print(">>> TRIGGER: ");
        Serial.println(labels[directionIndex]);

        myData.direction = directionIndex;
        esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
        
        delay(LOCKOUT_TIME); 
    }
    delay(10);
}
