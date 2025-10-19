#ifndef __CPU_TICK_H__
#define __CPU_TICK_H__

#include <stdint.h>

#define TICKS_PER_MS    (SystemCoreClock / 1000)
#define TICKS_PER_US    (TICKS_PER_MS / 1000)

void cpu_tick_init(void);
void cpu_tick_deinit(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);
uint64_t cpu_get_ticks(void);

#endif /* __CPU_TICK_H__ */
