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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Namespace: exports vfs_get_host_and_username */

#ifndef test_get_host_and_username
#include <config.h>
#endif
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
#include <arpa/inet.h>
#include <malloc.h>
#ifdef USE_TERMNET
#include <termnet.h>
#endif

#include <errno.h>
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
 * If the user is empty, e.g. ftp://@roxanne/private, then your login name
 * is supplied.
 *
 * returns malloced host, user.
 * returns a malloced strings with the pathname relative to the host.
 * */

char *vfs_split_url (char *path, char **host, char **user, int *port, char **pass,
				 int default_port, int flags)
{
    struct passwd *passwd_info;
    char *dir, *colon, *inner_colon, *at, *rest;
    char *retval;
    char *pcopy = strdup (path);
    char *pend   = pcopy + strlen (pcopy);
    int default_is_anon = flags & URL_DEFAULTANON;
    
    *pass = NULL;
    *port = default_port;
    *user = NULL;
    retval = NULL;
    
    dir = pcopy;
    if (!(flags & URL_NOSLASH)) {
	/* locate path component */
	for (; *dir != '/' && *dir; dir++)
	    ;
	if (*dir){
	    retval = strdup (dir);
	    *dir = 0;
	} else
	    retval = strdup ("/");
    }
    
    /* search for any possible user */
    at    = strchr (pcopy, '@');

    /* We have a username */
    if (at){
	*at = 0;
	inner_colon = strchr (pcopy, ':');
	if (inner_colon){
	    *inner_colon = 0;
	    inner_colon++;
	    if (*inner_colon == '@')
		*pass = NULL;
	    else
		*pass = strdup (inner_colon);
	}
	if (*pcopy != 0)
	    *user = strdup (pcopy);
	else
	    default_is_anon = 0;
	
	if (pend == at+1)
	    rest = at;
	else
	    rest = at + 1;
    } else
	rest = pcopy;

    if (!*user){
	if (default_is_anon)
	    *user = strdup ("anonymous");
	else {
	    if ((passwd_info = getpwuid (geteuid ())) == NULL)
		*user = strdup ("anonymous");
	    else {
		*user = strdup (passwd_info->pw_name);
	    }
	    endpwent ();
	}
    }
    /* Check if the host comes with a port spec, if so, chop it */
    colon = strchr (rest, ':');
    if (colon){
	*colon = 0;
        if (sscanf(colon+1, "%d", port)==1) {
	    if (*port <= 0 || *port >= 65536)
	        *port = default_port;
	} else {
	    while(1) {
	        colon++;
		switch(*colon) {
		    case 'C': *port = 1;
	                      break;
		    case 'r': *port = 2;
	                      break;
		    case 0: goto done;
		}
	    }
	}
    }
done:
    *host = strdup (rest);

    free (pcopy);
    return retval;
}

#ifdef test_get_host_and_username
struct tt {
    char *url;
    char *r_host;
    char *r_user;
    char *r_pass;
    char *r_dir;
    int  r_port;
};

struct tt tests [] = {
    { "host",                                "host", "anonymous", NULL, "/", 21 },
    { "host/dir",                            "host", "anonymous", NULL, "/dir", 21 },
    { "host:40/",                            "host", "anonymous", NULL, "/", 40 },
    { "host:40/dir",                         "host", "anonymous", NULL, "/dir", 40 },
    { "user@host",                           "host", "user",      NULL, "/", 21 },
    { "user@host/dir",                       "host", "user",      NULL, "/dir", 21 },
    { "user@host:40/",                       "host", "user",      NULL, "/", 40 },
    { "user@host:40/dir",                    "host", "user",      NULL, "/dir", 40 },
    { "user:pass@host",                      "host", "user",    "pass", "/", 21 },
    { "user:pass@host/dir",                  "host", "user",    "pass", "/dir", 21 },
    { "user:pass@host:40",                   "host", "user",    "pass", "/", 40 },
    { "user:pass@host:40/dir",               "host", "user",    "pass", "/dir", 40 },
    { "host/a@b",                            "host", "anonymous", NULL, "/a@b", 21 },
    { "host:40/a@b",                         "host", "anonymous", NULL, "/a@b", 40 },
    { "user@host/a@b",                       "host", "user",      NULL, "/a@b", 21 },
    { "user@host:40/a@b",                    "host", "user",      NULL, "/a@b", 40 },
    { "user:pass@host/a@b",                  "host", "user",    "pass", "/a@b", 21 },
    { "user:pass@host:40/a@b",               "host", "user",    "pass", "/a@b", 40 },
    { "host/a:b",                            "host", "anonymous", NULL, "/a:b", 21 },
    { "host:40/a:b",                         "host", "anonymous", NULL, "/a:b", 40 },
    { "user@host/a:b",                       "host", "user",      NULL, "/a:b", 21 },
    { "user@host:40/a:b",                    "host", "user",      NULL, "/a:b", 40 },
    { "user:pass@host/a:b",                  "host", "user",    "pass", "/a:b", 21 },
    { "user:pass@host:40/a:b",               "host", "user",    "pass", "/a:b", 40 },
    { "host/a/b",                            "host", "anonymous", NULL, "/a/b", 21 },
    { "host:40/a/b",                         "host", "anonymous", NULL, "/a/b", 40 },
    { "user@host/a/b",                       "host", "user",      NULL, "/a/b", 21 },
    { "user@host:40/a/b",                    "host", "user",      NULL, "/a/b", 40 },
    { "user:pass@host/a/b",                  "host", "user",    "pass", "/a/b", 21 },
    { "user:pass@host:40/a/b",               "host", "user",    "pass", "/a/b", 40 },
    { NULL }
};

/* tests with implicit login name */
struct tt tests2 [] = {
    { "@host",                           "host", "user",      NULL, "/", 21 },
    { "@host/dir",                       "host", "user",      NULL, "/dir", 21 },
    { "@host:40/",                       "host", "user",      NULL, "/", 40 },
    { "@host:40/dir",                    "host", "user",      NULL, "/dir", 40 },
    { ":pass@host",                      "host", "user",    "pass", "/", 21 },
    { ":pass@host/dir",                  "host", "user",    "pass", "/dir", 21 },
    { ":pass@host:40",                   "host", "user",    "pass", "/", 40 },
    { ":pass@host:40/dir",               "host", "user",    "pass", "/dir", 40 },
    { "@host/a@b",                       "host", "user",      NULL, "/a@b", 21 },
    { "@host:40/a@b",                    "host", "user",      NULL, "/a@b", 40 },
    { ":pass@host/a@b",                  "host", "user",    "pass", "/a@b", 21 },
    { ":pass@host:40/a@b",               "host", "user",    "pass", "/a@b", 40 },
    { "@host/a:b",                       "host", "user",      NULL, "/a:b", 21 },
    { "@host:40/a:b",                    "host", "user",      NULL, "/a:b", 40 },
    { ":pass@host/a:b",                  "host", "user",    "pass", "/a:b", 21 },
    { ":pass@host:40/a:b",               "host", "user",    "pass", "/a:b", 40 },
    { "@host/a/b",                       "host", "user",      NULL, "/a/b", 21 },
    { "@host:40/a/b",                    "host", "user",      NULL, "/a/b", 40 },
    { ":pass@host/a/b",                  "host", "user",    "pass", "/a/b", 21 },
    { ":pass@host:40/a/b",               "host", "user",    "pass", "/a/b", 40 },
    { NULL }
};

main ()
{
    int i, port, err;
    char *dir, *host, *user, *pass;
    struct passwd *passwd_info;
    char *current;
    
    if ((passwd_info = getpwuid (geteuid ())) == NULL)
	current = strdup ("anonymous");
    else {
		current= strdup (passwd_info->pw_name);
    }
    endpwent ();
    
    for (i = 0; tests [i].url; i++){
	err = 0;
	dir = get_host_and_username (tests [i].url, &host, &user, &port, 21, 1, &pass);

	if (strcmp (dir, tests [i].r_dir))
	    err++, printf ("dir: test %d flunked\n", i);
	
	if (!err && strcmp (host, tests [i].r_host))
	    err++, printf ("host: test %d flunked\n", i);
	
	if (!err && strcmp (user, tests [i].r_user))
	    err++, printf ("user: test %d flunked\n", i);

	if (!err && tests [i].r_pass)
	    if (strcmp (dir, tests [i].r_dir))
		err++, printf ("pass: test %d flunked\n", i);
		
	if (!err & tests [i].r_port != port)
	    err++, printf ("port: test %d flunked\n", i);

	if (err){
	    printf ("host=[%s] user=[%s] pass=[%s] port=[%d]\n",
		    host, user, pass, port);
	}
    }
    
    printf ("%d tests ok\nExtra tests:", i);
    
    for (i = 0; i < 4; i++){
	dir = get_host_and_username (tests [i].url, &host, &user, &port, 21, 0, &pass);
	if (strcmp (user, current))
	    printf ("ntest: flunked %d\n", i);
    }

    printf ("%d tests ok\nTests with implicit login name\n", i);
    
    for (i = 0; tests2 [i].url; i++){
	err = 0;
	dir = get_host_and_username (tests2 [i].url, &host, &user, &port, 21, 1, &pass);

	if (strcmp (dir, tests2 [i].r_dir))
	    err++, printf ("dir: test %d flunked\n", i);
	
	if (!err && strcmp (host, tests2 [i].r_host))
	    err++, printf ("host: test %d flunked\n", i);
	
	if (!err && strcmp (user, current))
	    err++, printf ("user: test %d flunked\n", i);

	if (!err && tests2 [i].r_pass)
	    if (strcmp (dir, tests2 [i].r_dir))
		err++, printf ("pass: test %d flunked\n", i);
		
	if (!err & tests2 [i].r_port != port)
	    err++, printf ("port: test %d flunked\n", i);

	if (err){
	    printf ("host=[%s] user=[%s] pass=[%s] port=[%d]\n",
		    host, user, pass, port);
	}
    }
    printf ("%d tests ok\n", i);
}
#endif
