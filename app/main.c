#include <stdio.h>
#include <stddef.h>
#include "main.h"
#include "board.h"
#include "bl_uart.h"
#include "bootloader.h"
int main()
{

    board_init();

    bl_uart_init();

    printf("hellow world\r\n");
    bootloader_main();

    while (1)
    {

    }
}

