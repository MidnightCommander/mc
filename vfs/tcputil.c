/* Server for the Midnight Commander Virtual File System.
   Routines for the tcp connection, includes the primitive rpc routines.
   
   Copyright (C) 1995, 1996 Miguel de Icaza
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#include <config.h>
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
#include <arpa/inet.h>
#include <malloc.h>

#ifdef HAVE_PMAP_SET
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#ifdef HAVE_RPC_PMAP_CLNT_H
#include <rpc/pmap_clnt.h>
#endif
#endif

#ifdef USE_TERMNET
#include <termnet.h>
#endif

#include <errno.h>
#include "tcputil.h"
#include "../src/dialog.h"	/* for message () */
#include "../src/mem.h"		/* for bcopy */
#include "../src/util.h"	/* for unix_error_string */
#include "../src/mad.h"
#include "mcfs.h"		/* for mcserver_port definition */

#define CHECK_SIG_PIPE(sock) if (got_sigpipe) \
     { tcp_invalidate_socket (sock); return got_sigpipe = 0; }

extern void tcp_invalidate_socket (int);

int got_sigpipe;

/* Reads a block on dest for len bytes from sock */
/* Returns a boolean indicating the success status */
int socket_read_block (int sock, char *dest, int len)
{
    int nread, n;

    for (nread = 0; nread < len;){
	n = read (sock, dest+nread, len-nread);
	if (n <= 0){
	    tcp_invalidate_socket (sock);
	    return 0;
	}
	nread += n;
    }
    return 1;
}

int socket_write_block (int sock, char *buffer, int len)
{
    int left, status;

    for (left = len; left > 0;){
	status = write (sock, buffer, left);
	CHECK_SIG_PIPE (sock);
	if (status < 0)
	    return 0;
	left -= status;
	buffer += status;
    }
    return 1;
}

int send_string (int sock, char *string)
{
    return socket_write_block (sock, string, strlen (string));
}

int rpc_send (int sock, ...)
{
    long int tmp, len, cmd;
    char *text;
    va_list ap;

    va_start (ap, sock);

    for (;;){
	cmd = va_arg (ap, int);
	switch (cmd){
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
	    fprintf (stderr, "Unknown rpc message\n");
	    abort ();
	}
    }
}

typedef struct sock_callback_t {
    int  sock;
    void (*cback)(int);
    struct sock_callback_t *link;
} sock_callback_t;

sock_callback_t *sock_callbacks = 0;

static void check_hooks (int sock)
{
    sock_callback_t *callback, *prev;

    for (prev=callback = sock_callbacks; callback; callback = callback->link){
	if (callback->sock != sock){
	    prev = callback;
	    continue;
	}
	callback->sock = -1;
	(callback->cback)(sock);
	if (callback == sock_callbacks){
	    sock_callbacks = callback->link;
	} else {
	    prev->link = callback->link;
	}
	free (callback);
	return;
    }
}

int rpc_get (int sock, ...)
{
    long int tmp, len;
    char *text, **str_dest;
    int  *dest, cmd;
    va_list ap;

    va_start (ap, sock);

    check_hooks (sock);

    for (;;){
	cmd = va_arg (ap, int);
	switch (cmd){
	case RPC_END:
	    va_end (ap);
	    return 1;

	case RPC_INT:
	    if (socket_read_block (sock, (char *) &tmp, sizeof (tmp)) == 0)
		return 0;
	    dest = va_arg (ap, int *);
	    *dest = ntohl (tmp);
	    break;

	    /* returns an allocated string */
	case RPC_LIMITED_STRING:
	case RPC_STRING:
	    if (socket_read_block (sock, (char *)&tmp, sizeof (tmp)) == 0)
		return 0;
	    len = ntohl (tmp);
	    if (cmd == RPC_LIMITED_STRING)
		if (len > 16*1024){
		    /* silently die */
		    abort ();
		}
	    if (len > 128*1024)
		    abort ();
	    text = malloc (len+1);
	    if (socket_read_block (sock, text, len) == 0)
		return 0;
	    str_dest = va_arg (ap, char **);
	    *str_dest = text;
	    text [len] = 0;
	    break;	    

	case RPC_BLOCK:
	    len = va_arg (ap, int);
	    text = va_arg (ap, char *);
	    if (socket_read_block (sock, text, len) == 0)
		return 0;
	    break;

	default:
	    fprintf (stderr, "Unknown rpc message\n");
	    abort ();
	}
    }
}

void rpc_add_get_callback (int sock, void (*cback)(int))
{
    sock_callback_t *new;

    new = malloc (sizeof (sock_callback_t));
    new->cback = cback;
    new->sock = sock;
    new->link = sock_callbacks;
    sock_callbacks = new;
}

#if defined(IS_AIX) || defined(linux) || defined(SCO_FLAVOR) || defined(__QNX__)
static void sig_pipe (int unused)
#else
static void sig_pipe (void)
#endif
{
    got_sigpipe = 1;
}

void tcp_init (void)
{
    struct sigaction sa;
    
    got_sigpipe = 0;
    sa.sa_handler = sig_pipe;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGPIPE, &sa, NULL);
}

int get_remote_port (struct sockaddr_in *sin, int *version)
{
    int              port;
#ifdef HAVE_PMAP_GETMAPS
    struct pmaplist  *pl;
    
    *version = 1;
    port     = mcserver_port;
    for (pl = pmap_getmaps (sin); pl; pl = pl->pml_next)
	if (pl->pml_map.pm_prog == RPC_PROGNUM &&
	    pl->pml_map.pm_prot == IPPROTO_TCP &&
	    pl->pml_map.pm_vers >= *version) {
	    *version = pl->pml_map.pm_vers;
	    port     = pl->pml_map.pm_port;
	}
    return port;
#else
#ifdef HAVE_PMAP_GETPORT
    for (*version = RPC_PROGVER; *version >= 1; (*version)--)
	if (port = pmap_getport (sin, RPC_PROGNUM, *version, IPPROTO_TCP))
	    return port;
#endif /* HAVE_PMAP_GETPORT */
#endif /* HAVE_PMAP_GETMAPS */
    *version = 1;
    return mcserver_port;
}


