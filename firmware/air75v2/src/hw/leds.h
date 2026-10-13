#pragma once
#include <stdbool.h>
#include "board_cfg.h"
#include "light.h"
void leds_init(void);
void leds_task(void);                              /* call every loop: advances background transfers (I2C boards); no-op on WS2812 boards */
bool leds_keys_busy(void);
void leds_show_keys(const rgb_t keys[BD_KEY_LEDS]);   /* encodes + starts the transfer; returns immediately */
void leds_show_side(const rgb_t side[BD_SIDE_LEDS]);  /* Air75 V2: bit-banged, blocks ~0.4 ms with interrupts off; Halo75 V2: queued with the next key frame */
void leds_power(bool on);
