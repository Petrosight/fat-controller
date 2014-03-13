/*
 *  FatController - Parallel execution handler
 *  Copyright (C) 2010 Nicholas Giles    http://www.4pmp.com
 *  
 *  This file is part of FatController.
 *
 *  FatController is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  FatController is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with FatController.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef JOBDISPATCHING_H
#define JOBDISPATCHING_H

#define DEFAULT_NO_THREADS 1
#define DEFAULT_SLEEP 30
#define DEFAULT_SLEEP_ON_ERROR 300
#define THREAD_STACK_HEADROOM 1048576
#define DEFAULT_THREAD_RUN_TIME_WARN 3600
#define DEFAULT_THREAD_RUN_TIME_MAX 0
#define DEFAULT_TERMINATION_TIMEOUT 30
#define DEFAULT_FI_WAIT_TIME_MAX -1

#define THREAD_MODEL_INDEPENDENT 1
#define THREAD_MODEL_DEPENDENT 2
#define THREAD_MODEL_FIXED_INTERVAL 3

/* Thread status values */
#define THREAD_STATUS_AVAILABLE 0
#define THREAD_STATUS_BOOTSTRAPPING -1
#define THREAD_STATUS_DONE_MORE -2
#define THREAD_STATUS_DONE_OK -3
#define THREAD_STATUS_DONE_FAIL -4
#define THREAD_STATUS_UNAVAILABLE -5

#define FI_WAIT_INDEFINITELY -1

#define UNUSED(expr) (void)(expr);

/* 
    Thread status values:
    Note that >0 is an active thread and the value denotes the PID of the child process
    and <-4 denotes a sleeping thread, negative timestamp when will finish sleeping.
*/

/* Define exit statuses */
#define EXIT_STATUS_OK 0
#define EXIT_STATUS_FAIL 255
#define EXIT_STATUS_OK_MORE 64


struct dispatching_settings
{
    char *logfile;
    char *logformat;
    char *errlogfile;
    int sleep;
    int sleepOnError;
    char *path;
    char *cmd;
    char **argv;
    int argc;
    int threads;
    int threadModel;
    int thread_run_time_warn;
    int thread_run_time_max;
    int termination_timeout;
    int fi_wait_time_max;
    int append_thread_id;
    int run_once;
};


/* Do we need all this in the header file? */

typedef struct
{
    long *id;
    int status;
    pthread_t *thread;
    time_t last_started_at;
    int termination_requested;
    int duration_warning_issued;
} slot;

typedef struct
{
    /* Specifies weather enough time has elapsed to create a new thread */
    int new_thread_required;
    
    /* Slot which is currently running the longest */
    slot *longest_running_slot;
    
    /* Slot which was the last one to be (re)started */
    slot *last_run_slot;
    
    /* Specifies if thread creation should be halted */
    int is_sleeping;
    
    /* Timestamp to sleep (halt thread creation) until (used if a process fails) */
    int sleep_until;
    
    /* Timestamp to wait until before terminating the longest running thread proc */
    int wait_until;
    
} fixed_interval_state;

typedef struct
{
    /* Number of unavailable slots */
    int unavailable_slots_count;
    
} independent_mode_state;

void logPipe(int pipefd);
void* signalHandler();
void slot_reset(slot *slotp);
void thread_proc_term(slot *slot);
void thread_proc_kill(slot *slot);
void thread_proc_term_all();
void check_thread(slot *slot);
void *task(void *i);
void dispatch(struct dispatching_settings *settings, int daemon);

int independentThreadModel(slot *slot, int daemon, int *running, void *state);
int dependentThreadModel(slot *slot, int daemon, int *running, void *state);
int fixedIntervalThreadModel(slot *slot, int daemon, int *running, void *state);

void init_state_independent_model(void *state);
void init_state_dependent_model(void *state);
void init_state_fixed_interval(void *state);

void presc_independent_model(void *state);
void postsc_independent_model(void *state);
void presc_dependent_model(void *state);
void postsc_dependent_model(void *state);
void presc_fixed_interval(void *state);
void postsc_fixed_interval(void *state);

#endif
