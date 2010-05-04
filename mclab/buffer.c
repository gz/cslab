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
#define BUFFER_SIZE 1640

char cache_pad0[CACHE_LINE];

/*consumer’s local control variables*/
int localWrite = 0;
int nextRead = 0;
int rBatch = 0;
char cache_pad1[CACHE_LINE - 3 * sizeof(int)];

/*producer’s local control variables*/
int localRead = 0;
int nextWrite = 0;
int wBatch = 0;
char cache_pad2[CACHE_LINE - 3 * sizeof(int)];

/*constants*/
int batchSize = 64;
char cache_pad3[CACHE_LINE - 3 * sizeof(int)];




struct buffer_structure {
    event_t storage[BUFFER_SIZE];

    char cache_pad0[CACHE_LINE];
    volatile int read;
    volatile int write;
    char cachePad1[CACHE_LINE - 2 * sizeof(int)];
};

/** Allocates & initializes a buffer and returns it to the client.
 * @return buffer
 */
buffer_ptr allocate_buffer() {

	buffer_ptr buffer = malloc( sizeof(buffer_t) );

	if(buffer) {
		buffer->read = 0;
		buffer->write = 0;
		int i;
		for(i=0; i<BUFFER_SIZE; i++) {
			//buffer->storage[i] = NULL;
		}
	}


    return buffer;
}


void free_buffer(buffer_ptr buffer) {
    free(buffer);
}

int NEXT(int current) {
	return (current+1) % (BUFFER_SIZE);
}

void produce_event(buffer_ptr buffer, event_t element) {
	//DEBUG_PRINT("produce\n");

	int afterNextWrite = NEXT(nextWrite);
	if (afterNextWrite == localRead) {
		while (afterNextWrite == buffer->read) {
			/* busy waiting */
		}
		localRead = buffer->read;
	}

	buffer->storage[nextWrite] = element;

	nextWrite = afterNextWrite;
	wBatch++;
	if (wBatch >= batchSize) {
		buffer->write = nextWrite;
		wBatch = 0;
	}

}

void produced_last_event(buffer_ptr buffer) {}

event_t consume_event(buffer_ptr buffer) {
	//DEBUG_PRINT("consume\n");

	if(nextRead == localWrite) {
		while(nextRead == buffer->write) {
			/*busy waiting*/
		}
		localWrite = buffer->write;
	}

	event_t element = buffer->storage[nextRead];
	nextRead = NEXT(nextRead);

	rBatch++;
	if(rBatch >= batchSize) {
		buffer->read = nextRead;
		rBatch = 0;
	}

	return element;

}

