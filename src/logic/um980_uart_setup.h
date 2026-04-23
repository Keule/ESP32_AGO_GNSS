/**
 * @file um980_uart_setup.h
 * @brief Runtime setup helper for dual UM980 UART mapping.
 */

#pragma once

#include <cstdint>

struct Um980UartSetup {
    uint32_t baud = 460800;
    bool swap_a = false;
    bool swap_b = false;
};

/// Initialize setup state from defaults/runtime config.
void um980SetupLoadDefaults(uint32_t baud_default);

/// Get current setup state snapshot.
Um980UartSetup um980SetupGet(void);

/// Set baud rate for both UM980 ports.
void um980SetupSetBaud(uint32_t baud);

/// Enable/disable RX/TX swap for UM980-A (port_idx=0) or UM980-B (port_idx=1).
void um980SetupSetSwap(uint8_t port_idx, bool enabled);

/// Apply current setup to both UARTs. Returns true if both begin calls succeeded.
bool um980SetupApply(void);

