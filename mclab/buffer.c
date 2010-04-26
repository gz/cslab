/*
 *  buffer.c
 *  prodconsumer
 *
 *  Created by Nicholas Matsakis on 4/2/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "buffer.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#if 0
#  define debug(args...) fprintf(args)
#else
#  define debug(args...)
#endif

#define BUFFER_SIZE 512

/*
 Simplest possible buffer structure: use a single, circular buffer.  
 
 wr_pos always points at a spot that is safe to write, and rd_pos always
 points at the next spot to read (it may not be safe).
 
 Attempts to read block if rd_pos == wr_pos.
 
 Attempts to write block if wr_pos == rd_pos - 1, because the next spot is
 not yet safe to write.
 
 You will probably want to change this quite a bit!
 */
struct buffer_t {
    event_t buffer[BUFFER_SIZE];
    int rd_pos;
    int wr_pos;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

buffer_t *allocate_buffer() {
    buffer_t *buffer = (buffer_t*) malloc(sizeof(buffer_t));
    if (buffer) {
        buffer->rd_pos = 0;
        buffer->wr_pos = 0;
        pthread_mutex_init(&buffer->lock, NULL);
        pthread_cond_init(&buffer->cond, NULL);
    }
    return buffer;
}

void free_buffer(buffer_t *buffer) {
    pthread_mutex_destroy(&buffer->lock);
    free(buffer);
}

void produce_event(buffer_t *buffer, event_t event) {
    int next_wr_pos;
    
    pthread_mutex_lock(&buffer->lock);        
    
    // block until the next spot is safe to write
    next_wr_pos = (buffer->wr_pos+1) % BUFFER_SIZE;
    while (next_wr_pos == buffer->rd_pos) {
        debug(stderr, "produce_block(%d,%d)\n",
                buffer->rd_pos, buffer->wr_pos);
        pthread_cond_wait(&buffer->cond, &buffer->lock);
    }
    
    buffer->buffer[buffer->wr_pos] = event;
    buffer->wr_pos = next_wr_pos;
    
    pthread_mutex_unlock(&buffer->lock);
    pthread_cond_broadcast(&buffer->cond);
}

void produced_last_event(buffer_t *buffer) {
    produce_event(buffer, NULL);
}

event_t consume_event(buffer_t *buffer) {    
    pthread_mutex_lock(&buffer->lock);
    
    // block until current spot is safe to read
    while (buffer->rd_pos == buffer->wr_pos) {
        debug(stderr, "consume_block(%d,%d)\n",
                buffer->rd_pos, buffer->wr_pos);
        pthread_cond_wait(&buffer->cond, &buffer->lock);
    }
    
    if ((buffer->rd_pos % 50) == 0)
        debug(stderr, "consume rd_pos = %d wr_pos = %d\n", 
              buffer->rd_pos, buffer->wr_pos);
    event_t result = buffer->buffer[buffer->rd_pos++];    
    buffer->rd_pos = buffer->rd_pos % BUFFER_SIZE;
    
    pthread_mutex_unlock(&buffer->lock);
    pthread_cond_broadcast(&buffer->cond);
    
    return result;
}

