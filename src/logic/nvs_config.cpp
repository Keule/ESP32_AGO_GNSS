/**
 * @file nvs_config.cpp
 * @brief NVS persistence helpers for RuntimeConfig — Phase 0 (S0-05).
 */

#include "nvs_config.h"

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

namespace {

static uint32_t floatToU32(float value) {
    uint32_t out = 0;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

static float u32ToFloat(uint32_t value) {
    float out = 0.0f;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

#if defined(ARDUINO_ARCH_ESP32)
static void loadString(nvs_handle_t handle, const char* key, char* dst, size_t dst_size) {
    size_t len = dst_size;
    if (nvs_get_str(handle, key, dst, &len) == ESP_OK) {
        dst[dst_size - 1] = '\0';
    }
}
#endif

}  // namespace

void nvsConfigLoad(RuntimeConfig& cfg) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    loadString(handle, nvs_keys::NTRIP_HOST, cfg.ntrip_host, sizeof(cfg.ntrip_host));
    loadString(handle, nvs_keys::NTRIP_MOUNT, cfg.ntrip_mountpoint, sizeof(cfg.ntrip_mountpoint));
    loadString(handle, nvs_keys::NTRIP_USER, cfg.ntrip_user, sizeof(cfg.ntrip_user));
    loadString(handle, nvs_keys::NTRIP_PASS, cfg.ntrip_password, sizeof(cfg.ntrip_password));

    uint16_t ntrip_port = 0;
    if (nvs_get_u16(handle, nvs_keys::NTRIP_PORT, &ntrip_port) == ESP_OK) {
        cfg.ntrip_port = ntrip_port;
    }

    uint32_t u32 = 0;
    if (nvs_get_u32(handle, nvs_keys::PID_KP, &u32) == ESP_OK) cfg.pid_kp = u32ToFloat(u32);
    if (nvs_get_u32(handle, nvs_keys::PID_KI, &u32) == ESP_OK) cfg.pid_ki = u32ToFloat(u32);
    if (nvs_get_u32(handle, nvs_keys::PID_KD, &u32) == ESP_OK) cfg.pid_kd = u32ToFloat(u32);

    uint8_t u8 = 0;
    if (nvs_get_u8(handle, nvs_keys::NET_MODE, &u8) == ESP_OK) cfg.net_mode = u8;
    if (nvs_get_u32(handle, nvs_keys::NET_IP, &u32) == ESP_OK) cfg.net_ip = u32;
    if (nvs_get_u32(handle, nvs_keys::NET_GW, &u32) == ESP_OK) cfg.net_gateway = u32;
    if (nvs_get_u32(handle, nvs_keys::NET_SUBNET, &u32) == ESP_OK) cfg.net_subnet = u32;
    if (nvs_get_u8(handle, nvs_keys::ACT_TYPE, &u8) == ESP_OK) cfg.actuator_type = u8;

    nvs_close(handle);
#else
    (void)cfg;
#endif
}

bool nvsConfigSave(const RuntimeConfig& cfg) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    bool ok = true;
    ok &= (nvs_set_str(handle, nvs_keys::NTRIP_HOST, cfg.ntrip_host) == ESP_OK);
    ok &= (nvs_set_u16(handle, nvs_keys::NTRIP_PORT, cfg.ntrip_port) == ESP_OK);
    ok &= (nvs_set_str(handle, nvs_keys::NTRIP_MOUNT, cfg.ntrip_mountpoint) == ESP_OK);
    ok &= (nvs_set_str(handle, nvs_keys::NTRIP_USER, cfg.ntrip_user) == ESP_OK);
    ok &= (nvs_set_str(handle, nvs_keys::NTRIP_PASS, cfg.ntrip_password) == ESP_OK);

    ok &= (nvs_set_u32(handle, nvs_keys::PID_KP, floatToU32(cfg.pid_kp)) == ESP_OK);
    ok &= (nvs_set_u32(handle, nvs_keys::PID_KI, floatToU32(cfg.pid_ki)) == ESP_OK);
    ok &= (nvs_set_u32(handle, nvs_keys::PID_KD, floatToU32(cfg.pid_kd)) == ESP_OK);

    ok &= (nvs_set_u8(handle, nvs_keys::NET_MODE, cfg.net_mode) == ESP_OK);
    ok &= (nvs_set_u32(handle, nvs_keys::NET_IP, cfg.net_ip) == ESP_OK);
    ok &= (nvs_set_u32(handle, nvs_keys::NET_GW, cfg.net_gateway) == ESP_OK);
    ok &= (nvs_set_u32(handle, nvs_keys::NET_SUBNET, cfg.net_subnet) == ESP_OK);
    ok &= (nvs_set_u8(handle, nvs_keys::ACT_TYPE, cfg.actuator_type) == ESP_OK);

    const esp_err_t commit_err = nvs_commit(handle);
    nvs_close(handle);
    return ok && (commit_err == ESP_OK);
#else
    (void)cfg;
    return false;
#endif
}

void nvsConfigFactoryReset(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    (void)nvs_erase_all(handle);
    (void)nvs_commit(handle);
    nvs_close(handle);
#endif
}

bool nvsConfigHasData(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t len = 0;
    const bool has_data = (nvs_get_str(handle, nvs_keys::NTRIP_HOST, nullptr, &len) == ESP_OK);
    nvs_close(handle);
    return has_data;
#else
    return false;
#endif
}
