/**
 * @file ads1118_compat.h
 * @brief ADS1118 adapter – switches between local and denkitronik library.
 *
 * Compile-time switch via platformio.ini build_flags:
 *
 *   -DUSE_DENKITRONIK_ADS1118
 *     → Uses denkitronik/ADS1118 from PlatformIO registry.
 *       4-byte SPI protocol, hardcoded SPI_MODE1.
 *       NO bit-inversion support (may fail with cheap modules).
 *
 *   (default, no define)
 *     → Uses local lib/ads1118/ with all extras:
 *       2-byte SPI protocol, auto SPI-mode detection,
 *       auto bit-inversion, DOUT test, shared-bus deselect,
 *       non-blocking readLoop().
 *
 * The adapter exposes a single interface (ADS1118Dev) so that
 * hal_impl.cpp does not need to know which backend is active.
 *
 * API (both backends):
 *   ADS1118Dev::begin(SPIClass& spi, int cs, deselect_fn)
 *   ADS1118Dev::detect()           → bool
 *   ADS1118Dev::isDetected()       → bool
 *   ADS1118Dev::readLoop(ch)       → int16_t
 *   ADS1118Dev::getFSR()           → float
 *   ADS1118Dev::isDoutInverted()   → bool
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>

// ===================================================================
#ifdef USE_DENKITRONIK_ADS1118
// ===================================================================
//
// Backend: denkitronik/ADS1118
//   - 4-byte protocol (2 reads per CS cycle)
//   - Hardcoded SPI_MODE1
//   - No bit-inversion support
//   - Blocking reads (we approximate non-blocking with timer)
//
// ===================================================================

#include "ADS1118.h"   // denkitronik library header

class ADS1118Dev {
public:
    ADS1118Dev() : _dk(nullptr), _detected(false), _last_raw(0), _last_read_ms(0) {}

    void begin(SPIClass& spi, int cs_pin, void (*deselect_fn)(void) = nullptr) {
        _deselect_fn = deselect_fn;
        _dk = new ::ADS1118(static_cast<uint8_t>(cs_pin), &spi);
        _dk->begin();
        _dk->setFullScaleRange(_dk->FSR_4096);
        _dk->setSamplingRate(_dk->RATE_128SPS);
        _dk->setSingleShotMode();
        _dk->setInputSelected(_dk->AIN_0);
    }

    bool detect() {
        if (!_dk) return false;
        if (_deselect_fn) _deselect_fn();

        uint16_t value = _dk->getADCValue(_dk->AIN_0);
        if (value == 0xFFFF) {
            Serial.println("ADS1118 [denkitronik] DETECT FAILED (0xFFFF)");
            _detected = false;
            return false;
        }
        float mV = _dk->getMilliVolts(_dk->AIN_0);
        Serial.printf("ADS1118 [denkitronik] DETECTED (raw=0x%04X, %.2fmV)\n", value, mV);
        _last_raw = static_cast<int16_t>(value);
        _detected = true;
        return true;
    }

    bool isDetected() const { return _detected; }

    int16_t readLoop(uint8_t channel) {
        if (!_detected || !_dk) return _last_raw;
        uint32_t now = millis();
        if (now - _last_read_ms >= 9) {
            if (_deselect_fn) _deselect_fn();
            uint8_t input;
            switch (channel & 3) {
                case 0: input = _dk->AIN_0; break;
                case 1: input = _dk->AIN_1; break;
                case 2: input = _dk->AIN_2; break;
                default: input = _dk->AIN_3; break;
            }
            uint16_t value = _dk->getADCValue(input);
            if (value != 0xFFFF) _last_raw = static_cast<int16_t>(value);
            _last_read_ms = now;
        }
        return _last_raw;
    }

    float getFSR() const { return 4.096f; }
    bool isDoutInverted() const { return false; }

private:
    ::ADS1118* _dk = nullptr;
    void (*_deselect_fn)(void) = nullptr;
    bool _detected;
    int16_t _last_raw;
    uint32_t _last_read_ms;
};

// ===================================================================
#else  // !USE_DENKITRONIK_ADS1118
// ===================================================================
//
// Backend: local lib/ads1118/
//   - 2-byte protocol (correct per datasheet)
//   - Auto SPI-mode detection (Mode0 / Mode1)
//   - Auto bit-inversion detection (cheap level-shifters)
//   - DOUT connectivity test (crosstalk / floating)
//   - Shared-bus deselect callback
//   - Non-blocking readLoop()
//
// ===================================================================

#include "ads1118.h"

class ADS1118Dev {
public:
    ADS1118Dev() : _ads(nullptr) {}

    void begin(SPIClass& spi, int cs_pin, void (*deselect_fn)(void) = nullptr) {
        _ads = new ::ADS1118(spi);
        if (deselect_fn) {
            _ads->begin(cs_pin, nullptr, 0, deselect_fn);
        } else {
            _ads->begin(cs_pin);
        }
    }

    bool detect() {
        return _ads ? _ads->detect() : false;
    }

    bool isDetected() const {
        return _ads ? _ads->isDetected() : false;
    }

    int16_t readLoop(uint8_t channel) {
        return _ads ? _ads->readLoop(channel) : 0;
    }

    float getFSR() const {
        return _ads ? _ads->getFSR() : 4.096f;
    }

    bool isDoutInverted() const {
        return _ads ? _ads->isDoutInverted() : false;
    }

private:
    ::ADS1118* _ads = nullptr;
};

#endif // USE_DENKITRONIK_ADS1118
