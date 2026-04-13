/**
 * @file control.h
 * @brief PID controller and 200 Hz control loop.
 *
 * Reads sensors, computes PID, writes actuator.
 * Safety monitoring is integrated.
 */

#pragma once

#include <cstdint>

/// PID controller state.
struct PidState {
    float kp;               // Proportional gain
    float ki;               // Integral gain
    float kd;               // Derivative gain
    float integral;         // Accumulated integral term
    float prev_error;       // Previous error for derivative
    float output_min;       // Minimum output
    float output_max;       // Maximum output
    uint32_t last_update_ms;
    bool   first_update;
};

/// Initialise PID with default parameters.
void pidInit(PidState* pid,
             float kp = 1.0f, float ki = 0.0f, float kd = 0.0f,
             float out_min = 0.0f, float out_max = 65535.0f);

/// Reset PID internal state (integral, prev_error).
void pidReset(PidState* pid);

/// Compute PID output.
/// @param pid     PID state
/// @param error   current error (setpoint - measurement)
/// @param dt_ms   time step in milliseconds
/// @return computed output value
float pidCompute(PidState* pid, float error, uint32_t dt_ms);

/// Initialise control subsystem (IMU, steer angle sensor, actuator).
void controlInit(void);

/// Run one control step. Should be called at 200 Hz.
/// Reads sensors, checks safety, computes PID, writes actuator.
void controlStep(void);

/// Update PID gains and actuator limits from AgIO steer settings (PGN 252).
/// AgOpenGPS v5 sends: Kp(uint8), HighPWM(uint8), LowPWM(uint8),
/// MinPWM(uint8), CountsPerDegree(uint8), WASOffset(int16), Ackerman(uint8).
/// @param kp              proportional gain (raw)
/// @param highPWM         maximum actuator PWM
/// @param lowPWM          deadband / no-action PWM band
/// @param minPWM          minimum actuator PWM for instant on
/// @param countsPerDegree sensor counts per degree
/// @param wasOffset       sensor zero offset (counts)
/// @param ackerman        Ackerman correction factor (value / 100.0)
void controlUpdateSettings(uint8_t kp, uint8_t highPWM, uint8_t lowPWM,
                           uint8_t minPWM, uint8_t countsPerDegree,
                           int16_t wasOffset, uint8_t ackerman);

// ===================================================================
// Globals
// ===================================================================

/// Setpoint from AgIO steer data (degrees). Written by commTask.
extern volatile float desiredSteerAngleDeg;
