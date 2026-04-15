/**
 * @file hal_bno085.cpp
 * @brief BNO085 HAL binding using the local esp32_bno08x_driver.
 */

#include "hal/hal.h"
#include "hardware_pins.h"

#include <Arduino.h>
#include <stddef.h>

extern "C" {
#include "bno08x_driver.h"
}

namespace {

constexpr uint32_t kBno085SpiHz = 1000000;
constexpr uint32_t kBno085PeriodUs = 5000;
constexpr float kRadToDeg = 57.2957795f;

BNO08x s_bno08x;
bool s_bno08x_begin_attempted = false;
bool s_bno08x_ready = false;
uint32_t s_bno08x_last_gyro_count = 0;
uint32_t s_bno08x_last_quat_count = 0;

float s_imu_yaw_cache = 0.0f;
float s_imu_roll_cache = 0.0f;
uint32_t s_imu_last_poll_us = 0;
uint32_t s_imu_last_data_us = 0;
bool s_imu_cache_valid = false;

void enableRuntimeReports() {
    BNO08x_enable_rotation_vector(&s_bno08x, kBno085PeriodUs);
    BNO08x_enable_gyro(&s_bno08x, kBno085PeriodUs);
    s_bno08x_last_gyro_count = BNO08x_get_gyro_update_count(&s_bno08x);
    s_bno08x_last_quat_count = BNO08x_get_quat_update_count(&s_bno08x);
    s_imu_last_data_us = 0;
    s_imu_cache_valid = false;
}

} // namespace

void hal_esp32_imu_spi_check_deadline(uint32_t period_us, uint32_t now_us);
bool hal_esp32_imu_raw_transfer(const uint8_t* tx, uint8_t* rx, size_t len);

void hal_imu_begin(void) {
    if (s_bno08x_begin_attempted) {
        return;
    }
    s_bno08x_begin_attempted = true;

    BNO08x_config_t imu_config = {
        SPI2_HOST,
        static_cast<gpio_num_t>(SENS_SPI_MOSI),
        static_cast<gpio_num_t>(SENS_SPI_MISO),
        static_cast<gpio_num_t>(SENS_SPI_SCK),
        static_cast<gpio_num_t>(CS_IMU),
        static_cast<gpio_num_t>(IMU_INT),
        static_cast<gpio_num_t>(IMU_RST),
        static_cast<gpio_num_t>(IMU_WAKE),
        kBno085SpiHz,
        0,
        5,
    };

    BNO08x_init(&s_bno08x, &imu_config);
    s_bno08x_ready = BNO08x_initialize(&s_bno08x);
    if (!s_bno08x_ready) {
        hal_log("ESP32: BNO085 init failed via shared FSPI transfer layer (CS=%d INT=%d RST=%d WAKE/PS0=%d)",
                CS_IMU, IMU_INT, IMU_RST, IMU_WAKE);
        return;
    }

    enableRuntimeReports();
    hal_log("ESP32: BNO085 initialised via esp32_bno08x_driver on shared FSPI (SCK=%d MISO=%d MOSI=%d CS=%d INT=%d RST=%d WAKE/PS0=%d freq=%luHz)",
            SENS_SPI_SCK,
            SENS_SPI_MISO,
            SENS_SPI_MOSI,
            CS_IMU,
            IMU_INT,
            IMU_RST,
            IMU_WAKE,
            (unsigned long)kBno085SpiHz);
}

void hal_imu_reset_pulse(uint32_t low_ms, uint32_t settle_ms) {
    if (s_bno08x_ready) {
        const bool ok = BNO08x_hard_reset(&s_bno08x);
        if (ok) {
            enableRuntimeReports();
        }
        hal_log("ESP32: IMU hard reset via esp32_bno08x_driver %s", ok ? "OK" : "FAIL");
        return;
    }

    pinMode(IMU_WAKE, OUTPUT);
    digitalWrite(IMU_WAKE, HIGH);

    const int int_before = digitalRead(IMU_INT);
    const int wake_before = digitalRead(IMU_WAKE);
    digitalWrite(IMU_RST, LOW);
    delay(low_ms);
    const int int_during = digitalRead(IMU_INT);
    const int rst_during = digitalRead(IMU_RST);
    digitalWrite(IMU_RST, HIGH);
    delay(settle_ms);
    const int int_after = digitalRead(IMU_INT);
    const int rst_after = digitalRead(IMU_RST);
    const int wake_after = digitalRead(IMU_WAKE);
    hal_log("ESP32: IMU reset pulse low=%lums settle=%lums INT(before=%d during=%d after=%d) RST(during=%d after=%d) WAKE/PS0(before=%d after=%d)",
            (unsigned long)low_ms,
            (unsigned long)settle_ms,
            int_before, int_during, int_after,
            rst_during, rst_after,
            wake_before, wake_after);
}

bool hal_imu_read(float* yaw_rate_dps, float* roll_deg) {
    if (!yaw_rate_dps || !roll_deg) return false;
    if (!s_bno08x_ready) return false;

    const uint32_t now_us = micros();
    if (!s_imu_cache_valid || (now_us - s_imu_last_poll_us) >= kBno085PeriodUs) {
        hal_esp32_imu_spi_check_deadline(kBno085PeriodUs, now_us);

        const uint32_t gyro_count = BNO08x_get_gyro_update_count(&s_bno08x);
        const uint32_t quat_count = BNO08x_get_quat_update_count(&s_bno08x);
        const bool has_new_data =
            (gyro_count != s_bno08x_last_gyro_count) ||
            (quat_count != s_bno08x_last_quat_count);

        if (!has_new_data) {
            const bool cached_data_fresh =
                s_imu_cache_valid &&
                s_imu_last_data_us != 0 &&
                (now_us - s_imu_last_data_us) <= 50000UL;
            if (!cached_data_fresh) {
                return false;
            }
            *yaw_rate_dps = s_imu_yaw_cache;
            *roll_deg = s_imu_roll_cache;
            return true;
        }

        if (!s_imu_cache_valid && (gyro_count == 0 || quat_count == 0)) {
            return false;
        }

        if (gyro_count != s_bno08x_last_gyro_count) {
            const float yaw_rate_rad_s = BNO08x_get_gyro_calibrated_velocity_Z(&s_bno08x);
            s_imu_yaw_cache = yaw_rate_rad_s * kRadToDeg;
            s_bno08x_last_gyro_count = gyro_count;
        }

        if (quat_count != s_bno08x_last_quat_count) {
            s_imu_roll_cache = BNO08x_get_roll_deg(&s_bno08x);
            s_bno08x_last_quat_count = quat_count;
        }

        s_imu_last_poll_us = now_us;
        s_imu_last_data_us = now_us;
        s_imu_cache_valid = true;
    }
    *yaw_rate_dps = s_imu_yaw_cache;
    *roll_deg = s_imu_roll_cache;
    return true;
}

bool hal_imu_detect(void) {
    hal_log("ESP32: IMU detect via esp32_bno08x_driver/shared FSPI: %s",
            s_bno08x_ready ? "OK" : "FAIL");
    return s_bno08x_ready;
}

bool hal_imu_probe_once(uint8_t* out_response) {
    if (!out_response) return false;
    if (s_bno08x_ready) {
        *out_response = 0x01;
        return true;
    }

    uint8_t tx[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t rx[4] = {0x00, 0x00, 0x00, 0x00};
    const bool transfer_ok = hal_esp32_imu_raw_transfer(tx, rx, sizeof(rx));
    *out_response = rx[0];
    return transfer_ok && rx[0] != 0x00 && rx[0] != 0xFF;
}

bool hal_imu_detect_boot_qualified(HalImuDetectStats* out) {
    HalImuDetectStats local = {};
    local.samples = 1;
    local.present = s_bno08x_ready;
    local.last_response = s_bno08x_ready ? 0x01 : 0x00;
    if (s_bno08x_ready) {
        local.ok_count = 1;
        local.other_count = 1;
    } else {
        local.zero_count = 1;
    }

    hal_log("ESP32: IMU boot check via esp32_bno08x_driver/shared FSPI: present=%s ok=%u/%u ff=%u zero=%u last=0x%02X",
            local.present ? "YES" : "NO",
            (unsigned)local.ok_count,
            (unsigned)local.samples,
            (unsigned)local.ff_count,
            (unsigned)local.zero_count,
            (unsigned)local.last_response);

    if (out) {
        *out = local;
    }
    return local.present;
}
