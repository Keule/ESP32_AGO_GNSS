/**
 * @file main.cpp
 * @brief TEST BUILD STEP 2 - HAL init: mutex, safety, sensor SPI.
 */

#include <Arduino.h>
#include <SPI.h>
#include "hardware_pins.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== STEP2: HAL INIT ===");
    Serial.flush();

    // Mutex (static allocation)
    StaticSemaphore_t buf;
    SemaphoreHandle_t mux = xSemaphoreCreateRecursiveMutexStatic(&buf);
    Serial.printf("[1] mutex: %s\n", mux ? "OK" : "FAIL");
    Serial.flush();

    // Safety pin
    pinMode(SAFETY_IN, INPUT_PULLUP);
    Serial.printf("[2] safety GPIO=%d: OK\n", SAFETY_IN);
    Serial.flush();

    // Sensor SPI bus (FSPI = SPI2_HOST)
    SPIClass sensorSPI(FSPI);
    Serial.print("[3] sensor SPI begin... "); Serial.flush();
    sensorSPI.begin(SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI, -1);
    Serial.println("OK");
    Serial.printf("    SCK=%d MISO=%d MOSI=%d\n", SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI);
    Serial.flush();

    Serial.println("=== STEP2: ALL OK ===");
    Serial.flush();
}

void loop() {
    delay(1000);
    Serial.println("loop alive");
    Serial.flush();
}
