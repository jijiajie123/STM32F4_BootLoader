#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

struct ringbuffer;

typedef struct ringbuffer* rb_t;

rb_t rb_init(uint8_t *buffer, uint32_t size);
bool rb_is_empty(rb_t rb);
bool rb_is_full(rb_t rb);
bool rb_write(rb_t rb, uint8_t data);
bool rb_read(rb_t rb, uint8_t *data);

#endif /* __RING_BUFFER_H__ */
