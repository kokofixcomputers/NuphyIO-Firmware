#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef void (*matrix_cb_t)(uint8_t slot, bool pressed);
void matrix_init(void);
void matrix_set_debounce(uint8_t press_ms, uint8_t release_ms);   /* nuphy.io's stabilization level */
void matrix_task(matrix_cb_t cb);      /* call every ~1 ms */
bool matrix_key_down(uint8_t row, uint8_t col);
