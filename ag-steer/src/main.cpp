/**
 * @file main.cpp
 * @brief TEST BUILD STEP 3 - GNSS UARTs + ADS1118 library init.
 */

#include <Arduino.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include "hardware_pins.h"
#include "ads1118.h"

static SPIClass sensorSPI(FSPI);
static HardwareSerial gnssMainSerial(1);
static HardwareSerial gnssHeadingSerial(2);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== STEP3: GNSS + ADS1118 ===");
    Serial.flush();

    // Sensor SPI
    sensorSPI.begin(SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI, -1);
    Serial.printf("[1] sensor SPI OK\n"); Serial.flush();

    // GNSS UARTs
    gnssMainSerial.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_MAIN_RX, GNSS_MAIN_TX);
    gnssHeadingSerial.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_HEADING_RX, GNSS_HEADING_TX);
    Serial.printf("[2] GNSS UARTs OK (Main RX=%d TX=%d, Heading RX=%d TX=%d)\n",
                  GNSS_MAIN_RX, GNSS_MAIN_TX, GNSS_HEADING_RX, GNSS_HEADING_TX);
    Serial.flush();

    // ADS1118 init
    pinMode(CS_IMU, OUTPUT);  digitalWrite(CS_IMU, HIGH);
    pinMode(CS_ACT, OUTPUT);  digitalWrite(CS_ACT, HIGH);
    Serial.printf("[3] CS pins OK (IMU=%d ACT=%d)\n", CS_IMU, CS_ACT); Serial.flush();

    ADS1118 ads(sensorSPI);
    ads.begin(CS_STEER_ANG, nullptr, 0, nullptr);
    Serial.printf("[4] ADS1118 begin OK (CS=%d)\n", CS_STEER_ANG); Serial.flush();

    // ADS1118 detect (takes ~50ms)
    Serial.print("[5] ADS1118 detect... "); Serial.flush();
    bool detected = ads.detect();
    if (detected) {
        Serial.printf("DETECTED (FSR=%.3fV, inv=%d, mode=%d)\n",
                      ads.getFSR(), ads.isDoutInverted(), ads.getSPIMode());
    } else {
        Serial.println("NOT FOUND");
    }
    Serial.flush();

    Serial.println("=== STEP3: ALL OK ===");
    Serial.flush();
}

void loop() {
    delay(2000);
    Serial.println("loop alive");
    Serial.flush();
}
