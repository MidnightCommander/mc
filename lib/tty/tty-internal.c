/*
   Internal stuff of the terminal controlling library.

   Copyright (C) 2019
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019.

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

/** \file
 *  \brief Source: internal stuff of the terminal controlling library.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib/global.h"

#include "tty-internal.h"

/*** global variables ****************************************************************************/

/* pipe to handle SIGWINCH */
int sigwinch_pipe[2];

/*** file scope macro definitions ****************************************************************/

/* some OSes don't provide O_CLOEXEC */
#if !defined O_CLOEXEC && defined O_NOINHERIT
/* Mingw spells it 'O_NOINHERIT'.  */
#define O_CLOEXEC O_NOINHERIT
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/*** global variables ****************************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_create_winch_pipe (void)
{
    int fd_flags;

    if (pipe (sigwinch_pipe) == -1)
    {
        perror (_("Cannot create pipe for SIGWINCH"));
        exit (EXIT_FAILURE);
    }

    /* If we read from an empty pipe, then read(2) will block until data is available.
     * If we write to a full pipe, then write(2) blocks until sufficient data has been read
     * from the pipe to allow the write to complete..
     * Therefore, use nonblocking I/O.
     */

    fd_flags = fcntl (sigwinch_pipe[0], F_GETFL, NULL);
    if (fd_flags != -1)
    {
        fd_flags |= O_NONBLOCK | O_CLOEXEC;
        fd_flags = fcntl (sigwinch_pipe[0], F_SETFL, fd_flags);
    }
    if (fd_flags == -1)
    {
        perror (_("Cannot configure write end of SIGWINCH pipe"));
        exit (EXIT_FAILURE);
    }

    fd_flags = fcntl (sigwinch_pipe[1], F_GETFL, NULL);
    if (fd_flags != -1)
    {
        fd_flags |= O_NONBLOCK | O_CLOEXEC;
        fd_flags = fcntl (sigwinch_pipe[1], F_SETFL, fd_flags);
    }
    if (fd_flags == -1)
    {
        perror (_("Cannot configure read end of SIGWINCH pipe"));
        exit (EXIT_FAILURE);
    }
}

/* --------------------------------------------------------------------------------------------- */
