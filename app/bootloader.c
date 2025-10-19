#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bootloader.h"
#include "ringbuffer.h"
#include "bl_uart.h"
#include "crc16.h"
#include "crc32.h"
#include "flash_ops.h"
#include "cpu_tick.h"
#include "stm32f4xx.h"

/*
自定义传输协议：
    | header | opcode | length | payload | crc16
    | 0xAA   | 1 byte | 2 byte | n byte  | 2 byte
    header: 固定为0xAA
    opcode: 操作码，表示具体的操作类型
    length: payload的长度，2字节表示，最大支持65535字节
    payload: 具体的数据内容，长度为length字段指定的值
    crc: 校验码，对opcode、length和payload进行简单的异或校验

    opcode定义：
        0x10: 查询BOOT参数
        0x11: 进入主程序
        0x1F: 重启芯片
        0x20: 擦除指定区域内容
        0x21: 回读指定区域内容
        0x22: 将data写入addr地址
        0x23: 校验Flash内容

    响应：
    | header | opcode | length | errcode | crc16
    | 0xAA   | 1 byte | 2 byte | 1 byte  | 2 byte


    errcode: 错误码，表示操作的结果
        0x00: 成功
        0x01: 未知操作码
        0x02: 数据长度超出限制
        0x03: 数据格式错误
        0x04: 校验失败
        0x05: 参数错误
        0xFF: 未知错误

*/

/* Flash 512k  bootloader: 48k  arginfo: 16k  app: */
#define ARGINFO_ADDRESS             0x0800C000
#define APP_ADDRESS                 0x08010000

#define ARGINFO_HEADER              0x1A2B3C4D

#define BOOTLOADER_VERSION_MAJOR    1
#define BOOTLOADER_VERSION_MINOR    0

/* rsp */
#define RSP_ACK_LEN         7
#define RSP_CRC_DATA_LEN    4
#define RSP_CRC_START_POS   1

/* ringbuffer */
#define RINGBUFFER_LENGTH           1024
#define PACKET_PAYLOAD_MAX_LENGTH   4096
#define PACKET_MAX_LENGTH           (1 + 1 + 2 + PACKET_PAYLOAD_MAX_LENGTH + 2)   // header + opcode + length + payload + crc

#define PACKET_RECV_BYTE_TIMEOUT     2000

typedef enum
{
    BL_OPCODE_NONE      = 0x00,     // 未知类型, 异常处理
    BL_OPCODE_INQUERY   = 0x10,     // 查询BOOT参数
    BL_OPCODE_BOOT      = 0x11,     // 进入主程序
    BL_OPCODE_RESET     = 0x1F,     // 重启芯片
    BL_OPCODE_ERASE     = 0x20,     // 擦除指定区域内容
    BL_OPCODE_READ      = 0x21,     // 回读指定区域内容
    BL_OPCODE_WRITE     = 0x22,     // 将data写入addr地址
    BL_OPCODE_VERIFY    = 0x23      // 校验Flash内容
} bl_opcode_t;

typedef enum
{
    BL_ERR_OK,
    BL_ERR_OPCODE,
    BL_ERR_OVERFLOW,
    BL_ERR_TIMEOUT,
    BL_ERR_FORMAT,
    BL_ERR_VERIFY,
    BL_ERR_PARAM,
    BL_ERR_UNKNOWN = 0xFF
} bl_err_t;

typedef enum
{
    BL_STATUS_HEADER,
    BL_STATUS_OPCODE,
    BL_STATUS_LENGTH,
    BL_STATUS_PAYLOAD,
    BL_STATUS_CRC
} bl_status_t;

typedef enum
{
    BL_INQUERY_PARAM_VERSION,
    BL_INQUERY_PARAM_MIU
} bl_inquery_param_t;

typedef struct
{
    uint32_t magic_head;
    uint32_t address;
    uint32_t length;
    uint32_t crc32;
} bl_arginfo_t;

static rb_t rx_rb;
static uint8_t rx_rb_buf[RINGBUFFER_LENGTH];
static uint8_t packet_buf[PACKET_MAX_LENGTH];
static uint16_t packet_index = 0;
static bl_status_t bl_status = BL_STATUS_HEADER;

static inline uint32_t get_u32_le_inc(uint8_t **p)
{
    uint8_t *ptr = *p;
    uint32_t val = ((uint32_t)ptr[0])       |
                   ((uint32_t)ptr[1] << 8)  |
                   ((uint32_t)ptr[2] << 16) |
                   ((uint32_t)ptr[3] << 24);
    *p += 4;
    return val;
}

static inline uint16_t get_u16_le_inc(uint8_t **p)
{
    uint8_t *ptr = *p;
    uint16_t val = ((uint16_t)ptr[0]) |
                   ((uint16_t)ptr[1] << 8);
    *p += 2;
    return val;
}

static inline uint8_t get_u8_le_inc(uint8_t **p)
{
    uint8_t val = **p;
    *p += 1;
    return val;
}

static inline uint8_t get_u8(const uint8_t *p, int i)
{
    return p[i];
}

static void bl_uart_recv_cb(uint8_t data)
{
    rb_write(rx_rb, data);
}

static void bl_packet_reset(void)
{
    packet_index = 0;
    bl_status = BL_STATUS_HEADER;
}

static void bl_response(uint8_t opcode, uint16_t length, uint8_t* data)
{
    uint8_t index = 0;
    uint8_t rsp_buf[RSP_ACK_LEN];

    rsp_buf[index++] = 0xAA;              // 0: Header (AA)
    rsp_buf[index++] = opcode;            // 1: Opcode
    rsp_buf[index++] = (length & 0xFF);   // 2: Length Low
    rsp_buf[index++] = (length >> 8);     // 3: Length High

    memcpy(&rsp_buf[index], data, length);
    index += length;

    uint16_t crc = crc16(&rsp_buf[RSP_CRC_START_POS], length + 3);
    rsp_buf[index++] = (crc & 0xFF);      // 5: CRC Low
    rsp_buf[index++] = (crc >> 8);        // 6: CRC High

    bl_uart_send(rsp_buf, index);
}

static void bl_response_ack(uint8_t opcode, uint16_t length, uint8_t errcode)
{

    bl_response(opcode, 1, &errcode);
    // uint8_t index = 0;
    // uint8_t rsp_buf[RSP_ACK_LEN];

    // rsp_buf[index++] = 0xAA;              // 0: Header (AA)
    // rsp_buf[index++] = opcode;            // 1: Opcode
    // rsp_buf[index++] = (length & 0xFF);   // 2: Length Low
    // rsp_buf[index++] = (length >> 8);     // 3: Length High
    // rsp_buf[index++] = errcode;           // 4: Errcode

    // // CRC 计算：明确从 rsp_buf[1] 开始，长度为 4 字节
    // uint16_t crc = crc16(&rsp_buf[RSP_CRC_START_POS], RSP_CRC_DATA_LEN);

    // rsp_buf[index++] = (crc & 0xFF);      // 5: CRC Low
    // rsp_buf[index++] = (crc >> 8);        // 6: CRC High

    // bl_uart_send(rsp_buf, index);
}

static bool bl_recv_handle(uint8_t byte)
{
    static uint16_t length = 0;
    static uint16_t recv_len = 0;
    bool pkt_full = false;
    packet_buf[packet_index++] = byte;

    switch (bl_status)
    {
        case BL_STATUS_HEADER:
        {
            recv_len = 0;

            printf("header\r\n");
            if (byte == 0xAA)   bl_status = BL_STATUS_OPCODE;
            else                bl_packet_reset();
            break;
        }
        case BL_STATUS_OPCODE:
        {
            printf("opcode\r\n");

            if (byte == BL_OPCODE_BOOT || byte == BL_OPCODE_RESET ||
                byte == BL_OPCODE_ERASE || byte == BL_OPCODE_READ ||
                byte == BL_OPCODE_WRITE || byte == BL_OPCODE_VERIFY ||
                byte == BL_OPCODE_INQUERY)
            {
                bl_status = BL_STATUS_LENGTH;
            }
            else
            {
                printf("unknown opcode: 0x%02X\r\n", byte);
                bl_packet_reset();
            }
            break;
        }
        case BL_STATUS_LENGTH:
        {
            printf("length\r\n");
            recv_len++;
            if (recv_len == 2)
            {
                length = (packet_buf[3] << 8) | packet_buf[2];  //big endian
                printf("length: %d\r\n", length);

                if (length > PACKET_PAYLOAD_MAX_LENGTH)
                {
                    printf("length overflow\r\n");
                    length = 0;
                    bl_packet_reset();
                }
                else if (length == 0)
                {
                    bl_status = BL_STATUS_CRC;
                }
                else
                {
                    bl_status = BL_STATUS_PAYLOAD;
                }
                recv_len = 0;
            }
            break;
        }
        case BL_STATUS_PAYLOAD:
        {
            printf("payload\r\n");

            recv_len++;
            if (recv_len == length)
            {
                bl_status = BL_STATUS_CRC;
                recv_len = 0;
            }
            break;
        }
        case BL_STATUS_CRC:
        {
            printf("crc\r\n");

            recv_len++;

            if (recv_len == 2)
            {
                uint16_t crc  = packet_buf[packet_index - 1] << 8 | packet_buf[packet_index - 2];
                uint16_t ccrc = crc16(&packet_buf[1], length + 3);
                if (crc == ccrc)
                {
                    printf("crc ok\r\n");
                    pkt_full = true;
                }
                else
                {
                    printf("crc err, recv: 0x%04X, calc: 0x%04X\r\n", crc, ccrc);
                }

                recv_len = 0;
                length = 0;
            }
            break;
        }
        default:
        {
            printf("unknown status\r\n");
            recv_len = 0;
            length = 0;
            break;
        }
    }
    return pkt_full;
}

static void bl_op_inquery_handle(void)
{
    printf("bl_op_inquery_handle\r\n");

    uint8_t opcode = packet_buf[1];
    uint8_t param = packet_buf[4];

    if (opcode != BL_OPCODE_INQUERY)
        return ;

    switch (param)
    {
        case BL_INQUERY_PARAM_VERSION:
        {
            uint8_t version[2] = {BOOTLOADER_VERSION_MAJOR, BOOTLOADER_VERSION_MINOR};
            bl_response(opcode, sizeof(version), version);
            break;
        }
        case BL_INQUERY_PARAM_MIU:
        {
            uint16_t mtu = PACKET_PAYLOAD_MAX_LENGTH;
            bl_response(opcode, sizeof(mtu), (uint8_t *)&mtu);
            break;
        }
    }
}

static void bl_op_boot_handle(void)
{
    extern void jump_to_app(uint32_t app_add);

    /* deinit usart, systick */
    extern void uart_deinit(void);
    extern void board_deinit(void);
    extern void cpu_tick_deinit(void);

    bl_response_ack(BL_OPCODE_BOOT, 1, BL_ERR_OK);

    cpu_tick_deinit();
    uart_deinit();
    board_deinit();

    jump_to_app(APP_ADDRESS);
}

static void bl_op_reset_handle(void)
{
    bl_response_ack(BL_OPCODE_RESET, 1, BL_ERR_OK);
    __disable_irq();
    NVIC_SystemReset();
}

static void bl_op_erase_handle(void)
{
    /* param: addr size -- 8 bytes */
    uint8_t *pbuf = &packet_buf[2];
    uint16_t length = get_u16_le_inc(&pbuf);
    if (length != 8)
    {
        bl_response_ack(BL_OPCODE_ERASE, 1, BL_ERR_FORMAT);
        return ;
    }

    uint8_t *pbuf = &packet_buf[4];
    uint32_t addr = get_u32_le_inc(&pbuf);
    uint32_t size = get_u32_le_inc(&pbuf);

    if (addr < APP_ADDRESS || size == 0 ||
        (addr + size) > (APP_ADDRESS + (512 - 64) * 1024))
    {
        bl_response_ack(BL_OPCODE_ERASE, 1, BL_ERR_PARAM);
        return ;
    }

    if (flash_erase(addr, size))
        bl_response_ack(BL_OPCODE_ERASE, 1, BL_ERR_OK);
    else
        bl_response_ack(BL_OPCODE_ERASE, 1, BL_ERR_UNKNOWN);
}

static void bl_op_write_handle(void)
{
    uint8_t *pbuf = &packet_buf[2];
    uint16_t length = get_u16_le_inc(&pbuf);

    if (length <= 8)
    {
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_FORMAT);
        return ;
    }

    uint32_t addr = get_u32_le_inc(&pbuf);
    uint32_t size = get_u32_le_inc(&pbuf);

    if (addr < APP_ADDRESS || size == 0 ||
        (addr + size) > (APP_ADDRESS + (512 - 64) * 1024))
    {
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_PARAM);
        return ;
    }

    if (flash_write(addr, pbuf, size))
        bl_response_ack(BL_OPCODE_WRITE, 1, BL_ERR_OK);
    else
        bl_response_ack(BL_OPCODE_WRITE, 1, BL_ERR_UNKNOWN);
}

static void bl_op_verify_handle(void)
{
    /* param: add size crc --- 12 bytes */
    uint8_t *pbuf = &packet_buf[2];
    uint16_t length = get_u16_le_inc(&pbuf);

    if (length != 12)
    {
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_FORMAT);
        return ;
    }

    uint32_t vaddr  = get_u32_le_inc(&pbuf);
    uint32_t vsize  = get_u32_le_inc(&pbuf);
    uint32_t vcrc32 = get_u32_le_inc(&pbuf);

    if (vaddr < APP_ADDRESS || vsize == 0 ||
        (vaddr + vsize) > (APP_ADDRESS + (512 - 64) * 1024))
    {
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_PARAM);
        return ;
    }

    uint32_t ccrc = crc32((const unsigned char *)vaddr, vsize);

    if (ccrc == vcrc32)
    {
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_OK);

        // 校验通过，写入arginfo
        bl_arginfo_t arginfo;

        arginfo.magic_head = ARGINFO_HEADER;
        arginfo.address    = vaddr;
        arginfo.length     = vsize;
        arginfo.crc32      = vcrc32;

        flash_erase(ARGINFO_ADDRESS, sizeof(arginfo));
        flash_write(ARGINFO_ADDRESS, (const uint8_t *)&arginfo, sizeof(arginfo));
    }
    else
        bl_response_ack(BL_OPCODE_VERIFY, 1, BL_ERR_VERIFY);
}

static void bl_packet_handle(void)
{
    bl_opcode_t opcode = packet_buf[1];
    switch (opcode)
    {
        case BL_OPCODE_NONE:
        {
            printf("none opcode\r\n");
            break;
        }
        case BL_OPCODE_INQUERY:
        {
            bl_op_inquery_handle();
            break;
        }
        case BL_OPCODE_BOOT:
        {
            bl_op_boot_handle();
            break;
        }
        case BL_OPCODE_RESET:
        {
            bl_op_reset_handle();
            break;
        }
        case BL_OPCODE_ERASE:
        {
            bl_op_erase_handle();
            break;
        }
        case BL_OPCODE_WRITE:
        {
            bl_op_write_handle();
            break;
        }
        case BL_OPCODE_VERIFY:
        {
            bl_op_verify_handle();
            break;
        }
        default:
        {
            bl_response_ack(opcode,1, BL_ERR_OPCODE);
            break;
        }
    }
}

void bootloader_main(void)
{
    printf("start bootloader\r\n");

    bl_uart_recv_callback_register(bl_uart_recv_cb);

    rx_rb = rb_init(rx_rb_buf, sizeof(rx_rb_buf));
    if (!rx_rb)
        return ;

    bool boot_trap = false;
    uint64_t last_byte_ticks = 0;
    uint64_t now_ticks = 0;

    while (1)
    {
        now_ticks = cpu_get_ticks();  // 循环开头统一更新时间

        if (boot_trap && (now_ticks - last_byte_ticks > TICKS_PER_MS * PACKET_RECV_BYTE_TIMEOUT))
        {
            boot_trap = false;
            bl_packet_reset();
            printf("recv timeout\r\n");
            continue;
        }

        if (rb_is_empty(rx_rb))
        {
            if (!boot_trap)
                last_byte_ticks = now_ticks; // 等待首字节，更新基准
            continue;
        }

        uint8_t byte;
        if (rb_read(rx_rb, &byte))
        {
            boot_trap = true;
            printf("recv byte: 0x%02X\r\n", byte);

            if (bl_recv_handle(byte))
            {
                printf("recv full packet\r\n");

                bl_packet_handle();
                bl_packet_reset();
            }

            last_byte_ticks = now_ticks; // 每次收到字节刷新超时基准
        }
    }
}
