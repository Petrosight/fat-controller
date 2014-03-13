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

#ifndef SUBPROCSLOG_H
#define SUBPROCSLOG_H

#define SUBPROCSLOG_SOURCESTATE_ACTIVE 1
#define SUBPROCSLOG_SOURCESTATE_MOTHBALLED 2

#define SUBPROCSLOG_UNINITIALIZED 0
#define SUBPROCSLOG_INITIALIZED 1

#define RV_FAIL -1
#define RV_OK 0


typedef struct subprocslog_source
{
    int id;
    int fd_stdout;
    int fd_stderr;
    int state;
    struct subprocslog_source *previous;
    struct subprocslog_source *next;
} subprocslog_source;

/* Initialises logging system, opens log files */
int subprocslog_initialize(char *loc_stdout, char *loc_stderr);

/* Closes file handlers for log files and reopens them.
   
   Note that this will write the remaining data in buffers from attached file
   descriptors and then detach them so that when the logging systen restarts
   they will no-longer be attached.
   
   TODO:   prevent this forced detatching.
*/
int subprocslog_reinitialize();

/* Closes file handlers for log files */
int subprocslog_deinitialize();

/* Iterates over connected sources and flushes stdout and stderr buffers
   to log files */
int subprocslog_write_buffers();

/* Adds a new source to the beginning of the source list */
int subprocslog_append_source(int fd_stdout, int fd_stderr);

/* Schedules ALL sources to be removed from the list after the next time it is
   read. */
void subprocslog_remove_sources();

/* Schedules a source to be removed from the list after the next time it is
   read.   Returns RV_OK if a matching source is found, otherwise RV_FAIL */
int subprocslog_remove_source(int source_id);

/* Checks logging system is initialised */
int subprocslog_is_initialized();

#endif
