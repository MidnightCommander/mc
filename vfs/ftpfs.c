/* Virtual File System: FTP file system.
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Ching Hui
               1995 Jakub Jelinek
               1995, 1996, 1997 Miguel de Icaza
	       1997 Norbert Warmuth
	       1998 Pavel Machek
   
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
   
/* FTPfs TODO:

- make it more robust - all the connects etc. should handle EADDRINUSE and
  ERETRY (have I spelled these names correctly?)
- make the user able to flush a connection - all the caches will get empty
  etc., (tarfs as well), we should give there a user selectable timeout
  and assign a key sequence.  
- use hash table instead of linklist to cache ftpfs directory.
 */

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

#ifdef USE_TERMNET
#include <termnet.h>
#endif

#include "../src/mem.h"
#define WANT_PARSE_LS_LGA
#include "vfs.h"
#include "tcputil.h"
#include "../src/util.h"
#include "../src/dialog.h"
#include "container.h"
#include "ftpfs.h"
#ifndef MAXHOSTNAMELEN
#    define MAXHOSTNAMELEN 64
#endif

#define UPLOAD_ZERO_LENGTH_FILE

static int ftpfserrno;
static int code;

/* Delay to retry a connection */
int ftpfs_retry_seconds = 30;

/* Method to use to connect to ftp sites */
int ftpfs_use_passive_connections = 1;

/* Use the ~/.netrc */
int use_netrc = 1;

extern char *home_dir;

/* Anonymous setup */
char *ftpfs_anonymous_passwd = 0;
int ftpfs_directory_timeout;

/* Proxy host */
char *ftpfs_proxy_host = 0;

/* Reget flag */
int do_reget = 0;

/* wether we have to use proxy by default? */
int ftpfs_always_use_proxy;

/* source routing host */
extern int source_route;

/* If set, then log dialog between server and client */
static int ftpfs_debug_server_dialog = 0;

/* Where we store the transactions */
static FILE *ftpfs_logfile;

/* If true, the directory cache is forced to reload */
static int force_expiration = 0;

struct linklist *ftpfs_connections_list;

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02
char reply_str [80];

static char *ftpfs_get_current_directory(struct ftpfs_connection *bucket);
static int __ftpfs_chdir (struct ftpfs_connection *bucket ,char *remote_path);

/* Extract the hostname and username from the path */
/* path is in the form: [user@]hostname:port/remote-dir, e.g.:
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * If the user is empty, e.g. ftp://@roxanne/private, then your login name
 * is supplied.
 * */

char *ftpfs_get_host_and_username (char *path, char **host, char **user, int *port, char **pass)
{
    return get_host_and_username (path, host, user, port, 21, 1, pass);
}

static int
select_on_two (int fd1, int fd2)
{
    fd_set set;
    struct timeval timeout;
    int v;
    int maxfd = (fd1 > fd2 ? fd1 : fd2) + 1;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(fd1, &set);
    FD_SET(fd2, &set);
    v = select (maxfd, &set, 0, 0, &timeout);
    if (v <= 0)
	return v;
    if (FD_ISSET (fd1, &set))
	return 1;
    if (FD_ISSET (fd2, &set))
	return 2;
    return -1;
}

static int
get_line (int sock, char *buf, int buf_len)
{
    int i, status;
    char c;

    for (i = 0; i < buf_len; i++, buf++) {
	if (read(sock, buf, sizeof(char)) <= 0)
	    return 0;
	if (ftpfs_debug_server_dialog){
	    fwrite (buf, 1, 1, ftpfs_logfile);
	    fflush (ftpfs_logfile);
	}
	if (*buf == '\n') {
	    *buf = 0;
	    return 1;
	}
    }
    *buf = 0;
    while ((status = read(sock, &c, sizeof(c))) > 0){
	if (ftpfs_debug_server_dialog){
	    fwrite (&c, 1, 1, ftpfs_logfile);
	    fflush (ftpfs_logfile);
	}
	if (c == '\n')
	    return 1;
    }
    return 0;
}

/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */
int getreply (int sock, char *string_buf, int string_len)
{
    char answer[1024];
    int i;
    
    for (;;) {
        if (!get_line(sock, answer, sizeof(answer))) {
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
			if (!get_line(sock, answer, sizeof(answer))) {
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

static void
ftpentry_destructor (void *data)
{
    struct ftpentry *fe = data;
    
    fe->count--;
    if ((fe->tmp_reget == 1 && fe->local_filename)){
	unlink (fe->local_filename);
	fe->tmp_reget = 0;
    }
	
    if (fe->count > 0)
        return;
    free(fe->name);
    if (fe->linkname)
	free(fe->linkname);
    if (fe->local_filename) {
        if (fe->local_is_temp) {
            if (!fe->local_stat.st_mtime)
	        unlink(fe->local_filename);
	    else {
	        struct stat sb;
	        
	        if (stat (fe->local_filename, &sb) >=0 && 
	            fe->local_stat.st_mtime == sb.st_mtime)
	            unlink (fe->local_filename); /* Delete only if it hasn't changed */
	    }
	}
	free(fe->local_filename);
	fe->local_filename = 0;
    }
    if (fe->remote_filename)
	free(fe->remote_filename);
    if (fe->l_stat)
	free(fe->l_stat);
    free(fe);
}

static void
ftpfs_dir_destructor(void *data)
{
    struct ftpfs_dir *fd = data;

    fd->count--;
    if (fd->count > 0) 
	return;
    free(fd->remote_path);
    linklist_destroy(fd->file_list, ftpentry_destructor);
    free(fd);
}

static int command (struct ftpfs_connection *bucket, int wait_reply,
		    char *fmt, ...)
{
    va_list ap;
    char buf[2048]; /* FIXME: buffer exceed ?? */
    int n, status;
    int sock = qsock (bucket);
    
    va_start (ap, fmt);
    vsprintf (buf, fmt, ap);
    va_end (ap);
    n = strlen(buf);
    buf[n++] = '\r';
    buf[n++] = '\n';
    buf[n] = 0;

    if (ftpfs_debug_server_dialog){
        if (strncmp (buf, "PASS ", 5) == 0) {
            char *tmp = "PASS <Password not logged>\r\n";
            fwrite (tmp, strlen (tmp), 1, ftpfs_logfile);
        } else
	    fwrite (buf, strlen (buf), 1, ftpfs_logfile);
	fflush (ftpfs_logfile);
    }
    got_sigpipe = 0;
    enable_interrupt_key();
    status = write(sock, buf, strlen(buf));
    if (status < 0){
	code = 421;
	if (errno == EPIPE){
	    got_sigpipe = 1;
	}
	disable_interrupt_key();
	return TRANSIENT;
    }
    disable_interrupt_key();
    
    if (wait_reply)
	return getreply (sock, (wait_reply & WANT_STRING) ? reply_str : NULL, sizeof (reply_str)-1);
    return COMPLETE;
}

static void
ftpfs_connection_close (void *data)
{
    struct ftpfs_connection *bucket = data;

    if (qsock (bucket) != -1){
	print_vfs_message ("ftpfs: Disconnecting from %s", qhost(bucket));
	command(bucket, NONE, "QUIT");
	close(qsock(bucket));
    }
}

static void
ftpfs_free_bucket (void *data)
{
    struct ftpfs_connection *bucket = data;

    free(qhost(bucket));
    free(quser(bucket));
    if (qcdir(bucket))
	free(qcdir(bucket));
    if (qhome(bucket))
    	free(qhome(bucket));
    if (qupdir(bucket))
        free(qupdir(bucket));
    if (bucket->password)
	wipe_password (bucket->password);
    linklist_destroy(qdcache(bucket), ftpfs_dir_destructor);
    free(bucket);
}

static void
ftpfs_connection_destructor(void *data)
{
    ftpfs_connection_close (data);
    ftpfs_free_bucket (data);
}

/* some defines only used by changetype */
/* These two are valid values for the second parameter */
#define TYPE_ASCII    0
#define TYPE_BINARY   1

/* This one is only used to initialize bucket->isbinary, don't use it as
   second parameter to changetype. */
#define TYPE_UNKNOWN -1 

static int
changetype (struct ftpfs_connection *bucket, int binary)
{
    if (binary != bucket->isbinary) {
        if (command (bucket, WAIT_REPLY, "TYPE %c", binary ? 'I' : 'A') != COMPLETE) {
	    ftpfserrno = EIO;
            return -1;
	}
        bucket->isbinary = binary;
    }
    return binary;
}

inline void
flush_all_directory(struct ftpfs_connection *bucket)
{
    linklist_delete_all(qdcache(bucket), ftpfs_dir_destructor);

}

/* This routine logs the user in */
static int 
login_server (struct ftpfs_connection *bucket, char *netrcpass)
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
		if (op == NULL) {
		    ftpfserrno = EPERM;
		    return 0;
		}
		bucket->password = strdup (op);
	    } else
		op = strdup (bucket->password);
        }
    }

    if (!anon || ftpfs_debug_server_dialog)
	pass = strdup (op);
    else
	pass = copy_strings ("-", op, 0);
    wipe_password (op);

    
    /* Proxy server accepts: username@host-we-want-to-connect*/
    if (qproxy (bucket)){
#if defined(HSC_PROXY)
	char *p, *host;
	int port;
	p = ftpfs_get_host_and_username(ftpfs_proxy_host, &host, &proxyname,
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
	    ftpfserrno = EPERM;
	    wipe_password (pass);
	    free (proxyname);
	    return 0;
	}
	name = strdup(quser (bucket));
#else
	name = copy_strings (quser(bucket), "@", 
		qhost(bucket)[0] == '!' ? qhost(bucket)+1 : qhost(bucket), 0);
#endif
    } else 
	name = strdup (quser (bucket));
    
    if (getreply (qsock(bucket), NULL, 0) == COMPLETE) {
#if defined(HSC_PROXY)
	if (qproxy(bucket)) {
	    print_vfs_message("ftpfs: sending proxy login name");
	    if (command (bucket, 1, "USER %s", proxyname) == CONTINUE) {
		print_vfs_message("ftpfs: sending proxy user password");
		if (command (bucket, 1, "PASS %s", proxypass) == COMPLETE)
		    {
			print_vfs_message("ftpfs: proxy authentication succeeded");
			if (command (bucket, 1, "SITE %s", qhost(bucket)+1) ==
			    COMPLETE) {
			    print_vfs_message("ftpfs: connected to %s", qhost(bucket)+1);
			}
			else {
			    bucket->failed_on_login = 1;
			    /* ftpfserrno = E; */
			    if (proxypass)
				wipe_password (proxypass);
    			    wipe_password (pass);
			    free (proxyname);
			    free (name);
			    return 0;
			}
		    }
		else {
		    bucket->failed_on_login = 1;
		    /* ftpfserrno = E; */
		    if (proxypass)
			wipe_password (proxypass);
		    wipe_password (pass);
		    free (proxyname);
		    free (name);
		    return 0;
		}
	    }
	    else {
		bucket->failed_on_login = 1;
		/* ftpfserrno = E; */
		if (proxypass)
		    wipe_password (proxypass);
		wipe_password (pass);
		free (proxyname);
		free (name);
		return 0;
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
	    /* ftpfserrno = E; */
	    if (bucket->password)
		wipe_password (bucket->password);
	    bucket->password = 0;
	    
	    /* This matches the end of the code below, just to make it
	     * obvious to the optimizer
	     */
	    wipe_password (pass);
	    free (name);
	    return 0;
	}
    }
    print_vfs_message ("ftpfs: Login incorrect for user %s ", quser(bucket));
    ftpfserrno = EPERM;
    wipe_password (pass);
    free (name);
    return 0;
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
    static int	loaded;

    if (loaded)
	return;

    if (exist_file (LIBDIR "mc.no_proxy") &&
	(npf = fopen (LIBDIR "mc.no_proxy", "r"))) {
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
#define HSC_DEFAULT_PORT 9875
    dir = get_host_and_username(proxy, host, &user, port, HSC_DEFAULT_PORT, 1,
				 &pass);
#else
    dir = get_host_and_username(proxy, host, &user, port, 21, 1,
				 &pass);
#endif
    free(user);
    if (pass)
	wipe_password (pass);
    if (dir)
	free(dir);
}

static int
ftpfs_open_socket(struct ftpfs_connection *bucket)
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
        ftpfserrno = EINVAL;
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
	    ftpfserrno = EINVAL;
	    if (free_host)
		free (host);
	    return -1;
	}
	server_address.sin_family = hp->h_addrtype;
	bcopy ((char *) hp->h_addr, (char *) &server_address.sin_addr,
	       hp->h_length);
    }

#define THE_PORT qproxy(bucket) ? port : qport (bucket)

    server_address.sin_port = htons (THE_PORT);

    /* Connect */
    if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	ftpfserrno = errno;
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
	ftpfserrno = errno;
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

static struct ftpfs_connection *
open_command_connection (char *host, char *user, int port, char *netrcpass)
{
    struct ftpfs_connection *bucket;
    int retry_seconds, count_down;

    bucket = xmalloc(sizeof(struct ftpfs_connection), 
		     "struct ftpfs_connection");
    
    if (bucket == NULL) {
	ftpfserrno = ENOMEM;
	return NULL;
    }
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
    bucket->isbinary = TYPE_UNKNOWN;

    /* We do not want to use the passive if we are using proxies */
    if (bucket->use_proxy)
	bucket->use_passive_connection = 0;

    if ((qdcache(bucket) = linklist_init()) == NULL) {
	ftpfserrno = ENOMEM;
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
	    ftpfs_free_bucket (bucket);
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
		ftpfs_connection_destructor (bucket);
		return NULL;
	    } 
	    if (ftpfs_retry_seconds){
		retry_seconds = ftpfs_retry_seconds;
		enable_interrupt_key ();
		for (count_down = retry_seconds; count_down; count_down--){
		    print_vfs_message ("Waiting to retry... %d (Control-C to cancel)\n", count_down);
		    sleep (1);
		    if (got_interrupt ()){
			/* ftpfserrno = E; */
			disable_interrupt_key ();
			ftpfs_free_bucket (bucket);
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
is_connection_closed(struct ftpfs_connection *bucket)
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
static struct ftpfs_connection *
ftpfs_open_link (char *host, char *user, int port, char *netrcpass)
{
    int sock;
    struct ftpfs_connection *bucket;
    struct linklist *lptr;
    
    for (lptr = ftpfs_connections_list->next; 
	 lptr != ftpfs_connections_list; lptr = lptr->next) {
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
		ftpfs_connection_destructor(bucket);
		return NULL;
	    }
	    return bucket;
	}
    }
    bucket = open_command_connection(host, user, port, netrcpass);
    if (bucket == NULL)
	return NULL;
    if (!linklist_insert(ftpfs_connections_list, bucket)) {
	ftpfserrno = ENOMEM;
	ftpfs_connection_destructor(bucket);
	return NULL;
    }
    return bucket;
}

/* The returned directory should always contain a trailing slash */
static char *ftpfs_get_current_directory(struct ftpfs_connection *bucket)
{
    char buf[4096], *bufp, *bufq;

    if (command(bucket, NONE, "PWD") == COMPLETE &&
        getreply(qsock(bucket), buf, sizeof(buf)) == COMPLETE) {
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
		    } else {
		        ftpfserrno = EIO;
		        return NULL;
		    }
		}
	    }
    }
    ftpfserrno = EIO;
    return NULL;
}

void ftpfs_fill_names (void (*func)(char *))
{
    struct linklist *lptr;
    char   *path_name;
    struct ftpfs_connection *bucket;
    
    if (!ftpfs_connections_list)
	return;
    lptr = ftpfs_connections_list;
    do {
	if ((bucket = lptr->data) != 0){

	    path_name = copy_strings ("/#ftp:", quser (bucket),
				      "@",      qhost (bucket), 
				      qcdir(bucket), 0);
	    (*func)(path_name);
	    free (path_name);
	}
	lptr = lptr->next;
    } while (lptr != ftpfs_connections_list);
}

    
/* Setup Passive ftp connection, we use it for source routed connections */
static int
setup_passive (int my_socket, struct ftpfs_connection *bucket, struct sockaddr_in *sa)
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
initconn (struct ftpfs_connection *bucket)
{
    struct sockaddr_in data_addr;
    int data, len = sizeof(data_addr);
    struct protoent *pe;

    getsockname(qsock(bucket), (struct sockaddr *) &data_addr, &len);
    data_addr.sin_port = 0;
    
    pe = getprotobyname("tcp");
    if (pe == NULL) {
	ftpfserrno = EIO;
	return -1;
    }
    data = socket (AF_INET, SOCK_STREAM, pe->p_proto);
    if (data < 0) {
	ftpfserrno = EIO;
        return -1;
    }

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
    ftpfserrno = EIO;
    return -1;
}

static int
open_data_connection (struct ftpfs_connection *bucket, char *cmd, char *remote, 
		int isbinary)
{
    struct sockaddr_in from;
    int s, j, data, fromlen = sizeof(from);
    
    if ((s = initconn (bucket)) == -1)
        return -1;
    if (changetype (bucket, isbinary) == -1)
        return -1;
    if (do_reget > 0){
	j = command (bucket, WAIT_REPLY, "REST %d", do_reget);
	do_reget = 0;
	if (j != CONTINUE)
	    return -1;
    }
    if (remote)
        j = command (bucket, WAIT_REPLY, "%s %s", cmd, remote);
    else
    	j = command (bucket, WAIT_REPLY, "%s", cmd);
    if (j != PRELIM) {
	ftpfserrno = EPERM;
        return -1;
    }
    enable_interrupt_key();
    if (bucket->use_passive_connection)
	data = s;
    else {
	data = accept (s, (struct sockaddr *)&from, &fromlen);
	if (data < 0) {
	    ftpfserrno = errno;
	    close(s);
	    return -1;
	}
	close(s);
    } 
    disable_interrupt_key();
    return data;
}

/* ftpfs_get_path:
 * makes BUCKET point to the connection bucket descriptor for PATH
 * returns a malloced string with the pathname relative to BUCKET.
 */
static char*
ftpfs_get_path (struct ftpfs_connection **bucket, char *path)
{
    char *user, *host, *remote_path, *pass;
    int port;

#ifndef BROKEN_PATHS
    if (strncmp (path, "/#ftp:", 6))
        return NULL; 	/* Normal: consider cd /bla/#ftp */ 
#else
    if (!(path = strstr (path, "/#ftp:")))
        return NULL;
#endif    
    path += 6;

    if (!(remote_path = ftpfs_get_host_and_username (path, &host, &user, &port, &pass)))
        ftpfserrno = ENOENT;
    else {
        if ((*bucket = ftpfs_open_link (host, user, port, pass)) == NULL) {
            free (remote_path);
	    remote_path = NULL;
	}
    }
    free (host);
    free (user);
    if (pass)
        wipe_password (pass);

    if (!remote_path) 
        return NULL;

    /* NOTE: Usage of tildes is deprecated, consider:
     * cd /#ftp:pavel@hobit
     * cd ~
     * And now: what do I want to do? Do I want to go to /home/pavel or to
     * /#ftp:hobit/home/pavel? I think first has better sense...
     */
    {
        int f = !strcmp( remote_path, "/~" );
	if (f || !strncmp( remote_path, "/~/", 3 )) {
	    char *s;
	    s = concat_dir_and_file( qhome (*bucket), remote_path +3-f );
	    free (remote_path);
	    remote_path = s;
	}
    }
    return remote_path;
}

static void
ftpfs_abort (struct ftpfs_connection *bucket, int dsock)
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
    if ((getreply(qsock(bucket), NULL, 0) == TRANSIENT) && (code == 426))
	getreply(qsock(bucket), NULL, 0);
}

static void
resolve_symlink(struct ftpfs_connection *bucket, struct ftpfs_dir *dir)
{
    char  buffer[2048] = "", *filename;
    int sock;
    FILE *fp;
    struct stat s;
    struct linklist *flist;
    struct ftpentry *fe;
    
    print_vfs_message("Resolving symlink...");

    if (strchr (dir->remote_path, ' ')) {
        if (__ftpfs_chdir(bucket, dir->remote_path) != COMPLETE) {
            print_vfs_message("ftpfs: CWD failed.");
	    return;
        }
        sock = open_data_connection (bucket, "LIST -lLa", ".", TYPE_ASCII);
    }
    else
        sock = open_data_connection (bucket, "LIST -lLa", 
                                     dir->remote_path, TYPE_ASCII);

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
	    if (ftpfs_debug_server_dialog){
		fputs (buffer, ftpfs_logfile);
	        fflush (ftpfs_logfile);
	    }
	    if (parse_ls_lga (buffer, &s, &filename, NULL)) {
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
    getreply(qsock(bucket), NULL, 0);
}

static int
get_line_interruptible (char *buffer, int size, int fd)
{
    int n;
    int i = 0;

    for (i = 0; i < size-1; i++) {
	n = read (fd, buffer+i, 1);
	if (n == -1 && errno == EINTR){
	    buffer [i] = 0;
	    return EINTR;
	}
	if (n == 0){
	    buffer [i] = 0;
	    return 0;
	}
	if (buffer [i] == '\n'){
	    buffer [i] = 0;
	    return 1;
	}
    }
    buffer [size-1] = 0;
    return 0;
}

/* Return true if path is the same directoy as the one we are on now */
static int
ftpfs_same_dir (char *path, struct ftpfs_connection *bucket)
{
    if (!qcdir (bucket))
	return 0;
    if (strcmp (path, qcdir (bucket)) == 0)
	return 1;
    return 0;
}

static struct ftpfs_dir *
retrieve_dir(struct ftpfs_connection *bucket, char *remote_path)
{
#ifdef OLD_READ
    FILE *fp;
#endif
    int sock, has_symlinks;
    struct linklist *file_list, *p;
    struct ftpentry *fe;
    char buffer[8192];
    struct ftpfs_dir *dcache;
    int got_intr = 0;
    int has_spaces = (strchr (remote_path, ' ') != NULL);

    for (p = qdcache(bucket)->next;p != qdcache(bucket);
	 p = p->next) {
	dcache = p->data;
	if (strcmp(dcache->remote_path, remote_path) == 0) {
	    struct timeval tim;

	    gettimeofday(&tim, NULL);
	    if ((tim.tv_sec < dcache->timestamp.tv_sec) && !force_expiration)
		return dcache;
	    else {
		force_expiration = 0;
		p->next->prev = p->prev;
		p->prev->next = p->next;
		ftpfs_dir_destructor(dcache);
		free (p);
		break;
	    }
	}
    }

    has_symlinks = 0;
    print_vfs_message("ftpfs: Reading FTP directory...");
    if (has_spaces) 
        if (__ftpfs_chdir(bucket, remote_path) != COMPLETE) {
            ftpfserrno = ENOENT;
	    print_vfs_message("ftpfs: CWD failed.");
	    return NULL;
        }

    file_list = linklist_init();
    if (file_list == NULL) {
	ftpfserrno = ENOMEM;
	print_vfs_message("ftpfs: couldn't get a file listing");
        return NULL;
    }
    dcache = xmalloc(sizeof(struct ftpfs_dir), 
		     "struct ftpfs_dir");
    if (dcache == NULL) {
	ftpfserrno = ENOMEM;
	linklist_destroy(file_list, NULL);
	print_vfs_message("ftpfs: FAIL");
	return NULL;
    }
    gettimeofday(&dcache->timestamp, NULL);
    dcache->timestamp.tv_sec += ftpfs_directory_timeout;
    dcache->file_list = file_list;
    dcache->remote_path = strdup(remote_path);
    dcache->count = 1;

    if (has_spaces)
        sock = open_data_connection (bucket, "LIST -la", ".", TYPE_ASCII);
    else {
	char *path = copy_strings (remote_path, PATH_SEP_STR, ".", (char *) 0);
	
	sock = open_data_connection (bucket, "LIST -la", path, TYPE_ASCII);
	free (path);
    }

    if (sock == -1)
	goto error_3;

#ifdef OLD_READ
#define close_this_sock(x,y) fclose (x)
    fp = fdopen(sock, "r");
    if (fp == NULL) {
	ftpfserrno = errno;
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

	if (ftpfs_debug_server_dialog){
	    fputs (buffer, ftpfs_logfile);
            fputs ("\n", ftpfs_logfile);
	    fflush (ftpfs_logfile);
	}
	if (buffer [0] == 0 && eof)
	    break;
	fe = xmalloc(sizeof(struct ftpentry), "struct ftpentry");
	fe->freshly_created = 0;
	fe->tmp_reget = 0;
	if (fe == NULL) {
	    ftpfserrno = ENOMEM;
	    goto error_1;
	}
        if (parse_ls_lga (buffer, &fe->s, &fe->name, &fe->linkname)) {
	    fe->count = 1;
	    fe->local_filename = fe->remote_filename = NULL;
	    fe->l_stat = NULL;
	    fe->bucket = bucket;
            (fe->s).st_ino = bucket->__inode_counter++;
	    if (S_ISLNK(fe->s.st_mode))
		has_symlinks = 1;
	    
	    if (!linklist_insert(file_list, fe)) {
		free(fe);
		ftpfserrno = ENOMEM;
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
	ftpfs_abort(bucket, fileno(fp));
#else
	ftpfs_abort(bucket, sock);
#endif
	close_this_sock(fp, sock);
	ftpfserrno = EINTR;
	goto error_3;
    }
    close_this_sock(fp, sock);
    disable_interrupt_key();
    if (getreply (qsock (bucket), NULL, 0) != COMPLETE) {
	ftpfserrno = EIO;
        goto error_3;
    }
    if (file_list->next == file_list) {
	ftpfserrno = EACCES;
	goto error_3;
    }
    if (!linklist_insert(qdcache(bucket), dcache)) {
	ftpfserrno = ENOMEM;
        goto error_3;
    }
    if (has_symlinks)
	resolve_symlink(bucket, dcache);
    print_vfs_message("ftpfs: got listing");
    return dcache;
error_1:
    disable_interrupt_key();
    close_this_sock(fp, sock);
#ifdef OLD_READ
error_2: 
#endif
    getreply(qsock(bucket), NULL, 0);
error_3:
    free(dcache->remote_path);
    free(dcache);
    linklist_destroy(file_list, ftpentry_destructor);
    print_vfs_message("ftpfs: failed");
    return NULL;
}

static int
store_file(struct ftpentry *fe)
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
    if (local_handle == -1) {
	ftpfserrno = EIO;
	return 0;
    }
    fstat(local_handle, &s);
    sock = open_data_connection(fe->bucket, "STOR", fe->remote_filename, TYPE_BINARY);
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
		    ftpfserrno = EINTR;
		    goto error_return;
		}
		else
		    continue;
	    }
	    ftpfserrno = errno;
	    goto error_return;
	}
	if (n == 0)
	    break;
    	while (write(sock, buffer, n) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt()) {
		    ftpfserrno = EINTR;
		    goto error_return;
		}
		else 
		    continue;
	    }
	    ftpfserrno = errno;
	    goto error_return;
	}
	total += n;
	print_vfs_message("ftpfs: storing file %d (%d)", 
			  total, s.st_size);
    }
    disable_interrupt_key();
    close(sock);
    close(local_handle);
    if (getreply (qsock (fe->bucket), NULL, 0) != COMPLETE) {
	ftpfserrno = EIO;
	return 0;
    }
    return 1;
error_return:
    disable_interrupt_key();
    close(sock);
    close(local_handle);
    getreply(qsock(fe->bucket), NULL, 0);
    return 0;
}

/* These variables are for the _ctl routine */
static char *localname = NULL;
static struct ftpentry *remoteent;
static int remotetotal = 0;
static int transfer_started = 0;
static char *remotebuffer;
static int isremotecopy = 0;
static int remotelocal_handle, remotesock, remoten, remotestat_size;

int retrieve_file_start(struct ftpentry *fe)
{
    if (fe->local_filename == NULL) {
	ftpfserrno = ENOMEM;
	return 0;
    }
    remotesock = open_data_connection(fe->bucket, "RETR", fe->remote_filename, TYPE_BINARY);
    if (remotesock == -1) {
	ftpfserrno = EACCES;
	free (fe->local_filename);
	fe->local_filename = NULL;
	return 0;
    }
    remotetotal = 0;
    remoteent = fe;
    return 1;
}

int retrieve_file_start2(struct ftpentry *fe)
{
    remotelocal_handle = open(fe->local_filename, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
    if (remotelocal_handle == -1) {
	ftpfserrno = EIO;
	free(fe->local_filename);
	fe->local_filename = NULL;
	fe->local_is_temp = 1;
	close(remotesock);
	return 0;
    }
    remotestat_size = fe->s.st_size;
    remotebuffer = xmalloc (8192, "");
    return 1;
}

void ftpfs_flushdir (void)
{
	    force_expiration = 1;
}

static int remove_temp_file (char *file_name);

int ftpfs_ctl (void *data, int ctlop, int arg)
{
    int n = 0;

    switch (ctlop) {
        case MCCTL_ISREMOTECOPY:
            return isremotecopy;
	    
        case MCCTL_REMOTECOPYCHUNK:
            if (!transfer_started)
                if (!retrieve_file_start2 (remoteent)){
                    return MCERR_TARGETOPEN;
		} else
		    transfer_started = 1;

	    enable_interrupt_key ();
            if (!remoten) {
		int v = select_on_two (remotesock, 0);
		
		if (((v < 0) && (errno == EINTR)) || v == 0){
		    disable_interrupt_key ();
		    return MCERR_DATA_ON_STDIN;
		}
		    
    	        if ((n = read(remotesock, remotebuffer, 8192)) < 0){
		    disable_interrupt_key ();
		    if (errno == EINTR)
			return MCERR_DATA_ON_STDIN;
		    else
			return MCERR_READ;
		}
	        if (n == 0) {
    	    	    if (getreply (qsock (remoteent->bucket), NULL, 0) != COMPLETE) {
	        	ftpfserrno = EIO;
	    	    }
    	    	    close(remotelocal_handle);
    	    	    close(remotesock);
		    if (localname){
			free (localname);
			localname = NULL;
		    }
		    disable_interrupt_key ();
		    transfer_started = 0;
	            return MCERR_FINISH;
	        }
		disable_interrupt_key ();
	        remotetotal += n;
	        remoten = n;
            } else
                n = remoten;
            if (write(remotelocal_handle, remotebuffer, remoten) < 0)
                return MCERR_WRITE;
            remoten = 0;
            return n;

	    /* We get this message if the transfer was aborted */
        case MCCTL_FINISHREMOTE:
	    if (localname) {
	        free (localname);
	        localname = NULL;
	    }
	    if (!arg) { /* OK */
	        if (stat (remoteent->local_filename, &remoteent->local_stat) < 0)
	            remoteent->local_stat.st_mtime = 0;
	    } else
	        remoteent->local_stat.st_mtime = 0;
	    transfer_started = 0;
	    ftpfs_abort (remoteent->bucket, remotesock);
	    ftpfserrno = EINTR;
	    return 0;
    }
    return 0;
}

int ftpfs_setctl (char *path, int ctlop, char *arg)
{
    switch (ctlop) {
	case MCCTL_REMOVELOCALCOPY:
	    return remove_temp_file (path);

        case MCCTL_SETREMOTECOPY: if (localname) free (localname);
            localname = strdup (vfs_canon (arg));
            return 1;
        default:
            return 0;
    }
}

int retrieve_file(struct ftpentry *fe)
{
    int total, tmp_reget = do_reget;
    char buffer[8192];
    int local_handle, sock, n;
    
    if (fe->local_filename)
        return 1;
    fe->local_stat.st_mtime = 0;
    fe->local_filename = tempnam (0, "ftpfs");
    fe->local_is_temp = 1;
    if (fe->local_filename == NULL) {
	ftpfserrno = ENOMEM;
	return 0;
    }
    local_handle = open(fe->local_filename, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
    if (local_handle == -1) {
	ftpfserrno = EIO;
	free(fe->local_filename);
	fe->local_filename = NULL;
	return 0;
    }
    sock = open_data_connection(fe->bucket, "RETR", fe->remote_filename, TYPE_BINARY);
    if (sock == -1) {
	ftpfserrno = EACCES;
	goto error_3;
    }

    /* Clear the interrupt status */
    enable_interrupt_key ();
    total = 0;
    if (tmp_reget > 0)
	total = tmp_reget;
    else
	total = 0;
    
    while (1) {
	int stat_size = fe->s.st_size;
	while ((n = read(sock, buffer, sizeof(buffer))) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt ()) {
		    disable_interrupt_key();
		    ftpfs_abort(fe->bucket, sock);
		    ftpfserrno = EINTR;
		    goto error_2;
		}
		else 
		    continue;
	    }
	    ftpfserrno = errno;
	    disable_interrupt_key();
	    goto error_1;
	}
	if (n == 0)
	    break;
	total += n;
	if (stat_size == 0)
	    print_vfs_message ("ftpfs: Getting file: %ld bytes transfered", 
			       total);
	else
	    print_vfs_message ("ftpfs: Getting file: %3d%% (%ld bytes transfered)",
			       total*100/stat_size, total);
        while (write(local_handle, buffer, n) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt()) {
		    ftpfs_abort(fe->bucket, sock);
		    ftpfserrno = EINTR;
		    goto error_2;
		}
		else
		    continue;
	    }
	    ftpfserrno = errno;
	    goto error_1;
	}
    }
    close(local_handle);
    close(sock);
    if (getreply (qsock (fe->bucket), NULL, 0) != COMPLETE) {
	ftpfserrno = EIO;
	goto error_2;
    }
    
    if (stat (fe->local_filename, &fe->local_stat) < 0)
        fe->local_stat.st_mtime = 0;
    
    if (tmp_reget > 0)
	fe->tmp_reget = 1;
    
    return 1;
error_1:
    getreply(qsock(fe->bucket), NULL, 0);
error_2:
    close(sock);
error_3:
    disable_interrupt_key ();
    close(local_handle);
    unlink(fe->local_filename);
    free(fe->local_filename);
    fe->local_filename = NULL;
    return 0;
}

static struct ftpentry *
_get_file_entry(struct ftpfs_connection *bucket, char *file_name, 
		int op, int flags)
{
    char *p, q;
    struct ftpentry *ent;
    struct linklist *file_list, *lptr;
    struct ftpfs_dir *dcache;
    struct stat sb;

    p = strrchr(file_name, '/');
    q = *p;
    *p = '\0';
    dcache = retrieve_dir(bucket, *file_name ? file_name : "/");
    if (dcache == NULL)
        return NULL;
    file_list = dcache->file_list;
    *p++ = q;
    if (!*p) 
        p = ".";
    for (lptr = file_list->next; lptr != file_list; lptr = lptr->next) {
	ent = lptr->data;
        if (strcmp(p, ent->name) == 0) {
	    if (S_ISLNK(ent->s.st_mode) && (op & FTPFS_RESOLVE_SYMLINK)) {
		if (ent->l_stat == NULL) {
		    ftpfserrno = ENOENT;
		    return NULL;
		}
		if (S_ISLNK(ent->l_stat->st_mode)) {
		    ftpfserrno = ELOOP;
		    return NULL;
		}
	    }
	    if (ent && (op & FTPFS_OPEN)) {
		mode_t fmode;

		fmode = S_ISLNK(ent->s.st_mode)
		    ? ent->l_stat->st_mode
		    : ent->s.st_mode;
		if (S_ISDIR(fmode)) {
		    ftpfserrno = EISDIR;
		    return NULL;
		}
		if (!S_ISREG(fmode)) {
		    ftpfserrno = EPERM;
		    return NULL;
		}
		if ((flags & O_EXCL) && (flags & O_CREAT)) {
		    ftpfserrno = EEXIST;
		    return NULL;
		}
		if (ent->remote_filename == NULL) {
		    ent->remote_filename = strdup(file_name);
		    if (ent->remote_filename == NULL) {
			ftpfserrno = ENOMEM;
			return NULL;
		    }
		}
		if (ent->local_filename == NULL || 
		    !ent->local_stat.st_mtime || 
		    stat (ent->local_filename, &sb) < 0 || 
		    sb.st_mtime != ent->local_stat.st_mtime) {
		    int handle;
		    
		    if (ent->local_filename){
		        free (ent->local_filename);
			ent->local_filename = 0;
		    }
		    if (flags & O_TRUNC) {
			ent->local_filename = tempnam (0, "ftpfs");
			if (ent->local_filename == NULL) {
			    ftpfserrno = ENOMEM;
			    return NULL;
			}
			handle = open(ent->local_filename, O_CREAT | O_TRUNC | O_RDWR | O_EXCL, 0600);
			if (handle < 0) {
			    ftpfserrno = EIO;
			    return NULL;
			}
			close(handle);
			if (stat (ent->local_filename, &ent->local_stat) < 0)
			    ent->local_stat.st_mtime = 0;
		    }
		    else {
		        if (localname != NULL) {
		            isremotecopy = 1;
		            ent->local_is_temp = 0;
		            ent->local_stat.st_mtime = 0;
		            ent->local_filename = strdup (localname);
		            if (!retrieve_file_start (ent)) {
		                isremotecopy = 0;
		                return NULL;
		            }
		            return ent;
		        }
		        if (!retrieve_file(ent))
			    return NULL;
		    }
		}
		else if (flags & O_TRUNC) {
			truncate(ent->local_filename, 0);
		}
	    }
	    return ent;
	}
    }
    if ((op & FTPFS_OPEN) && (flags & O_CREAT)) {
	int handle;

	ent = xmalloc(sizeof(struct ftpentry), "struct ftpentry");
	ent->freshly_created = 0;
	ent->tmp_reget = 0;
	if (ent == NULL) {
	    ftpfserrno = ENOMEM;
	    return NULL;
	}
	ent->count = 1;
	ent->linkname = NULL;
	ent->l_stat = NULL;
	ent->bucket = bucket;
	ent->name = strdup(p);
	ent->remote_filename = strdup(file_name);
	ent->local_filename = tempnam(0, "ftpfs");
	if (!ent->name && !ent->remote_filename && !ent->local_filename) {
	    ftpentry_destructor(ent);
	    ftpfserrno = ENOMEM;
	    return NULL;
	}
        handle = creat(ent->local_filename, 0700);
	if (handle == -1) {
	    ftpfserrno = EIO;
	    ftpentry_destructor(ent);
	    return NULL;
	}
	fstat(handle, &ent->s);
	close(handle);
#if 0
/* This is very wrong - like this a zero length file will be always created
   and usually preclude uploading anything more desirable */	
#if defined(UPLOAD_ZERO_LENGTH_FILE)
	if (!store_file(ent)) {
	    ftpentry_destructor(ent);
	    return NULL;
	}
#endif
#endif
	if (!linklist_insert(file_list, ent)) {
	    ftpfserrno = ENOMEM;
	    ftpentry_destructor(ent);
	    return NULL;
	}
	ent->freshly_created = 1;
	return ent;
    }
    else {
	ftpfserrno = ENOENT;
	return NULL;
    }
}

/* this just free's the local temp file. I don't know if the
   remote file can be used after this without crashing - paul
   psheer@obsidian.co.za psheer@icon.co.za */
static int remove_temp_file (char *file_name)
{
    char *p, q;
    struct ftpfs_connection *bucket;
    struct ftpentry *ent;
    struct linklist *file_list, *lptr;
    struct ftpfs_dir *dcache;

    if (!(file_name = ftpfs_get_path (&bucket, file_name)))
	return -1;
    p = strrchr (file_name, '/');
    q = *p;
    *p = '\0';
    dcache = retrieve_dir (bucket, *file_name ? file_name : "/");
    if (dcache == NULL)
	return -1;
    file_list = dcache->file_list;
    *p++ = q;
    if (!*p)
	p = ".";
    for (lptr = file_list->next; lptr != file_list; lptr = lptr->next) {
	ent = lptr->data;
	if (strcmp (p, ent->name) == 0) {
	    if (ent->local_filename) {
		unlink (ent->local_filename);
		free (ent->local_filename);
		ent->local_filename = 0;
		return 0;
	    }
	}
    }
    return -1;
}

static struct ftpentry *
get_file_entry(char *path, int op, int flags)
{
    struct ftpfs_connection *bucket;
    struct ftpentry *fe;
    char *remote_path;

    if (!(remote_path = ftpfs_get_path (&bucket, path)))
	return NULL;
    isremotecopy = 0;
    fe = _get_file_entry(bucket, remote_path, op,
			 flags);
    free(remote_path);
#if 0
    if (op & FTPFS_FREE_RESOURCE)
	vfs_add_noncurrent_stamps (&ftpfs_vfs_ops, (vfsid) bucket, NULL);
#endif
    return fe;
}

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

static int ftpfs_normal_flush = 1;

void ftpfs_hint_reread(int reread)
{
    if (reread)
	ftpfs_normal_flush++;
    else
	ftpfs_normal_flush--;
}

static int
send_ftp_command(char *filename, char *cmd, int flags)
{
    char *remote_path;
    struct ftpfs_connection *bucket;
    int r;
    int flush_directory_cache = (flags & OPT_FLUSH) && (ftpfs_normal_flush > 0);

    if (!(remote_path = ftpfs_get_path(&bucket, filename)))
	return -1;
    r = command (bucket, WAIT_REPLY, cmd, remote_path);
    free(remote_path);
    vfs_add_noncurrent_stamps (&ftpfs_vfs_ops, (vfsid) bucket, NULL);
    if (flags & OPT_IGNORE_ERROR)
	r = COMPLETE;
    if (r != COMPLETE) {
	ftpfserrno = EPERM;
	return -1;
    }
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
	p = "guest";
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

void
ftpfs_init ()
{
    ftpfs_connections_list = linklist_init();
    ftpfs_directory_timeout = FTPFS_DIRECTORY_TIMEOUT;
}

void
ftpfs_done(void)
{
    linklist_destroy(ftpfs_connections_list, ftpfs_connection_destructor);
    if (ftpfs_debug_server_dialog)
	fclose (ftpfs_logfile);
}

/* The callbacks */

struct ftpfs_filp {
    unsigned int has_changed:1;
    struct ftpentry *fe;
    int local_handle;
};

static void *ftpfs_open (char *file, int flags, int mode)
{
    struct ftpfs_filp *fp;
    struct ftpentry *fe;

    fp = xmalloc(sizeof(struct ftpfs_filp), "struct ftpfs_filp");
    if (fp == NULL) {
	ftpfserrno = ENOMEM;
        return NULL;
    }
    fe = get_file_entry(file, FTPFS_OPEN | FTPFS_RESOLVE_SYMLINK, flags);
    if (fe == NULL) {
	free(fp);
        return NULL;
    }
    if (!isremotecopy) {
        fp->local_handle = open(fe->local_filename, flags, mode);
        if (fp->local_handle < 0) {
	    ftpfserrno = errno;
	    free(fp);
	    return NULL;
        }
    } else
        fp->local_handle = -1;
#ifdef UPLOAD_ZERO_LENGTH_FILE        
    fp->has_changed = fe->freshly_created;
#else
    fp->has_changed = 0;
#endif    
    fp->fe = fe;
    qlock(fe->bucket)++;
    fe->count++;
    return fp;
}

static int ftpfs_read (void *data, char *buffer, int count)
{
    struct ftpfs_filp *fp;
    int n;
    
    fp = data;
    n = read(fp->local_handle, buffer, count);
    if (n < 0)
      ftpfserrno = errno;
    return n;
}

int ftpfs_write (void *data, char *buf, int nbyte)
{
    struct ftpfs_filp *fp;
    int n;
    
    fp = data;
    n = write(fp->local_handle, buf, nbyte);
    if (n < 0)
      ftpfserrno = errno;
    fp->has_changed = 1;
    return n;
}

static int ftpfs_close (void *data)
{
    struct ftpfs_filp *fp = data;
    int result = 0;

    if (fp->has_changed) {
	if (!store_file(fp->fe))
	    result = -1;
	if (ftpfs_normal_flush)
	    flush_all_directory(fp->fe->bucket);
    }
    if (fp->local_handle >= 0)
        close(fp->local_handle);
    qlock(fp->fe->bucket)--;
    ftpentry_destructor(fp->fe);
    free(fp);
    return result;
}

static int ftpfs_errno (void)
{
    return ftpfserrno;
}


/* Explanation:
 * On some operating systems (Slowaris 2 for example)
 * the d_name member is just a char long (nice trick that break everything),
 * so we need to set up some space for the filename.
 */
struct ftpfs_dirent {
    struct dirent dent;
#ifdef NEED_EXTRA_DIRENT_BUFFER
    char extra_buffer [MC_MAXPATHLEN];
#endif
    struct linklist *pos;
    struct ftpfs_dir *dcache;
};

/* Possible FIXME: what happens if one directory is opened twice ? */

static void *ftpfs_opendir (char *dirname)
{
    struct ftpfs_connection *bucket;
    char *remote_path;
    struct ftpfs_dirent *dirp;

    if (!(remote_path = ftpfs_get_path (&bucket, dirname)))
        return NULL;
    dirp = xmalloc(sizeof(struct ftpfs_dirent), "struct ftpfs_dirent");
    if (dirp == NULL) {
	ftpfserrno = ENOMEM;
	goto error_return;
    }
    dirp->dcache = retrieve_dir(bucket, remote_path);
    if (dirp->dcache == NULL)
        goto error_return;
    dirp->pos = dirp->dcache->file_list->next;
    free(remote_path);
    dirp->dcache->count++;
    return (void *)dirp;
error_return:
    vfs_add_noncurrent_stamps (&ftpfs_vfs_ops, (vfsid) bucket, NULL);
    free(remote_path);
    free(dirp);
    return NULL;
}

static void *ftpfs_readdir (void *data)
{
    struct ftpentry *fe;
    struct ftpfs_dirent *dirp = data;
    
    if (dirp->pos == dirp->dcache->file_list)
	return NULL;
    fe = dirp->pos->data;
    strcpy (&(dirp->dent.d_name [0]), fe->name);
#ifndef DIRENT_LENGTH_COMPUTED
    dirp->d_namlen = strlen (dirp->d_name);
#endif
    dirp->pos = dirp->pos->next;
    return (void *) &dirp->dent;
}

static int ftpfs_telldir (void *data)
{
    struct ftpfs_dirent *dirp = data;
    struct linklist *pos;
    int i = 0;
    
    pos = dirp->dcache->file_list->next;
    while( pos!=dirp->dcache->file_list) {
    	if (pos == dirp->pos)
	    return i;
	pos = pos->next;
	i++;
    }
    return -1;
}

static void ftpfs_seekdir (void *data, int pos)
{
    struct ftpfs_dirent *dirp = data;
    int i;

    dirp->pos = dirp->dcache->file_list->next;
    for (i=0; i<pos; i++)
        ftpfs_readdir(data);
}

static int ftpfs_closedir (void *info)
{
    struct ftpfs_dirent *dirp = info;
    ftpfs_dir_destructor(dirp->dcache);
    free(dirp);
    return 0;
}

static int ftpfs_lstat (char *path, struct stat *buf)
{
    struct ftpentry *fe;
    
    fe = get_file_entry(path, FTPFS_FREE_RESOURCE, 0);
    if (fe) {
	*buf = fe->s;
	return 0;
    }
    else
        return -1;
}

static int ftpfs_stat (char *path, struct stat *buf)
{
    struct ftpentry *fe;

    fe = get_file_entry(path, FTPFS_RESOLVE_SYMLINK | FTPFS_FREE_RESOURCE, 0);
    if (fe) {
	if (!S_ISLNK(fe->s.st_mode))
	    *buf = fe->s;
	else
	    *buf = *fe->l_stat;
	return 0;
    }
    else
        return -1;
}

int ftpfs_fstat (void *data, struct stat *buf)
{
    struct ftpfs_filp *fp = data;

    if (!S_ISLNK(fp->fe->s.st_mode))
	*buf = fp->fe->s;
    else
	*buf = *fp->fe->l_stat;
    return 0;
}

int ftpfs_chmod (char *path, int mode)
{
    char buf[40];
    
    sprintf(buf, "SITE CHMOD %4.4o %%s", mode & 07777);
    return send_ftp_command(path, buf, OPT_IGNORE_ERROR | OPT_FLUSH);
}

int ftpfs_chown (char *path, int owner, int group)
{
#if 0
    ftpfserrno = EPERM;
    return -1;
#else
/* Everyone knows it is not possible to chown remotely, so why bother them.
   If someone's root, then copy/move will always try to chown it... */
    return 0;
#endif    
}

static int ftpfs_readlink (char *path, char *buf, int size)
{
    struct ftpentry *fe;

    fe = get_file_entry(path, FTPFS_FREE_RESOURCE, 0);
    if (!fe)
	return -1;
    if (!S_ISLNK(fe->s.st_mode)) {
	ftpfserrno = EINVAL;
	return -1;
    }
    if (fe->linkname == NULL) {
	ftpfserrno = EACCES;
	return -1;
    }
    if (strlen(fe->linkname) >= size) {
	ftpfserrno = ERANGE;
	return -1;
    }
    strncpy(buf, fe->linkname, size);
    return strlen(fe->linkname);
}

static int ftpfs_unlink (char *path)
{
    return send_ftp_command(path, "DELE %s", 1);
}

static int ftpfs_symlink (char *n1, char *n2)
{
    ftpfserrno = EPERM;
    return -1;
}

static int ftpfs_rename (char *path1, char *path2)
{
    ftpfserrno = EPERM;
    return -1;
}

static int
__ftpfs_chdir (struct ftpfs_connection *bucket ,char *remote_path)
{
    int r;
    
    if (!bucket->cwd_defered && ftpfs_same_dir (remote_path, bucket))
	return COMPLETE;

    r = command (bucket, WAIT_REPLY, "CWD %s", remote_path);
    if (r != COMPLETE) {
	ftpfserrno = EIO;
    } else {
	if (qcdir(bucket))
	    free(qcdir(bucket));
	qcdir(bucket) = strdup (remote_path);
	bucket->cwd_defered = 0;
    }
    return r;
}

static int ftpfs_chdir (char *path)
{
    char *remote_path;
    struct ftpfs_connection *bucket;

    if (!(remote_path = ftpfs_get_path(&bucket, path)))
	return -1;
    if (qcdir(bucket))
        free(qcdir(bucket));
    qcdir(bucket) = remote_path;
    bucket->cwd_defered = 1;
    
    vfs_add_noncurrent_stamps (&ftpfs_vfs_ops, (vfsid) bucket, NULL);
    return 0;
}

static int ftpfs_lseek (void *data, off_t offset, int whence)
{
    struct ftpfs_filp *fp = data;

    return lseek(fp->local_handle, offset, whence);
}

static int ftpfs_mknod (char *path, int mode, int dev)
{
    ftpfserrno = EPERM;
    return -1;
}

static int ftpfs_mkdir (char *path, mode_t mode)
{
    return send_ftp_command(path, "MKD %s", 1);
}

static int ftpfs_rmdir (char *path)
{
    return send_ftp_command(path, "RMD %s", 1);
}

static int ftpfs_link (char *p1, char *p2)
{
    ftpfserrno = EPERM;
    return -1;
}

static vfsid ftpfs_getid (char *p, struct vfs_stamping **parent)
{
    struct ftpfs_connection *bucket;
    char *remote_path;

    *parent = NULL; /* We are not enclosed in any other fs */
    
    if (!(remote_path = ftpfs_get_path (&bucket, p)))
        return (vfsid) -1;
    else {
	free(remote_path);
    	return (vfsid) bucket;
    }
}

static int ftpfs_nothingisopen (vfsid id)
{
    return qlock((struct ftpfs_connection *)id) == 0;
}

static void ftpfs_free (vfsid id)
{
    struct ftpfs_connection *bucket = (struct ftpfs_connection *) id;
 
    ftpfs_connection_destructor(bucket);
    linklist_delete(ftpfs_connections_list, bucket);
}

static char *ftpfs_getlocalcopy (char *path)
{
    struct ftpfs_filp *fp = (struct ftpfs_filp *) ftpfs_open (path, O_RDONLY, 0);
    char *p;
    
    if (fp == NULL)
        return NULL;
    if (fp->fe->local_filename == NULL) {
        ftpfs_close ((void *) fp);
        return NULL;
    }
    p = strdup (fp->fe->local_filename);
    qlock(fp->fe->bucket)++;
    fp->fe->count++;
    ftpfs_close ((void *) fp);
    return p;
}

static void ftpfs_ungetlocalcopy (char *path, char *local, int has_changed)
{
    struct ftpfs_filp *fp = (struct ftpfs_filp *) ftpfs_open (path, O_WRONLY, 0);
    
    if (fp == NULL)
        return;
    if (!strcmp (fp->fe->local_filename, local)) {
        fp->has_changed = has_changed;
        qlock(fp->fe->bucket)--;
        ftpentry_destructor(fp->fe);
        ftpfs_close ((void *) fp);
    } else {
        /* Should not happen */
        ftpfs_close ((void *) fp);
        mc_def_ungetlocalcopy (path, local, has_changed);
    }
}

void ftpfs_set_debug (char *file)
{
    if ((ftpfs_logfile = fopen (file, "w+")) != NULL)
	ftpfs_debug_server_dialog = 1;
}

void ftpfs_forget (char *file)
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
    if (!(rp = ftpfs_get_host_and_username (file, &host, &user, &port, &pass))) {
        free (host);
	free (user);
	if (pass)
	    wipe_password (pass);
	return;
    }

    /* we do not care about the path actually */
    free (rp);
    
    for (l = ftpfs_connections_list->next; l != ftpfs_connections_list; l = l->next){
	struct ftpfs_connection *bucket = l->data;
	
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

#ifdef HAVE_MMAP
caddr_t ftpfs_mmap (caddr_t addr, size_t len, int prot, int flags, void *data, off_t offset)
{
    return (caddr_t)-1; /* We do not mmap to far away */
}

int ftpfs_munmap (caddr_t addr, size_t len, void *data)
{
    return -1;
}
#endif

vfs ftpfs_vfs_ops = {
    ftpfs_open,
    ftpfs_close,
    ftpfs_read,
    ftpfs_write,
    
    ftpfs_opendir,
    ftpfs_readdir,
    ftpfs_closedir,
    ftpfs_telldir,
    ftpfs_seekdir,

    ftpfs_stat,
    ftpfs_lstat,
    ftpfs_fstat,

    ftpfs_chmod,
    ftpfs_chown,
    NULL,

    ftpfs_readlink,
    ftpfs_symlink,
    ftpfs_link,
    ftpfs_unlink,

    ftpfs_rename,
    ftpfs_chdir,
    ftpfs_errno,
    ftpfs_lseek,
    ftpfs_mknod,
    
    ftpfs_getid,
    ftpfs_nothingisopen,
    ftpfs_free,
    
    ftpfs_getlocalcopy,
    ftpfs_ungetlocalcopy,

    ftpfs_mkdir,
    ftpfs_rmdir,
    ftpfs_ctl,
    ftpfs_setctl,
    ftpfs_forget
#ifdef HAVE_MMAP
    , ftpfs_mmap,
    ftpfs_munmap
#endif
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
