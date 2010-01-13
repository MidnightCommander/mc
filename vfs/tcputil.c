/* Network utilities for the Midnight Commander Virtual File System.
   
   Copyright (C) 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2005, 2007
   Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/**
 * \file
 * \brief Source: Virtual File System: Network utilities
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>

#include "netutil.h"

int got_sigpipe;

static void
sig_pipe (int unused)
{
    (void) unused;
    got_sigpipe = 1;
}

void
tcp_init (void)
{
    struct sigaction sa;

    got_sigpipe = 0;
    sa.sa_handler = sig_pipe;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGPIPE, &sa, NULL);
}
