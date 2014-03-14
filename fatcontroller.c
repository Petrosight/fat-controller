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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include "daemonise.h"
#include "jobdispatching.h"
#include "fatcontroller.h"
#include "dgetopts.h"
#include "sfmemlib.h"

static const char *LOG_FORMAT = NULL;

    void _syslog(int facility_priority, const char *format, ...)
    {
        char message[2048];
        int e;
        va_list va;

        va_start(va, format);

        e = vsprintf(message, format, va);
        syslog(facility_priority, LOG_FORMAT, message);
        va_end(va);
    }

    void setupSyslog(int debug, char *name, int daemonise, char *format)
    {
        if (debug == 1)
        {
            /* Debug logging */
            setlogmask(LOG_UPTO(LOG_DEBUG));
            
            /* LOG_PERROR - errors are sent to stderr as well */
            openlog(name, LOG_PID | LOG_CONS, daemonise ? LOG_DAEMON : LOG_USER);
        }
        else
        {
            /* Warning and below logging */
            setlogmask(LOG_UPTO(LOG_INFO));
            
            /* LOG_PERROR - errors are sent to stderr as well */
            openlog(name, LOG_PID, daemonise ? LOG_DAEMON : LOG_USER);
        }
        if (LOG_FORMAT == NULL) {
            LOG_FORMAT = format;
        }
    }

    void showStartupOptions(struct application_settings *ap_settings, struct daemon_settings *dm_settings, struct dispatching_settings *dp_settings)
    {
        int i=0;
        
        if (!ap_settings->debug)
        {
            return;
        }
        
        /* Application settings */
        printf("Application settings:\n");
        printf("Debug: %d\n", ap_settings->debug);
        printf("Daemonise: %d\n", ap_settings->daemonise);
        printf("Test fire: %d\n", ap_settings->test_fire);
        
        /* Daemoniser settings */
        printf("\nDaemon settings:\n");
        printf("Rundir: %s\n", dm_settings->rundir);
        printf("pid: %s\n", dm_settings->pidfile);
        printf("name: %s\n", dm_settings->name);
        
        /* Dispatcher settings */
        printf("\nDispatcher settings:\n");
        printf("Logfile: %s\n", dp_settings->logfile);
        printf("Error logfile: %s\n", dp_settings->errlogfile);
        printf("Sleep: %d\n", dp_settings->sleep);
        printf("SleepOnError: %d\n", dp_settings->sleepOnError);
        printf("Threads: %d\n", dp_settings->threads);
        printf("Run once: %d\n", dp_settings->run_once);
        printf("Cmd: %s\n", dp_settings->cmd);
        
        switch (dp_settings->threadModel)
        {
            case THREAD_MODEL_INDEPENDENT:
                printf("Thread model: INDEPENDENT\n");
                break;
            case THREAD_MODEL_DEPENDENT:
                printf("Thread model: DEPENDENT\n");
                break;
            case THREAD_MODEL_FIXED_INTERVAL:
                 printf("Thread model: FIXED INTERVAL\n");
                break;
            default:
                 printf("Thread model: Unknown (error)\n");
        }
            
        printf("Run time warn:: %d\n", dp_settings->thread_run_time_warn);
        printf("Run time max: %d\n", dp_settings->thread_run_time_max);
        printf("Termination timeout: %d\n", dp_settings->termination_timeout);
        printf("Maximum FI wait time: %d\n", dp_settings->fi_wait_time_max);
        printf("Append thread ID: %s\n", dp_settings->append_thread_id == 1 ? "YES" : "NO");
        
        printf("argc: %d\n", dp_settings->argc);
        
        for (i=0; i<dp_settings->argc; i++)
        {
            printf("argv[%d]: %s\n", i, dp_settings->argv[i]);
        }
    }
    
    int main(int argc, char **argv)
    {
        struct application_settings *ap_settings;
        struct daemon_settings *dm_settings;
        struct dispatching_settings *dp_settings;
        int fargc, err=1, daemonise_return_value=-2;
        
        /* Assign heap space */
        ap_settings = sfmalloc(sizeof (struct application_settings));
        dm_settings = sfmalloc(sizeof (struct daemon_settings));
        dp_settings = sfmalloc(sizeof (struct dispatching_settings));
        
        /* Initialise default values */
        ap_settings->debug = 0;
        ap_settings->daemonise = 0;
        ap_settings->test_fire = 0;
        
        dm_settings->name = sfmalloc(sizeof(char) * (strlen(DEFAULT_DAEMON_NAME)+1));
        strcpy(dm_settings->name, DEFAULT_DAEMON_NAME);
        dm_settings->rundir = NULL;
        dm_settings->pidfile = NULL;
        dm_settings->pidfd = -1;
        
        dp_settings->sleep = DEFAULT_SLEEP;
        dp_settings->sleepOnError = DEFAULT_SLEEP_ON_ERROR;
        dp_settings->threads = DEFAULT_NO_THREADS;
        dp_settings->logfile = NULL;
        dp_settings->errlogfile = NULL;
        dp_settings->path = NULL;
        dp_settings->cmd = NULL;
        dp_settings->argc = 0;
        dp_settings->argv = NULL;
        dp_settings->threadModel = THREAD_MODEL_DEPENDENT;
        dp_settings->thread_run_time_warn = DEFAULT_THREAD_RUN_TIME_WARN;
        dp_settings->thread_run_time_max = DEFAULT_THREAD_RUN_TIME_MAX;
        dp_settings->termination_timeout = DEFAULT_TERMINATION_TIMEOUT;
        dp_settings->fi_wait_time_max = DEFAULT_FI_WAIT_TIME_MAX;
        dp_settings->append_thread_id = 0;
        dp_settings->run_once = 0;

        dp_settings->logformat = sfmalloc(sizeof(char) * (strlen(DEFAULT_LOG_FORMAT)+1));;
        strcpy(dp_settings->logformat, DEFAULT_LOG_FORMAT);

        /* Process options - get settings from arguments */
        if (processOptions(argc, argv, ap_settings, dm_settings, dp_settings) == 0)
        {
            /* Show startup options (if in debug mode */
            //if (ap_settings->debug) showStartupOptions(ap_settings, dm_settings, dp_settings);
            
            /* Setup system logging */
            setupSyslog(
                    ap_settings->debug,
                    ap_settings->daemonise ? dm_settings->name : APPLICATION_NAME,
                    ap_settings->daemonise,
                    dp_settings->logformat
            );

            /* Only run if not in test_fire mode */
            if (ap_settings->test_fire == 0)
            {
                /* If in daemon mode: start the daemon - fork a new process, detatch from foreground and end parent process */
                if (ap_settings->daemonise)
                {
                    daemonise_return_value = daemonStart(dm_settings);
                    
                    if (daemonise_return_value == 0)
                    {
                        /* Daemon started ok (child process) */
                        err = 0;
                        
                        /* Start the job dispatcher - this is the main part of the application */
                        dispatch(dp_settings, ap_settings->daemonise);
                    
                        daemonShutdown(dm_settings);
                    }
                    else if (daemonise_return_value > 0)
                    {
                        /* Daemon started ok (parent process) */
                        err = 0;
                    }
                    else
                    {
                        /* Daemon not started ok (parent process) */
                    }
                }
                else
                {
                    /* Start the job dispatcher - this is the main part of the application */
                    //dispatch(dp_settings, ap_settings->daemonise);
                    dispatch(dp_settings, 1);
                } 
            }
        }
        
        free(ap_settings);
        
        free(dm_settings->name);
        free(dm_settings->rundir);
        free(dm_settings->pidfile);
        free(dm_settings);
        
        free(dp_settings->logfile);
        free(dp_settings->logformat);
        free(dp_settings->path);
        free(dp_settings->cmd);
        free(dp_settings->errlogfile);
        
        for (fargc = 0; fargc < dp_settings->argc; fargc++)
        {
            free(dp_settings->argv[fargc]);
        }
        
        free(dp_settings->argv);
        
        free(dp_settings);
        
        closelog();

        return err;
    }
