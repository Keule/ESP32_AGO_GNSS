/**
 * @file net.cpp
 * @brief Network / UDP communication implementation.
 *
 * Sends:
 *   - Steer Status Out (PGN 253) @ ~10 Hz
 *   - From Autosteer 2 (PGN 250) @ ~10 Hz
 *
 * Receives & dispatches:
 *   - Hello From AgIO (PGN 200)
 *   - Scan Request (PGN 202)
 *   - Subnet Change (PGN 201)
 *   - Steer Data In (PGN 254)
 *   - Steer Settings In (PGN 252)
 *   - Steer Config In (PGN 251)
 *   - Hardware Message (PGN 221)
 *
 * All frames use the AOG Ethernet protocol format.
 * Uses the PGN library for encoding/decoding.
 *
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 */

#include "net.h"
#include "pgn_codec.h"
#include "pgn_registry.h"
#include "modules.h"
#include "control.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "steer_config_bits.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NET
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>

// ===================================================================
// Status byte bitfield (from PGN 254 steer data)
// ===================================================================
constexpr uint8_t STATUS_BIT_WORK_SWITCH   = 0x01;  // bit 0
constexpr uint8_t STATUS_BIT_STEER_SWITCH  = 0x02;  // bit 1
constexpr uint8_t STATUS_BIT_STEER_ON      = 0x04;  // bit 2
constexpr int16_t STEER_STATUS_HEADING_INVALID_X10 = 9999;
constexpr int16_t STEER_STATUS_ROLL_INVALID_X10 = 8888;

struct ConfigBitMapping {
    uint8_t mask;
    const char* name;
    const char* behavior;
};

static const ConfigBitMapping kSet0BitMap[] = {
    {steer_cfg_set0::INVERT_WAS,            "set0.bit0 InvertWAS",            "WAS signal polarity invert"},
    {steer_cfg_set0::RELAY_ACTIVE_HIGH,     "set0.bit1 RelayActiveHigh",      "DRV enable polarity active-high"},
    {steer_cfg_set0::MOTOR_DRIVE_DIRECTION, "set0.bit2 MotorDriveDirection",  "motor drive direction invert"},
    {steer_cfg_set0::SINGLE_INPUT_WAS,      "set0.bit3 SingleInputWAS",       "single-ended WAS mode"},
    {steer_cfg_set0::CYTRON_DRIVER,         "set0.bit4 CytronDriver",         "DRV type Cytron (else IBT2-like)"},
    {steer_cfg_set0::STEER_SWITCH,          "set0.bit5 SteerSwitch",          "external steer switch input enabled"},
    {steer_cfg_set0::STEER_BUTTON,          "set0.bit6 SteerButton",          "momentary steer button mode"},
    {steer_cfg_set0::SHAFT_ENCODER,         "set0.bit7 ShaftEncoder",         "shaft encoder pulse protection"},
};

static const ConfigBitMapping kSet1BitMap[] = {
    {steer_cfg_set1::DANFOSS_VALVE,   "set1.bit0 DanfossValve",   "DRV output for Danfoss valve"},
    {steer_cfg_set1::PRESSURE_SENSOR, "set1.bit1 PressureSensor", "pressure sensor channel enabled"},
    {steer_cfg_set1::CURRENT_SENSOR,  "set1.bit2 CurrentSensor",  "current sensor channel enabled"},
    {steer_cfg_set1::USE_Y_AXIS,      "set1.bit3 UseYAxis",       "IMU axis remap for steering"},
};

static void logSteerConfigMap(const char* set_name,
                              uint8_t raw,
                              const ConfigBitMapping* table,
                              size_t table_len) {
    for (size_t i = 0; i < table_len; ++i) {
        const bool enabled = (raw & table[i].mask) != 0u;
        LOGI("NET", "%s %s=%u -> %s",
             set_name, table[i].name, enabled ? 1u : 0u, table[i].behavior);
    }
}

// ===================================================================
// Send interval tracking
// ===================================================================
static uint32_t s_last_send_ms = 0;
static const uint32_t SEND_INTERVAL_MS = 100;  // 10 Hz

// Rate-limit log messages to avoid serial spam from broadcast echo
static uint32_t s_last_invalid_log_ms = 0;
static uint32_t s_last_unhandled_log_ms = 0;

static int16_t scaleToInt16(float value, float scale) {
    const float scaled = value * scale;
    if (scaled > 32767.0f) return 32767;
    if (scaled < -32768.0f) return -32768;
    return static_cast<int16_t>(scaled);
}

static uint8_t pidOutputToPwmDisplay(uint16_t pid_output, bool settings_received) {
    if (settings_received) {
        return pid_output > 255u ? 255u : static_cast<uint8_t>(pid_output);
    }
    return static_cast<uint8_t>((static_cast<uint32_t>(pid_output) * 255u) / 65535u);
}

// ===================================================================
// Network config instance (defined here, declared in pgn_types.h)
// ===================================================================
AogNetworkConfig g_net_cfg;

// ===================================================================
// Initialise network
// ===================================================================
void netInit(void) {
    hal_net_init();
    LOGI("NET", "initialised (W5500 Ethernet)");
    LOGI("NET", "dest IP = %u.%u.%u.%u",
            g_net_cfg.dest_ip[0], g_net_cfg.dest_ip[1],
            g_net_cfg.dest_ip[2], g_net_cfg.dest_ip[3]);
}

// ===================================================================
// Process a single validated frame
// ===================================================================
void netProcessFrame(uint8_t src, uint8_t pgn,
                     const uint8_t* payload, size_t payload_len) {
    // Only process frames from AgIO (0x7F).
    // Ignore our own frames (0x7E) echoed back via broadcast,
    // and any other module-to-module traffic.
    if (src != aog_src::AGIO) return;

    switch (pgn) {
        case aog_pgn::HELLO_FROM_AGIO: {
            AogHelloFromAgio msg;
            if (pgnDecodeHelloFromAgio(payload, payload_len, &msg)) {
                LOGI("NET", "Hello from AgIO (module=0x%02X, ver=%u) -> sending ALL module hellos",
                        (unsigned)msg.moduleId, (unsigned)msg.agioVersion);
                modulesSendStartupErrors();
                modulesSendHellos();
            }
            break;
        }

        case aog_pgn::SCAN_REQUEST: {
            if (pgnDecodeScanRequest(payload, payload_len)) {
                LOGI("NET", "Scan request -> sending ALL module subnet replies");
                modulesSendStartupErrors();
                modulesSendSubnetReplies();
            }
            break;
        }

        case aog_pgn::SUBNET_CHANGE: {
            AogSubnetChange msg;
            if (pgnDecodeSubnetChange(payload, payload_len, &msg)) {
                // Update destination IP: set first 3 octets from subnet change
                g_net_cfg.dest_ip[0] = msg.ip_one;
                g_net_cfg.dest_ip[1] = msg.ip_two;
                g_net_cfg.dest_ip[2] = msg.ip_three;
                g_net_cfg.dest_ip[3] = 255;  // broadcast

                // Also update the HAL's destination IP (used by hal_net_send)
                hal_net_set_dest_ip(msg.ip_one, msg.ip_two, msg.ip_three, 255);

                LOGI("NET", "subnet changed, dest=%u.%u.%u.%u",
                        g_net_cfg.dest_ip[0], g_net_cfg.dest_ip[1],
                        g_net_cfg.dest_ip[2], g_net_cfg.dest_ip[3]);
            }
            break;
        }

        case aog_pgn::STEER_DATA_IN: {
            AogSteerDataIn msg;
            if (pgnDecodeSteerDataIn(payload, payload_len, &msg)) {
                const float steer_setpoint_deg = msg.steerAngle / 100.0f;
                const float speed_kmh = msg.speed / 10.0f;
                const uint32_t now_ms = hal_millis();

                // Output write phase: commit all command inputs consistently.
                desiredSteerAngleDeg = steer_setpoint_deg;
                {
                    StateLock lock;
                    g_nav.work_switch      = (msg.status & STATUS_BIT_WORK_SWITCH) != 0;
                    g_nav.steer_switch     = (msg.status & STATUS_BIT_STEER_SWITCH) != 0;
                    g_nav.last_status_byte = msg.status;
                    g_nav.gps_speed_kmh     = speed_kmh;
                    g_nav.watchdog_timer_ms = now_ms;
                }
            }
            break;
        }

        case aog_pgn::HARDWARE_MESSAGE: {
            // Incoming hardware message from AgIO (display or command)
            uint8_t dur = 0, color = 0;
            char msg_text[128];
            if (pgnDecodeHardwareMessage(payload, payload_len,
                                         &dur, &color,
                                         msg_text, sizeof(msg_text))) {
                LOGI("NET", "HW message from AgIO: [%u] (col=%u) \"%s\"",
                        (unsigned)dur, (unsigned)color, msg_text);
                // TODO: display on connected LCD, or process commands
            }
            break;
        }

        case aog_pgn::STEER_SETTINGS_IN: {
            AogSteerSettingsIn settings;
            if (pgnDecodeSteerSettingsIn(payload, payload_len, &settings)) {
                // Apply settings to PID controller
                controlUpdateSettings(settings.kp, settings.highPWM, settings.lowPWM,
                                     settings.minPWM, settings.countsPerDegree,
                                     settings.wasOffset, settings.ackerman);
                LOGI("NET", "SteerSettings applied (Kp=%u hiPWM=%u loPWM=%u minPWM=%u cnt=%u off=%d ack=%u)",
                        (unsigned)settings.kp, (unsigned)settings.highPWM,
                        (unsigned)settings.lowPWM, (unsigned)settings.minPWM,
                        (unsigned)settings.countsPerDegree, (int)settings.wasOffset,
                        (unsigned)settings.ackerman);
            }
            break;
        }

        case aog_pgn::STEER_CONFIG_IN: {
            AogSteerConfigIn config;
            if (pgnDecodeSteerConfigIn(payload, payload_len, &config)) {
                const uint8_t set0 = config.set0;
                const uint8_t set1 = config.ackermanFix;  // byte 3 is setting1 in AOG reference FW.
                const uint8_t unknown_set0 = set0 & static_cast<uint8_t>(~steer_cfg_set0::KNOWN_MASK);
                const uint8_t unknown_set1 = set1 & static_cast<uint8_t>(~steer_cfg_set1::KNOWN_MASK);
                const uint8_t applied_set0 = set0 & steer_cfg_set0::KNOWN_MASK;
                const uint8_t applied_set1 = set1 & steer_cfg_set1::KNOWN_MASK;

                LOGI("NET", "SteerConfig received (set0=0x%02X pulse=%u speed=%u set1=0x%02X)",
                        (unsigned)config.set0, (unsigned)config.maxPulse,
                        (unsigned)config.minSpeed, (unsigned)set1);

                logSteerConfigMap("CFG", applied_set0, kSet0BitMap, sizeof(kSet0BitMap) / sizeof(kSet0BitMap[0]));
                logSteerConfigMap("CFG", applied_set1, kSet1BitMap, sizeof(kSet1BitMap) / sizeof(kSet1BitMap[0]));

                if (unknown_set0 != 0u) {
                    LOGW("NET", "SteerConfig set0 unknown bits 0x%02X ignored", (unsigned)unknown_set0);
                }
                if (unknown_set1 != 0u) {
                    LOGW("NET", "SteerConfig set1 unknown bits 0x%02X ignored", (unsigned)unknown_set1);
                }

                // Store config in global state for future use
                {
                    StateLock lock;
                    g_nav.config_set0      = applied_set0;
                    g_nav.config_set1      = applied_set1;
                    g_nav.config_max_pulse = config.maxPulse;
                    g_nav.config_min_speed = config.minSpeed;
                    g_nav.config_received  = true;
                }
            }
            break;
        }

        default: {
            // Rate-limit unhandled PGN logs (max once per 10s)
            uint32_t now = hal_millis();
            if (now - s_last_unhandled_log_ms >= 10000) {
                s_last_unhandled_log_ms = now;

                // Use PGN registry to show human-readable name
                const char* name = pgnGetName(pgn);
                LOGW("NET", "unhandled PGN 0x%02X (%s) from Src 0x%02X (len=%zu)",
                        pgn, name, src, payload_len);
            }
            break;
        }
    }
}

// ===================================================================
// Poll for received UDP frames
// ===================================================================
void netPollReceive(void) {
    uint8_t rx_buf[aog_frame::MAX_FRAME];

    while (true) {
        uint16_t src_port = 0;
        int rx_len = hal_net_receive(rx_buf, sizeof(rx_buf), &src_port);

        if (rx_len <= 0) break;

        // Quick filter: skip non-AOG frames early
        // AOG frames start with 0x80 0x81, NMEA with '$' (0x24)
        if (rx_buf[0] != AOG_PREAMBLE_1) continue;

        // Validate frame (checks preamble, bounds, CRC)
        uint8_t frame_src = 0;
        uint8_t frame_pgn = 0;
        const uint8_t* payload = nullptr;
        size_t payload_len = 0;

        if (pgnValidateFrame(rx_buf, static_cast<size_t>(rx_len),
                             &frame_src, &frame_pgn,
                             &payload, &payload_len)) {
            netProcessFrame(frame_src, frame_pgn, payload, payload_len);
        } else {
            // Rate-limit invalid frame logs (max once per 10s)
            uint32_t now = hal_millis();
            if (now - s_last_invalid_log_ms >= 10000) {
                s_last_invalid_log_ms = now;
                LOGW("NET", "invalid frame (%d bytes from port %u, first=0x%02X)",
                        rx_len, src_port, rx_buf[0]);
            }
        }
    }
}

// ===================================================================
// Send periodic AOG frames
// ===================================================================
void netSendAogFrames(void) {
    // Skip if network not available
    if (!hal_net_is_connected()) return;

    uint32_t now = hal_millis();
    if (now - s_last_send_ms < SEND_INTERVAL_MS) return;
    s_last_send_ms = now;

    struct NetTxSnapshot {
        float steer_angle_deg = 0.0f;
        float heading_deg = 0.0f;
        float roll_deg = 0.0f;
        bool safety_ok = false;
        bool work_switch = false;
        bool steer_switch = false;
        uint16_t pid_output = 0;
        bool settings_received = false;
        uint32_t imu_timestamp_ms = 0;
        bool imu_quality_ok = false;
        uint32_t heading_timestamp_ms = 0;
        bool heading_quality_ok = false;
    } snap;

    // Input phase: take one consistent state snapshot.
    {
        StateLock lock;
        snap.steer_angle_deg = g_nav.steer_angle_deg;
        snap.heading_deg = g_nav.heading_deg;
        snap.roll_deg = g_nav.roll_deg;
        snap.safety_ok = g_nav.safety_ok;
        snap.work_switch = g_nav.work_switch;
        snap.steer_switch = g_nav.steer_switch;
        snap.pid_output = g_nav.pid_output;
        snap.settings_received = g_nav.settings_received;
        snap.imu_timestamp_ms = g_nav.imu_timestamp_ms;
        snap.imu_quality_ok = g_nav.imu_quality_ok;
        snap.heading_timestamp_ms = g_nav.heading_timestamp_ms;
        snap.heading_quality_ok = g_nav.heading_quality_ok;
    }

    // Processing phase: encode payloads from snapshot.
    uint8_t tx_buf[aog_frame::MAX_FRAME];
    size_t tx_len = 0;
    const int16_t angle_x100 = scaleToInt16(snap.steer_angle_deg, 100.0f);
    const bool imu_valid =
        dep_policy::isImuInputValid(now, snap.imu_timestamp_ms, snap.imu_quality_ok);
    const bool heading_valid =
        dep_policy::isFresh(now, snap.heading_timestamp_ms, dep_policy::IMU_FRESHNESS_TIMEOUT_MS) &&
        snap.heading_quality_ok;
    const int16_t heading_x10 = heading_valid
        ? scaleToInt16(snap.heading_deg, 10.0f)
        : STEER_STATUS_HEADING_INVALID_X10;
    const int16_t roll_x10 = imu_valid
        ? scaleToInt16(snap.roll_deg, 10.0f)
        : STEER_STATUS_ROLL_INVALID_X10;

    uint8_t switch_st = 0;
    if (!snap.safety_ok)  switch_st |= 0x80;
    if (snap.work_switch) switch_st |= 0x01;
    if (snap.steer_switch) switch_st |= 0x02;

    const uint8_t pwm_disp = pidOutputToPwmDisplay(snap.pid_output, snap.settings_received);

    // Output phase: network sends happen outside of state lock.
    tx_len = pgnEncodeSteerStatusOut(tx_buf, sizeof(tx_buf),
                                     angle_x100, heading_x10, roll_x10,
                                     switch_st, pwm_disp);
    if (tx_len > 0) {
        hal_net_send(tx_buf, tx_len, aog_port::STEER);
    }

    const uint8_t sensor_val = hal_steer_angle_read_sensor_byte();
    tx_len = pgnEncodeFromAutosteer2(tx_buf, sizeof(tx_buf), sensor_val);
    if (tx_len > 0) {
        hal_net_send(tx_buf, tx_len, aog_port::STEER);
    }
}
