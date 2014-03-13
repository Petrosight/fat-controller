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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include "extern.h"
#include "jobdispatching.h"
#include "sfmemlib.h"
#include "subprocslog.h"

pthread_mutex_t mutexSignal, mutexLog;
struct dispatching_settings *dp_settings;
slot **slots;
int handledSignal = -1;

/* Same as pipe but writes an error and halts execution on failure
 *
 */
static int pipe_safe(int pipefd[2])
{
    int pipe_err = pipe(pipefd);
    
    /* Check the pipe was created */
    if (pipe_err != 0)
    {
        _syslog(LOG_CRIT, "Error opening pipe: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    return pipe_err;
}

/* Connects the input of a pipe to a file descriptor and closes the output
 * (this is only required in the process which reads from the pipe).
 *
 */
static int connect_pipe_input(int pipefd[2], int input_fd)
{
    int fd;
    
    if ((fd = dup2(pipefd[1], input_fd)) == -1)
    {
        _syslog(LOG_CRIT, "Cannot duplicate stream file descriptor: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (close(pipefd[0]) != 0)
    {
        _syslog(LOG_CRIT, "Cannot close pipe file descriptor in child process: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    return fd;
}

/* Safely closes a file descriptor, logs a message to syslog and exits on error
 * 
 */
static void sfclose(const int fd, const char *error_message)
{
    if (close(fd) != 0)
    {
        _syslog(LOG_CRIT, "sfclose: %s : %s", error_message, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/**
 * Each thread will do this
 *
 */
void *task(void *i)
{
    /* Declarations - lots of comments due to poor variable names */
    
        /* Used in to unblock all signals in the sub-process before exec */
        sigset_t signalSet;
    
        /* Used for the thread id from the function param */
        long iid;
        char *tid;
        long *piid;
        
        /* Will contain the status of the sub-process when it ends */
        int stat_loc=0;
    
        /* Return value of waitpid */
        int wpid=0;
    
        int pipes=0, pipefd_stdout[2], pipefd_stderr[2];
        
        /* PID of the sub-process */
        pid_t pid;
        
        /* Variables only needed in the child process */
        int argc=0, argc_i, errsv;
        char **argv;
        
        int logger_id = -1;
    

    piid = (long *)i;
    iid = *piid;
    
    /* Say hello and show the thread number */
    _syslog(LOG_DEBUG, "Thread %ld: starting", iid);

    if (dp_settings->logfile != NULL)
    {
        pipes = pipe_safe(pipefd_stdout) + pipe_safe(pipefd_stderr);
    }

    /* Spawn a sub-process to run the program. */
    pid = fork();

    switch (pid)
    {
        case 0:
            setsid();
            sigfillset(&signalSet);
            pthread_sigmask(SIG_UNBLOCK, &signalSet, NULL );

            /* 25 = "--tid=" plus 19 character spaces for characters required to represent maximum long */
            tid = sfcalloc(25, sizeof(char));

            sprintf(tid, "--tid=%ld", iid);
        
            if (pipes == 0)
            {
                /* Pipe STDOUT of child to parent */
                connect_pipe_input(pipefd_stdout, STDOUT_FILENO);
                
                /* Pipe STDERR of child to parent */
                connect_pipe_input(pipefd_stderr, STDERR_FILENO);
            }
            
            /* We need to make an array containing the command plus all the individual arguments */
            argv = sfmalloc(sizeof(char *));
            argv[0] = sfmalloc((strlen(dp_settings->cmd) +1) * sizeof(char));
            strcpy(argv[0], dp_settings->cmd);
        
            /* Now add all the arguments */
            for (argc=0; argc<dp_settings->argc; argc++)
            {
                argv = sfrealloc(argv, (argc+2) *sizeof(char *));
                argv[(argc+1)] = sfmalloc(sizeof(char) * ( strlen(dp_settings->argv[argc])+1) );
                strcpy(argv[(argc+1)], dp_settings->argv[argc]);
            }

            if (dp_settings->append_thread_id == 1)
            {
                /* Add thread ID to args */
                argv = sfrealloc(argv, (argc+2) *sizeof(char *));
                argv[(argc+1)] = sfmalloc(sizeof(char) * (strlen(tid)+1) );
                strcpy(argv[(argc+1)], tid);
                
                argc++;
            }

            /* Null terminate the array */
            argv = sfrealloc(argv, (argc+2) *sizeof(char *));
            argv[(argc+1)] = sfmalloc(sizeof(NULL));
            argv[(argc+1)] = NULL;
            
            argc++;
            
            /* Close the logging so that the open file descriptor is closed in this child process */
            closelog();
        
            /* Replace this process */
            execv(argv[0],argv);

            _syslog(LOG_CRIT, "Failed to execute command %s   Error: [%d] %s", argv[0], errno, strerror(errno));
            
            /* Anything that executes after this point is an indication that execv failed, i.e. processes replacement failed */
            errsv = errno;
            
            _syslog(LOG_DEBUG, "EXECV: %s", strerror(errsv));
            _syslog(LOG_DEBUG, "Cmd: %s", argv[0]);

            /* Memory deassignment is purely academic as if we've got this far the we're heading to oblivion anyway */
            free(tid);
            
            for (argc_i=0; argc_i<argc; argc_i++)
            {
                free(argv[argc_i]);
            }
            free(argv);
            
            /* Close the pipe fd */
            if (close(pipefd_stdout[1]) != 0)
            {
                _syslog(LOG_CRIT, "Cannot close pipe file descriptor in child process.");
            }
            
            exit(EXIT_FAILURE); /* only if execv fails */
        case -1:
            /*printf("Thread %d: Fork failed\n", iid);*/
            _syslog(LOG_WARNING, "Thread %ld: Fork failed", iid);
            slots[iid]->status = -1 * (time(0) + dp_settings->sleepOnError);
            break;
        default:
            /*  parent process */
            slots[iid]->status = pid;

            if (pipes == 0)
            {
                sfclose(pipefd_stdout[1], "Cannot close STDOUT pipe input in parent process.");
                sfclose(pipefd_stderr[1], "Cannot close STDERR pipe input parent process.");

                /* Attach source ends of the pipe to the sub-process to the logging system */
                logger_id = subprocslog_append_source(pipefd_stdout[0], pipefd_stderr[0]);
                
                if (logger_id == RV_FAIL)
                {
                    _syslog(LOG_WARNING, "Could not append sub-process' pipes to logging system.");
                    
                    sfclose(pipefd_stdout[0], "Cannot close STDOUT pipe output in parent process.");
                    sfclose(pipefd_stderr[0], "Cannot close STDERR pipe output parent process.");
                }
            }
            
            do
            {
                wpid = waitpid(pid, &stat_loc, WUNTRACED
#ifdef WCONTINUED       /* Not all implementations support this */
                | WCONTINUED
#endif
                );
                
                if (wpid == -1)
                {
                    char buf[256];
                    strerror_r(errno, buf, 256);
                    _syslog(LOG_CRIT, "waitpid failed - errno:%d(%s)", errno, buf);
                    exit(EXIT_FAILURE);
                }


                if (WIFEXITED(stat_loc))
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Exited", iid);
                }
                else if (WIFSIGNALED(stat_loc))
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Killed with signal %d", iid, WTERMSIG(stat_loc));
                } 
                else if (WIFSTOPPED(stat_loc))
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Stopped (signal %d)", iid, WSTOPSIG(stat_loc));
#ifdef WIFCONTINUED     /* Not all implementations support this */
                }
                else if (WIFCONTINUED(stat_loc))
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Continued", iid);
#endif
                }
                else
                {    /* Non-standard case -- may never happen */
                    _syslog(LOG_DEBUG, "Thread %ld: Unexpected status (0x%x)", iid, stat_loc);
                }
            } while (!WIFEXITED(stat_loc) && !WIFSIGNALED(stat_loc));            
            
            
            _syslog(LOG_DEBUG, "Thread %ld: Child finished with exit code: %d", iid, WEXITSTATUS(stat_loc));

            if (pipes == 0 && logger_id != RV_FAIL)
            {
                if (subprocslog_remove_source(logger_id) != RV_OK)
                {
                    _syslog(LOG_WARNING, "Could not remove sub-process pipes from logging system. Source id: %d, fd_stdout: %d, fd_stderr: %d", logger_id, pipefd_stdout[0], pipefd_stderr[0]);
                    
                    sfclose(pipefd_stdout[0], "Cannot close STDOUT pipe output in parent process.");
                    sfclose(pipefd_stderr[0], "Cannot close STDERR pipe output in parent process.");
                }
            }
            else
            {
                _syslog(LOG_DEBUG, "Not removing source. Source id: %d, fd_stdout: %d, fd_stderr: %d", logger_id, pipefd_stdout[0], pipefd_stderr[0]);
            }
            
            /*
                if independent thread model
                    if exit status = EXIT_STATUS_OK_MORE
                        set THREAD_STATUS_AVAILABLE so thread can be restarted
                    else if is EXIT_STATUS_OK
                        set to sleep
                    else
                        set to sleepOnError
                else
                    if exit_status = EXIT_STATUS_OK_MORE
                        set to THREAD_STATUS_DONE_MORE
                    else if is EXIT_STATUS_OK
                        set to THREAD_STATUS_DONE_OK   (thread handler will then sleep)
                    else
                        set to THREAD_STATUS_DONE_FAIL (thread handler will then sleepOnError)
            */
            
            /*
                Log messages should perhaps be moved to the thread check and control logic (below)
                In Fixed interval mode, the messages here from dependent mode do not make sense
            */
            
            if (dp_settings->threadModel == THREAD_MODEL_INDEPENDENT)
            {
                if (WEXITSTATUS(stat_loc) == EXIT_STATUS_OK_MORE)
                {
                    _syslog(LOG_DEBUG, "Thread %ld: EXIT_STATUS_OK_MORE Returning to pool", iid);
                    slots[iid]->status = THREAD_STATUS_AVAILABLE;
                }
                else if (WEXITSTATUS(stat_loc) == EXIT_STATUS_OK)
                {
                    _syslog(LOG_DEBUG, "Thread %ld: EXIT_STATUS_OK Putting thread to sleep", iid);
                    slots[iid]->status = -1 * (time(0) + dp_settings->sleep);
                }
                else
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Exit Fail Putting thread to sleepOnError", iid);
                    slots[iid]->status = -1 * (time(0) + dp_settings->sleepOnError);
                }
            }
            else
            {
                if (WEXITSTATUS(stat_loc) == EXIT_STATUS_OK_MORE)
                {
                    _syslog(LOG_DEBUG, "Thread %ld: EXIT_STATUS_OK_MORE Returning to pool", iid);
                    slots[iid]->status = THREAD_STATUS_DONE_MORE;
                }
                else if (WEXITSTATUS(stat_loc) == EXIT_STATUS_OK)
                {
                    _syslog(LOG_DEBUG, "Thread %ld: EXIT_STATUS_OK Going to sleep", iid);
                    slots[iid]->status = THREAD_STATUS_DONE_OK;
                }
                else
                {
                    _syslog(LOG_DEBUG, "Thread %ld: Exit Fail Going to sleepOnError", iid);
                    slots[iid]->status = THREAD_STATUS_DONE_FAIL;
                }
            }
    }

    /* Re-initialise the slot struct ready for the next job */
    slot_reset(slots[iid]);
    
    /*printf("Thread %d: Finished\n", iid);*/
    _syslog(LOG_DEBUG, "Thread %ld: Finished", iid);

    /* Terminate the thread */
    pthread_exit((void*) i);
}

void thread_proc_term(slot *slot)
{
    if (slot->termination_requested == 0)
    {
        if (slot->status > 0)
        {
            int kv = kill(slot->status, SIGTERM);
            
            slot->termination_requested = time(NULL);
            
            _syslog(LOG_INFO, "Terminating %d.   Signal sent: %s", slot->status, kv ==0?"ok":"fail");
        }
        else
        {
            _syslog(LOG_WARNING, "Attempted termination of thread that is not running.");
        }
    }
    else
    {
        _syslog(LOG_WARNING, "Attempted termination of thread that is already pending termination");
    }
}

void thread_proc_kill(slot *slot)
{
    int kv = kill(slot->status, SIGKILL);
    
    _syslog(LOG_INFO, "Killed %d.   Signal sent: %s", slot->status, kv ==0?"ok":"fail");
}

void thread_proc_term_all()
{
    int i;

    for (i=0; i<dp_settings->threads;i++)
    {
        /*
            Would be good to do a check to make sure we're not killing something important
            For now we'll just make sure we're not going to kill init
        */
        if (slots[i]->status > 1)
        {
            thread_proc_term(slots[i]);
        }
    }
}

void check_thread(slot *slot)
{
    /* Check if running */
    if (slot->status > 0)
    {
        /* Check if termination requested */
        if (slot->termination_requested != 0)
        {
            /* Check for termination timeout */
            if (time(NULL) >= (slot->termination_requested + dp_settings->termination_timeout))
            {
                _syslog(LOG_WARNING, "Thread %d has not shutdown %ds after being issued SIGTERM", (int) *slot->id, dp_settings->termination_timeout);
                /* Zap! */
                thread_proc_kill(slot);
            }
        }
        else
        {
            /* Determine how long thread has run */
            int duration = time(NULL) - slot->last_started_at;
            
            /* Check duration: error */
            if (dp_settings->thread_run_time_max > 0
             && duration > dp_settings->thread_run_time_max)
            {
                _syslog(LOG_WARNING, "Thread %d has been running more than %ds.   Sending SIGTERM.", (int) *slot->id, dp_settings->thread_run_time_max);
                thread_proc_term(slot);
            } /* Check duration: warning */
            else if (dp_settings->thread_run_time_warn > 0
                  && duration > dp_settings->thread_run_time_warn
                  && slot->duration_warning_issued == 0)
            {
                _syslog(LOG_WARNING, "Thread %d has been running more than %ds", (int) *slot->id, dp_settings->thread_run_time_warn);
                slot->duration_warning_issued = 1;
            }
        }
    }
}

/**
 * Resets any variables that are specific to the runtime of the thread, ready
 * for being run again.
 */
void slot_reset(slot *slot)
{
    slot->termination_requested = 0;
    slot->duration_warning_issued = 0;
}

static void waitForThreads(int logging_enabled)
{
    int i, stoppedThreads, keepWaiting, stopSignals=0;
    
    keepWaiting = 1;

    /*printf("Waiting for threads to finish\n");*/
    _syslog(LOG_DEBUG, "Waiting for threads to finish");

    while(keepWaiting)
    {
        stoppedThreads = 0;
        
        for (i=0; i<dp_settings->threads;i++)
        {
            /* Check for long-running threads */
            check_thread(slots[i]);
            
            /* Check if thread has finished or is sleeping */
            if (slots[i]->status == 0 || slots[i]->status < -1)
            {
                stoppedThreads++;
            }
        }

        if (stoppedThreads == dp_settings->threads)
        {
            break;
        }

        /* Check for signals */
        pthread_mutex_lock(&mutexSignal);

        switch ( handledSignal )
        {
            case -1:
                break;

            case 0:
                /* The case for signals we're not interested in */
                handledSignal = -1;
                break;

            case SIGTERM:
            case SIGQUIT:
            case SIGINT:
                
                _syslog(LOG_DEBUG, "WaitForThreads: signal received: %d", handledSignal);
            
                if (stopSignals == 0)
                {
                    /* If this is the first extra stop signal after the initial signal was received 
                       to stop creating new instances and shutdown The Fat Controller, then nicely
                       ask all instances to terminate by sending them SIGTERM */
                    thread_proc_term_all();
                }
                else
                {
                    /* If this is the second extra stop signal then the user is obviously in a hurry
                       so we will stop the main Fat Controller process which will in turn terminate
                       all other processes */
                    keepWaiting = 0;
                }
                
                stopSignals++;
                
                handledSignal = -1;
                break;
        }

        pthread_mutex_unlock(&mutexSignal);

        /* Write logs */
        if (logging_enabled == 1)
        {
            if (subprocslog_write_buffers() != RV_OK)
            {
                logging_enabled = 0;
            }
        }
        
        usleep(200000);
    }
}

void* signalHandler()
{
    sigset_t signal_set;
    int sig;

    while (1)
    {
        /* Wait for any and all signals */
        sigfillset(&signal_set);

        sigwait(&signal_set, &sig);

        /* When we get this far, we've caught a signal */
        _syslog(LOG_DEBUG, "Main signal handler, caught signal: %d", sig);

        switch(sig)
        {
            /* SIGQUIT */
            case SIGTERM:
            case SIGQUIT:
                pthread_mutex_lock(&mutexSignal);
                handledSignal = SIGQUIT;
                pthread_mutex_unlock(&mutexSignal);
                break;

            /* SIGINT */
            case SIGINT:
                pthread_mutex_lock(&mutexSignal);
                handledSignal = SIGINT;
                pthread_mutex_unlock(&mutexSignal);
                break;

            /* SIGHUP */
            case SIGHUP:
                pthread_mutex_lock(&mutexSignal);
                handledSignal = SIGHUP;
                pthread_mutex_unlock(&mutexSignal);
                break;

            /* other signals 
            default:
                pthread_mutex_lock(&mutexSignal);
                handledSignal = 0;
                pthread_mutex_unlock(&mutexSignal);
                break;*/
        }
    }
    
    return (void*)0;
}

/**
 * Main
 *
 */
void dispatch(struct dispatching_settings *settings, int daemon)
{
    int rc, i;
    int running = 1;
    size_t stacksize;
    sigset_t signalSet;
    pthread_t threadSignalHandler;
    pthread_attr_t attr;
    int (*threadModel)(slot *slot, int daemon, int *running, void *state) = NULL;
    void (*pre_state_check)(void *state) = NULL;
    void (*post_state_check)(void *state) = NULL;
    void *state = NULL;
    
    /* If any calls to subprocslog_write_buffers fail then this is set to 0 and
       no more calls are made and The Fat Controller is shut down.
    */
    int logging_enabled = 1;
    
    /* Allocate space on the heap for thread slots */
    slots = sfmalloc(settings->threads*sizeof( slot *));
    
    dp_settings = settings;

    /* Initialise the signal mutex lock */
    pthread_mutex_init(&mutexSignal, NULL);

    /* Initialise and set thread attributes */
    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Ensure each thread has a given stack size - portability */
    pthread_attr_getstacksize(&attr, &stacksize);
    /*printf("Default stack size = %li\n", (long) stacksize);*/
    _syslog(LOG_DEBUG, "Default stack size = %li", (long) stacksize);

        /* Determine and set a new stack size */
        stacksize = sizeof(sigset_t) + THREAD_STACK_HEADROOM;
        /*printf("Amount of stack calculated per thread = %li\n", (long) stacksize);*/
        _syslog(LOG_DEBUG, "Amount of stack calculated per thread = %li", (long) stacksize);
        pthread_attr_setstacksize(&attr, stacksize);

    /* block all signals */
    sigfillset(&signalSet);
    pthread_sigmask(SIG_BLOCK, &signalSet, NULL );

    /* create the signal handling thread */
    pthread_create(&threadSignalHandler, NULL, signalHandler, NULL);

    /* Determine thread model */
    if (settings->threadModel == THREAD_MODEL_INDEPENDENT)
    {
        threadModel = &independentThreadModel;
        pre_state_check = &presc_independent_model;
        post_state_check = &postsc_independent_model;
        
        state = sfmalloc(sizeof(independent_mode_state));
        init_state_independent_model(state);
    }
    else if (settings->threadModel == THREAD_MODEL_DEPENDENT)
    {
        threadModel = &dependentThreadModel;
        pre_state_check = &presc_dependent_model;
        post_state_check = &postsc_dependent_model;
        
        /* At the moment this thread mode doesn't have state so this is a bit
           useless but it's here for completeness
        */
        init_state_dependent_model(state);
    }
    else if (settings->threadModel == THREAD_MODEL_FIXED_INTERVAL)
    {
        threadModel = &fixedIntervalThreadModel;
        pre_state_check = &presc_fixed_interval;
        post_state_check = &postsc_fixed_interval;
        
        state = sfmalloc(sizeof(fixed_interval_state));
        init_state_fixed_interval(state);
    }
    
    /* Allocate heap space and initialise slots */
    for (i=0; i<settings->threads;i++)
    {
        slots[i] = sfmalloc(sizeof(slot));
        
        slots[i]->id = sfmalloc(sizeof(long));
        *slots[i]->id = i;
        slots[i]->status = THREAD_STATUS_AVAILABLE;
        slots[i]->thread = sfcalloc(1, sizeof(pthread_t));
        slots[i]->last_started_at = 0;
        
        slot_reset(slots[i]);
    }
    
    /* If we're to run only once, then we must turn off repeated running */
    running = (settings->run_once > 0) ? 0 : 1;
    
    /* Initialise the sub-process logging system */
    /* (if there's no log file specified for stderr then use the stdout log) */
    if (subprocslog_initialize(settings->logfile, 
                               settings->errlogfile == NULL ? settings->logfile 
                                                            : settings->errlogfile) == RV_OK)
    {
        while (running > 0 || settings->run_once-- > 0)
        {
            (*pre_state_check)(state);
                    /* Scan through the thread slots */
            for (i=0; i<settings->threads;i++)
            {
                /* Flush output buffer */
                fflush(stdout);
                
                /* Check for long-running threads */
                check_thread(slots[i]);
                
                if ((*threadModel)(slots[i], daemon, &running, state) == 1)
                {
                    /* Create a new thread */
                    
                    /* Set this slot as used, -1 is a temporary value before being replaced by the PID of the forked process (prevents race hazards) */
                    slots[i]->status = THREAD_STATUS_BOOTSTRAPPING;

                    /*printf("Main: creating thread %d\n", i);*/
                    _syslog(LOG_DEBUG, "Main: creating thread %d", i);
                    
                    rc = pthread_create((slots[i]->thread), &attr, task, (void *) slots[i]->id );

                    if (rc)
                    {
                        printf("ERROR: return code from pthread_create() is %d\n", rc);
                        _syslog(LOG_CRIT, "ERROR: return code from pthread_create() is %d", rc);
                        exit(EXIT_FAILURE);
                    }
                    
                    pthread_detach(*(slots[i]->thread));
                    
                    slots[i]->last_started_at = time(0);
                }
            }
            

            pthread_mutex_lock(&mutexSignal);

            switch ( handledSignal )
            {
                case SIGTERM:
                case SIGQUIT:
                case SIGINT:
                    _syslog(LOG_DEBUG, "Main: shutdown signal: %d", handledSignal);
                    handledSignal = -1;
                    running = -1;
                    thread_proc_term_all();
                    break;
                
                case SIGHUP:
                    _syslog(LOG_DEBUG, "Main: SIGHUP");
                    handledSignal = -1;
                    /*thread_proc_term_all();*/
                    subprocslog_reinitialize();
                    break;
            }

            pthread_mutex_unlock(&mutexSignal);
            
            (*post_state_check)(state);
            
            /* Write any unwritten data collected from the stdout and stderr of sub processes */
            if (logging_enabled == 1)
            {
                if (subprocslog_write_buffers() != RV_OK)
                {
                    logging_enabled = 0;
                }
            }

            /* Sleep for a bit - all this thread creation is hard work! */
            usleep(200000);
        }
        
        /* Wait for all threads to end */
        waitForThreads(logging_enabled);
        
        /* Shutdown the sub-process logging system */
        subprocslog_deinitialize();
    }
    
    /* Free allocated memory */
    
    pthread_attr_destroy(&attr);
    
    free(state);
    
    for (i=0; i<settings->threads;i++)
    {
        free(slots[i]->id);
        
        free(slots[i]->thread);
        
        free(slots[i]);
    }
    
    free(slots);
    
    _syslog(LOG_DEBUG, "Bye");
}

int independentThreadModel(slot *slot, int daemon, int *running, void *state_vp)
{
    /*
        TODO:   The thread should simply return the result of the child process, 
        i.e. ok, ok+more, or fail.   This function should, based on that result
        decide what to do, i.e. mark thread as available or put it to sleep.
    */
    
    independent_mode_state *state = (independent_mode_state *) state_vp;
    
    if (slot->status == THREAD_STATUS_AVAILABLE)  /* Thread is currently not doing anything, so let's give it something to do */
    {
        /* This slot is spare */
        return 1;
    }
    else if (slot->status < THREAD_STATUS_BOOTSTRAPPING)     /* Thread is sleeping, see if we should wake it up */
    {
        if ( daemon == 0 )
        {
            /* Application mode */
            state->unavailable_slots_count++;
            
            if ( state->unavailable_slots_count == dp_settings->threads )
            {
                _syslog(LOG_DEBUG, "All threads finished.");
                *running = 0;
            }
        }
        else
        {
            /* Daemon mode */
            if (time(0) > (-1 * slot->status) )
            {
                /* Thread has slept long enough so let's wake it up */
                slot->status = THREAD_STATUS_AVAILABLE;
            }
        }
    }
    
    return 0;
}

int dependentThreadModel(slot *slot, int daemon, int *running, void *state)
{
    static int noThreads = 1;
    static int sleepUntil = 0;
    
    UNUSED(state);
    
    /*
    If running as an application (i.e. not as a daemon) then if a thread
    returns anything but THREAD_STATUS_DONE_MORE then it sets running=0
    which stops any new threads from being created.
    Note that when running is set to 0, noThreads is also set  1, so that
    there is no need to check running here if a thread is 
    THREAD_STATUS_AVAILABLE
    */
    
    if (slot->status == THREAD_STATUS_AVAILABLE)
    {
        if (*slot->id<noThreads && time(0) > sleepUntil)
        {
            return 1;
        }
    }
    else
    {
        if (slot->status == THREAD_STATUS_DONE_MORE)
        {
            /* Child returned ok+more */
            noThreads += (noThreads + 1) > dp_settings->threads ? 0 : 1;
            
            slot->status = THREAD_STATUS_AVAILABLE;
        }
        else if (slot->status == THREAD_STATUS_DONE_FAIL)
        {
            /* Child returned fail */
            noThreads = 1;
            
            sleepUntil = time(0) + dp_settings->sleepOnError;
            
            if ( daemon == 0 )
            {
                *running = 0;
            }
            
            slot->status = THREAD_STATUS_AVAILABLE;
        }
        else if (slot->status == THREAD_STATUS_DONE_OK)
        {
            /* Child returned ok+nomore */
            noThreads = 1;
            sleepUntil = time(0) + dp_settings->sleep;
            
            if ( daemon == 0 )
            {
                *running = 0;
            }
            
            slot->status = THREAD_STATUS_AVAILABLE;
        }
        /* If no condition matches then the thread is either still running or unavailable */
    }
    
    return 0;
}

int fixedIntervalThreadModel(slot *slot, int daemon, int *running, void *state_vp)
{
    fixed_interval_state *state = (fixed_interval_state *) state_vp;
    
    UNUSED(daemon);
    UNUSED(running);
    
    if (slot->status == THREAD_STATUS_DONE_OK
     || slot->status == THREAD_STATUS_DONE_MORE)
    {
        slot->status = THREAD_STATUS_AVAILABLE;
    }
    else if (slot->status == THREAD_STATUS_DONE_FAIL)
    {
        state->is_sleeping = 1;
        state->sleep_until = time(0) + dp_settings->sleepOnError;
        
        slot->status = THREAD_STATUS_AVAILABLE;
    }
    
    /* If thread is available */
    if (slot->status == THREAD_STATUS_AVAILABLE)
    {
        /* If thread required */
        if (state->new_thread_required == 1
         && state->is_sleeping == 0)
        {
            /* Set new thread not required */
            state->new_thread_required = 0;
            
            state->wait_until = 0;
            
            state->last_run_slot = slot;
            
            /* Create thread */
            return 1;
        }
    }
    else
    {
        /* If no condition matches then the thread is either still running or 
           unavailable, so check if it is the longest running */
        if (state->longest_running_slot == NULL
         || state->longest_running_slot->status < 1
         || state->longest_running_slot->last_started_at > slot->last_started_at)
        {
            state->longest_running_slot = slot;
        }
    }
    
    return 0;
}

/* State initialisers */

void init_state_independent_model(void *state_vp)
{
    independent_mode_state *state = (independent_mode_state *) state_vp;

    state->unavailable_slots_count = 0;
}

void init_state_dependent_model(void *state){ UNUSED(state); }

void init_state_fixed_interval(void *state_vp)
{
    fixed_interval_state *state = (fixed_interval_state *) state_vp;
    
    state->new_thread_required = 0;
    state->longest_running_slot = NULL;
    state->last_run_slot = NULL;
    state->is_sleeping = 0;
    state->sleep_until = 0;
    state->wait_until = 0;
}


/* Pre and post state handlers */

void presc_independent_model(void *state_vp)
{
    independent_mode_state *state = (independent_mode_state *) state_vp;

    state->unavailable_slots_count = 0;
}

void postsc_independent_model(void *state){ UNUSED(state); }
void presc_dependent_model(void *state){ UNUSED(state); }
void postsc_dependent_model(void *state){ UNUSED(state); }

void presc_fixed_interval(void *state_vp)
{
    fixed_interval_state *state = (fixed_interval_state *) state_vp;
    
    /* If thread creation is sleeping, see if we should wake it up */
    if (state->is_sleeping == 1)
    {
        if (state->sleep_until <= time(0))
        {
            state->is_sleeping = 0;
        }
    }
    
    /* Determine if a new thread is required */
    if (state->new_thread_required == 0)
    {
        /* If state has no value for last run slot (i.e. we haven't completed the first cycle yet) */
        if (state->last_run_slot == NULL)
        {
            state->new_thread_required = 1;
        }
        else
        {
            /* If there is a value, check if it was longer than now-sleep */
            if ((time(0) - dp_settings->sleep) >= state->last_run_slot->last_started_at)
            {
                state->new_thread_required = 1;
            }
        }
    }
}


void postsc_fixed_interval(void *state_vp)
{
    fixed_interval_state *state = (fixed_interval_state *) state_vp;
    
    /* Determine if we need to terminate a thread */
    
    /* If state indicates that a thread is required */
    if (state->new_thread_required == 1
     && state->is_sleeping == 0 )
    {
        if (dp_settings->fi_wait_time_max == -1)
        {
            /* If wait is -1, then skip termination */
            
            /*
                wait_until is set to -1 to indicate indefinite waiting.
                Currently this is only used to prevent the LOG below being repeated.
            */
            
            if (state->wait_until == 0)
            {
                _syslog(LOG_DEBUG, "No free thread available - started waiting indefinitely.");
            }
            
            state->wait_until = FI_WAIT_INDEFINITELY;
        }
        else if (dp_settings->fi_wait_time_max > 0
              && state->wait_until == 0)
        {
            /* If wait is >0 and we are not currently waiting, then start waiting */
            state->wait_until = time(0) + dp_settings->fi_wait_time_max;
            
            _syslog(LOG_DEBUG, "No free thread available - started waiting.");
        }
        else if (dp_settings->fi_wait_time_max ==0
                 || (state->wait_until > 0 
                     && state->wait_until <= time(0)))
        {
            /* If wait is zero or has timed out, then proceed to terminate the longest running thread proc */
            
            /* If we have not already requested the oldest thread to be terminated */
            if (state->longest_running_slot != NULL)
            {
                if (state->longest_running_slot->termination_requested == 0)
                {
                    _syslog(LOG_DEBUG, "Terminating oldest thread: %d, running %ds", state->longest_running_slot->status, (int) (time(0)-state->longest_running_slot->last_started_at));
                    
                    /* Request to terminate the oldest thread (longest running) */
                    /* This will be acted upon by the check_thread function */
                    thread_proc_term(state->longest_running_slot);
                }
            }
            else
            {
                /* Erroneous situation: new thread required but no longest running slot */
                _syslog(LOG_ERR, "postsc_fixed_interval: new thread required but no longest running slot defined");
            }
        }
    }
}
