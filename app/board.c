#include <stdio.h>
#include "stm32f4xx.h"
#include "board.h"

static void board_lowlevel_init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,  ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,  ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,  ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,  ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,  ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

}

static void board_lowlevel_deinit(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,  DISABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,  DISABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,  DISABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,  DISABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,  DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);

}

void board_init(void)
{
    board_lowlevel_init();
}

void board_deinit(void)
{
    board_lowlevel_deinit();
}


int fputc(int ch, FILE *f)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, (uint8_t) ch);
    return ch;
}
