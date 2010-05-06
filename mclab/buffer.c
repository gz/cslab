/* Producer & Consumer
 * ====================
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
#define CACHE_LINE 64
#define BUFFER_SIZE 1024
#define NEXT(current) ((current)+1) % (BUFFER_SIZE);


/*consumer’s local control variables*/
struct consumer {
	int localWrite;
	int nextRead;
	int rBatch;
	char padding[CACHE_LINE-3*sizeof(int)];
} c;


/*producer’s local control variables*/
struct producer {
	int localRead;
	int nextWrite;
	int wBatch;
	char padding[CACHE_LINE-3*sizeof(int)];
} p;

/*constants*/
static int batchSize = 128;
static char cache_pad3[CACHE_LINE - 1 * sizeof(int)];

struct buffer_structure {
	event_t storage[BUFFER_SIZE];

	char cache_pad0[CACHE_LINE];
	volatile int read;
	volatile int write;
	char cache_pad1[CACHE_LINE - 2 * sizeof(int)];
};
//int producer_waited = 0;
//int consumer_waited = 0;

/** Allocates & initializes a buffer and returns it to the client.
 * @return buffer
 */
buffer_ptr allocate_buffer() {


	// dummy writes to make sure the compiler is not trying to be smart
	buffer_ptr buffer = malloc( sizeof(buffer_t) );

	if(buffer) {
		buffer->read = 0;
		buffer->write = 0;
	}
	c.localWrite = 0;
	c.nextRead = 0;
	c.rBatch = 0;
	p.localRead = 0;
	p.nextWrite = 0;
	p.wBatch = 0;

	//producer_waited = 0;
	//consumer_waited = 0;

    return buffer;
}


void free_buffer(buffer_ptr buffer) {
    free(buffer);
}

void produce_event(buffer_ptr buffer, event_t element) {
	//DEBUG_PRINT("produce\n");

	int afterNextWrite = NEXT(p.nextWrite);
	if (afterNextWrite == p.localRead) {
		while (afterNextWrite == buffer->read) {
			/* busy waiting */
			//producer_waited++;
		}
		p.localRead = buffer->read;
	}

	buffer->storage[p.nextWrite] = element;

	p.nextWrite = afterNextWrite;
	p.wBatch++;

	if (p.wBatch >= batchSize) {
		buffer->write = p.nextWrite;
		p.wBatch = 0;
	}

}

void produced_last_event(buffer_ptr buffer) {
	buffer->write = p.nextWrite;
	//DEBUG_PRINT("\nproducer waited: %d\nconsumer waited: %d\n", producer_waited, consumer_waited);
}

event_t consume_event(buffer_ptr buffer) {
	//DEBUG_PRINT("consume\n");

	if(c.nextRead == c.localWrite) {
		while(c.nextRead == buffer->write) {
			/*busy waiting*/
			//consumer_waited++;
		}
		c.localWrite = buffer->write;
	}

	event_t element = buffer->storage[c.nextRead];
	c.nextRead = NEXT(c.nextRead);

	c.rBatch++;
	if(c.rBatch >= batchSize) {
		buffer->read = c.nextRead;
		c.rBatch = 0;
	}

	return element;

}

