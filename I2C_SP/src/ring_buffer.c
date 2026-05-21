#include "ring_buffer.h"

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buf_data, uint32_t size) {
    rb->buffer = buf_data;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

bool ring_buffer_push(ring_buffer_t *rb, uint8_t data) {
    uint32_t next = (rb->head + 1) % rb->size;
    if (next == rb->tail) {
        // 緩衝區已滿
        return false;
    }
    rb->buffer[rb->head] = data;
    rb->head = next;
    return true;
}

bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data) {
    if (rb->head == rb->tail) {
        // 緩衝區為空
        return false;
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    return true;
}

bool ring_buffer_is_empty(const ring_buffer_t *rb) {
    return (rb->head == rb->tail);
}

bool ring_buffer_is_full(const ring_buffer_t *rb) {
    return (((rb->head + 1) % rb->size) == rb->tail);
}

uint32_t ring_buffer_get_count(const ring_buffer_t *rb) {
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return rb->size - (rb->tail - rb->head);
    }
}

void ring_buffer_clear(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}
