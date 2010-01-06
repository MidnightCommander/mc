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
#include <pwd.h>
#include <unistd.h>
#include <string.h>

#include "../src/global.h"
#include "../vfs/utilvfs.h"
#include "../vfs/netutil.h"

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
    static char _initialized = 0;

    if (_initialized)
	return;

    got_sigpipe = 0;
    sa.sa_handler = sig_pipe;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGPIPE, &sa, NULL);

    _initialized = 1;
}

/**  Extract the hostname and username from the path
 *
 * Format of the path is [user@]hostname:port/remote-dir, e.g.:
 *
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * ftp://joe:password@foo.se
 *
 * @param path is an input string to be parsed
 * @param host is an outptun g_malloc()ed hostname
 * @param user is an outptut g_malloc()ed username
 *             (NULL if not specified)
 * @param port is an outptut integer port number
 * @param pass is an outptut g_malloc()ed password
 * @param default_port is an input default port
 * @param flags are parsing modifier flags (@see VFS_URL_FLAGS)
 *
 * @return g_malloc()ed host, user and pass if they are present.
 *         If the user is empty, e.g. ftp://@roxanne/private, and URL_USE_ANONYMOUS
 *         is not set, then the current login name is supplied.
 *         Return value is a g_malloc()ed string with the pathname relative to the
 *         host.
 */
char *
vfs_split_url (const char *path, char **host, char **user, int *port,
	       char **pass, int default_port, enum VFS_URL_FLAGS flags)
{
    char *dir, *colon, *inner_colon, *at, *rest;
    char *retval;
    char * const pcopy = g_strdup (path);
    const char *pend = pcopy + strlen (pcopy);

    if (pass)
	*pass = NULL;
    *port = default_port;
    *user = NULL;
    retval = NULL;

    dir = pcopy;
    if (!(flags & URL_NOSLASH)) {
	/* locate path component */
	while (*dir != PATH_SEP && *dir)
	    dir++;
	if (*dir) {
	    retval = g_strdup (dir);
	    *dir = 0;
	} else
	    retval = g_strdup (PATH_SEP_STR);
    }

    /* search for any possible user */
    at = strrchr (pcopy, '@');

    /* We have a username */
    if (at) {
	*at = 0;
	inner_colon = strchr (pcopy, ':');
	if (inner_colon) {
	    *inner_colon = 0;
	    inner_colon++;
	    if (pass)
		*pass = g_strdup (inner_colon);
	}
	if (*pcopy != 0)
	    *user = g_strdup (pcopy);

	if (pend == at + 1)
	    rest = at;
	else
	    rest = at + 1;
    } else
	rest = pcopy;

    if (!*user && !(flags & URL_USE_ANONYMOUS))
        *user = vfs_get_local_username();

    /* Check if the host comes with a port spec, if so, chop it */
    if ('[' == *rest) {
	colon = strchr (++rest, ']');
	if (colon) {
	    colon[0] = '\0';
	    colon[1] = '\0';
	    colon++;
	} else {
	    g_free (pcopy);
	    g_free (retval);
	    *host = NULL;
	    *port = 0;
	    return NULL;
	}
    } else
	colon = strchr (rest, ':');

    if (colon) {
	*colon = 0;
	if (sscanf (colon + 1, "%d", port) == 1) {
	    if (*port <= 0 || *port >= 65536)
		*port = default_port;
	} else {
	    while (*(++colon)) {
		switch (*colon) {
		case 'C':
		    *port = 1;
		    break;
		case 'r':
		    *port = 2;
		    break;
		}
	    }
	}
    }
    if (host)
	*host = g_strdup (rest);

    g_free (pcopy);
    return retval;
}
