#include "board.h"
#include "board_cfg.h"
#include "stm32f0xx.h"

extern volatile uint32_t dfu_magic;
static volatile uint32_t ticks;

void SysTick_Handler(void) { ticks++; }
uint32_t millis(void) { return ticks; }

void delay_us(uint32_t us) {
    const uint16_t start = (uint16_t)TIM14->CNT;
    const uint32_t want = us * 48u;
    while ((uint16_t)((uint16_t)TIM14->CNT - start) < want) {}
}

void board_enter_dfu(void) {
    __disable_irq();
    dfu_magic = 0xB007DF00u;
    NVIC_SystemReset();
}

static GPIO_TypeDef *const PORTS[4] = {GPIOA, GPIOB, GPIOC, GPIOD};
static inline void gpio_out(int port, int pin, int level) {
    GPIO_TypeDef *g = PORTS[port];
    g->MODER = (g->MODER & ~(3u << (pin * 2))) | (1u << (pin * 2));
    g->BSRR = level ? (1u << pin) : (1u << (pin + 16));
}
static inline void gpio_in_pullup(int port, int pin) {
    GPIO_TypeDef *g = PORTS[port];
    g->MODER &= ~(3u << (pin * 2));
    g->PUPDR = (g->PUPDR & ~(3u << (pin * 2))) | (1u << (pin * 2));
}

bool board_os_switch_is_mac(void) { const bool high = (GPIOC->IDR >> 1) & 1; return BD_OS_HIGH_IS_WIN ? !high : high; }   /* C1: high = Mac on the Air75 V2, high = Windows on the Halo75 V2 */
bool board_device_switch_is_usb(void) { return (GPIOC->IDR >> 0) & 1; }      /* C0 high = USB */

void board_uid(uint8_t out[12]) { for (int i = 0; i < 12; i++) out[i] = *(volatile uint8_t *)(0x1FFFF7ACu + i); }

void board_add_ms(uint32_t ms) { ticks += ms; }

/* The crystal-less USB clock: HSI48 trimmed against the host's SOF by the CRS. Off while the keyboard is on a wireless link (it saves the oscillator's current). */
static bool usb_clk_on = true;
void board_usb_clocks(bool on) {
    usb_clk_on = on;
    if (on) {
        RCC->CR2 |= RCC_CR2_HSI48ON;
        while (!(RCC->CR2 & RCC_CR2_HSI48RDY)) {}
        RCC->CFGR3 &= ~RCC_CFGR3_USBSW;
        RCC->APB1ENR |= RCC_APB1ENR_CRSEN | RCC_APB1ENR_USBEN;
        CRS->CFGR = (CRS->CFGR & ~CRS_CFGR_SYNCSRC) | CRS_CFGR_SYNCSRC_1;   /* sync source = USB SOF */
        CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
    } else {
        CRS->CR &= ~(CRS_CR_AUTOTRIMEN | CRS_CR_CEN);
        RCC->APB1ENR &= ~(RCC_APB1ENR_CRSEN | RCC_APB1ENR_USBEN);
        RCC->CR2 &= ~RCC_CR2_HSI48ON;
    }
}

/* Clock tree: 48 MHz from the PLL (HSI/2 x 12) and the crystal-less USB clock. Stop mode drops the PLL and HSI48, so power.c calls this again on wake. */
void board_clocks(void) {
    /* 48 MHz from the PLL (HSI/2 x 12), one flash wait state with prefetch */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL)) | RCC_CFGR_PLLSRC_HSI_DIV2 | RCC_CFGR_PLLMUL12;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
    if (usb_clk_on) board_usb_clocks(true);
}

void board_init(void) {
    board_clocks();

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN | RCC_AHBENR_DMAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* free-running 48 MHz timebase for microsecond delays and LED bit timing */
    TIM14->PSC = 0; TIM14->ARR = 0xFFFF; TIM14->CR1 = TIM_CR1_CEN;

    SysTick_Config(48000);                                                  /* 1 kHz */

    /* switches */
    gpio_in_pullup(PORT_C, 0); gpio_in_pullup(PORT_C, 1);
#ifdef BD_LED_DRIVER_IS31FL3733
    /* LED power: boost on, both IS31FL3733 chips out of shutdown (SDB1 = C6, SDB2 = C7, active high) */
    gpio_out(PORT_C, 2, 1); gpio_out(PORT_C, 6, 1); gpio_out(PORT_C, 7, 1);
#else
    /* LED power: boost on, both LED drivers enabled (enable pins low) */
    gpio_out(PORT_C, 2, 1); gpio_out(PORT_C, 6, 0); gpio_out(PORT_C, 9, 0);
#endif
    /* hold the nRF52 radio in reset: this firmware is wired-only for now and must not fight its UART */
    gpio_out(PORT_B, 4, 0);
}
