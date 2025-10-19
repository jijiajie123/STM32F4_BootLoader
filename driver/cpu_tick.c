#include "cpu_tick.h"
#include "stm32f4xx.h"


static volatile uint64_t g_ticks = 0;

void cpu_tick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000); // 1ms
}

void cpu_tick_deinit(void)
{
    // 1. 禁用 SysTick 计数器和中断 (最关键的一步)
    SysTick->CTRL = 0;

    // 2. 清除 SysTick 计数器和重载值 (可选，但让状态更明确)
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // 3. 清除 NVIC 中挂起的中断状态 (可选，但更彻底)
    NVIC_ClearPendingIRQ(SysTick_IRQn);
}

void delay_ms(uint32_t ms)
{
    uint64_t start = get_cur_tick();

    while ((get_cur_tick() - start) < ms * TICKS_PER_MS);
}

void delay_us(uint32_t us)
{
    uint64_t start = get_cur_tick();

    while ((get_cur_tick() - start) < us * TICKS_PER_US);
}

uint64_t cpu_get_ticks(void)
{
    uint64_t now, ret;
    do
    {
        now = g_ticks;
        ret = now + SysTick->LOAD - SysTick->VAL;
    } while (now != g_ticks);

    return ret;
}

void Systick_Handler(void)
{
    g_ticks += TICKS_PER_MS;
}
