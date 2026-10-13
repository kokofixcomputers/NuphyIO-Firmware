#include "rfio.h"
#include "board.h"
#include "board_cfg.h"
#include "stm32f0xx.h"

/* Reception runs on circular DMA (USART1_RX remapped to DMA1 channel 5) so the radio's bytes survive the side-LED bit-banging,
 * which keeps interrupts off for ~0.4 ms. */
#define RXQ 128
static uint8_t q[RXQ];
static uint8_t q_tail;

int rfio_getc(void) {
    const uint8_t head = (uint8_t)(RXQ - DMA1_Channel5->CNDTR) % RXQ;
    if (q_tail == head) return -1;
    const uint8_t b = q[q_tail];
    q_tail = (uint8_t)((q_tail + 1) % RXQ);
    return b;
}

void rfio_reset(void *ctx) {
    (void)ctx;
    GPIOB->BSRR = 1u << (4 + 16); delay_us(1000); delay_us(1000); delay_us(1000); delay_us(1000); delay_us(1000);
    GPIOB->BSRR = 1u << 4;
}

static GPIO_TypeDef *wake_port(void) { static GPIO_TypeDef *const P[3] = {GPIOA, GPIOB, GPIOC}; return P[BD_NRF_WAKE_PORT]; }

void rfio_tx(void *ctx, const uint8_t *b, uint8_t n) {
    (void)ctx;
    GPIO_TypeDef *const wk = wake_port();
    wk->BSRR = 1u << (BD_NRF_WAKE_PIN + 16);        /* wake the radio */
    delay_us(50);
    for (uint8_t i = 0; i < n; i++) {
        while (!(USART1->ISR & USART_ISR_TXE)) {}
        USART1->TDR = b[i];
    }
    while (!(USART1->ISR & USART_ISR_TC)) {}
    delay_us(50);
    wk->BSRR = 1u << BD_NRF_WAKE_PIN;
    delay_us(200);
}

void rfio_init(void) {
    /* wake pin (B8 on the Air75 V2, C4 on the Halo75 V2): output, high. B5 test (input pull-up), B6/B7 = USART1 AF0 with pull-ups */
    { GPIO_TypeDef *const wk = wake_port(); wk->MODER = (wk->MODER & ~(3u << (BD_NRF_WAKE_PIN * 2))) | (1u << (BD_NRF_WAKE_PIN * 2)); wk->BSRR = 1u << BD_NRF_WAKE_PIN; }
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 10) | (3u << 12) | (3u << 14))) | (2u << 12) | (2u << 14);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u << 10) | (3u << 12) | (3u << 14))) | (1u << 10) | (1u << 12) | (1u << 14);
    GPIOB->AFR[0] &= ~((0xFu << 24) | (0xFu << 28));
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    USART1->BRR = 48000000u / 460800u;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->CFGR1 |= SYSCFG_CFGR1_USART1RX_DMA_RMP;
    DMA1_Channel5->CCR = 0;
    DMA1_Channel5->CPAR = (uint32_t)&USART1->RDR;
    DMA1_Channel5->CMAR = (uint32_t)q;
    DMA1_Channel5->CNDTR = RXQ;
    DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_EN;
    USART1->CR3 = USART_CR3_DMAR | USART_CR3_OVRDIS;
    USART1->CR1 = USART_CR1_M0 | USART_CR1_PCE | USART_CR1_TE | USART_CR1_RE;   /* 8 data bits + even parity */
    USART1->CR1 |= USART_CR1_UE;
    GPIOB->BSRR = 1u << 4;                           /* release the nRF52 (board_init holds it in reset) */
}
