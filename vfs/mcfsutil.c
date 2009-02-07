/* Low-level protocol for MCFS.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/**
 * \file
 * \brief Source: Low-level protocol for MCFS
 * \author Miguel de Icaza
 * \date 1995, 1996
 */

#include <config.h>

#ifdef	WITH_MCFS
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_PMAP_SET
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#ifdef HAVE_RPC_PMAP_CLNT_H
#include <rpc/pmap_clnt.h>
#endif
#endif

#include <errno.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "mcfsutil.h"
#include "tcputil.h"
#include "mcfs.h"	/* tcp_invalidate_socket() */

#define CHECK_SIG_PIPE(sock) if (got_sigpipe) \
     { tcp_invalidate_socket (sock); return got_sigpipe = 0; }

/* Reads a block on dest for len bytes from sock */
/* Returns a boolean indicating the success status */
int
socket_read_block (int sock, char *dest, int len)
{
    int nread, n;

    for (nread = 0; nread < len;) {
	n = read (sock, dest + nread, len - nread);
	if (n <= 0) {
	    tcp_invalidate_socket (sock);
	    return 0;
	}
	nread += n;
    }
    return 1;
}

int
socket_write_block (int sock, const char *buffer, int len)
{
    int left, status;

    for (left = len; left > 0;) {
	status = write (sock, buffer, left);
	CHECK_SIG_PIPE (sock);
	if (status < 0)
	    return 0;
	left -= status;
	buffer += status;
    }
    return 1;
}

int
rpc_send (int sock, ...)
{
    long int tmp, len, cmd;
    char *text;
    va_list ap;

    va_start (ap, sock);

    for (;;) {
	cmd = va_arg (ap, int);
	switch (cmd) {
	case RPC_END:
	    va_end (ap);
	    return 1;

	case RPC_INT:
	    tmp = htonl (va_arg (ap, int));
	    write (sock, &tmp, sizeof (tmp));
	    CHECK_SIG_PIPE (sock);
	    break;

	case RPC_STRING:
	    text = va_arg (ap, char *);
	    len = strlen (text);
	    tmp = htonl (len);
	    write (sock, &tmp, sizeof (tmp));
	    CHECK_SIG_PIPE (sock);
	    write (sock, text, len);
	    CHECK_SIG_PIPE (sock);
	    break;

	case RPC_BLOCK:
	    len = va_arg (ap, int);
	    text = va_arg (ap, char *);
	    tmp = htonl (len);
	    write (sock, text, len);
	    CHECK_SIG_PIPE (sock);
	    break;

	default:
	    vfs_die ("Unknown rpc message\n");
	}
    }
}

int
rpc_get (int sock, ...)
{
    long int tmp, len;
    char *text, **str_dest;
    int *dest, cmd;
    va_list ap;

    va_start (ap, sock);

    for (;;) {
	cmd = va_arg (ap, int);
	switch (cmd) {
	case RPC_END:
	    va_end (ap);
	    return 1;

	case RPC_INT:
	    if (socket_read_block (sock, (char *) &tmp, sizeof (tmp)) == 0) {
		va_end (ap);
		return 0;
	    }
	    dest = va_arg (ap, int *);
	    *dest = ntohl (tmp);
	    break;

	    /* returns an allocated string */
	case RPC_LIMITED_STRING:
	case RPC_STRING:
	    if (socket_read_block (sock, (char *) &tmp, sizeof (tmp)) == 0) {
		va_end (ap);
		return 0;
	    }
	    len = ntohl (tmp);
	    if (cmd == RPC_LIMITED_STRING)
		if (len > 16 * 1024) {
		    /* silently die */
		    abort ();
		}
	    if (len > 128 * 1024)
		abort ();

	    /* Don't use glib functions here - this code is used by mcserv */
	    text = malloc (len + 1);
	    if (socket_read_block (sock, text, len) == 0) {
		free (text);
		va_end (ap);
		return 0;
	    }
	    text[len] = '\0';

	    str_dest = va_arg (ap, char **);
	    *str_dest = text;
	    break;

	case RPC_BLOCK:
	    len = va_arg (ap, int);
	    text = va_arg (ap, char *);
	    if (socket_read_block (sock, text, len) == 0) {
		va_end (ap);
		return 0;
	    }
	    break;

	default:
	    vfs_die ("Unknown rpc message\n");
	}
    }
}
#endif				/* WITH_MCFS */
