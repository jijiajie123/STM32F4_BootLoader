#ifndef __FLASH_OPS_H__
#define __FLASH_OPS_H__

#include <stdint.h>
#include <stdbool.h>

void flash_lock(void);
void flash_unlock(void);
bool flash_erase(uint32_t addr, uint32_t length);
bool flash_write(uint32_t addr, const uint8_t *buf, uint32_t length);

#endif /* __FLASH_OPS_H__ */
