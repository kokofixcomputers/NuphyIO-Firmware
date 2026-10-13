/* Vector table, reset handler and the software jump into the ST ROM (DFU) bootloader. */
#include <stdint.h>
#include "stm32f0xx.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

#define DFU_MAGIC 0xB007DF00u
volatile uint32_t dfu_magic __attribute__((section(".noinit")));

static void jump_to_rom_bootloader(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
    SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~SYSCFG_CFGR1_MEM_MODE) | SYSCFG_CFGR1_MEM_MODE_0;   /* map system memory at 0x0 */
    const uint32_t sp = *(volatile uint32_t *)0x1FFFC800u, pc = *(volatile uint32_t *)0x1FFFC804u;
    __set_MSP(sp);
    ((void (*)(void))pc)();
    for (;;) {}
}

void Reset_Handler(void) {
    if (dfu_magic == DFU_MAGIC) { dfu_magic = 0; jump_to_rom_bootloader(); }
    for (uint32_t *s = &_sidata, *d = &_sdata; d < &_edata;) *d++ = *s++;
    for (uint32_t *b = &_sbss; b < &_ebss;) *b++ = 0;
    main();
    for (;;) {}
}

void Default_Handler(void) { for (;;) {} }
#define WEAK_ISR __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK_ISR; void HardFault_Handler(void) WEAK_ISR; void SVC_Handler(void) WEAK_ISR;
void PendSV_Handler(void) WEAK_ISR; void SysTick_Handler(void) WEAK_ISR; void USB_IRQHandler(void) WEAK_ISR; void USART1_IRQHandler(void) WEAK_ISR;

__attribute__((section(".isr_vector"), used)) void (*const vectors[48])(void) = {
    (void (*)(void))&_estack, Reset_Handler, NMI_Handler, HardFault_Handler, 0, 0, 0, 0, 0, 0, 0, SVC_Handler, 0, 0, PendSV_Handler, SysTick_Handler,
    /* IRQ 0..26 unused */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* IRQ 27 */ USART1_IRQHandler, /* IRQ 28..30 unused */ 0,0,0,
    /* IRQ 31 */ USB_IRQHandler,
};
