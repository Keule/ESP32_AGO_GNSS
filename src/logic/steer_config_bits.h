#pragma once

#include <cstdint>

namespace steer_cfg_set0 {
constexpr uint8_t INVERT_WAS            = 1u << 0;
constexpr uint8_t RELAY_ACTIVE_HIGH     = 1u << 1;
constexpr uint8_t MOTOR_DRIVE_DIRECTION = 1u << 2;
constexpr uint8_t SINGLE_INPUT_WAS      = 1u << 3;
constexpr uint8_t CYTRON_DRIVER         = 1u << 4;
constexpr uint8_t STEER_SWITCH          = 1u << 5;
constexpr uint8_t STEER_BUTTON          = 1u << 6;
constexpr uint8_t SHAFT_ENCODER         = 1u << 7;
constexpr uint8_t KNOWN_MASK = INVERT_WAS | RELAY_ACTIVE_HIGH | MOTOR_DRIVE_DIRECTION |
                               SINGLE_INPUT_WAS | CYTRON_DRIVER | STEER_SWITCH |
                               STEER_BUTTON | SHAFT_ENCODER;
}  // namespace steer_cfg_set0

namespace steer_cfg_set1 {
constexpr uint8_t DANFOSS_VALVE   = 1u << 0;
constexpr uint8_t PRESSURE_SENSOR = 1u << 1;
constexpr uint8_t CURRENT_SENSOR  = 1u << 2;
constexpr uint8_t USE_Y_AXIS      = 1u << 3;
constexpr uint8_t KNOWN_MASK = DANFOSS_VALVE | PRESSURE_SENSOR | CURRENT_SENSOR | USE_Y_AXIS;
}  // namespace steer_cfg_set1
