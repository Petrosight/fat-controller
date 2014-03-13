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

#ifndef FATCONTROLLER_H
#define FATCONTROLLER_H

#define APPLICATION_NAME "FatController"

#define DEFAULT_LOG_FORMAT "FAT: %s"

/* Forward declaration of structs */
struct daemon_settings;
struct dispatching_settings;


struct application_settings
{
    int debug;
    int daemonise;
    int test_fire;
};

void _syslog(int facility_priority, const char *format, ...);
void setupSyslog(int debug, char *name, int daemonise, char *format);
void showStartupOptions(struct application_settings *ap_settings, struct daemon_settings *dm_settings, struct dispatching_settings *dp_settings);
int main(int argc, char **argv);

#endif
