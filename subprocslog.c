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
#include <pthread.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include "extern.h"
#include "sfmemlib.h"
#include "subprocslog.h"

/*
    Error checking and return values:
    ---------------------------------
    
    All external functions return an integer denoting success or failure.
    Upon success, RV_OK is returned, all other return values denote failure
    of one kind or another.   Failures MUST be handled by client code as they
    may denote a simple, recoverable (e.g. uninitialised) or more serious
    unrecoverable errors that leave the system in an intermediate state,
    e.g. deinitialises ok but cannot then release the lock on the initialised
    state variable.
    
    Currently it is not possible for the client to determine if it can recover
    from an error (as the only possible return values are RV_OK and RV_FAIL),
    so any return of RV_FAIL must be handled as unrecoverable and initiate
    immediate shutdown of the application.
*/

subprocslog_source *source_list_start = NULL;
pthread_mutex_t source_list_mutex;
unsigned int source_count = 0;
int initialized = SUBPROCSLOG_UNINITIALIZED;
FILE *log_stdout, *log_stderr;
char *loc_stdout = NULL, *loc_stderr = NULL;

pthread_rwlock_t initialized_state_rwlock = PTHREAD_RWLOCK_INITIALIZER;


/* --------
   Internal 
   -------- */

static int write_fdsource_to_fhsink(int source, FILE *sink)
{    
    char buffer[255];
    ssize_t bytes_read;
    size_t buffer_size, element_size;
    
    buffer_size = sizeof(buffer);
    element_size = sizeof(char);
    
    do
    {
        bytes_read = read(source, buffer, buffer_size);
                
        if (bytes_read > 0)
        {
            /* Write message */
            fwrite(buffer, element_size, bytes_read, sink);
        }
        else if(bytes_read == -1)
        {
            if (errno == EAGAIN)
            {
                /* No data yet, try again, so no need to report this error */
            }
            else
            {
                /* Error */
                _syslog(LOG_ERR, "Cannot read from file descriptor on receiving end of pipe from sub-process: ERRNO:%d, MSG:%s", errno, strerror(errno));
            }
        }
        
    } while (bytes_read > 0);
    
    fflush(sink);
    
    return RV_OK;
}


static int do_initialize()
{
    /* Check we're not already initialised */
    if (subprocslog_is_initialized() == 0)
    {
        syslog(LOG_ERR, "Cannot initialize - already initialized");
        
        return RV_FAIL;
    }
    
    /* Open FP for STDOUT */
    log_stdout = fopen(loc_stdout, "a+");
    
    if (log_stdout == NULL)
    {
        syslog(LOG_ERR, "Could not open log file for STDOUT: %s", loc_stdout);
        
        return RV_FAIL;
    }
    
    /* If a different file is specified, then open FP for STDERR */
    if (strcmp(loc_stdout, loc_stderr) == 0)
    {
        /* If the paths for stdout and stderr are the same then we only need one FP */
        log_stderr = log_stdout;
    }
    else
    {
        log_stderr = fopen(loc_stderr, "a+");
        
        if (log_stderr == NULL)
        {
            syslog(LOG_ERR, "Could not open log file for STDERR: %s", loc_stderr);
            
            return RV_FAIL;
        }
    }
    
    /* We're initialised */
    initialized = SUBPROCSLOG_INITIALIZED;
    
    return RV_OK;
}

static int do_deinitialize()
{
    /* Check we're already initialised */
    if (subprocslog_is_initialized() != 0)
    {
        syslog(LOG_ERR, "Cannot deinitialize - not initialized");
        
        return RV_FAIL;
    }
    
    /* We're NOT initialised */
    initialized = SUBPROCSLOG_UNINITIALIZED;
    
    if (log_stdout != log_stderr)
    {
        if (fclose(log_stderr) != 0)
        {
            syslog(LOG_WARNING, "Could not close log file for STDERR: [%d] %s", errno, strerror(errno));
            
            return RV_FAIL;
        }
    }
    
    if (fclose(log_stdout) != 0)
    {
        syslog(LOG_ERR, "Could not close log file for STDLOG: [%d] %s", errno, strerror(errno));
        
        return RV_FAIL;
    }
    
    return RV_OK;
}

/* Not needed yet
static int trywrlock_initialized()
{
    int lock_rv;

    lock_rv = pthread_rwlock_trywrlock(&initialized_state_rwlock);
    
    if (lock_rv != 0)
    {
        if (lock_rv == EBUSY)
        {
            syslog(LOG_ERR, "Could not acquire initializer lock, already locked for writing.");
        }
        else
        {
            syslog(LOG_ERR, "Could not acquire initializer lock: [%d] %s", errno, strerror(errno));
        }
        
        return RV_FAIL;
    }

    return RV_OK;
}
*/
static int pthread_rwlock_rdlock_elog(pthread_rwlock_t *rwlock)
{
    if (pthread_rwlock_rdlock(rwlock) != 0)
    {
        syslog(LOG_ERR, "FAIL: pthread_rwlock_rdlock(): [%d] %s", errno, strerror(errno));
        
        return RV_FAIL;
    }
    
    return RV_OK;
}

static int pthread_rwlock_wrlock_elog(pthread_rwlock_t *rwlock)
{
    if (pthread_rwlock_wrlock(rwlock) != 0)
    {
        syslog(LOG_ERR, "FAIL: pthread_rwlock_wrlock(): [%d] %s", errno, strerror(errno));
        
        return RV_FAIL;
    }
    
    return RV_OK;
}

static int pthread_rwlock_unlock_elog(pthread_rwlock_t *rwlock)
{
    if (pthread_rwlock_unlock(rwlock) != 0)
    {
        syslog(LOG_ERR, "FAIL: pthread_rwlock_unlock(): [%d] %s", errno, strerror(errno));
        
        return RV_FAIL;
    }
    
    return RV_OK;
}

/* Writes the contents of all the STDOUT and STDIN receive buffers from all
 * attached pipes.
 * 
 * Note that this function DOES NOT ACQUIRE A READ LOCK, all calling functions
 * must acquire such a lock before calling.
 */
static int do_write_buffers()
{
    subprocslog_source *current, *next;
    
    pthread_mutex_lock(&source_list_mutex);

    current = source_list_start;
    
    /* Iterate over list of sources */
    while (current != NULL)
    {
        /* For each source, dump STDOUT buffer */
        write_fdsource_to_fhsink(current->fd_stdout, log_stdout);
    
        /* Dump STDERR buffer */
        write_fdsource_to_fhsink(current->fd_stderr, log_stderr);
    
        /* Check if mothballed */
        if (current->state == SUBPROCSLOG_SOURCESTATE_MOTHBALLED)
        {
            /* Mothballed, so close FDs and remove from list */
            syslog(LOG_DEBUG, "subprocslog::do_write_buffers() closing id: %d, fd_stderr: %d", current->id, current->fd_stderr);
            
            if (close(current->fd_stderr) != 0)
            {
                syslog(LOG_CRIT, "subprocslog::do_write_buffers() cannot close stderr FD: %s", strerror(errno));
            }
            
            syslog(LOG_DEBUG, "subprocslog::do_write_buffers() closing id: %d, fd_stdout: %d", current->id, current->fd_stdout);
            
            if (close(current->fd_stdout) != 0)
            {
                syslog(LOG_CRIT, "subprocslog::do_write_buffers() cannot close stdout FD: %s", strerror(errno));
            }
            
            /* Now remove the struct from the list */
            if (current->previous == NULL)
            {
                /* We're removing the first one in the list */
                if (current->next != NULL)
                {
                    /* Only do this if it's not also the last, i.e. only one in the list */
                    current->next->previous = NULL;
                }
                
                source_list_start = current->next;
            }
            else if (current->next == NULL)
            {
                /* We're removing the last one from the list */
                
                if (current->previous == NULL)
                {
                    syslog(LOG_WARNING, "Null pointer when removing list element from end, element: %d", current->id);
                }
                
                current->previous->next = NULL;
            }
            else
            {
                /* We're removing one from somewhere in the middle */
                
                if (current->previous == NULL)
                {
                    syslog(LOG_WARNING, "Null pointer (previous) when removing list element from middle, element: %d", current->id);
                }
                
                if (current->next == NULL)
                {
                    syslog(LOG_WARNING, "Null pointer (next) when removing list element from middle, element: %d", current->id);
                }
                
                current->previous->next = current->next;
                
                current->next->previous = current->previous;
            }
            
            next = current->next;
            
            free(current);
            
            current = next;
            
            /*source_count--;*/
        }
        else
        {
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&source_list_mutex);
    
    return RV_OK;
}

/*  -----------------
    Public functions
    ----------------- */

int subprocslog_initialize(char *ploc_stdout, char *ploc_stderr)
{
    int init_rv;
    
    syslog(LOG_DEBUG, "Initialising subproc logging, stdout: %s, stderr: %s", ploc_stdout, ploc_stderr);

    /* Lock for writing */
    if (pthread_rwlock_wrlock_elog(&initialized_state_rwlock) != 0)
    {
        return RV_FAIL;
    }
    
    /* Copy the log file paths to file-scope variables - we may need them again when reinitialising */
    loc_stdout = sfrealloc(loc_stdout, (strlen(ploc_stdout) + 1) * sizeof(char));
    loc_stderr = sfrealloc(loc_stderr, (strlen(ploc_stderr) + 1) * sizeof(char));
    
    strcpy(loc_stdout, ploc_stdout);
    strcpy(loc_stderr, ploc_stderr);
    
    /* Now we can actually start initialising... */
    init_rv = do_initialize();
    
    return pthread_rwlock_unlock_elog(&initialized_state_rwlock) == 0
           ? init_rv
           : RV_FAIL;
}

int subprocslog_reinitialize()
{
    int init_rv;
    
    syslog(LOG_DEBUG, "Reinitialising subproc logging");
    
    /* Lock for writing */
    if (pthread_rwlock_wrlock_elog(&initialized_state_rwlock) != 0)
    {
        return RV_FAIL;
    }
    
    /* Close current FPs */
    if (do_deinitialize() != RV_OK)
    {
        pthread_rwlock_unlock_elog(&initialized_state_rwlock);
        
        return RV_FAIL;
    }
    
    /* Reopen FPs */
    init_rv = do_initialize();

    return pthread_rwlock_unlock_elog(&initialized_state_rwlock) == 0
           ? init_rv
           : RV_FAIL;
}

int subprocslog_deinitialize()
{
    int deinit_rv;
    
    syslog(LOG_DEBUG, "Deinitialising subproc logging");
    
    /* Lock for writing */
    if (pthread_rwlock_wrlock_elog(&initialized_state_rwlock) != 0)
    {
        return RV_FAIL;
    }
    
    subprocslog_remove_sources();
    
    do_write_buffers();
        
    deinit_rv = do_deinitialize();
    
    free(loc_stdout);
    free(loc_stderr);
    
    return pthread_rwlock_unlock_elog(&initialized_state_rwlock) == 0
           ? deinit_rv
           : RV_FAIL;
}

int subprocslog_write_buffers()
{
    int write_rv;
    
    if (pthread_rwlock_rdlock_elog(&initialized_state_rwlock) != RV_OK)
    {
        return RV_FAIL;
    }

    if (initialized != SUBPROCSLOG_INITIALIZED)
    {
        syslog(LOG_ERR, "subprocslog_write_buffers: Not initialised.");
        
        return RV_FAIL;
    }
    
    write_rv = do_write_buffers();
    
    return pthread_rwlock_unlock_elog(&initialized_state_rwlock) == 0
           ? write_rv
           : RV_FAIL;
}

int subprocslog_append_source(int fd_stdout, int fd_stderr)
{
    subprocslog_source *source;
    
    /* --- Init lock --- */
    
    pthread_rwlock_rdlock_elog(&initialized_state_rwlock);
    
    if (initialized != SUBPROCSLOG_INITIALIZED)
    {
        syslog(LOG_ERR, "subprocslog_append_source: Not initialised.");
        
        return RV_FAIL;
    }
    
    /* --- Function body --- */
    
        pthread_mutex_lock(&source_list_mutex);
    
        fcntl(fd_stdout, F_SETFL, O_NONBLOCK);
        fcntl(fd_stderr, F_SETFL, O_NONBLOCK);
    
        /* Create a new struct to hold file descriptors */
        source = sfmalloc(sizeof(subprocslog_source));

        if (source_count > 65000)
        {
            source_count = 0;
        }
    
        source->id = source_count++;
        source->fd_stdout = fd_stdout;
        source->fd_stderr = fd_stderr;
        source->state = SUBPROCSLOG_SOURCESTATE_ACTIVE;
        source->previous = NULL;
        source->next = source_list_start;
    
        if (source_list_start != NULL)
        {
            source_list_start->previous = source;
        }
        
        /* Add to beginning of linked list */
        source_list_start = source;
        
        syslog(LOG_DEBUG, "subprocslog_append_source: Appending fd_stderr: %d", fd_stderr);
        syslog(LOG_DEBUG, "subprocslog_append_source: Appending fd_stdout: %d", fd_stdout);
        
        pthread_mutex_unlock(&source_list_mutex);
    
    /* --- Init unlock --- */
    
    return pthread_rwlock_unlock_elog(&initialized_state_rwlock) == 0
           ? source->id
           : RV_FAIL;
}

void subprocslog_remove_sources()
{
    subprocslog_source *source;
    
    if (initialized != SUBPROCSLOG_INITIALIZED)
    {
        syslog(LOG_ERR, "subprocslog_remove_sources: Not initialised.");
        
        return;
    }
    
    source = source_list_start;
    
    while (source != NULL)
    {
        source->state = SUBPROCSLOG_SOURCESTATE_MOTHBALLED;
        
        source = source->next;
    }
}

int subprocslog_remove_source(int source_id)
{
    subprocslog_source *source;
    int return_value = RV_FAIL;
    
    if (initialized != SUBPROCSLOG_INITIALIZED)
    {
        syslog(LOG_ERR, "subprocslog_remove_source: Not initialised.");
        
        return RV_FAIL;
    }
    
    syslog(LOG_DEBUG, "subprocslog_remove_source: Removing source id: %d", source_id);
    
    pthread_mutex_lock(&source_list_mutex);
    source = source_list_start;
    
    while (source != NULL)
    {
        if (source->id == source_id)
        {
            source->state = SUBPROCSLOG_SOURCESTATE_MOTHBALLED;
            
            return_value = RV_OK;
            
            break;
        }
        
        source = source->next;
    }
    pthread_mutex_unlock(&source_list_mutex);
    
    return return_value;
}

int subprocslog_is_initialized()
{
    return initialized == SUBPROCSLOG_INITIALIZED ? 0 : -1;
}
