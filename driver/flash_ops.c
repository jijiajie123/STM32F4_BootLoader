#include <stdio.h>
#include <stdbool.h>
#include "main.h"
#include "flash_ops.h"
#include "stm32f4xx.h"

typedef struct sector
{
    uint32_t sector_number;
    uint32_t start_address;
    uint32_t size;
} sector_t;

static const sector_t sectors[] =
{
    {FLASH_Sector_0,  0x08000000, 16 * 1024},
    {FLASH_Sector_1,  0x08004000, 16 * 1024},
    {FLASH_Sector_2,  0x08008000, 16 * 1024},
    {FLASH_Sector_3,  0x0800C000, 16 * 1024},
    {FLASH_Sector_4,  0x08010000, 64 * 1024},
    {FLASH_Sector_5,  0x08020000, 128 * 1024},
    {FLASH_Sector_6,  0x08040000, 128 * 1024},
    {FLASH_Sector_7,  0x08060000, 128 * 1024},
    {FLASH_Sector_8,  0x08080000, 128 * 1024},
    {FLASH_Sector_9,  0x080A0000, 128 * 1024},
    {FLASH_Sector_10, 0x080C0000, 128 * 1024},
    {FLASH_Sector_11, 0x080E0000, 128 * 1024}
};

void flash_lock(void)
{
    FLASH_Lock();
}

void flash_unlock(void)
{
    FLASH_Unlock();
}

bool flash_erase(uint32_t addr, uint32_t length)
{
    flash_unlock();

    uint32_t sector_start_addr = 0, sector_end_addr = 0;
    for (uint16_t i = 0; i < ARRAY_SIZE(sectors); i++)
    {
        /* 检测扇区，如果和有需要擦除的地方重叠，就擦除 */
        sector_start_addr = sectors[i].start_address;
        sector_end_addr   = sector_start_addr + sectors[i].size - 1;

        if (!(sector_end_addr < addr || sector_start_addr >= addr + length))
        {
            if (FLASH_COMPLETE != FLASH_EraseSector(sectors[i].sector_number, VoltageRange_3))
            {
                printf("erase sector %lu failed\r\n", sectors[i].sector_number);
                flash_lock();
                return false;
            }
        }
    }
    flash_lock();
    return true;
}

bool flash_write(uint32_t addr, const uint8_t *buf, uint32_t length)
{
    //进行4字节对齐
    if (length % 4 !=0)
    {
        length = (length + 3) / 4 * 4;
    }


    flash_unlock();

    uint32_t cur_prgram_addr = addr;
    for (uint16_t i = 0; i < length / 4; i++)
    {
        if (FLASH_COMPLETE != FLASH_ProgramWord(cur_prgram_addr, *(uint32_t *)(buf + i * 4)))
        {
            printf("program word failed at 0X%04X\r\n", cur_prgram_addr);
            flash_lock();
            return false;
        }
    }
    flash_lock();

    return true;
}

