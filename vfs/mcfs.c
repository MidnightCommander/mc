/* Virtual File System: Midnight Commander file system.
   
   Copyright (C) 1995, 1996, 1997 The Free Software Foundation

   Written by Miguel de Icaza
              Andrej Borsenkow
	      Norbert Warmuth
   
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

/* Namespace: exports mcfs_vfs_ops, tcp_invalidate_socket */

#include <config.h>

#ifdef WITH_MCFS
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pwd.h>
#include <sys/types.h>		/* POSIX-required by sys/socket.h and netdb.h */
#include <netdb.h>		/* struct hostent */
#include <sys/socket.h>		/* AF_INET */
#include <netinet/in.h>		/* struct in_addr */
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

#include "utilvfs.h"

#include "vfs.h"
#include "mcfs.h"
#include "mcfsutil.h"
#include "tcputil.h"

#define MCFS_MAX_CONNECTIONS 32

static struct _mcfs_connection {
    char *host;
    char *user;
    char *home;
    int sock;
    int port;
    int version;
} mcfs_connections[MCFS_MAX_CONNECTIONS];


#define mcserver_port 9876

typedef struct _mcfs_connection mcfs_connection;

typedef struct {
    int handle;
    mcfs_connection *conn;
} mcfs_handle;

static char *mcfs_gethome (mcfs_connection * mc);
static int my_errno;
static struct vfs_class vfs_mcfs_ops;

/* Extract the hostname and username from the path */
/* path is in the form: hostname:user/remote-dir */
static char *
mcfs_get_host_and_username (const char *path, char **host, char **user,
			    int *port, char **pass)
{
    return vfs_split_url (path, host, user, port, pass, 0, 0);
}

static void
mcfs_fill_names (struct vfs_class *me, void (*func) (char *))
{
    int i;
    char *name;

    for (i = 0; i < MCFS_MAX_CONNECTIONS; i++) {
	if (mcfs_connections[i].host == 0)
	    continue;
	name = g_strconcat ("/#mc:", mcfs_connections[i].user,
			    "@", mcfs_connections[i].host, NULL);
	(*func) (name);
	g_free (name);
    }
}

/* This routine checks the server RPC version and logs the user in */
static int
mcfs_login_server (int my_socket, char *user, int port,
		   int port_autodetected, char *netrcpass, int *version)
{
    int result;
    char *pass;

    /* Send the version number */
    rpc_send (my_socket, RPC_INT, *version, RPC_END);
    if (0 == rpc_get (my_socket, RPC_INT, &result, RPC_END))
	return 0;

    if (result != MC_VERSION_OK) {
	message (1, _(" MCFS "),
		    _(" The server does not support this version "));
	close (my_socket);
	return 0;
    }

    /* FIXME: figure out why last_current_dir used to be passed here */
    rpc_send (my_socket, RPC_INT, MC_LOGIN, RPC_STRING, "/",
	      RPC_STRING, user, RPC_END);

    if (0 == rpc_get (my_socket, RPC_INT, &result, RPC_END))
	return 0;

    if (result == MC_NEED_PASSWORD) {
	if (port > 1024 && port_autodetected) {
	    int v;
	    v = query_dialog (_("Warning"),
			      _
			      (" The remote server is not running on a system port \n"
			       " you need a password to log in, but the information may \n"
			       " not be safe on the remote side.  Continue? \n"),
			      3, 2, _("&Yes"), _("&No"));

	    if (v == 1) {
		close (my_socket);
		return 0;
	    }
	}
	if (netrcpass != NULL)
	    pass = g_strdup (netrcpass);
	else
	    pass = vfs_get_password (_(" MCFS Password required "));
	if (!pass) {
	    rpc_send (my_socket, RPC_INT, MC_QUIT, RPC_END);
	    close (my_socket);
	    return 0;
	}
	rpc_send (my_socket, RPC_INT, MC_PASS, RPC_STRING, pass, RPC_END);

	wipe_password (pass);

	if (0 == rpc_get (my_socket, RPC_INT, &result, RPC_END))
	    return 0;

	if (result != MC_LOGINOK) {
	    message (1, _(" MCFS "), _(" Invalid password "));
	    rpc_send (my_socket, RPC_INT, MC_QUIT, RPC_END);
	    close (my_socket);
	    return 0;
	}
    }
    return my_socket;
}

static int
mcfs_get_remote_port (struct sockaddr_in *sin, int *version)
{
#ifdef HAVE_PMAP_GETMAPS
    int port;
    struct pmaplist *pl;

    *version = 1;
    port = mcserver_port;
    for (pl = pmap_getmaps (sin); pl; pl = pl->pml_next)
	if (pl->pml_map.pm_prog == RPC_PROGNUM
	    && pl->pml_map.pm_prot == IPPROTO_TCP
	    && pl->pml_map.pm_vers >= *version) {
	    *version = pl->pml_map.pm_vers;
	    port = pl->pml_map.pm_port;
	}
    return port;
#else
#ifdef HAVE_PMAP_GETPORT
    int port;
    for (*version = RPC_PROGVER; *version >= 1; (*version)--)
	if (port = pmap_getport (sin, RPC_PROGNUM, *version, IPPROTO_TCP))
	    return port;
#endif				/* HAVE_PMAP_GETPORT */
#endif				/* HAVE_PMAP_GETMAPS */
    *version = 1;
    return mcserver_port;
}

/* This used to be in utilvfs.c, but as it deals with portmapper, it
   is probably useful for mcfs */
static int
mcfs_create_tcp_link (char *host, int *port, int *version, char *caller)
{
    struct sockaddr_in server_address;
    unsigned long inaddr;
    struct hostent *hp;
    int my_socket;

    if (!*host)
	return 0;

    memset ((char *) &server_address, 0, sizeof (server_address));
    server_address.sin_family = AF_INET;

    /*  Try to use the dotted decimal number */
    if ((inaddr = inet_addr (host)) != -1)
	memcpy ((char *) &server_address.sin_addr, (char *) &inaddr,
		sizeof (inaddr));
    else {
	if ((hp = gethostbyname (host)) == NULL) {
	    message (1, caller, _(" Cannot locate hostname: %s "),
			host);
	    return 0;
	}
	memcpy ((char *) &server_address.sin_addr, (char *) hp->h_addr,
		hp->h_length);
    }

    /* Try to contact a remote portmapper to obtain the listening port */
    if (*port == 0) {
	*port = mcfs_get_remote_port (&server_address, version);
	if (*port < 1)
	    return 0;
    } else
	*version = 1;

    server_address.sin_port = htons (*port);

    if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	message (1, caller, _(" Cannot create socket: %s "),
		    unix_error_string (errno));
	return 0;
    }
    if (connect (my_socket, (struct sockaddr *) &server_address,
		 sizeof (server_address)) < 0) {
	message (1, caller, _(" Cannot connect to server: %s "),
		    unix_error_string (errno));
	close (my_socket);
	return 0;
    }
    return my_socket;
}

static int
mcfs_open_tcp_link (char *host, char *user,
		    int *port, char *netrcpass, int *version)
{
    int my_socket;
    int old_port = *port;

    my_socket = mcfs_create_tcp_link (host, port, version, " MCfs ");
    if (my_socket <= 0)
	return 0;

    /* We got the connection to the server, verify if the server
       implements our version of the RPC mechanism and then login
       the user.
     */
    return mcfs_login_server (my_socket, user, *port, old_port == 0,
			      netrcpass, version);
}

static int mcfs_get_free_bucket_init = 1;
static mcfs_connection *
mcfs_get_free_bucket (void)
{
    int i;

    if (mcfs_get_free_bucket_init) {
	mcfs_get_free_bucket_init = 0;
	for (i = 0; i < MCFS_MAX_CONNECTIONS; i++)
	    mcfs_connections[i].host = 0;
    }
    for (i = 0; i < MCFS_MAX_CONNECTIONS; i++) {
	if (!mcfs_connections[i].host)
	    return &mcfs_connections[i];
    }
    /* This can't happend, since we have checked for max connections before */
    vfs_die ("Internal error: mcfs_get_free_bucket");
    return 0;			/* shut up, stupid gcc */
}

/* This routine keeps track of open connections */
/* Returns a connected socket to host */
static mcfs_connection *
mcfs_open_link (char *host, char *user, int *port, char *netrcpass)
{
    static int mcfs_open_connections = 0;
    int i, sock, version;
    mcfs_connection *bucket;

    /* Is the link actually open? */
    if (mcfs_get_free_bucket_init) {
	mcfs_get_free_bucket_init = 0;
	for (i = 0; i < MCFS_MAX_CONNECTIONS; i++)
	    mcfs_connections[i].host = 0;
    } else
	for (i = 0; i < MCFS_MAX_CONNECTIONS; i++) {
	    if (!mcfs_connections[i].host)
		continue;
	    if ((strcmp (host, mcfs_connections[i].host) == 0) &&
		(strcmp (user, mcfs_connections[i].user) == 0))
		return &mcfs_connections[i];
	}
    if (mcfs_open_connections == MCFS_MAX_CONNECTIONS) {
	message (1, MSG_ERROR, _(" Too many open connections "));
	return 0;
    }

    if (!
	(sock =
	 mcfs_open_tcp_link (host, user, port, netrcpass, &version)))
	return 0;

    bucket = mcfs_get_free_bucket ();
    mcfs_open_connections++;
    bucket->host = g_strdup (host);
    bucket->user = g_strdup (user);
    bucket->home = 0;
    bucket->port = *port;
    bucket->sock = sock;
    bucket->version = version;

    return bucket;
}

static int
mcfs_is_error (int result, int errno_num)
{
    if (!(result == -1))
	return my_errno = 0;
    else
	my_errno = errno_num;
    return 1;
}

static int
mcfs_set_error (int result, int errno_num)
{
    if (result == -1)
	my_errno = errno_num;
    else
	my_errno = 0;
    return result;
}

static char *
mcfs_get_path (mcfs_connection **mc, const char *path)
{
    char *user, *host, *remote_path;
    char *pass;
    int port;

    /* An absolute path name, try to determine connection socket */
    if (strncmp (path, "/#mc:", 5))
	return NULL;
    path += 5;

    /* Port = 0 means that mcfs_create_tcp_link will try to contact the
     * remote portmapper to get the port number
     */
    port = 0;
    if ((remote_path =
	 mcfs_get_host_and_username (path, &host, &user, &port, &pass)))
	if (!(*mc = mcfs_open_link (host, user, &port, pass))) {
	    g_free (remote_path);
	    remote_path = NULL;
	}
    g_free (host);
    g_free (user);
    if (pass)
	wipe_password (pass);

    if (!remote_path)
	return NULL;

    /* NOTE: tildes are deprecated. See ftpfs.c */
    {
	int f = !strcmp (remote_path, "/~");
	if (f || !strncmp (remote_path, "/~/", 3)) {
	    char *s;
	    s = concat_dir_and_file (mcfs_gethome (*mc),
				     remote_path + 3 - f);
	    g_free (remote_path);
	    remote_path = s;
	}
    }
    return remote_path;
}

/* Simple function for routines returning only an integer from the server */
static int
mcfs_handle_simple_error (int sock, int return_status)
{
    int status, error;

    if (0 == rpc_get (sock, RPC_INT, &status, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    if (mcfs_is_error (status, error))
	return -1;
    if (return_status)
	return status;
    return 0;
}

/* Nice wrappers */
static int
mcfs_rpc_two_paths (int command, char *s1, char *s2)
{
    mcfs_connection *mc;
    char *r1, *r2;

    if ((r1 = mcfs_get_path (&mc, s1)) == 0)
	return -1;

    if ((r2 = mcfs_get_path (&mc, s2)) == 0) {
	g_free (r1);
	return -1;
    }

    rpc_send (mc->sock,
	      RPC_INT, command, RPC_STRING, r1, RPC_STRING, r2, RPC_END);
    g_free (r1);
    g_free (r2);
    return mcfs_handle_simple_error (mc->sock, 0);
}

static int
mcfs_rpc_path (int command, char *path)
{
    mcfs_connection *mc;
    char *remote_file;

    if ((remote_file = mcfs_get_path (&mc, path)) == 0)
	return -1;

    rpc_send (mc->sock,
	      RPC_INT, command, RPC_STRING, remote_file, RPC_END);

    g_free (remote_file);
    return mcfs_handle_simple_error (mc->sock, 0);
}

static int
mcfs_rpc_path_int (int command, char *path, int data)
{
    mcfs_connection *mc;
    char *remote_file;

    if ((remote_file = mcfs_get_path (&mc, path)) == 0)
	return -1;

    rpc_send (mc->sock,
	      RPC_INT, command,
	      RPC_STRING, remote_file, RPC_INT, data, RPC_END);

    g_free (remote_file);
    return mcfs_handle_simple_error (mc->sock, 0);
}

static int
mcfs_rpc_path_int_int (int command, char *path, int n1, int n2)
{
    mcfs_connection *mc;
    char *remote_file;

    if ((remote_file = mcfs_get_path (&mc, path)) == 0)
	return -1;

    rpc_send (mc->sock,
	      RPC_INT, command,
	      RPC_STRING, remote_file, RPC_INT, n1, RPC_INT, n2, RPC_END);

    g_free (remote_file);
    return mcfs_handle_simple_error (mc->sock, 0);
}

static char *
mcfs_gethome (mcfs_connection *mc)
{
    char *buffer;

    if (mc->home)
	return g_strdup (mc->home);
    else {
	rpc_send (mc->sock, RPC_INT, MC_GETHOME, RPC_END);
	if (0 == rpc_get (mc->sock, RPC_STRING, &buffer, RPC_END))
	    return g_strdup (PATH_SEP_STR);
	mc->home = buffer;
	return g_strdup (buffer);
    }
}

/* The callbacks */
static void *
mcfs_open (struct vfs_class *me, const char *file, int flags, int mode)
{
    char *remote_file;
    mcfs_connection *mc;
    int result, error_num;
    mcfs_handle *remote_handle;

    if (!(remote_file = mcfs_get_path (&mc, file)))
	return 0;

    rpc_send (mc->sock, RPC_INT, MC_OPEN, RPC_STRING, remote_file, RPC_INT,
	      flags, RPC_INT, mode, RPC_END);
    g_free (remote_file);

    if (0 ==
	rpc_get (mc->sock, RPC_INT, &result, RPC_INT, &error_num, RPC_END))
	return 0;

    if (mcfs_is_error (result, error_num))
	return 0;

    remote_handle = g_new (mcfs_handle, 2);
    remote_handle->handle = result;
    remote_handle->conn = mc;

    return remote_handle;
}

static int
mcfs_read (void *data, char *buffer, int count)
{
    mcfs_handle *info = (mcfs_handle *) data;
    int result, error;
    int handle;
    mcfs_connection *mc;

    mc = info->conn;
    handle = info->handle;

    rpc_send (mc->sock, RPC_INT, MC_READ, RPC_INT, handle,
	      RPC_INT, count, RPC_END);

    if (0 ==
	rpc_get (mc->sock, RPC_INT, &result, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    if (mcfs_is_error (result, error))
	return 0;

    if (0 == rpc_get (mc->sock, RPC_BLOCK, result, buffer, RPC_END))
	return mcfs_set_error (-1, EIO);

    return result;
}

static int
mcfs_write (void *data, char *buf, int nbyte)
{
    mcfs_handle *info = (mcfs_handle *) data;
    mcfs_connection *mc;
    int handle;

    mc = info->conn;
    handle = info->handle;

    rpc_send (mc->sock,
	      RPC_INT, MC_WRITE,
	      RPC_INT, handle,
	      RPC_INT, nbyte, RPC_BLOCK, nbyte, buf, RPC_END);

    return mcfs_handle_simple_error (mc->sock, 1);
}

static int
mcfs_close (void *data)
{
    mcfs_handle *info = (mcfs_handle *) data;
    mcfs_connection *mc;
    int handle, result, error;

    if (!data)
	return -1;

    handle = info->handle;
    mc = info->conn;

    rpc_send (mc->sock, RPC_INT, MC_CLOSE, RPC_INT, handle, RPC_END);

    if (0 ==
	rpc_get (mc->sock, RPC_INT, &result, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    mcfs_is_error (result, error);

    g_free (data);
    return result;
}

static int
mcfs_errno (struct vfs_class *me)
{
    return my_errno;
}

typedef struct dir_entry {
    char *text;
    struct dir_entry *next;
    struct stat my_stat;
    int merrno;
} dir_entry;

typedef struct {
    mcfs_connection *conn;
    int handle;
    dir_entry *entries;
    dir_entry *current;
} opendir_info;

static void *
mcfs_opendir (struct vfs_class *me, char *dirname)
{
    opendir_info *mcfs_info;
    mcfs_connection *mc;
    int handle, error_num;
    char *remote_dir;
    int result;

    if (!(remote_dir = mcfs_get_path (&mc, dirname)))
	return 0;

    rpc_send (mc->sock, RPC_INT, MC_OPENDIR, RPC_STRING, remote_dir,
	      RPC_END);
    g_free (remote_dir);

    if (0 ==
	rpc_get (mc->sock, RPC_INT, &result, RPC_INT, &error_num, RPC_END))
	return 0;

    if (mcfs_is_error (result, error_num))
	return 0;

    handle = result;

    mcfs_info = g_new (opendir_info, 1);
    mcfs_info->conn = mc;
    mcfs_info->handle = handle;
    mcfs_info->entries = 0;
    mcfs_info->current = 0;

    return mcfs_info;
}

static int mcfs_get_stat_info (mcfs_connection * mc, struct stat *buf);

static int
mcfs_loaddir (opendir_info *mcfs_info)
{
    int status, error;
    mcfs_connection *mc = mcfs_info->conn;
    int link = mc->sock;
    int first = 1;

    rpc_send (link, RPC_INT, MC_READDIR, RPC_INT, mcfs_info->handle,
	      RPC_END);

    for (;;) {
	int entry_len;
	dir_entry *new_entry;

	if (!rpc_get (link, RPC_INT, &entry_len, RPC_END))
	    return 0;

	if (entry_len == 0)
	    break;

	new_entry = g_new (dir_entry, 1);
	new_entry->text = g_new0 (char, entry_len + 1);

	new_entry->next = 0;
	if (first) {
	    mcfs_info->entries = new_entry;
	    mcfs_info->current = new_entry;
	    first = 0;
	} else {
	    mcfs_info->current->next = new_entry;
	    mcfs_info->current = new_entry;
	}

	if (!rpc_get
	    (link, RPC_BLOCK, entry_len, new_entry->text, RPC_END))
	    return 0;

	/* Then we get the status from the lstat */
	if (!rpc_get (link, RPC_INT, &status, RPC_INT, &error, RPC_END))
	    return 0;

	if (mcfs_is_error (status, error))
	    new_entry->merrno = error;
	else {
	    new_entry->merrno = 0;
	    if (!mcfs_get_stat_info (mc, &(new_entry->my_stat)))
		return 0;
	}
    }
    mcfs_info->current = mcfs_info->entries;

    return 1;
}

static void
mcfs_free_dir (dir_entry *de)
{
    if (!de)
	return;
    mcfs_free_dir (de->next);
    g_free (de->text);
    g_free (de);
}

static union vfs_dirent mcfs_readdir_data;

/* The readdir routine loads the complete directory */
/* It's too slow to ask the server each time */
/* It now also sends the complete lstat information for each file */
static struct stat *cached_lstat_info;

static void *
mcfs_readdir (void *info)
{
    opendir_info *mcfs_info;
    char *dirent_dest;

    mcfs_info = (opendir_info *) info;

    if (!mcfs_info->entries)
	if (!mcfs_loaddir (mcfs_info))
	    return NULL;

    if (mcfs_info->current == 0) {
	cached_lstat_info = 0;
	mcfs_free_dir (mcfs_info->entries);
	mcfs_info->entries = 0;
	return NULL;
    }
    dirent_dest = mcfs_readdir_data.dent.d_name;
    strncpy (dirent_dest, mcfs_info->current->text, MC_MAXPATHLEN);
    dirent_dest[MC_MAXPATHLEN] = 0;
    cached_lstat_info = &mcfs_info->current->my_stat;
    mcfs_info->current = mcfs_info->current->next;

    compute_namelen (&mcfs_readdir_data.dent);

    return &mcfs_readdir_data;
}

static int
mcfs_closedir (void *info)
{
    opendir_info *mcfs_info = (opendir_info *) info;
    dir_entry *p, *q;

    rpc_send (mcfs_info->conn->sock, RPC_INT, MC_CLOSEDIR,
	      RPC_INT, mcfs_info->handle, RPC_END);

    for (p = mcfs_info->entries; p;) {
	q = p;
	p = p->next;
	g_free (q->text);
	g_free (q);
    }
    g_free (info);
    return 0;
}

static time_t
mcfs_get_time (mcfs_connection *mc)
{
    int sock = mc->sock;

    if (mc->version == 1) {
	struct tm tt;

	rpc_get (sock,
		 RPC_INT, &tt.tm_sec,
		 RPC_INT, &tt.tm_min,
		 RPC_INT, &tt.tm_hour,
		 RPC_INT, &tt.tm_mday,
		 RPC_INT, &tt.tm_year, RPC_INT, &tt.tm_mon, RPC_END);
	tt.tm_year -= 1900;
	tt.tm_isdst = 0;

	return mktime (&tt);
    } else {
	char *buf;
	long tm;

	rpc_get (sock, RPC_STRING, &buf, RPC_END);
	sscanf (buf, "%lx", &tm);
	g_free (buf);

	return (time_t) tm;
    }
}

static int
mcfs_get_stat_info (mcfs_connection *mc, struct stat *buf)
{
    long mylong;
    int sock = mc->sock;

    buf->st_dev = 0;

    rpc_get (sock, RPC_INT, &mylong, RPC_END);
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    buf->st_rdev = mylong;
#endif
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_ino = mylong;
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_mode = mylong;
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_nlink = mylong;
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_uid = mylong;
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_gid = mylong;
    rpc_get (sock, RPC_INT, &mylong, RPC_END);
    buf->st_size = mylong;

    if (!rpc_get (sock, RPC_INT, &mylong, RPC_END))
	return 0;
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    buf->st_blocks = mylong;
#endif
    buf->st_atime = mcfs_get_time (mc);
    buf->st_mtime = mcfs_get_time (mc);
    buf->st_ctime = mcfs_get_time (mc);
    return 1;
}

static int
mcfs_stat_cmd (int cmd, char *path, struct stat *buf)
{
    char *remote_file;
    mcfs_connection *mc;
    int status, error;

    if ((remote_file = mcfs_get_path (&mc, path)) == 0)
	return -1;

    rpc_send (mc->sock, RPC_INT, cmd, RPC_STRING, remote_file, RPC_END);
    g_free (remote_file);
    if (!rpc_get (mc->sock, RPC_INT, &status, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, errno);

    if (mcfs_is_error (status, error))
	return -1;

    if (mcfs_get_stat_info (mc, buf))
	return 0;
    else
	return mcfs_set_error (-1, EIO);
}

static int
mcfs_stat (struct vfs_class *me, char *path, struct stat *buf)
{
    return mcfs_stat_cmd (MC_STAT, path, buf);
}

static int
mcfs_lstat (struct vfs_class *me, char *path, struct stat *buf)
{
    int path_len = strlen (path);
    int entry_len = strlen (mcfs_readdir_data.dent.d_name);

    /* Hack ... */
    if (strcmp (path + path_len - entry_len,
		mcfs_readdir_data.dent.d_name) == 0 && cached_lstat_info) {
	*buf = *cached_lstat_info;
	return 0;
    }
    return mcfs_stat_cmd (MC_LSTAT, path, buf);
}

static int
mcfs_fstat (void *data, struct stat *buf)
{
    mcfs_handle *info = (mcfs_handle *) data;
    int result, error;
    int handle, sock;

    sock = info->conn->sock;
    handle = info->handle;

    rpc_send (sock, RPC_INT, MC_FSTAT, RPC_INT, handle, RPC_END);
    if (!rpc_get (sock, RPC_INT, &result, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    if (mcfs_is_error (result, error))
	return -1;

    if (mcfs_get_stat_info (info->conn, buf))
	return 0;
    else
	return mcfs_set_error (-1, EIO);
}

static int
mcfs_chmod (struct vfs_class *me, char *path, int mode)
{
    return mcfs_rpc_path_int (MC_CHMOD, path, mode);
}

static int
mcfs_chown (struct vfs_class *me, char *path, int owner, int group)
{
    return mcfs_rpc_path_int_int (MC_CHOWN, path, owner, group);
}

static int
mcfs_utime (struct vfs_class *me, char *path, struct utimbuf *times)
{
    mcfs_connection *mc;
    int status;
    char *file;

    if (!(file = mcfs_get_path (&mc, path)))
	return -1;

    status = 0;
    if (mc->version >= 2) {
	char abuf[BUF_SMALL];
	char mbuf[BUF_SMALL];
	long atime, mtime;

	atime = (long) times->actime;
	mtime = (long) times->modtime;

	g_snprintf (abuf, sizeof (abuf), "%lx", atime);
	g_snprintf (mbuf, sizeof (mbuf), "%lx", mtime);

	rpc_send (mc->sock, RPC_INT, MC_UTIME,
		  RPC_STRING, file,
		  RPC_STRING, abuf, RPC_STRING, mbuf, RPC_END);
	status = mcfs_handle_simple_error (mc->sock, 0);

    }
    g_free (file);
    return (status);
}

static int
mcfs_readlink (struct vfs_class *me, char *path, char *buf, int size)
{
    char *remote_file, *stat_str;
    int status, error;
    mcfs_connection *mc;

    if (!(remote_file = mcfs_get_path (&mc, path)))
	return -1;

    rpc_send (mc->sock, RPC_INT, MC_READLINK, RPC_STRING, remote_file,
	      RPC_END);
    g_free (remote_file);
    if (!rpc_get (mc->sock, RPC_INT, &status, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    if (mcfs_is_error (status, errno))
	return -1;

    if (!rpc_get (mc->sock, RPC_STRING, &stat_str, RPC_END))
	return mcfs_set_error (-1, EIO);

    status = strlen (stat_str);
    if (status < size)
	size = status;
    strncpy (buf, stat_str, size);
    g_free (stat_str);
    return size;
}

static int
mcfs_unlink (struct vfs_class *me, char *path)
{
    return mcfs_rpc_path (MC_UNLINK, path);
}

static int
mcfs_symlink (struct vfs_class *me, char *n1, char *n2)
{
    return mcfs_rpc_two_paths (MC_SYMLINK, n1, n2);
}

static int
mcfs_rename (struct vfs_class *me, char *a, char *b)
{
    return mcfs_rpc_two_paths (MC_RENAME, a, b);
}

static int
mcfs_chdir (struct vfs_class *me, char *path)
{
    char *remote_dir;
    mcfs_connection *mc;
    int status, error;

    if (!(remote_dir = mcfs_get_path (&mc, path)))
	return -1;

    rpc_send (mc->sock, RPC_INT, MC_CHDIR, RPC_STRING, remote_dir,
	      RPC_END);
    g_free (remote_dir);
    if (!rpc_get (mc->sock, RPC_INT, &status, RPC_INT, &error, RPC_END))
	return mcfs_set_error (-1, EIO);

    if (mcfs_is_error (status, error))
	return -1;
    return 0;
}

static int
mcfs_lseek (void *data, off_t offset, int whence)
{
    mcfs_handle *info = (mcfs_handle *) data;
    int handle, sock;

    sock = info->conn->sock;
    handle = info->handle;

    /* FIXME: off_t may be too long to fit */
    rpc_send (sock, RPC_INT, MC_LSEEK, RPC_INT, handle, RPC_INT,
	      (int) offset, RPC_INT, whence, RPC_END);

    return mcfs_handle_simple_error (sock, 1);
}

static int
mcfs_mknod (struct vfs_class *me, char *path, int mode, int dev)
{
    return mcfs_rpc_path_int_int (MC_MKNOD, path, mode, dev);
}

static int
mcfs_mkdir (struct vfs_class *me, char *path, mode_t mode)
{
    return mcfs_rpc_path_int (MC_MKDIR, path, mode);
}

static int
mcfs_rmdir (struct vfs_class *me, char *path)
{
    return mcfs_rpc_path (MC_RMDIR, path);
}

static int
mcfs_link (struct vfs_class *me, char *p1, char *p2)
{
    return mcfs_rpc_two_paths (MC_LINK, p1, p2);
}

/* Gives up on a socket and reopnes the connection, the child own the socket
 * now
 */
static void
mcfs_forget (char *path)
{
    char *host, *user, *pass, *p;
    int port, i, vers;

    if (strncmp (path, "/#mc:", 5))
	return;

    path += 5;
    if (path[0] == '/' && path[1] == '/')
	path += 2;

    if ((p =
	 mcfs_get_host_and_username (path, &host, &user, &port,
				     &pass)) == 0) {
	g_free (host);
	g_free (user);
	if (pass)
	    wipe_password (pass);
	return;
    }
    for (i = 0; i < MCFS_MAX_CONNECTIONS; i++) {
	if ((strcmp (host, mcfs_connections[i].host) == 0) &&
	    (strcmp (user, mcfs_connections[i].user) == 0) &&
	    (port == mcfs_connections[i].port)) {

	    /* close socket: the child owns it now */
	    close (mcfs_connections[i].sock);

	    /* reopen the connection */
	    mcfs_connections[i].sock =
		mcfs_open_tcp_link (host, user, &port, pass, &vers);
	}
    }
    g_free (p);
    g_free (host);
    g_free (user);
    if (pass)
	wipe_password (pass);
}

static int
mcfs_setctl (struct vfs_class *me, char *path, int ctlop, void *arg)
{
    switch (ctlop) {
    case VFS_SETCTL_FORGET:
	mcfs_forget (path);
	return 0;
    }
    return 0;
}

void
init_mcfs (void)
{
    vfs_mcfs_ops.name = "mcfs";
    vfs_mcfs_ops.prefix = "mc:";
    vfs_mcfs_ops.fill_names = mcfs_fill_names;
    vfs_mcfs_ops.open = mcfs_open;
    vfs_mcfs_ops.close = mcfs_close;
    vfs_mcfs_ops.read = mcfs_read;
    vfs_mcfs_ops.write = mcfs_write;
    vfs_mcfs_ops.opendir = mcfs_opendir;
    vfs_mcfs_ops.readdir = mcfs_readdir;
    vfs_mcfs_ops.closedir = mcfs_closedir;
    vfs_mcfs_ops.stat = mcfs_stat;
    vfs_mcfs_ops.lstat = mcfs_lstat;
    vfs_mcfs_ops.fstat = mcfs_fstat;
    vfs_mcfs_ops.chmod = mcfs_chmod;
    vfs_mcfs_ops.chown = mcfs_chown;
    vfs_mcfs_ops.utime = mcfs_utime;
    vfs_mcfs_ops.readlink = mcfs_readlink;
    vfs_mcfs_ops.symlink = mcfs_symlink;
    vfs_mcfs_ops.link = mcfs_link;
    vfs_mcfs_ops.unlink = mcfs_unlink;
    vfs_mcfs_ops.rename = mcfs_rename;
    vfs_mcfs_ops.chdir = mcfs_chdir;
    vfs_mcfs_ops.ferrno = mcfs_errno;
    vfs_mcfs_ops.lseek = mcfs_lseek;
    vfs_mcfs_ops.mknod = mcfs_mknod;
    vfs_mcfs_ops.mkdir = mcfs_mkdir;
    vfs_mcfs_ops.rmdir = mcfs_rmdir;
    vfs_mcfs_ops.setctl = mcfs_setctl;
    vfs_register_class (&vfs_mcfs_ops);
}

static void
mcfs_free_bucket (int bucket)
{
    g_free (mcfs_connections[bucket].host);
    g_free (mcfs_connections[bucket].user);
    g_free (mcfs_connections[bucket].home);

    /* Set all the fields to zero */
    mcfs_connections[bucket].host =
	mcfs_connections[bucket].user = mcfs_connections[bucket].home = 0;
    mcfs_connections[bucket].sock = mcfs_connections[bucket].version = 0;
}

static int
mcfs_invalidate_socket (int sock)
{
    int i, j = -1;

    for (i = 0; i < MCFS_MAX_CONNECTIONS; i++)
	if (mcfs_connections[i].sock == sock) {
	    mcfs_free_bucket (i);
	    j = 0;
	}

    if (j == -1)
	return -1;		/* It was not our sock */
    /* Break from any possible loop */
    mc_chdir ("/");
    return 0;
}

void
tcp_invalidate_socket (int sock)
{
    mcfs_invalidate_socket (sock);
}

#endif				/* WITH_MCFS */
