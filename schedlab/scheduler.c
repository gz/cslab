/* Scheduler Algorithm:
 * ====================
 * It's a simple round robin algorithm using a doubly Linked List.
 * The scheduler always searches through the list and finds the first
 * occurring element with currently highest priority. This elements is
 * then placed at the end of the process list and will be scheduled for
 * the next time slot. Newly created processes will be placed in top
 * of the list.
 *
 * Priority Handling:
 * ====================
 * The Scheduler Algorithm will look at internal_priority to determine
 * the priority of a given process. The internal_priority is automatically
 * decreased by the scheduler from time to time (by looking at `timeslots`)
 * to ensure that no starvation occurs.
 *
 * Locking Model:
 * ====================
 *   1. Process calls sch_locked for resource res
 *      1. sch_locked checks if resource is free
 *      	a) resource is free: update global locktable
 *      	b) resource is not free: set requested resource in `requested_locks` in process descriptor
 *             Priority Inversion: Raise priority of currently locking process to the same level as
 *             the process requesting the lock.
 *   2. sch_unlocked:
 *      free resource in locktable
 *   3. sch_schedule:
 *  	process can be scheduled if all locks in `requested_locks` can be acquired.
 *
 *  Authors:
 *  ====================
 *  team07:
 *  Boris Bluntschli (borisb@student.ethz.ch)
 *  Gerd Zellweger (zgerd@student.ethz.ch)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "scheduler.h"

/**
 * PID of the currently scheduled process
 */
extern int current;

// Some basic types & macros used in the scheduler code
#define max(x, y) ((x) > (y) ? (x) : (y))
typedef unsigned int U32;
typedef int boolean;
#define TRUE 1
#define FALSE 0

// Macro for Debugging
#define DEBUG 1
#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
	#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release */
#endif

/**
 * This struct holds information about a currently running Process.
 */
struct process_descriptor {
	int PID;
	int priority; 			// priority assigned by the user
	int internal_priority;	// current priority given by the scheduler, to make sure we have no starvation
	int timeslots;			// number of timeslots this process has already occupied
	U32 requested_locks;	// locks this process has requested but not acquired yet.
							// this is modeled as a bitmask and fits nicely in 32bits since we have 32 resources

	struct process_descriptor* prev;
	struct process_descriptor* next;
};
typedef struct process_descriptor* process;


// pointer to list of currently running processes
static process start_plist;

/**
 *  This table stores which resource is currently used by which process
 *  -1 means no process currently has a lock on this resource.
 */
static int locktable[32] = {
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1
};

// Macros for Resource Locking & Querying
#define RESOURCE_IS_FREE(resnr) ((locktable[(resnr)] == -1) ? 1 : 0)
#define PROCESS_WANTS_RESOURCE(p, resnr) (  !!(((p)->requested_locks) & (1<<(resnr)))  )
#define PROCESS_HAS_RESOURCE(p, resnr) ( (p)->PID == locktable[(resnr)] )

/**
 * This locks a resource `res` for process `p`.
 *  - Remove res in requested locks.
 *  - Update locktable.
 * This function expects the resource `res` to be free.
 * @param p process who wants to lock
 * @param res resource to lock
 */
inline static void lock_resource(process p, int res) {
	assert(RESOURCE_IS_FREE(res));

	p->requested_locks = p->requested_locks & (~(1 << res));
	locktable[res] = p->PID;
}

/** This function frees a resource in the global locktable.
 * @param res resource to free
 */
inline static void unlock_resource(int res) {
	locktable[res] = -1;
}

/** This signals for a process `p` that we need to lock resource
 * `res` before it can be run again.
 * @param p process who wants lock
 * @param res resource to lock
 */
inline static void wait_for_resource(process p, int res) {
	p->requested_locks = p->requested_locks | (1 << res);
}


/** This function checks if we can schedule a process.
 * @param p process which we want to check.
 * @return true if process can be scheduled, false otherwise.
 */
static boolean can_schedule(process p) {
	boolean can_schedule = TRUE;

	int i;
	for(i=0; i<32; i++) {
		if(PROCESS_WANTS_RESOURCE(p, i) && !RESOURCE_IS_FREE(i)) {
			can_schedule = FALSE;
			break;
		}
	}

	return can_schedule;
}

/** This acquires all locks for a process p which he wants.
 * It assumes that all the resources we want to lock are
 * free (can_schedule(p)).
 * @param p process which wants to acquire all resources
 */
static void acquire_all_locks(process p) {
	assert(can_schedule(p));

	int i;
	for(i=0; i<32; i++) {
		if(PROCESS_WANTS_RESOURCE(p, i)) {
			lock_resource(p, i);
		}
	}

}

/** Releases all locks a process has currently acquired.
 * @param p process for which we release the locks
 */
static void release_all_locks(process p) {

	int i;
	for(i=0; i<32; i++) {
		if(PROCESS_HAS_RESOURCE(p, i)) {
			unlock_resource(i);
		}
	}

}


/** Removes element in list.
 * @param p element to remove.
 */
static void list_remove(process p) {
	if(p->prev == NULL) {
		start_plist = p->next;
	} else {
		p->prev->next = p->next;
	}
	if(p->next != NULL) {
		p->next->prev = p->prev;
	}
}

/** Inserts element at the end of the list.
 * @param p element to insert.
 */
static void list_put_back(process p) {

	if(start_plist != NULL) {

		process old_last = start_plist;
		while(old_last->next != NULL) {
			old_last = old_last->next;
		}

		// old last points to last_element in start_plist
		old_last->next = p;
		p->prev = old_last;
		p->next = NULL;

	} else { // list is empty
		start_plist = p;
	}


}

/** Inserts element at the beginning of the list.
 * @param p element to insert.
 */
static void list_put_front(process p) {

	process old_first = start_plist;
	process new_first = p;

	if(old_first != NULL) {
		old_first->prev = new_first;
	}
	new_first->next = old_first;
	new_first->prev = NULL;
	start_plist = new_first;

}

/** Returns pointer to process in process list with PID number pid.
 * @param pid number of the process we want.
 * @return Pointer to process with p->PID = `pid` or NULL if not found.
 */
static process list_find_by_pid(int pid) {

	// Walk through list
	process current_process = start_plist;
	while(current_process != NULL) {

		if(current_process->PID == pid) {
			return current_process; // process found
		}

		current_process = current_process->next;
	}

	return NULL; // process not found
}


/** The function finds the _first_ element in the list which has the highest
 * priority overall elements in the list.
 *
 * @return First process in list which has highest internal priority overall
 * list elements or NULL if list is empty or no process can be scheduled.
 */
static process list_find_schedulable_by_highest_priority() {


	process highest_priority_process = NULL;

	if(start_plist != NULL) {

		// Walk through list
		process current_process = start_plist;
		while(current_process != NULL) {

			// its important to adjust only for strict higher priorities otherwise the scheduler would not be fair
			// for processes of the same priority, since the last scheduled process is always placed at the end of
			// the list
			//DEBUG_PRINT("PID:%d Schedule:%d lock_requested:%hd\n", current_process->PID, CAN_SCHEDULE(current_process), current_process->requested_locks);
			if( can_schedule(current_process) && highest_priority_process != NULL) {
				if(current_process->internal_priority > highest_priority_process->internal_priority ) {
					highest_priority_process = current_process; // higher priority process found
				}
			}
			// this is to set the process for the first time
			else if(can_schedule(current_process)) {
				highest_priority_process = current_process;
			}

			current_process = current_process->next;
		}

	}


	return highest_priority_process; // process not found
}


/**	Schedules the next process (i.e. sets `current` to the next PID).
 *	This function further makes sure that the next process acquires all the locks
 * 	it needs and places the currently scheduled thread at the end of the
 * 	process list so we have a round robin scheme for scheduling.
 * 	In addition it decreases the `internal_priority` of long running processes
 * 	to make sure we have no starvation.
 */
void sch_schedule() {
	 // set current to -1 in case we have nothing to schedule.
	current = -1;

	if(start_plist != NULL) {
		process p = list_find_schedulable_by_highest_priority();
		if(p != NULL) {
			acquire_all_locks(p);
			list_remove(p);
			list_put_back(p);

			//DEBUG_PRINT("Scheduling Process %d with internal priority %d\n", p->PID, p->internal_priority);
			current = p->PID;
			p->timeslots += 1;

			// adjust priority for long running process
			if(p->timeslots % 8 == 0)
				p->internal_priority = max(0, (p->internal_priority)-1); // decrease priority

		}
		else { /* nothing to schedule */ }
	} /* else { nothing to schedule } */

}

/** This function sets the priority and internal priority of
 * 	a given process based on the OS priority levels.
 *  	- p->priority is the priority of the process visible by the user
 *  	- p->internal_priority is the priority which the priority relevant to
 *		  the scheduler. This value can be decereased from time to time to make
 *		  sure we have no starvation. Also this value could be set differently than
 *		  p->priority, for example p->priority*2 etc.
 * @param p process to set priority
 * @param priority new priority set by user
 */
static void set_priority(process p, int priority) {
	p->priority = priority;
	p->internal_priority = priority;
}

/** Start a new process with the given PID and priority.
 * @param PID Process ID of the new process
 * @param priority of the new process
 */
void sch_start(int PID, int priority) {

	process p = malloc( sizeof(struct process_descriptor) );

	// setting up the new process struct
	p->PID = PID;
	set_priority(p, priority);
	p->timeslots = 0;
	p->requested_locks = 0x0;
	p->prev = NULL;
	p->next = NULL;

	// add to running process list
	list_put_front(p);
}

/** Signal to the scheduler that a process (indicated by PID) has exited.
 *   - remove process from running processes list
 *   - release locks which have not been released yet
 *   - set current to -1 since we have to find a new process now
 *   - free allocated ressources
 * @param PID process id of the finished process.
 */
void sch_exit (int PID) {
	process p = list_find_by_pid(PID);
	//DEBUG_PRINT("PROCESS pid:%d finished\n", p->PID);

	list_remove(p);
	release_all_locks(p);
	current = -1;
	free(p);
}

/** Renice process PID with the given priority.
 * @param PID process id
 * @param priority new value for process priority
 */
void sch_renice(int PID, int priority) {
	process p = list_find_by_pid(PID);
	set_priority(p, priority);
}

/** Process PID wants to lock resource res.
 *  - If resource is free acquire lock directly through global locktable
 *  - If not free, renice the currently locking thread to the priority of
 *    the locker to avoid starvation [priority inversion]
 * @param PID process ID
 * @param res to lock
 */
void sch_locked(int PID, int res) {
	process p = list_find_by_pid(PID);

	if(RESOURCE_IS_FREE(res)) {
		lock_resource(p, res);
	}
	else {
		//DEBUG_PRINT("Process PID: %d has to wait for ressource %d\n", p->PID, res);
		process locker = list_find_by_pid(locktable[res]);

		// since resource is not free the process owning the resource _has_ to be found through list_find_by_pid
		assert(locker != NULL);
		sch_renice(locker->PID, max(p->priority, locker->priority));

		wait_for_resource(p, res);
	}
}

/** Process PID releases the lock on resource res.
 * @param PID process ID
 * @param res to release
 */
void sch_unlocked(int PID, int res) {
	process p = list_find_by_pid(PID);

	// ensure that only PID which currently has the ressource can free it...
	assert(locktable[res] == p->PID);
	unlock_resource(res);
}


/** Tells the scheduler that the simulation is finished and that
 *  resources can be freed.
 */
void sch_finalize() {
	// Nothing to do here...
}
