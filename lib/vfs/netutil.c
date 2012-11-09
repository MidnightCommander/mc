/*
   Network utilities for the Midnight Commander Virtual File System.

   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * \brief Source: Virtual File System: Network utilities
 */

#include <config.h>

#include <stdlib.h>
#include <signal.h>

#include "lib/global.h"

#include "netutil.h"

/*** global variables ****************************************************************************/

SIG_ATOMIC_VOLATILE_T got_sigpipe = 0;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
sig_pipe (int unused)
{
    (void) unused;
    got_sigpipe = 1;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tcp_init (void)
{
    static gboolean initialized = FALSE;
    struct sigaction sa;

    if (initialized)
        return;

    got_sigpipe = 0;
    sa.sa_handler = sig_pipe;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGPIPE, &sa, NULL);

    initialized = TRUE;
}

/* --------------------------------------------------------------------------------------------- */
