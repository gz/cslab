/*
  (c) ETH Zurich 2006-2007
  Matteo Corti <matteo.corti@id.ethz.ch

*/

/* PID of the currently scheduled process */
int current;

/* schedule the next process */
void sch_schedule();

/* start a new process with the given PID and priority */
void sch_start(int PID, int priority);

/* signal to the scheduler that a process (indicated by PID) has exited */
void sch_exit (int PID);

/* renice process PID with the given priority */
void sch_renice(int PID, int priority);

/* process PID wants to lock resource res */
void sch_locked  (int PID, int res);

/* process PID releases the lock on resource res */
void sch_unlocked(int PID, int res);

/* tells the scheduler that the simulation is finished and that
   resources can be freed
*/
void sch_finalize();
