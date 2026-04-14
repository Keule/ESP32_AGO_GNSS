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

namespace {
#if defined(FEAT_IMU_BRINGUP)
constexpr bool k_imu_bringup_mode = true;
#else
constexpr bool k_imu_bringup_mode = false;
#endif

constexpr uint32_t k_detect_interval_ms = 1000;
constexpr uint32_t k_read_interval_ms = 100;
uint32_t s_last_detect_ms = 0;
uint32_t s_last_read_ms = 0;
uint32_t s_read_ok = 0;
uint32_t s_read_fail = 0;
bool s_last_detect_ok = false;
}  // namespace

bool imuBringupModeEnabled(void) {
    return k_imu_bringup_mode;
}

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

void imuBringupInit(void) {
    HalImuSpiInfo spi = {};
    hal_imu_get_spi_info(&spi);

    LOGI("IMU-BRINGUP",
         "mode=ON imu_spi pins[sck=%d miso=%d mosi=%d cs=%d int=%d] params[freq=%luHz mode=%u] intervals[detect=%lums read=%lums]",
         spi.sck_pin, spi.miso_pin, spi.mosi_pin, spi.cs_pin, spi.int_pin,
         (unsigned long)spi.freq_hz, (unsigned)spi.mode,
         (unsigned long)k_detect_interval_ms, (unsigned long)k_read_interval_ms);

    s_last_detect_ms = 0;
    s_last_read_ms = 0;
    s_read_ok = 0;
    s_read_fail = 0;
    s_last_detect_ok = false;
}

void imuBringupTick(void) {
    const uint32_t now_ms = hal_millis();

    if (now_ms - s_last_detect_ms >= k_detect_interval_ms) {
        s_last_detect_ms = now_ms;
        s_last_detect_ok = hal_imu_detect();
        LOGI("IMU-BRINGUP", "detect=%s", s_last_detect_ok ? "OK" : "FAIL");
    }

    if (now_ms - s_last_read_ms >= k_read_interval_ms) {
        s_last_read_ms = now_ms;
        float yaw_rate = 0.0f;
        float roll = 0.0f;
        const bool ok = hal_imu_read(&yaw_rate, &roll);
        if (ok) {
            s_read_ok++;
        } else {
            s_read_fail++;
        }

        LOGI("IMU-BRINGUP",
             "read=%s yaw=%.3f roll=%.3f cnt_ok=%lu cnt_fail=%lu detect=%s",
             ok ? "OK" : "FAIL",
             yaw_rate,
             roll,
             (unsigned long)s_read_ok,
             (unsigned long)s_read_fail,
             s_last_detect_ok ? "OK" : "FAIL");
    }
}
