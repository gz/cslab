/*
 * (c) ETH Zurich 2006-2007
 * Matteo Corti <matteo.corti@id.ethz.ch>
 * Mathias Payer <mathias.payer@inf.ethz.ch>
 */

#include "scheduler.h"
#include "simulator.h"
#include <string.h>

static void usage() {
  fprintf(stderr, "Usage: ./schedule file\n");
  exit(EXIT_FAILURE);
}

static int compare_event(const void *e1, const void *e2) {
  if ((*(struct event **)e1)->start_time < (*(struct event **)e2)->start_time)
    return -1;
  else if ((*(struct event **)e1)->start_time == (*(struct event **)e2)->start_time)
    return 0;
  else
    return 1;
}


static int compare_proc(const void *p1, const void *p2) {

  if (*(struct process **)p1 == NULL) {
    if (*(struct process **)p2 == NULL)
      return 0;
    return 1;
  }

  if (*(struct process **)p2 == NULL)
    return -1;

  if ((*(struct process **)p1)->start_time < (*(struct process **)p2)->start_time)
    return -1;
  else if ((*(struct process **)p1)->start_time == (*(struct process **)p2)->start_time)
    return 0;
  else
    return 1;
}

int main(int argc, char ** argv) {

  FILE            *fd;

  int              ret;

  int              time;
  char             event[MAX_EVENT_SIZE];
  size_t           id;
  int              duration;
  int              arg;
  int              line;

  /* helper variable to store the old length of a reallocated data structure */
  int              old_size;

  double           a_turnaround_time = 0.0;
  double           a_response_time   = 0.0;
  double           a_waiting_time    = 0.0;
  int              idle_time         =   0;

  size_t           size_procs        = INITIAL_BUFFER_SIZE;
  struct process **procs;
  size_t          *pid_map;
  int              no_procs          = 0;

  struct event    *e;
  size_t           i;
  size_t           j;
  
  int              wct;
  size_t           next_start;

  int              finished;

  procs   = malloc(sizeof(struct process *)*size_procs);
  if (!procs) {
    fprintf(stderr, "Error: out of memory!\n");
    exit(EXIT_FAILURE);
  }
  pid_map = malloc(sizeof(size_t)*size_procs);
  if (!pid_map) {
    fprintf(stderr, "Error: out of memory!\n");
    exit(EXIT_FAILURE);
  }
  
  memset(procs,    0, sizeof(struct process *)*size_procs);
  memset(pid_map, -1, sizeof(size_t)*size_procs);

  if (argc != 2) {
    usage();
  }

  printf("Scheduling %s\n", argv[1]);

  /* open the schedule file */
  if (!(fd = fopen(argv[1], "r"))) {
    fprintf(stderr, "Error: Cannot open %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  line = 0;

  while((ret = fscanf(fd, "%i %s %zi %i %i\n", &time, event, &id, &duration, &arg)) != EOF) {

    line++;

    if (ret != 5) {
      fprintf(stderr, "Error: cannot parse command line\n");
      exit(EXIT_FAILURE);

    }
    
    if (strncmp("start", event, strlen("start")) == 0) {

      if (id >= size_procs) {
        /* grow the procs array */
	size_t old_size = size_procs;
        size_procs = id + 1;
        procs   = realloc(procs,   sizeof(struct process *)*size_procs);
        pid_map = realloc(pid_map, sizeof(size_t)*size_procs);
	memset(procs   + old_size,  0, sizeof(struct process *)*(size_procs-old_size));
        memset(pid_map + old_size, -1, sizeof(size_t)*(size_procs-old_size));
      }

      /* check priority */
      if (arg < 0 || arg > MAX_PRIORITY) {
        fprintf(stderr, "Error at line %i: illegal priority %d\n", line, arg);
        exit(EXIT_FAILURE);
      }

      /* create a new process */
      if (procs[id] != NULL) {
        fprintf(stderr, "Error: process %zi already exists!\n", id);
        exit(EXIT_FAILURE);
      }
        
      procs[id] = malloc(sizeof(struct process));
      if (!procs[id]) {
        fprintf(stderr, "Error: out of memory!\n");
        exit(EXIT_FAILURE);
      }        
      procs[id]->size_events     = INITIAL_BUFFER_SIZE;
      procs[id]->events          = calloc(procs[id]->size_events, sizeof(struct event*));
      if (!procs[id]->events) {
        fprintf(stderr, "Error: out of memory!\n");
        exit(EXIT_FAILURE);
      }
      procs[id]->id              =   id;
      procs[id]->no_events       =    0;
      procs[id]->run_time        =    0;
      procs[id]->start_time      = time;
      procs[id]->next_event      =    0;
      procs[id]->priority        =  arg;
      procs[id]->turnaround_time =   -1;
      procs[id]->response_time   =   -1;
      procs[id]->wait_time       =    0;
      procs[id]->start_wait      =   -1;

      e             = malloc(sizeof(struct event));
      if (!e) {
        fprintf(stderr, "Error: out of memory\n");
        exit(EXIT_FAILURE);
      }
      e->start_time = duration;
      e->resource   = -1;
      e->event      = STOP;
      procs[id]->events[procs[id]->no_events++] = e;
      
    } else {
      
      /* add an event to an existing process */
      if (procs[id] == NULL) {
        fprintf(stderr, "Error at line %i: adding an event to a non existing process (%zi)\n", line, id);
        exit(EXIT_FAILURE);
      }
      
      e             = malloc(sizeof(struct event));
      if (!e) {
        fprintf(stderr, "Error: out of memory\n");
        exit(EXIT_FAILURE);
      }
      e->start_time = time;
      e->resource   = arg;

      if (strncmp("lock", event, strlen("lock")) == 0) {

        /* check resource ID */
        if (arg < 0 ||  arg > MAX_RESOURCES) {
          fprintf(stderr, "Error at line %i: resource %d does not exist\n", line, arg);
          exit(EXIT_FAILURE);
        }

	e->event = LOCK;

	if (procs[id]->no_events == procs[id]->size_events) {
          old_size               = procs[id]->size_events;
	  procs[id]->size_events = procs[id]->size_events*2;
	  procs[id]->events      = realloc(procs[id]->events,
					   procs[id]->size_events*sizeof(struct event *));
          memset(procs[id]->events+old_size,
                 0,
                 sizeof(struct event *)*(procs[id]->size_events-old_size));
	}
	procs[id]->events[procs[id]->no_events++] = e;
	
	/* unlock after duration */
	e             = malloc(sizeof(struct event));
        if (!e) {
          fprintf(stderr, "Error: out of memory\n");
          exit(EXIT_FAILURE);
        }
	e->start_time = time+duration;
	e->resource   = arg;
	e->event      = UNLOCK;

	if (procs[id]->no_events == procs[id]->size_events) {
          old_size               = procs[id]->size_events;
	  procs[id]->size_events = procs[id]->size_events*2;
	  procs[id]->events      = realloc(procs[id]->events,
					   procs[id]->size_events*sizeof(struct event *));
          memset(procs[id]->events+old_size,
                 0,
                 sizeof(struct event *)*(procs[id]->size_events-old_size));
	}
	procs[id]->events[procs[id]->no_events++] = e;

      } else if (strncmp("renice", event, strlen("renice")) == 0) {

	/* schedule a RENICE event TIME after the starttime of the process */
	e->event = RENICE;
	e->start_time = time;
	e->resource   = arg;
        
	if (procs[id]->no_events == procs[id]->size_events) {
          old_size               = procs[id]->size_events;
	  procs[id]->size_events = procs[id]->size_events*2;
	  procs[id]->events      = realloc(procs[id]->events,
					   procs[id]->size_events*sizeof(struct event *));
          memset(procs[id]->events+old_size,
                 0,
                 sizeof(struct event *)*(procs[id]->size_events-old_size));
	}
	procs[id]->events[procs[id]->no_events++] = e;
	/* do NOT adjust process-priority here, it will reset the initial priority of the process, which is BAD */
	//	procs[id]->priority = arg;

      } else {
	fprintf(stderr, "Error at line %i: Unknown event: %s\n", line, event);
	exit(EXIT_FAILURE);
      }

    }

  }

  (void)fclose(fd);

  qsort(procs, size_procs, sizeof(struct process*), compare_proc);

  /* build the PID map table */
  for(i = 0; i < size_procs; i++) {
    if (procs[i] != NULL) {
      pid_map[procs[i]->id] = i;
    }
  }
  
  /* sort events */
  for(i = 0; i < size_procs; i++) {
    if (procs[i] != NULL) {
      qsort(procs[i]->events,
	    procs[i]->no_events,
	    sizeof(struct event *),
	    compare_event);
    }
  }

  wct        = 0;
  next_start = 0;
  
  for ever {

    /* see if we need to start a process */
    while (next_start < size_procs &&
	procs[next_start] != NULL &&
	procs[next_start]->start_time <= wct ) {
#ifdef DEBUG
      printf("# start %i with priority %i\n",
             (int)(procs[next_start]->id),
             procs[next_start]->priority
             );
#endif
      sch_start((int)(procs[next_start]->id), procs[next_start]->priority);
      next_start++;
    }

    /* do we have a new event? */
    for (i=0; i< next_start; i++) {

      while (procs[i]->no_events > procs[i]->next_event &&
             procs[i]->events[procs[i]->next_event]->start_time <=
             procs[i]->run_time) {
        
	switch(procs[i]->events[procs[i]->next_event]->event) {
	case STOP:
	  sch_exit((int)(procs[i]->id));
	  procs[i]->next_event = procs[i]->no_events;
	  break;
	case LOCK:
#ifdef DEBUG
      printf("# %i locks %i\n",
             (int)(procs[i]->id),
             procs[i]->events[procs[i]->next_event]->resource             
             );
#endif
          if (procs[i]->start_wait < 0) {
            procs[i]->start_wait = wct;
          }
	  sch_locked  ((int)(procs[i]->id),
                       procs[i]->events[procs[i]->next_event]->resource);
	  break;
	case UNLOCK:
#ifdef DEBUG
      printf("# %i unlocks %i\n",
             (int)(procs[i]->id),
             procs[i]->events[procs[i]->next_event]->resource             
             );
#endif
	  sch_unlocked((int)(procs[i]->id),
                       procs[i]->events[procs[i]->next_event]->resource);
	  break;
        case RENICE:
#ifdef DEBUG
          printf("# renice %i with priority %i\n",
                 (int)(procs[i]->id),
                 procs[i]->events[procs[i]->next_event]->resource
                 );
#endif
          procs[i]->priority = procs[i]->events[procs[i]->next_event]->resource;
          sch_renice((int)(procs[i]->id),
                     procs[i]->events[procs[i]->next_event]->resource);
          break;
	default:
	  break;
          
        }
        
        procs[i]->next_event++;

      }

    }

    /* are we at the end? */
    finished = 1;
    for(i = 0; i < size_procs; i++) {
      if (procs[i] != NULL) {
        if (procs[i]->next_event < procs[i]->no_events) {
          finished = 0;
        } else {
          if (procs[i]->turnaround_time == -1) {
            procs[i]->turnaround_time = wct - procs[i]->start_time;
          }
        }
      }
    }
    
    if (finished) break;

    /* end of time quantum -> schedule the next process */
    
    sch_schedule();

#ifdef DEBUG
    if (current != -1 && (current>=0 && current<size_procs)) {
      printf("%d: scheduling %zi (prio %d) (time %i)\n",
             wct,
             procs[pid_map[current]]->id,
             procs[pid_map[current]]->priority,
             procs[pid_map[current]]->run_time+1
             );
    } else {
      printf("%d: -\n", wct);
    }
#endif

    if (current == -1) {
      idle_time++;
    }

    /* accounting */
    if (current != -1 && (current>=0 && current<size_procs)) {
      procs[pid_map[current]]->run_time++;

      if (procs[pid_map[current]]->response_time == -1) {
        /* the process is scheduled for the first time */
        procs[pid_map[current]]->response_time = wct - procs[pid_map[current]]->start_time;
      }

      if (procs[pid_map[current]]->start_wait >= 0) {
        procs[pid_map[current]]->wait_time += wct - procs[pid_map[current]]->start_wait;
        procs[pid_map[current]]->start_wait = -1;
      }

    }

    wct++;

  }

  /* Compute some statistics */

  /* sort events */
  no_procs          = 0;
  a_turnaround_time = 0.0;
  a_response_time   = 0.0;
  a_waiting_time    = 0.0;
  for(i = 0; i < size_procs; i++) {
    if (procs[i] != NULL) {
      no_procs++;
      a_response_time   += procs[i]->response_time;
      a_turnaround_time += procs[i]->turnaround_time;
      a_waiting_time    += procs[i]->wait_time;
    }
  }
  a_response_time   = a_response_time   / no_procs;
  a_turnaround_time = a_turnaround_time / no_procs; 
  a_waiting_time    = a_waiting_time    / no_procs;


  printf("\nStatistics\n");
  printf("============\n\n");

  printf("# processes:\t\t%d\n",              no_procs);
  printf("simulation time:\t%d time units\n", wct);
  printf("\n");
  printf("av response time:\t%.1f\n",         a_response_time);
  printf("av turnaround time:\t%.1f\n",       a_turnaround_time);
  printf("av waiting time:\t%.1f\n",          a_waiting_time);
  printf("CPU utilization:\t%.2f%%\n",        (1.0-(double)idle_time/wct)*100);

  /* free resources */
  sch_finalize();
  if (pid_map) {
    free(pid_map);
  }
  if (procs) {
    for(i = 0; i < size_procs; i++) {
      if (procs[i] != NULL) {
        /* free events */
        if (procs[i]->events != NULL) {
          for(j = 0; j < procs[i]->size_events; j++) {
            if (procs[i]->events[j]) {
              free(procs[i]->events[j]);
            }
          }
          free(procs[i]->events);
        }
        free(procs[i]);
      }
    }
    free(procs);
  }
  
  return 0;

}
