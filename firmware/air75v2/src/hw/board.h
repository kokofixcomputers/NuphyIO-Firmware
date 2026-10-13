#pragma once
#include <stdbool.h>
#include <stdint.h>

/* ---- pins (from keyboards/nuphy/air75v2/ansi/config.h in the QMK tree) ---- */
#define PORT_A 0
#define PORT_B 1
#define PORT_C 2
/* DEVICE_MODE C0 (high = USB), OS_MODE C1 (high = Mac), DC_BOOST C2, LED enables C6 (keys) / C9 (side),
 * side data C8, key data A7 (SPI1 MOSI), nRF52 reset B4 (active low), nRF wake B8, nRF UART B6/B7. */

void     board_init(void);
uint32_t millis(void);
void     delay_us(uint32_t us);        /* <= 1000 */
void     board_clocks(void);           /* (re)start the PLL / HSI48 clock tree - after stop mode */
void     board_usb_clocks(bool on);    /* HSI48 + CRS + USB peripheral clock */
void     board_add_ms(uint32_t ms);    /* credit time that passed while SysTick was stopped */
void     board_enter_dfu(void);        /* software reset into the ST ROM bootloader */
bool     board_os_switch_is_mac(void);
bool     board_device_switch_is_usb(void);
void     board_uid(uint8_t out[12]);
