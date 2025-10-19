#ifndef __BL_UART_H__
#define __BL_UART_H__

#include <stdint.h>

typedef void (*bl_uart_recv_callback_t)(uint8_t data);

void bl_uart_init(void);
void bl_uart_recv_callback_register(bl_uart_recv_callback_t cb);
void bl_uart_send(uint8_t *data, uint16_t length);

#endif /* __BL_UART_H__*/
