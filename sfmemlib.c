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
#include <stdlib.h>
#include <syslog.h>
#include "extern.h"
#include "sfmemlib.h"

/* 
    SFMEMLIB v1.01
    Nick Giles 2010
    http://wwww.4pmp.com/

    A simple library which wraps malloc, calloc and realloc functions to ensure
    they succeed.   If the functiond don't succeed then the application is
    terminated so you don't have to pepper your code with condition statements
    to check the calls succeeded.
    
    Note that this is not a drop-in replacement for all instances of malloc and
    realloc, there may well be times when you can take another, less drastic
    action than termination.

*/

void *sfmalloc(size_t sz)
{
    void *ret = malloc(sz);
    
    if (ret == NULL)
    {
        _syslog(LOG_CRIT, "Out of memory");
        exit(EXIT_FAILURE);
    }
    
    return ret;
}

void *sfcalloc(size_t nelem, size_t elsize)
{
    void *ret = calloc(nelem, elsize);
    
    if (ret == NULL)
    {
        _syslog(LOG_CRIT, "Out of memory");
        exit(EXIT_FAILURE);
    }
    
    return ret;
}

void *sfrealloc(void *ptr, size_t sz)
{
    void *ret = realloc(ptr, sz);
    
    if (ret == NULL)
    {
        _syslog(LOG_CRIT, "Out of memory");
        exit(EXIT_FAILURE);
    }
    
    return ret;
}
