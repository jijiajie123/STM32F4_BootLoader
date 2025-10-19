#include "main.h"
#include "stm32f4xx.h"
#include "uart.h"

typedef struct uart_dev
{
    uint8_t NVIC_IRQChannel;
    uint32_t gpio_rx_pin;
    uint32_t gpio_tx_pin;
    uint16_t GPIO_PinSource_rx;
    uint16_t GPIO_PinSource_tx;
    uint8_t  gpio_af_dev;
    GPIO_TypeDef *gpio_port;
    USART_TypeDef *dev_handle;
} uart_dev_t;

static uart_dev_t uart_devs[] =
{
    {USART1_IRQn, GPIO_Pin_10, GPIO_Pin_9, GPIO_PinSource10, GPIO_PinSource9, GPIO_AF_USART1, GPIOA, USART1},
    {USART2_IRQn, GPIO_Pin_3,  GPIO_Pin_2, GPIO_PinSource3,  GPIO_PinSource2, GPIO_AF_USART2, GPIOA, USART2},
};

void uart_gpio_config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    for (int i = 0; i < ARRAY_SIZE(uart_devs); i++)
    {
        GPIO_PinAFConfig(uart_devs[i].gpio_port, uart_devs[i].GPIO_PinSource_rx, uart_devs[i].gpio_af_dev);
        GPIO_PinAFConfig(uart_devs[i].gpio_port, uart_devs[i].GPIO_PinSource_tx, uart_devs[i].gpio_af_dev);

        GPIO_StructInit(&GPIO_InitStructure);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
        GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
        GPIO_InitStructure.GPIO_Pin = uart_devs[i].gpio_rx_pin | uart_devs[i].gpio_tx_pin;
        GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

        GPIO_Init(uart_devs[i].gpio_port, &GPIO_InitStructure);
    }
}

void uart_lowlevel_init(void)
{
    USART_InitTypeDef USART_InitStructure;

    for (int i = 0; i < ARRAY_SIZE(uart_devs); i++)
    {
        USART_StructInit(&USART_InitStructure);
        USART_InitStructure.USART_BaudRate = 115200;
        USART_InitStructure.USART_WordLength = USART_WordLength_8b;
        USART_InitStructure.USART_StopBits = USART_StopBits_1;
        USART_InitStructure.USART_Parity = USART_Parity_No;
        USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
        USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

        USART_Init(uart_devs[i].dev_handle, &USART_InitStructure);
        USART_Cmd(uart_devs[i].dev_handle, ENABLE);
    }
}

void uart_it_config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    for (int i = 0; i < ARRAY_SIZE(uart_devs); i++)
    {
        NVIC_InitStructure.NVIC_IRQChannel = uart_devs[i].NVIC_IRQChannel;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        USART_ITConfig(uart_devs[i].dev_handle, USART_IT_RXNE, ENABLE);
    }
}


void uart_deinit(void)
{
    for (int i = 0; i < ARRAY_SIZE(uart_devs); i++)
    {
        USART_Cmd(uart_devs[i].dev_handle, DISABLE);
        USART_ITConfig(uart_devs[i].dev_handle, USART_IT_RXNE, DISABLE);
        USART_DeInit(uart_devs[i].dev_handle);
        NVIC_DisableIRQ(uart_devs[i].NVIC_IRQChannel);
        NVIC_ClearPendingIRQ(uart_devs[i].NVIC_IRQChannel);
    }
}
