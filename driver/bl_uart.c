#include <stddef.h>
#include "stm32f4xx.h"
#include "bl_uart.h"

bl_uart_recv_callback_t bl_uart_recv_callback = NULL;

void bl_uart_init(void)
{
    extern void uart_gpio_config(void);
    extern void uart_lowlevel_init(void);
    extern void uart_it_config(void);

    uart_gpio_config();
    uart_it_config();
    uart_lowlevel_init();
}

void bl_uart_recv_callback_register(bl_uart_recv_callback_t cb)
{
    bl_uart_recv_callback = cb;
}

void bl_uart_send(uint8_t *data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, data[i]);
    }
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = USART_ReceiveData(USART1);
        if (bl_uart_recv_callback)
            bl_uart_recv_callback(data);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
