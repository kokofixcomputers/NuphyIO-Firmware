#pragma once
#include <stdbool.h>
#include <stdint.h>
void rfio_init(void);                              /* USART1 (B6 TX / B7 RX, 460800 8E1), wake pin B8, releases the nRF52 from reset */
void rfio_tx(void *ctx, const uint8_t *b, uint8_t n);   /* wake pin low -> send -> wake pin high */
void rfio_reset(void *ctx);                        /* pulse the nRF reset line (B4) */
int  rfio_getc(void);                              /* -1 when the receive ring is empty */
