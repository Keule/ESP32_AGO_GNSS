/**
 * @file main.cpp
 * @brief TEST BUILD STEP 3a - only GNSS UARTs, isolate crash.
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include "hardware_pins.h"

static HardwareSerial gnssMainSerial(1);
static HardwareSerial gnssHeadingSerial(2);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== STEP3a: GNSS UARTs only ===");
    Serial.flush();

    Serial.print("[1] GNSS Main begin (RX=45 TX=46 @ 460800)... ");
    Serial.flush();
    gnssMainSerial.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_MAIN_RX, GNSS_MAIN_TX);
    Serial.println("OK");
    Serial.flush();

    Serial.print("[2] GNSS Heading begin (RX=43 TX=44 @ 460800)... ");
    Serial.flush();
    gnssHeadingSerial.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_HEADING_RX, GNSS_HEADING_TX);
    Serial.println("OK");
    Serial.flush();

    Serial.println("=== STEP3a: ALL OK ===");
    Serial.flush();
}

void loop() {
    delay(2000);
    Serial.println("loop alive");
    Serial.flush();
}
