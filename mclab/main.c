/*
 *  multicode_lab, main.c
 *
 *  This file is the main driver.  Try ./main -h for help on the
 *  commands.  Edit the table in the main() method to test your code
 *  in different ways (it is commented below).
 */

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "main.h"
#include "buffer.h"
#include "dummy_buffer.h"

#define CONSUME 1  // run a consumer thread
#define PRODUCE 2  // run a producer thread
#define CHECK   4  // check the results are correct
#define SEPBUFS 8  // allocate separate buffer structs for prod/cons

#pragma mark -
#pragma mark Buffer Funcs Structure

typedef void* (*create_func)();
typedef void (*destroy_func)(void *);
typedef void (*produce_event_func)(void *, void *);
typedef void (*finish_func)(void *);
typedef void* (*consume_event_func)(void *);

struct buffer_funcs_t {
    create_func create;
    destroy_func destroy;
    produce_event_func produce;
    finish_func finish;
    consume_event_func consume;
};
typedef struct buffer_funcs_t buffer_funcs_t;

struct buffer_funcs_t student = {
    (create_func)        allocate_buffer,
    (destroy_func)       free_buffer,
    (produce_event_func) produce_event,
    (finish_func)        produced_last_event,
    (consume_event_func) consume_event,    
};

struct buffer_funcs_t dummy = {
    (create_func)        dummy_allocate_buffer,
    (destroy_func)       dummy_free_buffer,
    (produce_event_func) dummy_produce_event,
    (finish_func)        dummy_produced_last_event,
    (consume_event_func) dummy_consume_event,
};


#pragma mark -
#pragma mark Time Measurement

typedef long long benchmark_t;

benchmark_t gettime() {
    struct timeval val;
    benchmark_t res;
    
    gettimeofday(&val, NULL);
    res = val.tv_sec;
    res *= 1000000;
    res += val.tv_usec;
    return res;
}

#pragma mark -
#pragma mark Simple Driver

struct simple_argument_t {
    struct buffer_funcs_t *funcs;
    void *buffer;
    int flags;
    long n; // number of events to write / read
};
typedef struct simple_argument_t simple_argument_t;

void *simple_producer_thread(void *_arg) {
    simple_argument_t *arg = (simple_argument_t*)_arg;
    long i;
    union {
        event_t event;
        long integer;
    } u;
    
    for (i = 0; i < arg->n; i++) {
      u.event = NULL;
      u.integer = i+1;
      assert(u.integer == i+1);
      arg->funcs->produce(arg->buffer, u.event);
    }
    arg->funcs->finish(arg->buffer);
    return NULL;
}

void *simple_consumer_thread(void *_arg) {
    simple_argument_t *arg = (simple_argument_t*)_arg;
    int i;
    union {
        event_t event;
        long integer;
    } u;
    
    if (arg->flags & CHECK)
        for (i = 0; i < arg->n; i++) {
            u.event = arg->funcs->consume(arg->buffer);
            if (u.integer != i+1) {
                fprintf(stderr, "Error: event #%d was %lx (%p), not %x\n", 
                        i, u.integer, u.event, i+1);
                exit(1);
            }
        }
    else 
        for (i = 0; i < arg->n; i++)
            arg->funcs->consume(arg->buffer);
    
    return NULL;
}

benchmark_t simple_launch(buffer_funcs_t *funcs, long n, int flags) {
    simple_argument_t prodarg = { funcs, NULL, flags, n };
    simple_argument_t consarg = { funcs, NULL, flags, n };
    pthread_t produce, consume;

    // Create buffers:
    prodarg.buffer = funcs->create();
    if (flags & SEPBUFS)
        consarg.buffer = funcs->create();
    else
        consarg.buffer = prodarg.buffer;

    // Measure total time:    
    benchmark_t start = gettime();
    if (flags & PRODUCE)
        pthread_create(&produce, NULL, simple_producer_thread, &prodarg);
    if (flags & CONSUME)
        pthread_create(&consume, NULL, simple_consumer_thread, &consarg);
    if (flags & PRODUCE)
        pthread_join(produce, NULL);
    if (flags & CONSUME)
        pthread_join(consume, NULL);
    benchmark_t stop = gettime();
    
    // Free memory:
    funcs->destroy(prodarg.buffer);
    if (flags & SEPBUFS)
        funcs->destroy(consarg.buffer);
    
    return stop - start;
}

#pragma mark -
#pragma mark Main Routine

typedef struct configuration_t {
    const char *label;
    buffer_funcs_t *funcs;
    int flags;
    benchmark_t *timing;
    benchmark_t sum;
    double average;
    benchmark_t median;
    double overhead;
} configuration_t;

void gather_data(configuration_t *config,
                 long trials, 
                 long n)
{
    long i;
    config->timing = (benchmark_t*)malloc(sizeof(benchmark_t) * trials);
    
    printf("Benchmarking %s", config->label); fflush(stdout);
    
    // run 1 uncounted "warm-up" trial
    simple_launch(config->funcs, n, config->flags);    

    for (i = 0; i < trials; i++) {
        printf("."); fflush(stdout);
        config->timing[i] = simple_launch(config->funcs, n, config->flags);
    }
    
    printf("\n");
}

int compare_benchmark(const void *_one, const void *_two) {
    const benchmark_t **one = (const benchmark_t **)_one;
    const benchmark_t **two = (const benchmark_t **)_two;
    return (*one) - (*two);
}

benchmark_t median(int cnt, benchmark_t *timing) {
    qsort(timing, cnt, sizeof(benchmark_t),  compare_benchmark);
    if ((cnt % 2) == 0) {
        // even number
        return (timing[cnt / 2] + timing[cnt / 2 - 1]) / 2;
    } else {
        // odd number
        return timing[cnt / 2];
    }
}

long convert(char *s) {
    char *err = NULL;
    long result = strtol(s, &err, 10);

    if (*err) {
        fprintf(stderr, "Invalid number: %s\n", s);
        return -1;
    }

    return result;
}

int main(int argc, char *argv[]) {
    long i, j, trials, n;

    /* TABLE ____________________________________________________________ */
    /* This table determines what will be executed.  Each line defines one
       test configuration.  The first line is always the "reference"
       configuration against which others are compared. 

       A table line looks like: { name, funcs, flags, } where the 
       name is a string, funcs is a buffers_funcs_t*, and flags is
       a set of integer flags which are passed to the test runner.
    
       The thing you are most likely to modify is the flags. The valid
       flags are defined at the top of this file.  Their meanings are:

       CONSUME // run a consumer thread
       PRODUCE // run a producer thread
       CHECK   // check the results are correct
       SEPBUFS // allocate separate buffer structs for prod/cons

       For example, when you start developing, you may want to try
       removing the CHECK flag initially so that you don't have to
       worry about synchronization.  The SEPBUFS flag actually allocates
       a separate copy of your buffer for the producer and consumer
       so that they cannot actually talk to one another; just because
       they are using separate structures, however, does not mean that
       they cannot interfere with one another, so this can be another
       useful flag for isolating problems.
    */
    struct configuration_t config[] = {
        { "dummy_prod", &dummy, PRODUCE },
        { "dummy_cons", &dummy, CONSUME },
        { "student", &student, PRODUCE|CONSUME|CHECK },
    };
    const int num_configs = sizeof(config) / sizeof(configuration_t);
    /* END TABLE ________________________________________________________ */
    
    // default parameters:
#   define DEF_TRIALS 3L
#   define DEF_N (50000L*1024)
    trials = DEF_TRIALS;
    n = DEF_N;

    if (argc >= 2) trials = convert(argv[1]);
    if (argc >= 3) n = convert(argv[2]);

    if (trials <= 0 || n <= 0) {
        fprintf(stderr, "Usage: %s [trials] [events per trial]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "If each test is taking way too long, lower the\n");
        fprintf(stderr, "values for events per trial.  The defaults are\n");
        fprintf(stderr, "trials=%ld, events per trial=%ld\n", DEF_TRIALS,
                DEF_N);
        return 1;
    }

    // Gather data
    for (i = 0; i < num_configs; i++)
        gather_data(config+i, trials, n);
    
    // Compute average and overhead
    for (i = 0; i < trials; i++) {
        for (j = 0; j < num_configs; j++) {
            config[j].sum += config[j].timing[i];
        }
    }
    for (i = 0; i < num_configs; i++) {
        config[i].average = ((double)config[i].sum) / trials;
        config[i].overhead = config[i].average / config[0].average;
        
        config[i].median = median(trials, config[i].timing);
        config[i].overhead = ((double)config[i].median) / ((double)config[0].median);
    }
    
    // Dump data in tabular form
    printf("%-15s %10s %10s", "Test", "Overhead", "Median");
    for (i = 0; i < trials; i++) printf(" %10ld", i);
    printf("\n");
    for (i = 0; i < num_configs; i++) {
        printf("%-15s %10.2lf %10lld", 
               config[i].label, config[i].overhead, config[i].median);
        for (j = 0; j < trials; j++)
            printf(" %10lld", config[i].timing[j]);
        printf("\n");
    }
    
    return 0;
}
