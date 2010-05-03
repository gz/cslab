/* Producer & Consumer
 * ====================
 *
 *
 * Authors
 * ====================
 * team07:
 * Boris Bluntschli (borisb@student.ethz.ch)
 * Gerd Zellweger (zgerd@student.ethz.ch)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "buffer.h"


// Some basic types & macros
typedef int boolean;
#define TRUE 1
#define FALSE 0
typedef unsigned int uint;


// Macro for Debugging
#define DEBUG 0
#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
	#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release */
#endif


// Global Definitions & Variables
#define BUFFER_SIZE 512


struct buffer_structure {
    event_t storage[BUFFER_SIZE];
    int write_index;
    int read_index;
};

/** Allocates & initializes a buffer and returns it to the client.
 * @return buffer
 */
buffer_ptr allocate_buffer() {

	buffer_ptr buffer = malloc( sizeof(buffer_t) );

	if(buffer) {
		buffer->write_index = 0;
		buffer->read_index = 0;
	}

    return buffer;
}


void free_buffer(buffer_ptr buffer) {
    free(buffer);
}

void produce_event(buffer_ptr buffer, event_t event) {
	//assert(event != NULL);

	// mutually exclusive
	int previous_index = buffer->write_index++;
	// end mutually exclusive
	DEBUG_PRINT("write at %d\n", previous_index);
	buffer->storage[previous_index % BUFFER_SIZE] = event;
}

void produced_last_event(buffer_ptr buffer) {
    buffer->write_index += 1;
}

event_t consume_event(buffer_ptr buffer) {
    
    // block until current spot is safe to read
    while (buffer->read_index == buffer->write_index) { DEBUG_PRINT("busy wait with write index %d\n", buffer->write_index); }
    DEBUG_PRINT("read_index is %d\n", buffer->read_index);
    event_t result = buffer->storage[buffer->read_index];
    buffer->read_index = buffer->read_index++ % BUFFER_SIZE;
    
    return result;
}

