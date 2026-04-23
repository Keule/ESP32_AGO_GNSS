/**
 * @file um980_uart_setup.cpp
 * @brief Runtime setup helper for dual UM980 UART mapping.
 */

#include "um980_uart_setup.h"

#include "fw_config.h"
#include "hal/hal.h"

static Um980UartSetup s_um980_setup;

void um980SetupLoadDefaults(uint32_t baud_default) {
    s_um980_setup.baud = (baud_default == 0) ? 460800u : baud_default;
    s_um980_setup.swap_a = false;
    s_um980_setup.swap_b = false;
}

Um980UartSetup um980SetupGet(void) {
    return s_um980_setup;
}

void um980SetupSetBaud(uint32_t baud) {
    if (baud == 0) return;
    s_um980_setup.baud = baud;
}

void um980SetupSetSwap(uint8_t port_idx, bool enabled) {
    if (port_idx == 0) {
        s_um980_setup.swap_a = enabled;
    } else if (port_idx == 1) {
        s_um980_setup.swap_b = enabled;
    }
}

bool um980SetupApply(void) {
    const int8_t a_rx = s_um980_setup.swap_a ? GNSS_UART1_TX : GNSS_UART1_RX;
    const int8_t a_tx = s_um980_setup.swap_a ? GNSS_UART1_RX : GNSS_UART1_TX;
    const int8_t b_rx = s_um980_setup.swap_b ? GNSS_UART2_TX : GNSS_UART2_RX;
    const int8_t b_tx = s_um980_setup.swap_b ? GNSS_UART2_RX : GNSS_UART2_TX;

    const bool um980_a_ok = hal_gnss_uart_begin(0, s_um980_setup.baud, a_rx, a_tx);
    const bool um980_b_ok = hal_gnss_uart_begin(1, s_um980_setup.baud, b_rx, b_tx);

    hal_log("Main: UM980 UART setup baud=%lu -> A(rx=%d tx=%d swap=%s)=%s B(rx=%d tx=%d swap=%s)=%s",
            static_cast<unsigned long>(s_um980_setup.baud),
            static_cast<int>(a_rx),
            static_cast<int>(a_tx),
            s_um980_setup.swap_a ? "ON" : "OFF",
            um980_a_ok ? "OK" : "FAIL",
            static_cast<int>(b_rx),
            static_cast<int>(b_tx),
            s_um980_setup.swap_b ? "ON" : "OFF",
            um980_b_ok ? "OK" : "FAIL");

    return um980_a_ok && um980_b_ok;
}

