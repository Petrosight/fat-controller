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

#ifndef DGETOPTS_H
#define DGETOPTS_H

/* 
 * Processes the command line arguments, populating the passed settings structs
 * 
 * Returns 0 if arguments processed and checked correctly, indicating exeution
 * can continue.
 * 
 * Returns 1 if execution should not continue, for example if there are missing
 * required arguments, the passed arguments are not valid or simply that the
 * user wants to just see the help info.
 *
 */
int processOptions(int argc, char **argv, struct application_settings *ap_settings, struct daemon_settings *dm_settings, struct dispatching_settings *dp_settings);

#endif
