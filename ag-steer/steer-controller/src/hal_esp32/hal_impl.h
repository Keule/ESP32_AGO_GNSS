/**
 * @file hal_impl.h
 * @brief ESP32-S3 HAL implementation declarations.
 */

#pragma once

#include "hal/hal.h"

/// Initialise all ESP32 HAL subsystems.
void hal_esp32_init_all(void);

/// Check if the shared SPI bus is currently busy (SD card access).
/// Sensor reads should be skipped when this returns true.
bool hal_spi_busy(void);
