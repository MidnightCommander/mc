/* Virtual File System: FTP file system.
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Ching Hui
               1995 Jakub Jelinek
               1995, 1996, 1997 Miguel de Icaza
	       1997 Norbert Warmuth
	       1998 Pavel Machek
   
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
   
/* FTPfs TODO:

- make it more robust - all the connects etc. should handle EADDRINUSE and
  ERETRY (have I spelled these names correctly?)
- make the user able to flush a connection - all the caches will get empty
  etc., (tarfs as well), we should give there a user selectable timeout
  and assign a key sequence.  
- use hash table instead of linklist to cache ftpfs directory.
 */

/* Namespace pollution: horrible */

#include <config.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <pwd.h>
#include <ctype.h>	/* For isdigit */
#ifdef SCO_FLAVOR
#	include <sys/timeb.h>	/* alex: for struct timeb definition */
#endif /* SCO_FLAVOR */
#include <time.h>
#include <sys/types.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif
#include "../src/fs.h"
#include "../src/mad.h"
#include "../src/setup.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/main.h"
#include <netdb.h>		/* struct hostent */
#include <sys/socket.h>		/* AF_INET */
#include <netinet/in.h>		/* struct in_addr */
#ifdef HAVE_SETSOCKOPT
#    include <netinet/ip.h>	/* IP options */
#endif
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>
#ifndef SCO_FLAVOR
#	include <sys/time.h>	/* alex: this redefines struct timeval */
#endif /* SCO_FLAVOR */
#include <sys/param.h>
#undef MIN
#undef MAX
#include <glib.h>

#ifdef USE_TERMNET
#include <termnet.h>
#endif

#include "../src/mem.h"
#include "vfs.h"
#include "tcputil.h"
#include "../src/util.h"
#include "../src/dialog.h"
#include "container.h"
#include "ftpfs.h"
#ifndef MAXHOSTNAMELEN
#    define MAXHOSTNAMELEN 64
#endif

#define ERRNOR(x,y) do { my_errno = x; return y; } while(0)
#define UPLOAD_ZERO_LENGTH_FILE

static int my_errno;
static int code;

/* Delay to retry a connection */
int ftpfs_retry_seconds = 30;

/* Method to use to connect to ftp sites */
int ftpfs_use_passive_connections = 1;

/* Method used to get directory listings:
 * 1: try 'LIST -la <path>', if it fails
 *    fall back to CWD <path>; LIST 
 * 0: always use CWD <path>; LIST  
 */
int ftpfs_use_unix_list_options = 1;

/* Use the ~/.netrc */
int use_netrc = 1;

extern char *home_dir;

/* Anonymous setup */
char *ftpfs_anonymous_passwd = 0;
int ftpfs_directory_timeout;

/* Proxy host */
char *ftpfs_proxy_host = 0;

/* wether we have to use proxy by default? */
int ftpfs_always_use_proxy;

/* source routing host */
extern int source_route;

/* Where we store the transactions */
static FILE *logfile = NULL;

/* If true, the directory cache is forced to reload */
static int force_expiration = 0;

static struct linklist *connections_list;

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02
static char reply_str [80];

static struct direntry *_get_file_entry(struct connection *bucket, 
                char *file_name, int op, int flags);

static char    *ftpfs_get_current_directory (struct connection *bucket);
static int      ftpfs_chdir_internal        (struct connection *bucket,
					     char *remote_path);
static void     free_bucket                 (void *data);
static void     connection_destructor       (void *data);
static void     flush_all_directory         (struct connection *bucket);
static int      get_line                    (int sock, char *buf, int buf_len,
					     char term);
static char    *get_path                    (struct connection **bucket,
					     char *path);

/* Extract the hostname and username from the path */

/*
 * path is in the form: [user@]hostname:port/remote-dir, e.g.:
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * If the user is empty, e.g. ftp://@roxanne/private, then your login name
 * is supplied.
 *
 */

static char *
my_get_host_and_username (char *path, char **host, char **user, int *port, char **pass)
{
    return vfs_split_url (path, host, user, port, pass, 21, URL_DEFAULTANON);
}

/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */
static int
get_reply (int sock, char *string_buf, int string_len)
{
    char answer[1024];
    int i;
    
    for (;;) {
        if (!get_line(sock, answer, sizeof(answer), '\n')) {
	    if (string_buf)
		*string_buf = 0;
	    code = 421;
	    return 4;
	}
	switch(sscanf(answer, "%d", &code)) {
	    case 0:
	        if (string_buf) {
		    strncpy(string_buf, answer, string_len - 1);
		    *(string_buf + string_len - 1) = 0;
		}
	        code = 500;
	        return 5;
	    case 1:
 		if (answer[3] == '-') {
		    while (1) {
			if (!get_line(sock, answer, sizeof(answer), '\n')) {
			    if (string_buf)
				*string_buf = 0;
			    code = 421;
			    return 4;
			}
			if ((sscanf(answer, "%d", &i) > 0) && 
			    (code == i) && (answer[3] == ' '))
			    break;
		    }
		}
	        if (string_buf) {
		    strncpy(string_buf, answer, string_len - 1);
		    *(string_buf + string_len - 1) = 0;
		}
		return code / 100;
	}
    }
}

static int
command (struct connection *bucket, int wait_reply, char *fmt, ...)
{
    va_list ap;
    char *str, *fmt_str;
    int n, status;
    int sock = qsock (bucket);
    
    va_start (ap, fmt);

    fmt_str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    str = copy_strings (fmt_str, "\r\n", NULL);
    g_free (fmt_str);

    if (logfile){
        if (strncmp (str, "PASS ", 5) == 0){
            char *tmp = "PASS <Password not logged>\r\n";
	    
            fwrite (tmp, strlen (tmp), 1, logfile);
        } else
	    fwrite (str, strlen (str), 1, logfile);

	fflush (logfile);
    }

    got_sigpipe = 0;
    enable_interrupt_key ();
    status = write (sock, str, strlen (str));
    free (str);
    
    if (status < 0){
	code = 421;

	if (errno == EPIPE){
	    got_sigpipe = 1;
	}
	disable_interrupt_key ();
	return TRANSIENT;
    }
    disable_interrupt_key ();
    
    if (wait_reply)
	return get_reply (sock, (wait_reply & WANT_STRING) ? reply_str : NULL, sizeof (reply_str)-1);
    return COMPLETE;
}

static void
connection_close (void *data)
{
    struct connection *bucket = data;

    if (qsock (bucket) != -1){
	print_vfs_message ("ftpfs: Disconnecting from %s", qhost(bucket));
	command(bucket, NONE, "QUIT");
	close(qsock(bucket));
    }
}

/* some defines only used by changetype */
/* These two are valid values for the second parameter */
#define TYPE_ASCII    0
#define TYPE_BINARY   1

/* This one is only used to initialize bucket->isbinary, don't use it as
   second parameter to changetype. */
#define TYPE_UNKNOWN -1 

static int
changetype (struct connection *bucket, int binary)
{
    if (binary != bucket->isbinary) {
        if (command (bucket, WAIT_REPLY, "TYPE %c", binary ? 'I' : 'A') != COMPLETE) ERRNOR (EIO, -1);
        bucket->isbinary = binary;
    }
    return binary;
}

/* This routine logs the user in */
static int 
login_server (struct connection *bucket, char *netrcpass)
{
#if defined(HSC_PROXY)
    char *proxypass, *proxyname;
#endif
    char *pass;
    char *op;
    char *name;			/* login user name */
    int  anon = 0;

    bucket->isbinary = TYPE_UNKNOWN;    
    if (netrcpass)
        op = strdup (netrcpass);
    else {
        if (!strcmp (quser(bucket), "anonymous") || 
            !strcmp (quser(bucket), "ftp")) {
	    op = strdup(ftpfs_anonymous_passwd);
	    anon = 1;
         } else {
            char *p;

	    if (!bucket->password){
		p = copy_strings (" FTP: Password required for ", quser(bucket), 
				  " ", NULL);
		op = vfs_get_password (p);
		free (p);
		if (op == NULL) ERRNOR (EPERM, 0);
		bucket->password = strdup (op);
	    } else
		op = strdup (bucket->password);
        }
    }

    if (!anon || logfile)
	pass = strdup (op);
    else
	pass = copy_strings ("-", op, 0);
    wipe_password (op);

    
    /* Proxy server accepts: username@host-we-want-to-connect*/
    if (qproxy (bucket)){
#if defined(HSC_PROXY)
	char *p, *host;
	int port;
	p = my_get_host_and_username(ftpfs_proxy_host, &host, &proxyname,
					&port, &proxypass);
	if (p)
	    free (p);
	
	free(host);
	if (proxypass)
	    wipe_password (proxypass);
	p = copy_strings(" Proxy: Password required for ", proxyname, " ",
			 NULL);
	proxypass = vfs_get_password (p);
	free(p);
	if (proxypass == NULL) {
	    wipe_password (pass);
	    free (proxyname);
	    ERRNOR (EPERM, 0);
	}
	name = strdup(quser (bucket));
#else
	name = copy_strings (quser(bucket), "@", 
		qhost(bucket)[0] == '!' ? qhost(bucket)+1 : qhost(bucket), 0);
#endif
    } else 
	name = strdup (quser (bucket));
    
    if (get_reply (qsock(bucket), NULL, 0) == COMPLETE) {
#if defined(HSC_PROXY)
	if (qproxy(bucket)) {
	    print_vfs_message("ftpfs: sending proxy login name");
	    if (command (bucket, 1, "USER %s", proxyname) != CONTINUE)
		goto proxyfail;
	    print_vfs_message("ftpfs: sending proxy user password");
	    if (command (bucket, 1, "PASS %s", proxypass) != COMPLETE)
		goto proxyfail;
	    print_vfs_message("ftpfs: proxy authentication succeeded");
	    if (command (bucket, 1, "SITE %s", qhost(bucket)+1) != COMPLETE)
		goto proxyfail;
	    print_vfs_message("ftpfs: connected to %s", qhost(bucket)+1);
	    if (0) {
	    proxyfail:
		bucket->failed_on_login = 1;
		/* my_errno = E; */
		if (proxypass)
		    wipe_password (proxypass);
		wipe_password (pass);
		free (proxyname);
		free (name);
		ERRNOR (EPERM, 0);
	    }
	    if (proxypass)
		wipe_password (proxypass);
	    free (proxyname);
	}
#endif
	print_vfs_message("ftpfs: sending login name");
	code = command (bucket, WAIT_REPLY, "USER %s", name);

	switch (code){
	case CONTINUE:
	    print_vfs_message("ftpfs: sending user password");
            if (command (bucket, WAIT_REPLY, "PASS %s", pass) != COMPLETE)
		break;

	case COMPLETE:
	    print_vfs_message("ftpfs: logged in");
	    wipe_password (pass);
	    free (name);
	    return 1;

	default:
	    bucket->failed_on_login = 1;
	    /* my_errno = E; */
	    if (bucket->password)
		wipe_password (bucket->password);
	    bucket->password = 0;
	    
	    goto login_fail;
	}
    }
    print_vfs_message ("ftpfs: Login incorrect for user %s ", quser(bucket));
login_fail:
    wipe_password (pass);
    free (name);
    ERRNOR (EPERM, 0);
}

#ifdef HAVE_SETSOCKOPT
static void
setup_source_route (int socket, int dest)
{
    char buffer [20];
    char *ptr = buffer;

    if (!source_route)
	return;
    bzero (buffer, sizeof (buffer));
    *ptr++ = IPOPT_LSRR;
    *ptr++ = 3 + 8;
    *ptr++ = 4;			/* pointer */

    /* First hop */
    bcopy ((char *) &source_route, ptr, sizeof (int));
    ptr += 4;

    /* Second hop (ie, final destination) */
    bcopy ((char *) &dest, ptr, sizeof (int));
    ptr += 4;
    while ((ptr - buffer) & 3)
	ptr++;
    if (setsockopt (socket, IPPROTO_IP, IP_OPTIONS,
		    buffer, ptr - buffer) < 0)
	message_2s (1, MSG_ERROR, _(" Could not set source routing (%s)"), unix_error_string (errno));
}
#else
#define setup_source_route(x,y)
#endif
    
static struct no_proxy_entry {
    char  *domain;
    void  *next;
} *no_proxy;

static void
load_no_proxy_list ()
{
    /* FixMe: shouldn't be hardcoded!!! */
    char		s[258]; /* provide for 256 characters and nl */
    struct no_proxy_entry *np, *current = 0;
    FILE	*npf;
    int		c;
    char	*p;
    char 	*mc_file;
    static int	loaded;

    if (loaded)
	return;

    mc_file = concat_dir_and_file (mc_home, "mc.no_proxy");
    if (exist_file (mc_file) &&
	(npf = fopen (mc_file, "r"))) {
	while (fgets (s, 258, npf) || !(feof (npf) || ferror (npf))) {
	    if (!(p = strchr (s, '\n'))) {	/* skip bogus entries */ 
		while ((c = getc (npf)) != EOF && c != '\n')
		    ;
		continue;
	    }

	    if (p == s)
		continue;

	    *p = '\0';
	    p = xmalloc (strlen (s), "load_no_proxy_list:1");
	    np = xmalloc (sizeof (*np), "load_no_proxy_list:2");
	    strcpy (p, s);
	    np->domain = p;
	    np->next   = 0;
	    if (no_proxy)
		current->next = np;
	    else
		no_proxy = np;
	    current = np;
	}

	fclose (npf);
	loaded = 1;
    }
    free (mc_file);
}

static int
ftpfs_check_proxy (char *host)
{
    struct no_proxy_entry	*npe;

    if (!ftpfs_proxy_host || !*ftpfs_proxy_host || !host || !*host)
	return 0;		/* sanity check */

    if (*host == '!')
	return 1;
    
    if (!ftpfs_always_use_proxy)
	return 0;

    if (!strchr (host, '.'))
	return 0;

    load_no_proxy_list ();
    for (npe = no_proxy; npe; npe=npe->next) {
	char	*domain = npe->domain;

	if (domain[0] == '.') {
	    int		ld = strlen (domain);
	    int		lh = strlen (host);

	    while (ld && lh && host[lh - 1] == domain[ld - 1]) {
		ld--;
		lh--;
	    }

	    if (!ld)
		return 0;
	} else
	    if (!strcasecmp (host, domain))
		return 0;
    }

    return 1;
}

static void
ftpfs_get_proxy_host_and_port (char *proxy, char **host, int *port)
{
    char *user, *pass, *dir;

#if defined(HSC_PROXY)
#define PORT 9875
#else
#define PORT 21
#endif
    dir = vfs_split_url(proxy, host, &user, port, &pass, PORT, URL_DEFAULTANON);

    free(user);
    if (pass)
	wipe_password (pass);
    if (dir)
	free(dir);
}

static int
ftpfs_open_socket(struct connection *bucket)
{
    struct   sockaddr_in server_address;
    struct   hostent *hp;
    int      my_socket;
    char     *host;
    int      port;
    int      free_host = 0;
    
    /* Use a proxy host? */
    host = qhost(bucket);

    if (!host || !*host){
	print_vfs_message ("ftpfs: Invalid host name.");
        my_errno = EINVAL;
	return -1;
    }

    /* Hosts to connect to that start with a ! should use proxy */
    if (qproxy(bucket)) {
	ftpfs_get_proxy_host_and_port (ftpfs_proxy_host, &host, &port);
	free_host = 1;
    }
    
    /* Get host address */
    bzero ((char *) &server_address, sizeof (server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr (host);
    if (server_address.sin_addr.s_addr != -1)
	server_address.sin_family = AF_INET;
    else {
	hp = gethostbyname(host);
	if (hp == NULL){
	    print_vfs_message("ftpfs: Invalid host address.");
	    my_errno = EINVAL;
	    if (free_host)
		free (host);
	    return -1;
	}
	server_address.sin_family = hp->h_addrtype;

	/* We copy only 4 bytes, we can not trust hp->h_length, as it comes from the DNS */
	bcopy ((char *) hp->h_addr, (char *) &server_address.sin_addr, 4);
    }

#define THE_PORT qproxy(bucket) ? port : qport (bucket)

    server_address.sin_port = htons (THE_PORT);

    /* Connect */
    if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	my_errno = errno;
        if (free_host)
	    free (host);
	return -1;
    }
    setup_source_route (my_socket, server_address.sin_addr.s_addr);
    
    print_vfs_message("ftpfs: making connection to %s", host);
    if (free_host)
	free (host);

    enable_interrupt_key(); /* clear the interrupt flag */

    if (connect (my_socket, (struct sockaddr *) &server_address,
	     sizeof (server_address)) < 0){
	my_errno = errno;
	if (errno == EINTR && got_interrupt())
	    print_vfs_message("ftpfs: connection interrupted by user");
	else
	    print_vfs_message("ftpfs: connection to server failed: %s",
			      unix_error_string(errno));
	disable_interrupt_key();
	close (my_socket);
	return -1;
    }
    disable_interrupt_key();
    return my_socket;
}

static struct connection *
open_command_connection (char *host, char *user, int port, char *netrcpass)
{
    struct connection *bucket;
    int retry_seconds, count_down;

    bucket = xmalloc(sizeof(struct connection), 
		     "struct connection");
    
    if (bucket == NULL) ERRNOR (ENOMEM, NULL);
#ifdef HAVE_MAD
    {
	extern void *watch_free_pointer;

	if (!watch_free_pointer)
	    watch_free_pointer = host;
    }
#endif
    qhost(bucket) = strdup (host);
    quser(bucket) = strdup (user);
    qcdir(bucket) = NULL;
    qport(bucket) = port;
    qlock(bucket) = 0;
    qhome(bucket) = NULL;
    qproxy(bucket)= 0;
    qupdir(bucket)= 0;
    qdcache(bucket)=0;
    bucket->__inode_counter = 0;
    bucket->lock = 0;
    bucket->use_proxy = ftpfs_check_proxy (host);
    bucket->password = 0;
    bucket->use_passive_connection = ftpfs_use_passive_connections | source_route;
    bucket->use_source_route = source_route;
    bucket->strict_rfc959_list_cmd = !ftpfs_use_unix_list_options;
    bucket->isbinary = TYPE_UNKNOWN;

    /* We do not want to use the passive if we are using proxies */
    if (bucket->use_proxy)
	bucket->use_passive_connection = 0;

    if ((qdcache(bucket) = linklist_init()) == NULL) {
	my_errno = ENOMEM;
	free (qhost(bucket));
	free (quser(bucket));
	free (bucket);
	return NULL;
    }
    
    retry_seconds = 0;
    do { 
	bucket->failed_on_login = 0;

	qsock(bucket) = ftpfs_open_socket(bucket);
	if (qsock(bucket) == -1)  {
	    free_bucket (bucket);
	    return NULL;
	}

	if (login_server(bucket, netrcpass)) {
	    /* Logged in, no need to retry the connection */
	    break;
	} else {
	    if (bucket->failed_on_login){
		/* Close only the socket descriptor */
		close (qsock (bucket));
	    } else {
		connection_destructor (bucket);
		return NULL;
	    } 
	    if (ftpfs_retry_seconds){
		retry_seconds = ftpfs_retry_seconds;
		enable_interrupt_key ();
		for (count_down = retry_seconds; count_down; count_down--){
		    print_vfs_message ("Waiting to retry... %d (Control-C to cancel)", count_down);
		    sleep (1);
		    if (got_interrupt ()){
			/* my_errno = E; */
			disable_interrupt_key ();
			free_bucket (bucket);
			return NULL;
		    }
		}
		disable_interrupt_key ();
	    }
	}
    } while (retry_seconds);
    
    qhome(bucket) = ftpfs_get_current_directory (bucket);
    if (!qhome(bucket))
        qhome(bucket) = strdup ("/");
    qupdir(bucket) = strdup ("/"); /* FIXME: I changed behavior to ignore last_current_dir */
    return bucket;
}

static int
is_connection_closed(struct connection *bucket)
{
    fd_set rset;
    struct timeval t;

    if (got_sigpipe){
	return 1;
    }
    t.tv_sec = 0;
    t.tv_usec = 0;
    FD_ZERO(&rset);
    FD_SET(qsock(bucket), &rset);
    while (1) {
	if (select(qsock(bucket) + 1, &rset, NULL, NULL, &t) < 0)
	    if (errno != EINTR)
		return 1;
	return 0;
#if 0
	if (FD_ISSET(qsock(bucket), &rset)) {
	    n = read(qsock(bucket), &read_ahead, sizeof(read_ahead));
	    if (n <= 0) 
		return 1;
	} 	else
#endif
    }
}

/* This routine keeps track of open connections */
/* Returns a connected socket to host */
static struct connection *
open_link (char *host, char *user, int port, char *netrcpass)
{
    int sock;
    struct connection *bucket;
    struct linklist *lptr;
    
    for (lptr = connections_list->next; 
	 lptr != connections_list; lptr = lptr->next) {
	bucket = lptr->data;
	if ((strcmp (host, qhost(bucket)) == 0) &&
	    (strcmp (user, quser(bucket)) == 0) &&
	    (port == qport(bucket))) {
	    
	    /* check the connection is closed or not, just hack */
	    if (is_connection_closed(bucket)) {
		flush_all_directory(bucket);
		sock = ftpfs_open_socket(bucket);
		if (sock != -1) {
		    close(qsock(bucket));
		    qsock(bucket) = sock;
		    if (login_server(bucket, netrcpass))
			return bucket;
		} 
		
		/* connection refused */
		lptr->prev->next = lptr->next;
		lptr->next->prev = lptr->prev;
		connection_destructor(bucket);
		return NULL;
	    }
	    return bucket;
	}
    }
    bucket = open_command_connection(host, user, port, netrcpass);
    if (bucket == NULL)
	return NULL;
    if (!linklist_insert(connections_list, bucket)) {
	my_errno = ENOMEM;
	connection_destructor(bucket);
	return NULL;
    }
    return bucket;
}

/* The returned directory should always contain a trailing slash */
static char *ftpfs_get_current_directory(struct connection *bucket)
{
    char buf[4096], *bufp, *bufq;

    if (command(bucket, NONE, "PWD") == COMPLETE &&
        get_reply(qsock(bucket), buf, sizeof(buf)) == COMPLETE) {
    	bufp = NULL;
	for (bufq = buf; *bufq; bufq++)
	    if (*bufq == '"') {
	        if (!bufp) {
		    bufp = bufq + 1;
	        } else {
		    *bufq = 0;
		    if (*bufp) {
		        if (*(bufq - 1) != '/') {
		            *bufq++ = '/';
		            *bufq = 0;
		        }
		        return strdup (bufp);
		    } else ERRNOR (EIO, NULL);
		}
	    }
    }
    my_errno = EIO;
    return NULL;
}

    
/* Setup Passive ftp connection, we use it for source routed connections */
static int
setup_passive (int my_socket, struct connection *bucket, struct sockaddr_in *sa)
{
    int xa, xb, xc, xd, xe, xf;
    char n [6];
    char *c = reply_str;
    
    if (command (bucket, WAIT_REPLY | WANT_STRING, "PASV") != COMPLETE)
	return 0;

    /* Parse remote parameters */
    for (c = reply_str + 4; (*c) && (!isdigit (*c)); c++)
	;
    if (!*c)
	return 0;
    if (!isdigit (*c))
	return 0;
    if (sscanf (c, "%d,%d,%d,%d,%d,%d", &xa, &xb, &xc, &xd, &xe, &xf) != 6)
	return 0;
    n [0] = (unsigned char) xa;
    n [1] = (unsigned char) xb;
    n [2] = (unsigned char) xc;
    n [3] = (unsigned char) xd;
    n [4] = (unsigned char) xe;
    n [5] = (unsigned char) xf;

    bcopy ((void *)n,     &(sa->sin_addr.s_addr), 4);
    bcopy ((void *)&n[4], &(sa->sin_port), 2);
    setup_source_route (my_socket, sa->sin_addr.s_addr);
    if (connect (my_socket, (struct sockaddr *) sa, sizeof (struct sockaddr_in)) < 0)
	return 0;
    return 1;
}

static int
initconn (struct connection *bucket)
{
    struct sockaddr_in data_addr;
    int data, len = sizeof(data_addr);
    struct protoent *pe;

    getsockname(qsock(bucket), (struct sockaddr *) &data_addr, &len);
    data_addr.sin_port = 0;
    
    pe = getprotobyname("tcp");
    if (pe == NULL) ERRNOR (EIO, -1);
    data = socket (AF_INET, SOCK_STREAM, pe->p_proto);
    if (data < 0) ERRNOR (EIO, -1);

#ifdef ORIGINAL_CONNECT_CODE
    if (bucket->use_source_route){
	int c;
	
	if ((c = setup_passive (data, bucket, &data_addr)))
	    return data;
	print_vfs_message("ftpfs: could not setup passive mode for source routing");
	bucket->use_source_route = 0;
    }
#else
    if (bucket->use_passive_connection){
	if ((bucket->use_passive_connection = setup_passive (data, bucket, &data_addr)))
	    return data;

	bucket->use_source_route = 0;
	bucket->use_passive_connection = 0;
	print_vfs_message ("ftpfs: could not setup passive mode");
    }
#endif
    /* If passive setup fails, fallback to active connections */
    /* Active FTP connection */
    if (bind (data, (struct sockaddr *)&data_addr, len) < 0)
	goto error_return;
    getsockname(data, (struct sockaddr *) &data_addr, &len);
    if (listen (data, 1) < 0)
	goto error_return;
    {
	unsigned char *a = (unsigned char *)&data_addr.sin_addr;
	unsigned char *p = (unsigned char *)&data_addr.sin_port;
	
	if (command (bucket, WAIT_REPLY, "PORT %d,%d,%d,%d,%d,%d", a[0], a[1], 
		     a[2], a[3], p[0], p[1]) != COMPLETE)
	    goto error_return;
    }
    return data;
error_return:
    close(data);
    my_errno = EIO;
    return -1;
}

static int
open_data_connection (struct connection *bucket, char *cmd, char *remote, 
		int isbinary, int reget)
{
    struct sockaddr_in from;
    int s, j, data, fromlen = sizeof(from);
    
    if ((s = initconn (bucket)) == -1)
        return -1;
    if (changetype (bucket, isbinary) == -1)
        return -1;
    if (reget > 0){
	j = command (bucket, WAIT_REPLY, "REST %d", reget);
	if (j != CONTINUE)
	    return -1;
    }
    if (remote)
        j = command (bucket, WAIT_REPLY, "%s %s", cmd, remote);
    else
    	j = command (bucket, WAIT_REPLY, "%s", cmd);
    if (j != PRELIM) ERRNOR (EPERM, -1);
    enable_interrupt_key();
    if (bucket->use_passive_connection)
	data = s;
    else {
	data = accept (s, (struct sockaddr *)&from, &fromlen);
	if (data < 0) {
	    my_errno = errno;
	    close(s);
	    return -1;
	}
	close(s);
    } 
    disable_interrupt_key();
    return data;
}

static void
my_abort (struct connection *bucket, int dsock)
{
    static unsigned char ipbuf[3] = { IAC, IP, IAC };
    fd_set mask;
    char buf[1024];

    print_vfs_message("ftpfs: aborting transfer.");
    if (send(qsock(bucket), ipbuf, sizeof(ipbuf), MSG_OOB) != sizeof(ipbuf)) {
	print_vfs_message("ftpfs: abort error: %s", unix_error_string(errno));
	return;
    }
    
    if (command(bucket, NONE, "%cABOR", DM) != COMPLETE){
	print_vfs_message ("ftpfs: abort failed");
	return;
    }
    if (dsock != -1) {
	FD_ZERO(&mask);
	FD_SET(dsock, &mask);
	if (select(dsock + 1, &mask, NULL, NULL, NULL) > 0)
	    while (read(dsock, buf, sizeof(buf)) > 0);
    }
    if ((get_reply(qsock(bucket), NULL, 0) == TRANSIENT) && (code == 426))
	get_reply(qsock(bucket), NULL, 0);
}

static void
resolve_symlink_without_ls_options(struct connection *bucket, struct dir *dir)
{
    struct linklist *flist;
    struct direntry *fe, *fel;
    char tmp[MC_MAXPATHLEN];
    
    dir->symlink_status = FTPFS_RESOLVING_SYMLINKS;
    for (flist = dir->file_list->next; flist != dir->file_list; flist = flist->next) {
        /* flist->data->l_stat is alread initialized with 0 */
        fel = flist->data;
        if (S_ISLNK(fel->s.st_mode)) {
  	    if (fel->linkname[0] == '/') {
		if (strlen (fel->linkname) >= MC_MAXPATHLEN)
		    continue;
 	        strcpy (tmp, fel->linkname);
	    } else {
		if ((strlen (dir->remote_path) + strlen (fel->linkname)) >= MC_MAXPATHLEN)
		    continue;
                strcpy (tmp, dir->remote_path);
                if (tmp[1] != '\0')
                   strcat (tmp, "/");
                strcat (tmp + 1, fel->linkname);
	    }
	    for ( ;; ) {
		canonicalize_pathname (tmp);
                fe = _get_file_entry(bucket, tmp, 0, 0);
                if (fe) {
                    if (S_ISLNK (fe->s.st_mode) && fe->l_stat == 0) {
		        /* Symlink points to link which isn't resolved, yet. */
			if (fe->linkname[0] == '/') {
		            if (strlen (fe->linkname) >= MC_MAXPATHLEN)
		                break;
			    strcpy (tmp, fe->linkname);
			} else {
			    /* at this point tmp looks always like this
			       /directory/filename, i.e. no need to check
				strrchr's return value */
			    *(strrchr (tmp, '/') + 1) = '\0'; /* dirname */
		            if ((strlen (tmp) + strlen (fe->linkname)) >= MC_MAXPATHLEN)
		                break;
			    strcat (tmp, fe->linkname);
			}
			continue;
                    } else {
	                fel->l_stat = xmalloc(sizeof(struct stat), 
		 			 "resolve_symlink: struct stat");
			if ( S_ISLNK (fe->s.st_mode))
 		            *fel->l_stat = *fe->l_stat;
			else
 		            *fel->l_stat = fe->s;
                        (*fel->l_stat).st_ino = bucket->__inode_counter++;
                    }
	        }
                break;
	    }
        }
    }
    dir->symlink_status = FTPFS_RESOLVED_SYMLINKS;
}

static void
resolve_symlink_with_ls_options(struct connection *bucket, struct dir *dir)
{
    char  buffer[2048] = "", *filename;
    int sock;
    FILE *fp;
    struct stat s;
    struct linklist *flist;
    struct direntry *fe;
    
    dir->symlink_status = FTPFS_RESOLVED_SYMLINKS;
    if (strchr (dir->remote_path, ' ')) {
        if (ftpfs_chdir_internal (bucket, dir->remote_path) != COMPLETE) {
            print_vfs_message("ftpfs: CWD failed.");
	    return;
        }
        sock = open_data_connection (bucket, "LIST -lLa", ".", TYPE_ASCII, 0);
    }
    else
        sock = open_data_connection (bucket, "LIST -lLa", 
                                     dir->remote_path, TYPE_ASCII, 0);

    if (sock == -1) {
	print_vfs_message("ftpfs: couldn't resolve symlink");
	return;
    }
    
    fp = fdopen(sock, "r");
    if (fp == NULL) {
	close(sock);
	print_vfs_message("ftpfs: couldn't resolve symlink");
	return;
    }
    enable_interrupt_key();
    flist = dir->file_list->next;
    while (1) {
	do {
	    if (flist == dir->file_list)
		goto done;
	    fe = flist->data;
	    flist = flist->next;
	} while (!S_ISLNK(fe->s.st_mode));
	while (1) {
	    if (fgets (buffer, sizeof (buffer), fp) == NULL)
		goto done;
	    if (logfile){
		fputs (buffer, logfile);
	        fflush (logfile);
	    }
	    if (vfs_parse_ls_lga (buffer, &s, &filename, NULL)) {
		int r = strcmp(fe->name, filename);
		free(filename);
		if (r == 0) {
		    fe->l_stat = xmalloc(sizeof(struct stat), 
					 "resolve_symlink: struct stat");
		    if (fe->l_stat == NULL)
			goto done;
		    *fe->l_stat = s;
                    (*fe->l_stat).st_ino = bucket->__inode_counter++;
		    break;
		}
		if (r < 0)
		    break;
	    }
	}
    }
done:
    while (fgets(buffer, sizeof(buffer), fp) != NULL);
    disable_interrupt_key();
    fclose(fp);
    get_reply(qsock(bucket), NULL, 0);
}

static void
resolve_symlink(struct connection *bucket, struct dir *dir)
{
    print_vfs_message("Resolving symlink...");

    if (bucket->strict_rfc959_list_cmd) 
	resolve_symlink_without_ls_options(bucket, dir);
    else
        resolve_symlink_with_ls_options(bucket, dir);
}


#define X "ftp"
#define X_myname "/#ftp:"
#define vfs_X_ops vfs_ftpfs_ops
#define X_fill_names ftpfs_fill_names
#define X_hint_reread ftpfs_hint_reread
#define X_flushdir ftpfs_flushdir
#define X_done ftpfs_done
#include "shared_ftp_fish.c"

static char*
get_path (struct connection **bucket, char *path)
{
    return s_get_path (bucket, path, "/#ftp:");
}


static struct dir *
retrieve_dir(struct connection *bucket, char *remote_path, int resolve_symlinks)
{
#ifdef OLD_READ
    FILE *fp;
#endif
    int sock, has_symlinks;
    struct linklist *file_list, *p;
    struct direntry *fe;
    char buffer[8192];
    struct dir *dcache;
    int got_intr = 0;
    int has_spaces = (strchr (remote_path, ' ') != NULL);

    canonicalize_pathname (remote_path);
    for (p = qdcache(bucket)->next;p != qdcache(bucket);
	 p = p->next) {
	dcache = p->data;
	if (strcmp(dcache->remote_path, remote_path) == 0) {
	    struct timeval tim;

	    gettimeofday(&tim, NULL);
	    if (((tim.tv_sec < dcache->timestamp.tv_sec) && !force_expiration)
                || dcache->symlink_status == FTPFS_RESOLVING_SYMLINKS) {
                if (resolve_symlinks && dcache->symlink_status == FTPFS_UNRESOLVED_SYMLINKS)
	            resolve_symlink(bucket, dcache);
		return dcache;
	    } else {
		force_expiration = 0;
		p->next->prev = p->prev;
		p->prev->next = p->next;
		dir_destructor(dcache);
		free (p);
		break;
	    }
	}
    }

    has_symlinks = 0;
    if (bucket->strict_rfc959_list_cmd)
        print_vfs_message("ftpfs: Reading FTP directory %s... (don't use UNIX ls options)", remote_path);
    else
        print_vfs_message("ftpfs: Reading FTP directory %s...", remote_path);
    if (has_spaces || bucket->strict_rfc959_list_cmd) 
        if (ftpfs_chdir_internal (bucket, remote_path) != COMPLETE) {
            my_errno = ENOENT;
	    print_vfs_message("ftpfs: CWD failed.");
	    return NULL;
        }

    file_list = linklist_init();
    if (file_list == NULL) ERRNOR (ENOMEM, NULL);
    dcache = xmalloc(sizeof(struct dir), 
		     "struct dir");
    if (dcache == NULL) {
	my_errno = ENOMEM;
	linklist_destroy(file_list, NULL);
	print_vfs_message("ftpfs: FAIL");
	return NULL;
    }
    gettimeofday(&dcache->timestamp, NULL);
    dcache->timestamp.tv_sec += ftpfs_directory_timeout;
    dcache->file_list = file_list;
    dcache->remote_path = strdup(remote_path);
    dcache->count = 1;
    dcache->symlink_status = FTPFS_NO_SYMLINKS;

    if (bucket->strict_rfc959_list_cmd == 1) 
        sock = open_data_connection (bucket, "LIST", 0, TYPE_ASCII, 0);
    else if (has_spaces)
        sock = open_data_connection (bucket, "LIST -la", ".", TYPE_ASCII, 0);
    else {
	char *path = copy_strings (remote_path, PATH_SEP_STR, ".", (char *) 0);
	sock = open_data_connection (bucket, "LIST -la", path, TYPE_ASCII, 0);
	free (path);
    }

    if (sock == -1)
	goto fallback;

#ifdef OLD_READ
#define close_this_sock(x,y) fclose (x)
    fp = fdopen(sock, "r");
    if (fp == NULL) {
	my_errno = errno;
	close(sock);
        goto error_2;
    }
#else
#define close_this_sock(x,y) close (y)
#endif
    
    /* Clear the interrupt flag */
    enable_interrupt_key ();
    
    errno = 0;
#ifdef OLD_READ
    while (fgets (buffer, sizeof (buffer), fp) != NULL) {
	if (got_intr = got_interrupt ())
	    break;
#else
    while ((got_intr = get_line_interruptible (buffer, sizeof (buffer), sock)) != EINTR){
#endif
	int eof = got_intr == 0;

	if (logfile){
	    fputs (buffer, logfile);
            fputs ("\n", logfile);
	    fflush (logfile);
	}
	if (buffer [0] == 0 && eof)
	    break;
	fe = xmalloc(sizeof(struct direntry), "struct direntry");
	fe->freshly_created = 0;
	fe->local_filename = NULL;
	if (fe == NULL) {
	    my_errno = ENOMEM;
	    goto error_1;
	}
        if (vfs_parse_ls_lga (buffer, &fe->s, &fe->name, &fe->linkname)) {
	    fe->count = 1;
	    fe->local_filename = fe->remote_filename = NULL;
	    fe->l_stat = NULL;
	    fe->bucket = bucket;
            (fe->s).st_ino = bucket->__inode_counter++;
	    if (S_ISLNK(fe->s.st_mode))
		has_symlinks = 1;
	    
	    if (!linklist_insert(file_list, fe)) {
		free(fe);
		my_errno = ENOMEM;
	        goto error_1;
	    }
	}
	else
	    free(fe);
	if (eof)
	    break;
    }
    if (got_intr){
	disable_interrupt_key();
	print_vfs_message("ftpfs: reading FTP directory interrupt by user");
#ifdef OLD_READ
	my_abort(bucket, fileno(fp));
#else
	my_abort(bucket, sock);
#endif
	close_this_sock(fp, sock);
	my_errno = EINTR;
	goto error_3;
    }
    close_this_sock(fp, sock);
    disable_interrupt_key();
    if ( (get_reply (qsock (bucket), NULL, 0) != COMPLETE) || (file_list->next == file_list))
        goto fallback;
    if (!linklist_insert(qdcache(bucket), dcache)) {
	my_errno = ENOMEM;
        goto error_3;
    }
    if (has_symlinks) {
        if (resolve_symlinks)
	    resolve_symlink(bucket, dcache);
        else
           dcache->symlink_status = FTPFS_UNRESOLVED_SYMLINKS;
    }
    print_vfs_message("ftpfs: got listing");
    return dcache;
error_1:
    disable_interrupt_key();
    close_this_sock(fp, sock);
#ifdef OLD_READ
error_2: 
#endif
    get_reply(qsock(bucket), NULL, 0);
error_3:
    free(dcache->remote_path);
    free(dcache);
    linklist_destroy(file_list, direntry_destructor);
    print_vfs_message("ftpfs: failed");
    return NULL;

fallback:
    if (bucket->__inode_counter == 0 && (!bucket->strict_rfc959_list_cmd)) {
        /* It's our first attempt to get a directory listing from this
	server (UNIX style LIST command) */
        bucket->strict_rfc959_list_cmd = 1;
	free(dcache->remote_path);
	free(dcache);
	linklist_destroy(file_list, direntry_destructor);
	return retrieve_dir (bucket, remote_path, resolve_symlinks);
    }
    my_errno = EACCES;
    free(dcache->remote_path);
    free(dcache);
    linklist_destroy(file_list, direntry_destructor);
    print_vfs_message("ftpfs: failed; nowhere to fallback to");
    return NULL;
}

static int
store_file(struct direntry *fe)
{
    int local_handle, sock, n, total;
#ifdef HAVE_STRUCT_LINGER
    struct linger li;
#else
    int flag_one = 1;
#endif
    char buffer[8192];
    struct stat s;

    local_handle = open(fe->local_filename, O_RDONLY);
    unlink (fe->local_filename);
    if (local_handle == -1) ERRNOR (EIO, 0);
    fstat(local_handle, &s);
    sock = open_data_connection(fe->bucket, "STOR", fe->remote_filename, TYPE_BINARY, 0);
    if (sock < 0) {
	close(local_handle);
	return 0;
    }
#ifdef HAVE_STRUCT_LINGER
    li.l_onoff = 1;
    li.l_linger = 120;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *) &li, sizeof(li));
#else
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &flag_one, sizeof (flag_one));
#endif
    total = 0;
    
    enable_interrupt_key();
    while (1) {
	while ((n = read(local_handle, buffer, sizeof(buffer))) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt()) {
		    my_errno = EINTR;
		    goto error_return;
		}
		else
		    continue;
	    }
	    my_errno = errno;
	    goto error_return;
	}
	if (n == 0)
	    break;
    	while (write(sock, buffer, n) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt()) {
		    my_errno = EINTR;
		    goto error_return;
		}
		else 
		    continue;
	    }
	    my_errno = errno;
	    goto error_return;
	}
	total += n;
	print_vfs_message("ftpfs: storing file %d (%d)", 
			  total, s.st_size);
    }
    disable_interrupt_key();
    close(sock);
    close(local_handle);
    if (get_reply (qsock (fe->bucket), NULL, 0) != COMPLETE) ERRNOR (EIO, 0);
    return 1;
error_return:
    disable_interrupt_key();
    close(sock);
    close(local_handle);
    get_reply(qsock(fe->bucket), NULL, 0);
    return 0;
}

static int 
linear_start(struct direntry *fe, int offset)
{
    fe->local_stat.st_mtime = 0;
    fe->data_sock = open_data_connection(fe->bucket, "RETR", fe->remote_filename, TYPE_BINARY, offset);
    if (fe->data_sock == -1)
	ERRNOR (EACCES, 0);
    fe->linear_state = LS_LINEAR_OPEN;
    return 1;
}

static void
linear_abort (struct direntry *fe)
{
    my_abort(fe->bucket, fe->data_sock);
    fe->data_sock = -1;
}

static int
linear_read (struct direntry *fe, void *buf, int len)
{
    int n;
    while ((n = read (fe->data_sock, buf, len))<0) {
        if ((errno == EINTR) && !got_interrupt())
	    continue;
	break;
    }

    if (n<0) linear_abort(fe);

    if (!n) {
        if ((get_reply (qsock (fe->bucket), NULL, 0) != COMPLETE)) {
	    my_errno = EIO;
	    n=-1;
	}
	close (fe->data_sock);
	fe->data_sock = -1;
    }
    ERRNOR (errno, n);
}

static void
linear_close (struct direntry *fe)
{
    if (fe->data_sock != -1)
        linear_abort(fe);
}

int ftpfs_ctl (void *data, int ctlop, int arg)
{
    struct filp *fp = data;
    switch (ctlop) {
        case MCCTL_IS_NOTREADY:
	    {
	        int v;
		
		if (!fp->fe->linear_state)
		    vfs_die ("You may not do this");
		if (fp->fe->linear_state == LS_LINEAR_CLOSED)
		    return 0;

		v = select_on_two (fp->fe->data_sock, 0);
		if (((v < 0) && (errno == EINTR)) || v == 0)
		    return 1;
		return 0;
	    }
        default:
	    return 0;
    }
}

static int
send_ftp_command(char *filename, char *cmd, int flags)
{
    char *remote_path;
    struct connection *bucket;
    int r;
    int flush_directory_cache = (flags & OPT_FLUSH) && (normal_flush > 0);

    if (!(remote_path = get_path(&bucket, filename)))
	return -1;
    r = command (bucket, WAIT_REPLY, cmd, remote_path);
    free(remote_path);
    vfs_add_noncurrent_stamps (&vfs_ftpfs_ops, (vfsid) bucket, NULL);
    if (flags & OPT_IGNORE_ERROR)
	r = COMPLETE;
    if (r != COMPLETE) ERRNOR (EPERM, -1);
    if (flush_directory_cache)
	flush_all_directory(bucket);
    return 0;
}

/* This routine is called as the last step in load_setup */
void
ftpfs_init_passwd(void)
{
    struct passwd *passwd_info;
    char *p, hostname[MAXHOSTNAMELEN];
    struct hostent *hp;

    ftpfs_anonymous_passwd = load_anon_passwd ();
    if (ftpfs_anonymous_passwd)
	return;

    if ((passwd_info = getpwuid (geteuid ())) == NULL)
	p = "unknown";
    else
	p = passwd_info->pw_name;
    gethostname(hostname, sizeof(hostname));
    hp = gethostbyname(hostname);
    if (hp != NULL)
	ftpfs_anonymous_passwd = copy_strings (p, "@", hp->h_name, NULL);
    else
	ftpfs_anonymous_passwd = copy_strings (p, "@", hostname, NULL);
    endpwent ();
}

int
ftpfs_init (vfs *me)
{
    connections_list = linklist_init();
    ftpfs_directory_timeout = FTPFS_DIRECTORY_TIMEOUT;
    return 1;
}

static int ftpfs_chmod (vfs *me, char *path, int mode)
{
    char buf[40];
    
    sprintf(buf, "SITE CHMOD %4.4o %%s", mode & 07777);
    return send_ftp_command(path, buf, OPT_IGNORE_ERROR | OPT_FLUSH);
}

static int ftpfs_chown (vfs *me, char *path, int owner, int group)
{
#if 0
    my_errno = EPERM;
    return -1;
#else
/* Everyone knows it is not possible to chown remotely, so why bother them.
   If someone's root, then copy/move will always try to chown it... */
    return 0;
#endif    
}

static int ftpfs_unlink (vfs *me, char *path)
{
    return send_ftp_command(path, "DELE %s", OPT_FLUSH);
}

/* Return true if path is the same directoy as the one we are on now */
static int
is_same_dir (char *path, struct connection *bucket)
{
    if (!qcdir (bucket))
	return 0;
    if (strcmp (path, qcdir (bucket)) == 0)
	return 1;
    return 0;
}

static int
ftpfs_chdir_internal (struct connection *bucket ,char *remote_path)
{
    int r;
    
    if (!bucket->cwd_defered && is_same_dir (remote_path, bucket))
	return COMPLETE;

    r = command (bucket, WAIT_REPLY, "CWD %s", remote_path);
    if (r != COMPLETE) {
	my_errno = EIO;
    } else {
	if (qcdir(bucket))
	    free(qcdir(bucket));
	qcdir(bucket) = strdup (remote_path);
	bucket->cwd_defered = 0;
    }
    return r;
}

static int ftpfs_rename (vfs *me, char *path1, char *path2)
{
    send_ftp_command(path1, "RNFR %s", OPT_FLUSH);
    return send_ftp_command(path2, "RNTO %s", OPT_FLUSH);
}

static int ftpfs_mkdir (vfs *me, char *path, mode_t mode)
{
    return send_ftp_command(path, "MKD %s", OPT_FLUSH);
}

static int ftpfs_rmdir (vfs *me, char *path)
{
    return send_ftp_command(path, "RMD %s", OPT_FLUSH);
}

void ftpfs_set_debug (char *file)
{
    logfile = fopen (file, "w+");
}

static void my_forget (char *file)
{
    struct linklist *l;
    char *host, *user, *pass, *rp;
    int port;

#ifndef BROKEN_PATHS
    if (strncmp (file, "/#ftp:", 6))
        return; 	/* Normal: consider cd /bla/#ftp */ 
#else
    if (!(file = strstr (file, "/#ftp:")))
        return;
#endif    

    file += 6;
    if (!(rp = my_get_host_and_username (file, &host, &user, &port, &pass))) {
        free (host);
	free (user);
	if (pass)
	    wipe_password (pass);
	return;
    }

    /* we do not care about the path actually */
    free (rp);
    
    for (l = connections_list->next; l != connections_list; l = l->next){
	struct connection *bucket = l->data;
	
	if ((strcmp (host, qhost (bucket)) == 0) &&
	    (strcmp (user, quser (bucket)) == 0) &&
	    (port == qport (bucket))){
	    
	    /* close socket: the child owns it now */
	    close (bucket->sock);
	    bucket->sock = -1;

	    /* reopen the connection */
	    bucket->sock = ftpfs_open_socket (bucket);
	    if (bucket->sock != -1)
		login_server (bucket, pass);
	    break;
	}
    }
    free (host);
    free (user);
    if (pass)
        wipe_password (pass);
}

vfs vfs_ftpfs_ops = {
    NULL,	/* This is place of next pointer */
    "File Tranfer Protocol (ftp)",
    F_NET,	/* flags */
    "ftp:",	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    ftpfs_init,
    ftpfs_done,
    ftpfs_fill_names,
    NULL,

    s_open,
    s_close,
    s_read,
    s_write,
    
    s_opendir,
    s_readdir,
    s_closedir,
    s_telldir,
    s_seekdir,

    s_stat,
    s_lstat,
    s_fstat,

    ftpfs_chmod,
    ftpfs_chown,	/* not really implemented but returns success */
    NULL,

    s_readlink,
    NULL,
    NULL,
    ftpfs_unlink,

    ftpfs_rename,
    s_chdir,
    s_errno,
    s_lseek,
    NULL,
    
    s_getid,
    s_nothingisopen,
    s_free,
    
    s_getlocalcopy,
    s_ungetlocalcopy,

    ftpfs_mkdir,
    ftpfs_rmdir,
    ftpfs_ctl,
    s_setctl

MMAPNULL
};

#ifdef USE_NETRC
static char buffer[100];
static char *netrc, *netrcp;

static int netrc_next (void)
{
    char *p;
    int i;
    static char *keywords [] = { "default", "machine", 
        "login", "password", "passwd",
        "account", "macdef" };

    while (1) {
        netrcp = skip_separators (netrcp);
        if (*netrcp != '\n')
            break;
        netrcp++;
    }
    if (!*netrcp)
	return 0;
    p = buffer;
    if (*netrcp == '"') {
	for (;*netrcp != '"' && *netrcp; netrcp++) {
	    if (*netrcp == '\\')
		netrcp++;
	    *p++ = *netrcp;
	}
    } else {
	for (;*netrcp != '\n' && *netrcp != '\t' && *netrcp != ' ' &&
	    *netrcp != ',' && *netrcp; netrcp++) {
	    if (*netrcp == '\\')
		netrcp++;
	    *p++ = *netrcp;
	}
    }
    *p = 0;
    if (!*buffer)
	return 0;
    for (i = 0; i < sizeof (keywords) / sizeof (keywords [0]); i++)
	if (!strcmp (keywords [i], buffer))
	    break;
    return i + 1;
}

int lookup_netrc (char *host, char **login, char **pass)
{
    char *netrcname, *tmp;
    char hostname[MAXHOSTNAMELEN], *domain;
    int keyword;
    struct stat mystat;
    static int be_angry = 1;
    static struct rupcache {
        struct rupcache *next;
        char *host;
        char *login;
        char *pass;
    } *rup_cache = NULL, *rupp;

    for (rupp = rup_cache; rupp != NULL; rupp = rupp->next)
        if (!strcmp (host, rupp->host)) {
            if (rupp->login != NULL)
                *login = strdup (rupp->login);
            if (rupp->pass != NULL)
                *pass = strdup (rupp->pass);
            return 0;
        }
    netrcname = xmalloc (strlen (home_dir) + strlen ("/.netrc") + 1, "netrc");
    strcpy (netrcname, home_dir);
    strcat (netrcname, "/.netrc");
    netrcp = netrc = load_file (netrcname);
    if (netrc == NULL) {
        free (netrcname);
	return 0;
    }
    if (gethostname (hostname, sizeof (hostname)) < 0)
	*hostname = 0;
    if (!(domain = strchr (hostname, '.')))
	domain = "";

    while ((keyword = netrc_next ())) {
        if (keyword == 2) {
	    if (netrc_next () != 8)
		continue;
	    if (strcasecmp (host, buffer) &&
	        ((tmp = strchr (host, '.')) == NULL ||
		strcasecmp (tmp, domain) ||
		strncasecmp (host, buffer, tmp - host) ||
		buffer [tmp - host]))
		continue;
	} else if (keyword != 1)
	    continue;
	while ((keyword = netrc_next ()) > 2) {
	    switch (keyword) {
		case 3:
		    if (netrc_next ())
			if (*login == NULL)
			    *login = strdup (buffer);
			else if (strcmp (*login, buffer))
			    keyword = 20;
		    break;
		case 4:
		case 5:
		    if (strcmp (*login, "anonymous") && strcmp (*login, "ftp") &&
			stat (netrcname, &mystat) >= 0 &&
			(mystat.st_mode & 077)) {
			if (be_angry) {
			    message_1s (1, MSG_ERROR, _("~/.netrc file has not correct mode.\n"
							"Remove password or correct mode."));
			    be_angry = 0;
			}
			free (netrc);
			free (netrcname);
			return -1;
		    }
		    if (netrc_next () && *pass == NULL)
			*pass = strdup (buffer);
		    break;
		case 6:
		    if (stat (netrcname, &mystat) >= 0 && 
		        (mystat.st_mode & 077)) {
			if (be_angry) {
			    message_1s (1, MSG_ERROR, _("~/.netrc file has not correct mode.\n"
							"Remove password or correct mode."));
			    be_angry = 0;
			}
			free (netrc);
			free (netrcname);
			return -1;
		    }
		    netrc_next ();
		    break;
		case 7:
		    for (;;) {
		        while (*netrcp != '\n' && *netrcp);
		        if (*netrcp != '\n')
		            break;
		        netrcp++;
		        if (*netrcp == '\n' || !*netrcp)
		            break;
		    }
		    break;
	    }
	    if (keyword == 20)
	        break;
	}
	if (keyword == 20)
	    continue;
	else
	    break;
    }
    rupp = (struct rupcache *) xmalloc (sizeof (struct rupcache), "");
    rupp->host = strdup (host);
    rupp->login = rupp->pass = 0;
    
    if (*login != NULL)
        rupp->login = strdup (*login);
    if (*pass != NULL)
        rupp->pass = strdup (*pass);
    rupp->next = rup_cache;
    rup_cache = rupp;
    
    free (netrc);
    free (netrcname);
    return 0;
}

#ifndef HAVE_STRNCASECMP
int strncasecmp (char *s, char *d, int l)
{
    int result;
    
    while (l--){
	if (result = (0x20 | *s) - (0x20 | *d))
	    break;
	if (!*s)
	    return 0;
	s++;
	d++;
    }
}
#endif
#endif /* USE_NETRC */
