/* Producer & Consumer Headerfile */
#ifndef BUFFER_H
#define BUFFER_H

#include "main.h"

/* The type for the buffering system.  Defined in buffer.c. */
typedef struct buffer_structure buffer_t;
typedef struct buffer_structure* buffer_ptr;


/* Allocate a new buffering system.  In some driver modes, more than one
   buffer may be used simultaneously! */
buffer_ptr allocate_buffer();

/* Free any memory associated with a buffer_t */
void free_buffer(buffer_ptr);

/* Produces a new event.  Blocks until there is space to write the event. 
   The event parameter, 'event', cannot be NULL.  
 
   Note that events may be produced from a separate thread from the one 
   in which they are consumed; however, you may assume that they are only 
   produced from a single thread. */
void produce_event(buffer_ptr buffer, event_t event);

/* Indicates that the last event has been produced. */
void produced_last_event(buffer_ptr buffer);

/* Consumes an event from 'buffer', blocking if no events are available.
   Once all events have been consumed and produced_last_event() has been
   called, returns NULL.  Once a NULL value has been returned, further
   behavior is undefined. 
 
   Note that events may be consumed from a separate thread from the one 
   in which they are produced; however, you may assume that they are only 
   consumed from a single thread. */
event_t consume_event(buffer_ptr buffer);

#endif

