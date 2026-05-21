#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buffer;
    uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buf_data, uint32_t size);
bool ring_buffer_push(ring_buffer_t *rb, uint8_t data);
bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data);
bool ring_buffer_is_empty(const ring_buffer_t *rb);
bool ring_buffer_is_full(const ring_buffer_t *rb);
uint32_t ring_buffer_get_count(const ring_buffer_t *rb);
void ring_buffer_clear(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif // RING_BUFFER_H
