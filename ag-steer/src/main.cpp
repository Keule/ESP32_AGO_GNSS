/**
 * @file main.cpp
 * @brief TEST BUILD STEP 3b - UART2 heading: try different baud rates.
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
    Serial.println("=== STEP3b: UART2 baud rate test ===");
    Serial.flush();

    // UART1 always works (we know from STEP3a)
    gnssMainSerial.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_MAIN_RX, GNSS_MAIN_TX);
    Serial.printf("[OK] UART1 Main @ 460800 (RX=%d TX=%d)\n", GNSS_MAIN_RX, GNSS_MAIN_TX);
    Serial.flush();

    // Test UART2 with increasing baud rates
    const int bauds[] = {9600, 115200, 460800};
    const char* baud_names[] = {"9600", "115200", "460800"};

    for (int i = 0; i < 3; i++) {
        Serial.printf("[TEST] UART2 Heading @ %s (RX=%d TX=%d)... ",
                      baud_names[i], GNSS_HEADING_RX, GNSS_HEADING_TX);
        Serial.flush();

        gnssHeadingSerial.updateBaudRate(bauds[i]);
        // If updateBaudRate doesn't work, end and re-begin
        gnssHeadingSerial.end();
        gnssHeadingSerial.begin(bauds[i], SERIAL_8N1, GNSS_HEADING_RX, GNSS_HEADING_TX);
        Serial.println("OK");
        Serial.flush();
    }

    Serial.println("=== STEP3b: ALL OK ===");
    Serial.flush();
}

void loop() {
    delay(2000);
    Serial.println("loop alive");
    Serial.flush();
}
