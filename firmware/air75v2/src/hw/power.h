/* Low-power waiting: sleep mode between SysTick ticks while awake, and periodic STOP mode (level-2 sleep on battery). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void power_idle(void);                       /* wait for the next interrupt (1 kHz SysTick, USB, ...) */
/* Enter STOP mode until a key line falls (EXTI) or `period_ms` has passed (RTC wake-up timer), then restore the clocks.
 * Returns true when a key is down on wake. SysTick is stopped meanwhile; the elapsed time is credited to millis(). */
bool power_stop(uint16_t period_ms);
