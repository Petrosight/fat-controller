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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "fatcontroller.h"
#include "daemonise.h"
#include "jobdispatching.h"
#include "dgetopts.h"
#include "sfmemlib.h"

    static int flag_help;
    static int flag_daemonise;
    static int flag_debug;
    static int flag_itm;
    static int flag_ftm;
    static int flag_ati;
    static int flag_run_once;
    static int flag_test_fire;

    static void showhelp()
    {
        printf("Usage: fatcontroller [OPTIONS]\n");
        printf("Starts a daemon/application which repeatedly\n");
        printf("runs (multiple instance of) a given command\n");
        printf("\n");
        printf("Required arguments:\n");
        printf("    -c, --command                Command, e.g. php\n");
        printf("    -l, --log-file               Stdout of child processes will be written into\n");
        printf("                                 this file e.g. /var/log/mydaemon.log\n");
        printf("                                 Can also be used in application mode.\n");
        printf("\n");
        printf("Required arguments for daemon operation:\n");
        printf("    -w, --working-directory      Working directory e.g. /home/shunty/scripts\n");
        printf("    -i, --pid-file               File in which to store process ID\n");
        printf("\n");
        printf("Optional arguments:\n");
        printf("    -f, --log-format             A printf style format string for use as log format.\n");
        printf("    -n, --daemon-name            Name of the daemon (primarily for syslog).\n");
        printf("        --daemonise              Specifies to run as a daemon.\n");
        printf("        --debug                  Debug mode - prints lots of info to syslog.\n");
        printf("    -s, --sleep                  Sleep time(s) between processes (default: 30)\n");
        printf("    -e, --sleep-on-error         Sleep time (s) on error (default: 300)\n");
        printf("    -t, --threads                Number of threads (default: 1)\n");
        printf("    -a, --arguments              Command arguments, e.g. \"-f hello.php\"\n");
        printf("        --independent-threads    Specifies independent thread model\n");
        printf("        --fixed-interval-threads Specifies fixed-interval thread model\n");
        printf("        --proc-run-time-warn     Warn if child process runs longer than (s)\n");
        printf("        --proc-run-time-max      Maximum child process run time (s)\n");
        printf("        --proc-term-timeout      Maximum wait for process termination (s)\n");
        printf("        --fixed-interval-wait    Maximum wait for free thread in FI mode (s)\n");
        printf("        --append-thread-id       Will append --tid=x to processes.\n");
        printf("        --err-log-file           Logs stderr of child processes.\n");
        printf("        --run-once               Run script only once, e.g. if daemon proc\n");
        printf("        --test-fire              Initialise but do not run, useful for testing\n");
        printf("        --help                   This help screen\n");
        printf("\n");
        printf("For more details on how to use these options, see the website.\n");
        printf("\n");
        printf("Latest updates at www.4pmp.com\n");
        printf("Report bugs to <info@4pmp.com>\n");
        
    }

    static void splitArgs(char *input, char ***argv, int *argc)
    {
        char *token, **data;
        
        *argc = 0;
        
        /* Get the first token (splitting on " ") */
        token = strtok(input," ");

        data = *argv;
        
        while( token != NULL)
        {
            /* Allocate more heap space for a new entry in the array */
            data = sfrealloc(data, ((*argc)+1) * sizeof(char *));
        
            /* Allocate space on the heap for the string */
            data[(*argc)] = sfcalloc((strlen(token) + 1), sizeof(char));
            
            /* Assign string from strtok result */
            strcpy(data[(*argc)], token);
            
            /* Get the next token */
            token = strtok(NULL," ");
            
            /* Increment number of elements in array */
            (*argc)++;
        }
        
        /* We need to update argv as realloc may have had to move the memory block (i.e. pointer value might have changed) */
        *argv = data;
        
        /*
        if (data != NULL)
        {
            int fargc;
            for (fargc=0; fargc < *argc; fargc++)
            {
                free(data[fargc]);
            }
            
            free(data);
        }
            */
        
        return;
    }
    
    static int parseOptions(int argc, char **argv, struct application_settings *ap_settings, struct daemon_settings *dm_settings, struct dispatching_settings *dp_settings)
    {
        int c, err=0;
        int fi=0, fw=0, fl=0, fp=0, fc=0;
        opterr = 0;
        
        /* Reset option flags */
        flag_help = 0, flag_daemonise = 0, flag_debug = 0, flag_itm = 0;
        flag_ftm = 0, flag_ati = 0, flag_run_once = 0, flag_test_fire = 0;
        
        while (1)
        {
            static struct option long_options[] =
            {
                {"pid-file",               required_argument, 0,               'i'},
                {"working-directory",      required_argument, 0,               'w'},
                {"log-file",               required_argument, 0,               'l'},
                {"log-format",             required_argument, 0,               'f'},
                {"sleep",                  required_argument, 0,               's'},
                {"sleep-on-error",         required_argument, 0,               'e'},
                {"path",                   required_argument, 0,               'p'},
                {"help",                   no_argument,       &flag_help,      1},
                {"threads",                required_argument, 0,               't'},
                {"daemon-name",            required_argument, 0,               'n'},
                {"debug",                  no_argument,       &flag_debug,     1},
                {"daemonise",              no_argument,       &flag_daemonise, 1},
                {"command",                required_argument, 0,               'c'},
                {"arguments",              required_argument, 0,               'a'},
                {"independent-threads",    no_argument,       &flag_itm,       1},
                {"fixed-interval-threads", no_argument,       &flag_ftm,       1},
                {"proc-run-time-warn",     required_argument, 0,               256},
                {"proc-run-time-max",      required_argument, 0,               257},
                {"proc-term-timeout",      required_argument, 0,               258},
                {"fixed-interval-wait",    required_argument, 0,               259},
                {"err-log-file",           required_argument, 0,               260},
                {"append-thread-id",       no_argument,       &flag_ati,         1},
                {"run-once",               no_argument,       &flag_run_once,    1},
                {"test-fire",              no_argument,       &flag_test_fire,   1},
                {0, 0, 0, 0}
            };
            
            /* getopt_long stores the option index here. */
            int option_index = 0;

            c = getopt_long (argc, argv, "i:w:f:l:n:s:e:t:p:c:a:", long_options, &option_index);

            /* Detect the end of the options. */
            if (c == -1)
            {
                break;
            }


            switch (c)
            {
                case 0:
                    /* If this option set a flag, do nothing else now. */
                    if (long_options[option_index].flag != 0)
                    {
                        break;
                    }
                    break;

                case 'i':
                    dm_settings->pidfile = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dm_settings->pidfile, optarg);
                    fi = 1;
                    break;

                case 'w':
                    dm_settings->rundir = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dm_settings->rundir, optarg);
                    fw = 1;
                    break;
                
                case 'l':
                    dp_settings->logfile = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dp_settings->logfile, optarg);
                    fl = 1;
                    break;

                case 'f':
                    if (dp_settings->logformat != NULL) {
                        free(dp_settings->logformat);
                    }
                    dp_settings->logformat = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dp_settings->logformat, optarg);
                    fl = 1;
                    break;

                case 'n':
                    dm_settings->name = sfrealloc(dm_settings->name, sizeof(char) * (strlen(optarg)+1));
                    strcpy(dm_settings->name, optarg);
                    break;
                
                case 's':
                    dp_settings->sleep = atoi(&optarg[0]);
                    break;
                
                case 'e':
                    dp_settings->sleepOnError = atoi(&optarg[0]);
                    break;
                
                case 't':
                    dp_settings->threads = atoi(&optarg[0]);
                    break;
                
                case 'p':
                    dp_settings->path = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dp_settings->path, optarg);
                    fp = 1;
                    break;
                
                case 'c':
                    dp_settings->cmd = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dp_settings->cmd, optarg);
                    fc = 1;
                    break;
                
                case 'a':
                    splitArgs(optarg, &dp_settings->argv, &dp_settings->argc);
                    break;
                
                case 256:
                    dp_settings->thread_run_time_warn = atoi(&optarg[0]);
                    break;

                case 257:
                    dp_settings->thread_run_time_max = atoi(&optarg[0]);
                    break;

                case 258:
                    dp_settings->termination_timeout = atoi(&optarg[0]);
                    break;

                case 259:
                    dp_settings->fi_wait_time_max = atoi(&optarg[0]);
                    break;
                
                case 260:
                    dp_settings->errlogfile = sfmalloc(sizeof(char) * (strlen(optarg)+1));
                    strcpy(dp_settings->errlogfile, optarg);
                    break;

                case '?':
                    fprintf(stderr, "Unrecognised option: %s\n", argv[optind-1]);
                    err++;
                    break;

                default:
                    abort();
            }
        }
        
        if (flag_help) /* help, nothing to check */
        {
            return 0;
        }
        else if (err) /* error, nothing to check */
        {
            /*fprintf(stderr, "Invalid arguments.\n");*/
            return 1;
        }
        
        
        if (flag_daemonise)
        {
            ap_settings->daemonise = 1;
        }

        if (flag_itm && flag_ftm)
        {
            fprintf(stderr, "Multiple thread models specified.\n");
            
            return 1;
        }
        
        if (flag_itm)
        {
            dp_settings->threadModel = THREAD_MODEL_INDEPENDENT;
        }
        else if (flag_ftm)
        {
            dp_settings->threadModel = THREAD_MODEL_FIXED_INTERVAL;
        }
        
        if (flag_ati)
        {
            dp_settings->append_thread_id = 1;
        }

        if (flag_run_once)
        {
            dp_settings->run_once = 1;
        }
        
        if (flag_test_fire)
        {
            ap_settings->test_fire = 1;
        }

        if (flag_debug)
        {
            ap_settings->debug = 1;
        }
        
        /* Check we found all the required args */
        if (fc == 0 || fl == 0)
        {
            fprintf(stderr, "Missing required arguments.\n");
            
            return 1;
        }
        
        if (flag_daemonise && (fi == 0 || fw == 0))
        {
            fprintf(stderr, "Missing required arguments for daemon operation.\n");
            
            return 1;
        }
        
        return 0;
    }
    
    int processOptions(int argc, char **argv, struct application_settings *ap_settings, struct daemon_settings *dm_settings, struct dispatching_settings *dp_settings)
    {
        int c;
        
        if ( argc == 1 ) /* without arguments */
        {
    	    showhelp();
    	    
            return 1;
    	}
        
        /* Parse input args */
        c = parseOptions(argc, argv, ap_settings, dm_settings, dp_settings);
        
        /* Check for failure */
        if (c > 0)
        {
            showhelp();
            
            return 1;
        }
        
        /* Check if the user wants to see the help file */
        if (flag_help)
        {
            showhelp();
            
            return 1;
        }
        
        return 0;
    }
