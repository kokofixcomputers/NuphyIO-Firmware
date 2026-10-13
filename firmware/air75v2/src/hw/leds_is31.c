/*
 * Halo75 V2 LEDs: two IS31FL3733 matrix drivers (12 x 16 dots each) on I2C1 (SCL = B8, SDA = B9), chip addresses 0x50 and 0x53, enabled by
 * SDB1 = C6 / SDB2 = C7. 83 key LEDs followed by the 45-LED halo ring share the two chips (the wiring is generated into board_data.h from
 * QMK's g_is31fl3733_leds table). Each chip's 192 PWM registers (page 1) are refreshed with one DMA-driven I2C write; the chip keeps
 * register page 1 selected, so a frame is a single 193-byte transfer per chip and never blocks the key scan.
 */
#include "leds.h"
#include "board.h"
#include "board_data.h"
#include "stm32f0xx.h"

#define CHIPS 2
#define PWM_BYTES 192
#define LED_COUNT (BD_KEY_LEDS + BD_SIDE_LEDS)
static const uint8_t ADDR[CHIPS] = {0x50, 0x53};          /* 7-bit: AD pins GND/GND and GND/VCC */

static uint8_t pwm[CHIPS][1 + PWM_BYTES];                 /* [0] = start register (0), then the 192 PWM registers */
static uint8_t dirty;                                     /* chips whose buffer changed */
static uint8_t flying = 0xFF;                             /* chip currently being sent by DMA */
static uint32_t flying_since;
static bool powered = true;

/* ---- I2C ---- */
static bool wait_flag(uint32_t flag) {
    for (uint32_t t = 0; t < 200000; t++) {
        const uint32_t isr = I2C1->ISR;
        if (isr & flag) return true;
        if (isr & I2C_ISR_NACKF) return false;
    }
    return false;
}
static void i2c_clear(void) { I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF; }

static bool i2c_write(uint8_t addr, const uint8_t *d, uint8_t n) {       /* blocking, n <= 255, ends with STOP */
    i2c_clear();
    I2C1->CR2 = ((uint32_t)addr << 1) | ((uint32_t)n << 16) | I2C_CR2_AUTOEND | I2C_CR2_START;
    bool ok = true;
    for (uint8_t i = 0; i < n && ok; i++) { ok = wait_flag(I2C_ISR_TXIS); if (ok) I2C1->TXDR = d[i]; }
    if (ok) ok = wait_flag(I2C_ISR_STOPF);
    i2c_clear();
    return ok;
}
static bool reg_write(uint8_t chip, uint8_t reg, uint8_t val) { const uint8_t b[2] = {reg, val}; return i2c_write(ADDR[chip], b, 2); }
static void page(uint8_t chip, uint8_t p) { reg_write(chip, 0xFE, 0xC5); reg_write(chip, 0xFD, p); }      /* unlock, then select */

static void chips_init(void) {
    for (uint8_t c = 0; c < CHIPS; c++) {
        page(c, 0);                                                  /* LED control: switch every dot on (brightness comes from PWM) */
        uint8_t on[25]; on[0] = 0; for (int i = 1; i < 25; i++) on[i] = 0xFF;
        i2c_write(ADDR[c], on, 25);
        page(c, 1);                                                  /* PWM: all dark */
        uint8_t z[1 + PWM_BYTES] = {0};
        i2c_write(ADDR[c], z, sizeof z);
        page(c, 3);                                                  /* function registers */
        reg_write(c, 0x0F, 0x01);                                    /* SW pull-up 0.5 kOhm  (de-ghost) */
        reg_write(c, 0x10, 0x01);                                    /* CS pull-down 0.5 kOhm */
        reg_write(c, 0x01, 0xFF);                                    /* global current: maximum */
        reg_write(c, 0x00, 0x01);                                    /* normal operation (software shutdown off), no sync, 8.4 kHz PWM */
    }
    for (int i = 0; i < 12; i++) delay_us(1000);                    /* the chips need ~10 ms to wake */
    for (uint8_t c = 0; c < CHIPS; c++) page(c, 1);                  /* park on the PWM page for streaming */
    for (uint8_t c = 0; c < CHIPS; c++) for (int i = 1; i <= PWM_BYTES; i++) pwm[c][i] = 0;
    dirty = 0;
}

void leds_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC->CFGR3 |= RCC_CFGR3_I2C1SW;                                  /* I2C1 kernel clock = SYSCLK (48 MHz) */
    /* B8 = SCL, B9 = SDA: alternate function 1, open drain, pull-up */
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 16) | (3u << 18))) | (2u << 16) | (2u << 18);
    GPIOB->OTYPER |= (1u << 8) | (1u << 9);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u << 16) | (3u << 18))) | (1u << 16) | (1u << 18);
    GPIOB->OSPEEDR |= (3u << 16) | (3u << 18);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(0xFFu)) | 0x11u;
    I2C1->CR1 = 0;
    I2C1->TIMINGR = 0x50330309u;                                     /* 400 kHz at 48 MHz (RM0091 table) */
    I2C1->CR1 = I2C_CR1_PE;
    chips_init();
}

/* ---- frame transfer (DMA1 channel 2 = I2C1_TX) ---- */
static void start_chip(uint8_t c) {
    flying = c; flying_since = millis();
    i2c_clear();
    DMA1_Channel2->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF2;
    DMA1_Channel2->CPAR = (uint32_t)&I2C1->TXDR;
    DMA1_Channel2->CMAR = (uint32_t)pwm[c];
    DMA1_Channel2->CNDTR = 1 + PWM_BYTES;
    DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_PL_1 | DMA_CCR_EN;
    I2C1->CR1 |= I2C_CR1_TXDMAEN;
    I2C1->CR2 = ((uint32_t)ADDR[c] << 1) | ((uint32_t)(1 + PWM_BYTES) << 16) | I2C_CR2_AUTOEND | I2C_CR2_START;
    dirty &= (uint8_t)~(1u << c);
}

static void advance(void) {
    if (flying != 0xFF) {
        const uint32_t isr = I2C1->ISR;
        const bool done = (isr & I2C_ISR_STOPF) != 0, fail = (isr & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)) != 0 || (millis() - flying_since) > 30;
        if (!done && !fail) return;
        DMA1_Channel2->CCR = 0;
        I2C1->CR1 &= ~I2C_CR1_TXDMAEN;
        if (fail && !done) { I2C1->CR1 &= ~I2C_CR1_PE; I2C1->CR1 |= I2C_CR1_PE; dirty |= (uint8_t)(1u << flying); }    /* reset the peripheral, resend */
        i2c_clear();
        flying = 0xFF;
    }
    if (powered) for (uint8_t c = 0; c < CHIPS; c++) if (dirty & (1u << c)) { start_chip(c); return; }
}

void leds_task(void) { advance(); }
bool leds_keys_busy(void) { advance(); return flying != 0xFF; }

static void put(int led, rgb_t c) {
    const uint8_t chip = BD_IS31_CHIP[led];
    uint8_t *p = pwm[chip];
    const uint8_t r = BD_IS31_R[led], g = BD_IS31_G[led], b = BD_IS31_B[led];
    if (p[1 + r] != c.r || p[1 + g] != c.g || p[1 + b] != c.b) { p[1 + r] = c.r; p[1 + g] = c.g; p[1 + b] = c.b; dirty |= (uint8_t)(1u << chip); }
}

void leds_show_keys(const rgb_t keys[BD_KEY_LEDS]) {
    if (leds_keys_busy()) return;
    for (int i = 0; i < BD_KEY_LEDS; i++) put(i, keys[i]);
    advance();
}

void leds_show_side(const rgb_t side[BD_SIDE_LEDS]) {                 /* ring position -> halo LED; goes out with the next key frame */
    for (int p = 0; p < BD_SIDE_LEDS; p++) put(BD_KEY_LEDS + BD_RING[p], side[p]);
}

void leds_power(bool on) {
    if (on == powered) return;
    while (flying != 0xFF) advance();
    powered = on;
    if (on) {
        GPIOC->BSRR = 1u << 2;                                       /* boost */
        GPIOC->MODER = (GPIOC->MODER & ~((3u << 12) | (3u << 14))) | (1u << 12) | (1u << 14);
        GPIOC->BSRR = (1u << 6) | (1u << 7);                         /* SDB1 / SDB2 high */
        for (int i = 0; i < 5; i++) delay_us(1000);
        chips_init();
        for (int i = 0; i < PWM_BYTES + 1; i++) { pwm[0][i] = 0; pwm[1][i] = 0; }
        dirty = 0x03;
    } else {
        GPIOC->BSRR = 1u << (2 + 16);                                /* boost off, chips into shutdown (pins released like the stock firmware) */
        GPIOC->MODER &= ~((3u << 12) | (3u << 14));
    }
}
