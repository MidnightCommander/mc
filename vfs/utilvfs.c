/* Utilities for VFS modules.

   Currently includes login and tcp open socket routines.
   
   Copyright (C) 1995, 1996 Miguel de Icaza
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Namespace: exports vfs_split_url */

#include <config.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <errno.h>

#include "utilvfs.h"

#include "vfs.h"

/* Extract the hostname and username from the path */
/* path is in the form: [user@]hostname:port/remote-dir, e.g.:
 *
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * ftp://joe:password@foo.se
 *
 * Returns g_malloc()ed host, user and pass they are present.
 * If the user is empty, e.g. ftp://@roxanne/private, and URL_ALLOW_ANON
 * is not set, then the current login name is supplied.
 *
 * Return value is a g_malloc()ed string with the pathname relative to the
 * host.
 */

char *
vfs_split_url (const char *path, char **host, char **user, int *port,
	       char **pass, int default_port, int flags)
{
    struct passwd *passwd_info;
    char *dir, *colon, *inner_colon, *at, *rest;
    char *retval;
    char *pcopy = g_strdup (path);
    char *pend = pcopy + strlen (pcopy);

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
    at = strchr (pcopy, '@');

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

    if (!*user && !(flags & URL_ALLOW_ANON)) {
	passwd_info = getpwuid (geteuid ());
	if (passwd_info && passwd_info->pw_name)
	    *user = g_strdup (passwd_info->pw_name);
	else {
	    /* This is very unlikely to happen */
	    *user = g_strdup ("anonymous");
	}
	endpwent ();
    }

    /* Check if the host comes with a port spec, if so, chop it */
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
