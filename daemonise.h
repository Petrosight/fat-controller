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
#ifndef DAEMONISE_H
#define DAEMONISE_H

#define DEFAULT_DAEMON_NAME "FatController"

struct daemon_settings
{
    char *rundir;
    char *pidfile;
    char *name;
    int pidfd; /* PID File Descriptor */
};

    void daemonShutdown(struct daemon_settings *settings);
    int daemonize(char *rundir, char *pidfile, int *pidfd);
    int daemonStart(struct daemon_settings *settings);
#endif
