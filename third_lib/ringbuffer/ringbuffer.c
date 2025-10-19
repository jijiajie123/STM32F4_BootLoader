#include "ringbuffer.h"
#include <stddef.h>

//使用柔性数组来实现ringbuffer
struct ringbuffer
{
    uint32_t size;         //buffer的大小
    uint32_t read_index;   //读索引
    uint32_t write_index;  //写索引
    uint8_t buffer[];    //柔性数组成员，实际大小为size
};

rb_t rb_init(uint8_t *buffer, uint32_t size)
{
    if (buffer == NULL || size <= sizeof(struct ringbuffer))
        return NULL; // 总大小必须大于 ringbuffer 结构体以至少保留 1 字节数据
    rb_t rb = (rb_t)buffer;
    rb->size = size - sizeof(struct ringbuffer); //减去ringbuffer结构体的大小
    rb->read_index = 0;
    rb->write_index = 0;
    return rb;
}

 bool rb_is_empty(rb_t rb)
 {
    return rb->read_index == rb->write_index;
 }

 bool rb_is_full(rb_t rb)
 {
    return ((rb->write_index + 1) % rb->size) == rb->read_index;
 }

 bool rb_write(rb_t rb, uint8_t data)
 {
    if (rb_is_full(rb))
        return false; //缓冲区满
    rb->buffer[rb->write_index] = data;
    rb->write_index = (rb->write_index + 1) % rb->size; //环形递增
    return true;
 }

bool rb_read(rb_t rb, uint8_t *data)
{
    if (rb_is_empty(rb))
        return false; //缓冲区空
    *data = rb->buffer[rb->read_index];
    rb->read_index = (rb->read_index + 1) % rb->size; //环形递增
    return true;
}
