#include "dummy_buffer.h"
#include <stdlib.h>
#include <stdio.h>

#define BUFFER_SIZE 1024

struct dummy_buffer_t {
    int pos;
    event_t data[BUFFER_SIZE];
};

dummy_buffer_t *dummy_allocate_buffer() {
    dummy_buffer_t *dummy = (dummy_buffer_t *)calloc(1, sizeof(dummy_buffer_t));
    return dummy;
}

void dummy_free_buffer(dummy_buffer_t *buffer) {
    free(buffer);
}

static void produce_event(dummy_buffer_t *buffer, event_t event)
{
    buffer->data[buffer->pos] = event;
    buffer->pos = (buffer->pos + 1) % BUFFER_SIZE;
}

void dummy_produce_event(dummy_buffer_t *buffer, event_t event) {
    produce_event(buffer, event);
}

void dummy_produced_last_event(dummy_buffer_t *buffer) {
    produce_event(buffer, NULL);
}

event_t dummy_consume_event(dummy_buffer_t *buffer) {
    buffer->pos = (buffer->pos + 1) % BUFFER_SIZE;
    return buffer->data[buffer->pos];
}
