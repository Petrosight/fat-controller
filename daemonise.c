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
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "extern.h"
#include "daemonise.h"


    void daemonShutdown(struct daemon_settings *settings)
    {
        _syslog(LOG_INFO, "Daemon exiting");
        
        close(settings->pidfd);
    }

    /*
        Next steps:
        Change daemonize to accept the settings struct, change rundir and pidfile vars to use struct.
        In jobdispatching we need pidfd and close it after fork.
    */
    
    int daemonize(char *rundir, char *pidfile, int *pidfd)
    {
        int pid, sid, i;
        char str[10];

        /*
            To make this a useful daemonising library, we should perhaps call getppid() here.
            If the return value is 1, i.e. init process, then that means that we are already
            a daemon and the calling function should be informed.
        */
        
        /* Fork */
        pid = fork();

        if (pid < 0)
        {
            /* Could not fork */
            /*exit(EXIT_FAILURE);*/
            _syslog(LOG_ERR, "Fork failed.");
            return -1;
        }
        else if (pid > 0)
        {
            /* Child created ok, so exit parent process */
            printf("Child process created: %d\n", pid);
            
            _syslog(LOG_INFO, "Daemon process started [%d]", pid);

            /*exit(EXIT_SUCCESS);*/
            return pid;
        }

        /* Child continues */

        umask(027); /* Set full file permissions */

        /* Get a new session ID */
        sid = setsid();

        if (sid < 0)
        {
            /*exit(EXIT_FAILURE);*/
            return -8;
        }

        /* close all descriptors */
        /*
            This section has been removed; in The Fat Controller we know that
            there are no open resources.   If this were used as a daemonising
            library then we could extend this function to take a function
            pointer which if not NULL, will be called and should hhave the job
            of closing any open file descriptors in the child process which are
            inherited from the parent.
        
            for (i = getdtablesize(); i >= 0; --i)
            {
                if (close(i) != 0)
                {
                    _syslog(LOG_CRIT, "Could not close File Descriptor: %d", i);
                    return -16;
                }
            }
        */
        /*
            Replace with  getrlimit() and RLIMIT_NOFILE
            http://pubs.opengroup.org/onlinepubs/007908799/xsh/getrlimit.html
            https://publib.boulder.ibm.com/infocenter/iseries/v5r4/index.jsp?topic=/apis/getrlim.htm
        
            _syslog(LOG_DEBUG, "getdtablesize: %d", getdtablesize());
        
        */

        if (close(STDIN_FILENO) != 0)
        {
            _syslog(LOG_CRIT, "Could not close File Descriptor: STDIN");
            return -16;
        }
        
        if (close(STDOUT_FILENO) != 0)
        {
            _syslog(LOG_CRIT, "Could not close File Descriptor: STDOUT");
            return -16;
        }
        
        if (close(STDERR_FILENO) != 0)
        {
            _syslog(LOG_CRIT, "Could not close File Descriptor: STDERR");
            return -16;
        }

        /* Reopen the standard file descriptors and point them somewhere safe, i.e. /dev/null */
        i = open("/dev/null", O_RDWR);
        
        if (i == -1)
        {
            _syslog(LOG_CRIT, "Could not re-open STDIN File Descriptor");
            return -32;
        }
        
        if (dup(i) == -1)
        {
            _syslog(LOG_CRIT, "Could not re-open STDOUT File Descriptor");
            return -32;
        }
        
        if (dup(i) == -1)
        {
            _syslog(LOG_CRIT, "Could not re-open STDERR File Descriptor");
            return -32;
        }
        
        /* change running directory */
        if (chdir(rundir) != 0)
        {
            _syslog(LOG_WARNING, "Could not change to directory '%s': [%d] %s", rundir, errno, strerror(errno));
        }

        /* Ensure only one copy */
        *pidfd = open(pidfile, O_RDWR|O_CREAT|O_TRUNC, 0600);

        if (*pidfd == -1 )
        {
            /* Couldn't open lock file */
            _syslog(LOG_CRIT, "Could not open PID lock file %s, exiting", pidfile);
            /*exit(EXIT_FAILURE);*/
            return -2;
        }

        /* Try to lock file */
        if (lockf(*pidfd,F_TLOCK,0) == -1)
        {
            /* Couldn't get lock on lock file */
            _syslog(LOG_CRIT, "Could not lock PID lock file %s, exiting", pidfile);
            /*exit(EXIT_FAILURE);*/
            return -4;
        }

        /* Get and format PID */
        sprintf(str,"%d\n",getpid());

        /* write pid to lockfile */
        write(*pidfd, str, strlen(str));
        
        _syslog(LOG_DEBUG, "Daemon process initialised.");
        
        return 0;
    }

    
    int daemonStart(struct daemon_settings *settings)
    {
        _syslog(LOG_INFO, "Daemon starting up");

        /* Deamonize */
        return daemonize(settings->rundir, settings->pidfile, &(settings->pidfd));
    }
