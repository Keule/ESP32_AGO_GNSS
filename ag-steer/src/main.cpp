/**
 * @file main.cpp
 * @brief TEST BUILD STEP 3c - swap UART1/UART2 to isolate the issue.
 *
 * Test A: UART2 on GPIO 45/46 (pins that work with UART1)
 * Test B: UART1 on GPIO 43/44 (pins that crash with UART2)
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include "hardware_pins.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== STEP3c: UART SWAP TEST ===");
    Serial.flush();

    // TEST A: UART2 on the WORKING pins (45/46)
    HardwareSerial testA(2);
    Serial.print("[A] UART2 on GPIO 45/46... "); Serial.flush();
    testA.begin(9600, SERIAL_8N1, GNSS_MAIN_RX, GNSS_MAIN_TX);
    Serial.println("OK"); Serial.flush();
    testA.end();

    // TEST B: UART1 on the CRASHING pins (43/44)
    HardwareSerial testB(1);
    Serial.print("[B] UART1 on GPIO 43/44... "); Serial.flush();
    testB.begin(9600, SERIAL_8N1, GNSS_HEADING_RX, GNSS_HEADING_TX);
    Serial.println("OK"); Serial.flush();
    testB.end();

    // TEST C: UART2 as LOCAL var on GPIO 43/44
    HardwareSerial testC(2);
    Serial.print("[C] UART2 local on GPIO 43/44... "); Serial.flush();
    testC.begin(9600, SERIAL_8N1, GNSS_HEADING_RX, GNSS_HEADING_TX);
    Serial.println("OK"); Serial.flush();
    testC.end();

    Serial.println("=== STEP3c: ALL OK ===");
    Serial.flush();
}

void loop() {
    delay(2000);
    Serial.println("loop alive");
    Serial.flush();
}
