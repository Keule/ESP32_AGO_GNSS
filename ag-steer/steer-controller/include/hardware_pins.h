/**
 * @file hardware_pins.h
 * @brief Central pin and bus definitions for LilyGO T-ETH-Lite boards.
 *
 * Supported targets:
 *   - LILYGO_T_ETH_LITE_ESP32
 *   - LILYGO_T_ETH_LITE_ESP32S3
 *
 * This project keeps one canonical mapping per SoC target, selected via
 * PlatformIO build flags.
 */

#pragma once

#if defined(LILYGO_T_ETH_LITE_ESP32) && defined(LILYGO_T_ETH_LITE_ESP32S3)
  #error "Only one LilyGO T-ETH-Lite SoC target can be defined at once."
#endif

#if !defined(LILYGO_T_ETH_LITE_ESP32) && !defined(LILYGO_T_ETH_LITE_ESP32S3)
  #error "Missing target define: set LILYGO_T_ETH_LITE_ESP32 or LILYGO_T_ETH_LITE_ESP32S3."
#endif

// Guard against accidental duplicate IMU pin definitions from build flags or
// other headers. This file is the single source of truth for IMU wiring.
#if defined(IMU_RST) || defined(IMU_INT) || defined(CS_IMU) || defined(IMU_WAKE)
  #error "IMU pins already defined elsewhere; keep IMU pin mapping canonical in hardware_pins.h"
#endif

// ---------------------------------------------------------------------------
// SoC-specific pin and bus mapping
// ---------------------------------------------------------------------------
#if defined(LILYGO_T_ETH_LITE_ESP32S3)
  #define HW_SOC_LABEL "ESP32-S3"
  #define SENS_SPI_HOST FSPI
  #define SENS_SPI_HOST_LABEL "FSPI/SPI2_HOST"

  // W5500 Ethernet (board fixed)
  #define ETH_SCK        10
  #define ETH_MISO       11
  #define ETH_MOSI       12
  #define ETH_CS          9
  #define ETH_INT        13
  #define ETH_RST        14

  // Shared sensor SPI bus
  #define SENS_SPI_SCK   47
  #define SENS_SPI_MISO  21
  #define SENS_SPI_MOSI  38

  // IMU
  #define IMU_INT        46
  #define IMU_RST        41
  #define CS_IMU         40
  #define IMU_WAKE       15

  // Additional chip-selects
  #define CS_STEER_ANG   18
  #define CS_ACT         16

  // SD card (OTA)
  #define SD_SPI_SCK      7
  #define SD_SPI_MISO     5
  #define SD_SPI_MOSI     6
  #define SD_CS          42

  // Misc I/O
  #define SAFETY_IN       4
  #define LOG_SWITCH_PIN 46
#elif defined(LILYGO_T_ETH_LITE_ESP32)
  #define HW_SOC_LABEL "ESP32"
  #define SENS_SPI_HOST HSPI
  #define SENS_SPI_HOST_LABEL "HSPI/SPI2"

  // W5500 Ethernet (board fixed)
  #define ETH_SCK        10
  #define ETH_MISO       11
  #define ETH_MOSI       12
  #define ETH_CS          9
  #define ETH_INT        13
  #define ETH_RST        14

  // Shared sensor SPI bus (ESP32 variant mapping)
  #define SENS_SPI_SCK   14
  #define SENS_SPI_MISO  15
  #define SENS_SPI_MOSI  13

  // IMU
  #define IMU_INT        34
  #define IMU_RST        32
  #define CS_IMU         27
  #define IMU_WAKE       33

  // Additional chip-selects
  #define CS_STEER_ANG   26
  #define CS_ACT         25

  // SD card (OTA)
  #define SD_SPI_SCK      5
  #define SD_SPI_MISO    19
  #define SD_SPI_MOSI    23
  #define SD_CS          22

  // Misc I/O
  #define SAFETY_IN       4
  #define LOG_SWITCH_PIN 35
#endif

// ---------------------------------------------------------------------------
// Firmware OTA files on SD card
// ---------------------------------------------------------------------------
#define SD_FW_FILE_PRIMARY   "/firmware.bin"
#define SD_FW_FILE_ALT       "/update.bin"

// Version file on SD card (optional - contains e.g. "1.2.3")
#define SD_FW_VERSION_FILE   "/firmware.version"
