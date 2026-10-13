/* 6x17 matrix: rows are driven low one at a time, columns are read with pull-ups (pressed = low). */
#include "matrix.h"
#include "board.h"
#include "board_data.h"
#include "stm32f0xx.h"

static uint8_t deb_press = 3, deb_release = 5;
void matrix_set_debounce(uint8_t press_ms, uint8_t release_ms) { deb_press = press_ms ? press_ms : 1; deb_release = release_ms ? release_ms : 1; }

static GPIO_TypeDef *const PORTS[3] = {GPIOA, GPIOB, GPIOC};
static uint32_t raw[BD_ROWS], state[BD_ROWS];
static uint8_t  cnt[BD_ROWS][BD_COLS];

static uint32_t read_cols(void) {
    const uint32_t a = GPIOA->IDR, b = GPIOB->IDR, c = GPIOC->IDR;
    const uint32_t in[3] = {a, b, c};
    uint32_t m = 0;
    for (int i = 0; i < BD_COLS; i++)
        if (!((in[BD_COL_PIN[i][0]] >> BD_COL_PIN[i][1]) & 1u)) m |= 1u << i;      /* low = pressed */
    return m;
}

static uint32_t all_cols_high(void) {
    const uint32_t m = read_cols();
    return m == 0;
}

void matrix_init(void) {
    for (int r = 0; r < BD_ROWS; r++) {
        GPIO_TypeDef *g = PORTS[BD_ROW_PIN[r][0]]; const int p = BD_ROW_PIN[r][1];
        g->BSRR = 1u << p;                                                          /* idle high */
        g->MODER = (g->MODER & ~(3u << (p * 2))) | (1u << (p * 2));                 /* push-pull output */
    }
    for (int c = 0; c < BD_COLS; c++) {
        GPIO_TypeDef *g = PORTS[BD_COL_PIN[c][0]]; const int p = BD_COL_PIN[c][1];
        g->MODER &= ~(3u << (p * 2));
        g->PUPDR = (g->PUPDR & ~(3u << (p * 2))) | (1u << (p * 2));                 /* input, pull-up */
    }
}

static void scan_raw(void) {
    for (int r = 0; r < BD_ROWS; r++) {
        GPIO_TypeDef *g = PORTS[BD_ROW_PIN[r][0]]; const int p = BD_ROW_PIN[r][1];
        for (int w = 0; w < 50 && !all_cols_high(); w++) { __NOP(); }              /* let the previous row's columns recover */
        g->BSRR = 1u << (p + 16);                                                   /* select row */
        for (volatile int d = 0; d < 12; d++) {}                                    /* settle ~2 us */
        raw[r] = read_cols();
        g->BSRR = 1u << p;                                                          /* deselect */
    }
}

void matrix_task(matrix_cb_t cb) {
    scan_raw();
    for (int r = 0; r < BD_ROWS; r++) {
        for (int c = 0; c < BD_COLS; c++) {
            if (BD_SLOT[r][c] == 0xFF) continue;
            const bool now = (raw[r] >> c) & 1u, was = (state[r] >> c) & 1u;
            if (now == was) { cnt[r][c] = 0; continue; }
            const uint8_t need = now ? deb_press : deb_release;
            if (++cnt[r][c] >= need) {
                cnt[r][c] = 0;
                state[r] ^= 1u << c;
                if (cb) cb(BD_SLOT[r][c], now);
            }
        }
    }
}

bool matrix_key_down(uint8_t row, uint8_t col) { return (state[row] >> col) & 1u; }
