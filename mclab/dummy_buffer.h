/*
 Your competitor: the dummy buffer!  It's unsynchronized, and basically just
 writes values into an array.  There is no guarantee that what you read in 
 is what you get out, or even that you read out the same number of things 
 that you write it.  
 */

#include "main.h"
typedef struct dummy_buffer_t dummy_buffer_t;
dummy_buffer_t *dummy_allocate_buffer();
void dummy_free_buffer(dummy_buffer_t *);
void dummy_produce_event(dummy_buffer_t *dummy_buffer, event_t event);
void dummy_produced_last_event(dummy_buffer_t *dummy_buffer);
event_t dummy_consume_event(dummy_buffer_t *dummy_buffer);

