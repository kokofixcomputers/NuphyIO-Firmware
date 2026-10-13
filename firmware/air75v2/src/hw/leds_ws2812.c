/*
 * Key LEDs (84 x WS2812-class, data on A7): SPI1 MOSI at 3 MHz with DMA1 channel 3 (the SPI1_TX request).
 * Each WS2812 bit becomes three SPI bits: 0 -> 100 (0.33 us high), 1 -> 110 (0.67 us high), 1.0 us per bit.
 * Side LEDs (12, data on C8): bit-banged against the 48 MHz TIM14 timebase.
 */
#include "leds.h"
#include "stm32f0xx.h"

#define KEY_LEDS BD_KEY_LEDS
#define SIDE_LEDS BD_SIDE_LEDS
#define RESET_BYTES 64                         /* ~170 us of low at 3 MHz */
#define KEY_BUF (KEY_LEDS * 9 + RESET_BYTES)

static uint8_t spibuf[KEY_BUF];

static void encode_byte(uint8_t *o, uint8_t v) {
    uint32_t w = 0;
    for (int k = 7; k >= 0; k--) w = (w << 3) | (((v >> k) & 1) ? 6u : 4u);   /* 110 / 100 */
    o[0] = (uint8_t)(w >> 16); o[1] = (uint8_t)(w >> 8); o[2] = (uint8_t)w;
}

void leds_task(void) {}

void leds_power(bool on) {
    if (on) { GPIOC->BSRR = (1u << 2); GPIOC->BSRR = (1u << (6 + 16)) | (1u << (9 + 16)); }
    else    { GPIOC->BSRR = (1u << (2 + 16)); GPIOC->BSRR = (1u << 6) | (1u << 9); }
}

void leds_init(void) {
    /* A7 = SPI1_MOSI (AF0), fast */
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 14)) | (2u << 14);
    GPIOA->OSPEEDR |= (3u << 14);
    GPIOA->AFR[0] &= ~(0xFu << 28);
    /* C8 = side data, plain output, idle low */
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 16)) | (1u << 16);
    GPIOC->BSRR = 1u << (8 + 16);

    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1 | SPI_CR1_BR_0 | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;   /* /16 = 3 MHz, TX only */
    SPI1->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2;                                                                      /* 8-bit frames */
    SPI1->CR1 |= SPI_CR1_SPE;
}

bool leds_keys_busy(void) {
    if (DMA1_Channel3->CCR & DMA_CCR_EN) {
        if (DMA1_Channel3->CNDTR != 0) return true;
        DMA1_Channel3->CCR &= ~DMA_CCR_EN;
        SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
    }
    return (SPI1->SR & SPI_SR_BSY) != 0;
}

void leds_show_keys(const rgb_t keys[KEY_LEDS]) {
    if (leds_keys_busy()) return;
    for (int i = 0; i < KEY_LEDS; i++) {                            /* WS2812 expects G, R, B */
        encode_byte(&spibuf[i * 9 + 0], keys[i].g);
        encode_byte(&spibuf[i * 9 + 3], keys[i].r);
        encode_byte(&spibuf[i * 9 + 6], keys[i].b);
    }
    for (int i = KEY_LEDS * 9; i < KEY_BUF; i++) spibuf[i] = 0;
    DMA1->IFCR = DMA_IFCR_CGIF3;
    DMA1_Channel3->CPAR = (uint32_t)&SPI1->DR;
    DMA1_Channel3->CMAR = (uint32_t)spibuf;
    DMA1_Channel3->CNDTR = KEY_BUF;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_PL_1;      /* mem -> periph, 8-bit */
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    DMA1_Channel3->CCR |= DMA_CCR_EN;
}

/* ---- side LEDs ---- */
static inline void wait_until(uint16_t target) { while ((int16_t)((uint16_t)TIM14->CNT - target) < 0) {} }

static void side_byte(uint8_t v) {
    for (int k = 7; k >= 0; k--) {
        const uint16_t t0 = (uint16_t)TIM14->CNT;
        GPIOC->BSRR = 1u << 8;
        if ((v >> k) & 1) { wait_until(t0 + 33); GPIOC->BSRR = 1u << (8 + 16); wait_until(t0 + 53); }   /* 1: ~0.7 us high, ~1.1 us bit */
        else              { __NOP(); __NOP(); __NOP(); GPIOC->BSRR = 1u << (8 + 16); wait_until(t0 + 53); }   /* 0: ~0.15 us high */
    }
}

void leds_show_side(const rgb_t side[SIDE_LEDS]) {
    __disable_irq();
    for (int i = 0; i < SIDE_LEDS; i++) { side_byte(side[i].g); side_byte(side[i].r); side_byte(side[i].b); }
    __enable_irq();
}
