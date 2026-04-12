/**
 * @file main.cpp
 * @brief TEST BUILD - minimal firmware to isolate boot crash.
 *
 * STEP 1: Only Serial. If this works, add more step by step.
 */

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== STEP1: SERIAL OK ===");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
    Serial.println("=== STEP1: DONE ===");
    Serial.flush();
}

void loop() {
    delay(1000);
    Serial.println("loop alive");
    Serial.flush();
}
