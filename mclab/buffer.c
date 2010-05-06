/* Producer & Consumer
 * ====================
 * Our producer/consumer code implements the following ideas:
 *  - Make sure that concurrent threads access variables in different cache lines
 *    as far as possible
 *  - Use busy waiting instead of locking
 *    This implies to have to variables head/tail for writes/reads in the buffer.
 *  - Shared variables between producer and consumer are updated not every time
 *    to minimize synchronization overhead
 *  - Reduce the frequency of access to shared variables by introducing local
 *    control variables (local_head & local_tail)
 *
 * The implementation is a lock free shared ring buffer implementation
 * based on the paper "A Lock-Free, Cache-Efficient Shared Ring Buffer for
 * Multi-Core Architectures" by Patrick P. C. Lee, Tian Bu, Girish Chandranmenon.
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
#define DEBUG 1
#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
	#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release */
#endif


// Global Definitions & Variables
#define CACHE_LINE 64 		// cache line is 64 bytes (this can vary from cpu to cpu)
#define BUFFER_SIZE 1024 	// buffer size (we tried multiples of 2 and took the one which gave us the best results)
#define BATCH_SIZE 128		// batch size, this means that buffer->head/tail are only updated every 128 inserts/updates
							// (we tried multiples of 2 and took the one which gave us the best results)
#define NEXT(current) ((current)+1) % (BUFFER_SIZE); // returns the next element in a circular buffer of size BUFFER_SIZE


/**
 * Variables used by the consumer.
 **/
static struct consumer {
	int local_head;		// is always <= buffer->head
	int next_tail;		// used to store actual tail until we update buffer->tail
	int batch_counter;	// determines when to update buffer->tail
	char padding[CACHE_LINE-3*sizeof(int)]; // make sure this struct is placed in its own cache line
} c;


/**
 * Variables used by the producer.
 **/
static struct producer {
	int local_tail;		// is always <= buffer->tail
	int next_head;		// used to store actual head until we update buffer->head
	int batch_counter;	// determines when to update buffer->head
	char padding[CACHE_LINE-3*sizeof(int)]; // make sure this struct is placed in its own cache line
} p;


/**
 * Buffer structure contains the data and the shared variables between
 * the producer and consumer.
 * Please note that head and tail do not actually have to correspond to the real
 * head and tail values since we update them only after BATCH_SIZE read/writes
 * to the buffer.
 * Head and tail are marked as volatile since they're shared between
 * the producer and consumer and it is vital that they're written
 * back to memory every time they change.
 */
struct buffer_structure {
	event_t storage[BUFFER_SIZE];

	char padding0[CACHE_LINE];
	volatile int tail;
	volatile int head;
	char padding1[CACHE_LINE - 2 * sizeof(int)];
};


/** Allocates & initializes a buffer and returns it to the client.
 * @return buffer
 */
buffer_ptr allocate_buffer() {

	buffer_ptr buffer = malloc( sizeof(buffer_t) );

	if(buffer) {
		buffer->tail = 0;
		buffer->head = 0;

		// initialize variables for consumer/producer
		c.local_head = 0;
		c.next_tail = 0;
		c.batch_counter = 0;
		p.local_tail = 0;
		p.next_head = 0;
		p.batch_counter = 0;
	}

    return buffer;
}


void free_buffer(buffer_ptr buffer) {
    free(buffer);
}


void produce_event(buffer_ptr buffer, event_t element) {

	int after_next_write = NEXT(p.next_head);
	if (after_next_write == p.local_tail) {
		// wait until consumer has read enough and we can write again
		while (after_next_write == buffer->tail) {
			/* busy waiting */
		}
		p.local_tail = buffer->tail;
	}

	buffer->storage[p.next_head] = element;
	p.next_head = after_next_write;

	// updates buffer->head every time batch counter reaches BATCH_SIZE
	p.batch_counter++;
	if (p.batch_counter >= BATCH_SIZE) {
		buffer->head = p.next_head;
		p.batch_counter = 0;
	}

}


void produced_last_event(buffer_ptr buffer) {
	buffer->head = p.next_head;
}


event_t consume_event(buffer_ptr buffer) {

	if(c.next_tail == c.local_head) {
		// wait until producer has produced something and we can read again
		while(c.next_tail == buffer->head) {
			/* busy waiting */
		}
		c.local_head = buffer->head;
	}

	event_t element = buffer->storage[c.next_tail];
	c.next_tail = NEXT(c.next_tail);

	// updates buffer->tail every time batch counter reaches BATCH_SIZE
	c.batch_counter++;
	if(c.batch_counter >= BATCH_SIZE) {
		buffer->tail = c.next_tail;
		c.batch_counter = 0;
	}

	return element;

}

