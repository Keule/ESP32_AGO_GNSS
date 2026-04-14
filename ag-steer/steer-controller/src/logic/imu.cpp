/**
 * @file imu.cpp
 * @brief IMU driver implementation (stub for BNO085 SPI).
 *
 * TODO: Implement full BNO085 SH-2 / sensor hub protocol over SPI.
 * Currently reads via HAL stub (PC sim returns dummy values).
 */

#include "imu.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_IMU
#include "esp_log.h"
#include "log_ext.h"

void imuInit(void) {
    hal_imu_begin();
    LOGI("IMU", "initialised (BNO085 SPI stub)");
}

bool imuUpdate(void) {
    float yaw_rate = 0.0f;
    float roll = 0.0f;

    if (!hal_imu_read(&yaw_rate, &roll)) {
        StateLock lock;
        g_nav.imu_quality_ok = false;
        return false;
    }

    const uint32_t now_ms = hal_millis();
    const bool plausible = dep_policy::isImuPlausible(yaw_rate, roll);

    {
        StateLock lock;
        g_nav.yaw_rate_dps = yaw_rate;
        g_nav.roll_deg = roll;
        g_nav.imu_timestamp_ms = now_ms;
        g_nav.imu_quality_ok = plausible;
        g_nav.timestamp_ms = now_ms;
    }

    return plausible;
}
