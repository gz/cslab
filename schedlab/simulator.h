#include <stdio.h>
#include <stdlib.h>

#define MAX_EVENT_SIZE      16
#define INITIAL_BUFFER_SIZE 32
#define MAX_PRIORITY         2
#define MAX_RESOURCES       32

#define ever (;;)

enum EVENT {
  LOCK,
  RENICE,
  STOP,
  UNLOCK
};

struct event {
  enum EVENT event;
  int        start_time;
  int        resource;
};

struct process {

  /* events */
  struct event **events;
  size_t         size_events;
  size_t         no_events;

  size_t         id;
  int            run_time;
  int            start_time;
  size_t         next_event;
  int            priority;

  /* stats */
  int            turnaround_time;
  int            response_time;
  int            wait_time;
  int            start_wait;

};

/* function prototypes */

static void   usage();
static int    compare_event(const void *e1, const void *e2);
