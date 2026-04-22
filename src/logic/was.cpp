/**
 * @file was.cpp
 * @brief Wheel Angle Sensor (WAS) implementation.
 */

#include "was.h"

#include "dependency_policy.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_WAS
#include "esp_log.h"
#include "log_ext.h"

namespace {
float s_last_was_angle_deg = 0.0f;

bool was_enabled_check() {
    return feat::ads();
}

bool was_health_check(uint32_t now_ms) {
    return wasIsHealthy(now_ms);
}
}  // namespace

void wasInit(void) {
    hal_steer_angle_begin();
    LOGI("WAS", "initialised (SPI stub)");
}

bool wasUpdate(void) {
    const float angle = hal_steer_angle_read_deg();
    const uint32_t now_ms = hal_millis();
    const bool plausible = dep_policy::isSteerAnglePlausible(angle);

    {
        StateLock lock;
        g_nav.steer_angle_deg = angle;
        g_nav.steer_angle_timestamp_ms = now_ms;
        g_nav.steer_angle_quality_ok = plausible;
    }

    s_last_was_angle_deg = angle;
    return plausible;
}

bool wasIsHealthy(uint32_t now_ms) {
    StateLock lock;
    if (!g_nav.steer_angle_quality_ok) return false;
    return dep_policy::isFresh(now_ms,
                               g_nav.steer_angle_timestamp_ms,
                               dep_policy::STEER_ANGLE_FRESHNESS_TIMEOUT_MS);
}

float steerAngleReadDeg(void) {
    const bool ok = wasUpdate();
    if (!ok) return 0.0f;
    return s_last_was_angle_deg;
}

const ModuleOps was_ops = {
    "WAS",
    was_enabled_check,
    wasInit,
    wasUpdate,
    was_health_check
};
