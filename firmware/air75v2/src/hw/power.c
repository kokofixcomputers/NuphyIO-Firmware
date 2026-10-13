/*
 * STOP-mode sleep for the level-2 sleep on battery. In STOP the core, PLL, HSI48 and all peripheral clocks are off (~tens of uA instead of mA);
 * RAM and the GPIO states are kept. Two things end it:
 *   - a key press: every matrix row is driven low and the column pins (pulled up) get falling-edge EXTI lines. EXTI has one line per pin number,
 *     and some boards put columns of the same number on different ports (the Air96: B10 / A10 / C10 ...), so only one column per number wakes
 *     us at once;
 *   - the RTC wake-up timer (LSI), every `period_ms`: we then read the columns once, which catches the keys that share an EXTI line (a tap
 *     shorter than the period on such a key can be missed).
 * On wake the clock tree is rebuilt and the time spent asleep is added to millis(). This is only used when no USB host is attached.
 */
#include "power.h"
#include "board.h"
#include "board_data.h"
#include "stm32f0xx.h"

static GPIO_TypeDef *const PORTS[4] = {GPIOA, GPIOB, GPIOC, GPIOD};
static volatile uint8_t woke;               /* bit 0: RTC timer, bit 1: key line */
static bool rtc_ready;

void RTC_IRQHandler(void) { RTC->ISR &= ~RTC_ISR_WUTF; EXTI->PR = EXTI_PR_PR20; woke |= 1; }
static void exti_irq(void) { EXTI->PR = 0xFFFFu; woke |= 2; }
void EXTI0_1_IRQHandler(void) { exti_irq(); }
void EXTI2_3_IRQHandler(void) { exti_irq(); }
void EXTI4_15_IRQHandler(void) { exti_irq(); }

void power_idle(void) { __WFI(); }

static uint32_t read_cols(void) {
    const uint32_t in[4] = {GPIOA->IDR, GPIOB->IDR, GPIOC->IDR, GPIOD->IDR};
    uint32_t m = 0;
    for (int i = 0; i < BD_COLS; i++) if (!((in[BD_COL_PIN[i][0]] >> BD_COL_PIN[i][1]) & 1u)) m |= 1u << i;
    return m;
}

static void rtc_arm(uint16_t period_ms) {
    if (!rtc_ready) {
        RCC->APB1ENR |= RCC_APB1ENR_PWREN;
        PWR->CR |= PWR_CR_DBP;                                            /* backup-domain access for the RTC registers */
        RCC->CSR |= RCC_CSR_LSION; while (!(RCC->CSR & RCC_CSR_LSIRDY)) {}
        RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_1;     /* RTC clock = LSI (~40 kHz) */
        RCC->BDCR |= RCC_BDCR_RTCEN;
        EXTI->IMR |= EXTI_IMR_MR20; EXTI->RTSR |= EXTI_RTSR_TR20;           /* wake-up timer = EXTI line 20 */
        NVIC_EnableIRQ(RTC_IRQn);
        rtc_ready = true;
    }
    RTC->WPR = 0xCA; RTC->WPR = 0x53;
    RTC->CR &= ~RTC_CR_WUTE;
    while (!(RTC->ISR & RTC_ISR_WUTWF)) {}
    uint32_t ticks = (uint32_t)period_ms * 25u / 10u;                      /* RTCCLK / 16 = ~2.5 kHz */
    if (ticks < 2) ticks = 2;
    RTC->WUTR = ticks - 1;
    RTC->CR = (RTC->CR & ~RTC_CR_WUCKSEL) | RTC_CR_WUTIE | RTC_CR_WUTE;      /* WUCKSEL = 0: RTCCLK / 16 */
    RTC->ISR &= ~RTC_ISR_WUTF;
    RTC->WPR = 0xFF;
}

bool power_stop(uint16_t period_ms) {
    for (int r = 0; r < BD_ROWS; r++) PORTS[BD_ROW_PIN[r][0]]->BSRR = 1u << (BD_ROW_PIN[r][1] + 16);   /* all rows low: any key pulls its column low */
    uint16_t lines = 0;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
    for (int c = 0; c < BD_COLS; c++) {
        const uint8_t port = BD_COL_PIN[c][0], pin = BD_COL_PIN[c][1];
        if (lines & (1u << pin)) continue;                                  /* one column per EXTI line */
        lines |= (uint16_t)(1u << pin);
        SYSCFG->EXTICR[pin >> 2] = (SYSCFG->EXTICR[pin >> 2] & ~(0xFu << ((pin & 3) * 4))) | ((uint32_t)port << ((pin & 3) * 4));
    }
    EXTI->PR = lines; EXTI->FTSR |= lines; EXTI->IMR |= lines;
    NVIC_EnableIRQ(EXTI0_1_IRQn); NVIC_EnableIRQ(EXTI2_3_IRQn); NVIC_EnableIRQ(EXTI4_15_IRQn);
    rtc_arm(period_ms);

    woke = 0;
    bool down = read_cols() != 0;                                           /* a key may have gone down while we were setting up */
    if (!down) {
        const uint32_t systick = SysTick->CTRL;
        SysTick->CTRL = systick & ~SysTick_CTRL_TICKINT_Msk;                 /* a pending tick would end the sleep at once */
        PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS | PWR_CR_CWUF;      /* STOP, regulator in low-power mode */
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        __DSB(); __WFI();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        SysTick->CTRL = systick;
        board_clocks();                                                     /* the wake-up leaves us on HSI: rebuild PLL / HSI48 */
        down = read_cols() != 0;
        if (woke & 1) board_add_ms(period_ms);                              /* SysTick did not run: credit the nominal time */
    }

    EXTI->IMR &= ~(uint32_t)lines; EXTI->FTSR &= ~(uint32_t)lines;
    for (int r = 0; r < BD_ROWS; r++) PORTS[BD_ROW_PIN[r][0]]->BSRR = 1u << BD_ROW_PIN[r][1];            /* rows back to idle (high) */
    return down;
}
