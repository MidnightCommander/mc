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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
   
/* FTPfs TODO:

- make it more robust - all the connects etc. should handle EADDRINUSE and
  ERETRY (have I spelled these names correctly?)
- make the user able to flush a connection - all the caches will get empty
  etc., (tarfs as well), we should give there a user selectable timeout
  and assign a key sequence.  
- use hash table instead of linklist to cache ftpfs directory.

What to do with this?


     * NOTE: Usage of tildes is deprecated, consider:
     * cd /#ftp:pavel@hobit
     * cd ~
     * And now: what do I want to do? Do I want to go to /home/pavel or to
     * /#ftp:hobit/home/pavel? I think first has better sense...
     *
    {
        int f = !strcmp( remote_path, "/~" );
	if (f || !strncmp( remote_path, "/~/", 3 )) {
	    char *s;
	    s = concat_dir_and_file( qhome (*bucket), remote_path +3-f );
	    g_free (remote_path);
	    remote_path = s;
	}
    }


 */

/* Namespace pollution: horrible */

#include <config.h>
#include <sys/types.h>          /* POSIX-required by sys/socket.h and netdb.h */
#include <netdb.h>		/* struct hostent */
#include <sys/socket.h>		/* AF_INET */
#include <netinet/in.h>		/* struct in_addr */
#ifdef HAVE_SETSOCKOPT
#    include <netinet/ip.h>	/* IP options */
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <arpa/ftp.h>
#include <arpa/telnet.h>
#include <sys/param.h>

#include "utilvfs.h"

#include "xdirentry.h"
#include "vfs.h"
#include "tcputil.h"
#include "../src/dialog.h"
#include "../src/setup.h"	/* for load_anon_passwd */
#include "ftpfs.h"
#ifndef MAXHOSTNAMELEN
#    define MAXHOSTNAMELEN 64
#endif

#define UPLOAD_ZERO_LENGTH_FILE
#define SUP super->u.ftp
#define FH_SOCK fh->u.ftp.sock

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif


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

/* First "CWD <path>", then "LIST -la ." */
int ftpfs_first_cd_then_ls;

/* Use the ~/.netrc */
int use_netrc = 1;

/* Anonymous setup */
char *ftpfs_anonymous_passwd = NULL;
int ftpfs_directory_timeout = 900;

/* Proxy host */
char *ftpfs_proxy_host = NULL;

/* wether we have to use proxy by default? */
int ftpfs_always_use_proxy;

/* source routing host */
extern int source_route;

/* Where we store the transactions */
static FILE *logfile = NULL;

/* If true, the directory cache is forced to reload */
static int force_expiration = 0;

#ifdef FIXME_LATER_ALIGATOR
static struct linklist *connections_list;
#endif

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02
static char reply_str [80];

/* char *translate_path (struct ftpfs_connection *bucket, char *remote_path)
   Translate a Unix path, i.e. MC's internal path representation (e.g. 
   /somedir/somefile) to a path valid for the remote server. Every path 
   transfered to the remote server has to be mangled by this function 
   right prior to sending it.
   Currently only Amiga ftp servers are handled in a special manner.
   
   When the remote server is an amiga:
   a) strip leading slash if necesarry
   b) replace first occurance of ":/" with ":"
   c) strip trailing "/."
 */

static char *ftpfs_get_current_directory (vfs *me, vfs_s_super *super);
static int ftpfs_chdir_internal (vfs *me, vfs_s_super *super, char *remote_path);
static int command (vfs *me, vfs_s_super *super, int wait_reply, const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));
static int ftpfs_open_socket (vfs *me, vfs_s_super *super);
static int login_server (vfs *me, vfs_s_super *super, const char *netrcpass);
static int lookup_netrc (const char *host, char **login, char **pass);

static char *
translate_path (vfs *me, vfs_s_super *super, const char *remote_path)
{
    if (!SUP.remote_is_amiga)
	return g_strdup (remote_path);
    else {
	char *ret, *p;

	if (logfile) {
	    fprintf (logfile, "MC -- translate_path: %s\n", remote_path);
	    fflush (logfile);
	}

	/* strip leading slash(es) */
	while (*remote_path == '/')
	    remote_path++;

	/*
	 * Don't change "/" into "", e.g. "CWD " would be
	 * invalid.
	 */
        if (*remote_path == '\0')
	    return g_strdup ("."); 

	ret = g_strdup (remote_path);

	/* replace first occurance of ":/" with ":" */
	if ((p = strchr (ret, ':')) && *(p + 1) == '/')
	    strcpy (p + 1, p + 2);

	/* strip trailing "/." */
	if ((p = strrchr (ret, '/')) && *(p + 1) == '.' && *(p + 2) == '\0')
	    *p = '\0';
	return ret;
    }
}

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

#define FTP_COMMAND_PORT   21

static void
ftp_split_url(char *path, char **host, char **user, int *port, char **pass)
{
    char *p;

    p = vfs_split_url (path, host, user, port, pass, FTP_COMMAND_PORT,
		       URL_ALLOW_ANON);

    if (!*user) {
	/* Look up user and password in netrc */
	if (use_netrc)
	    lookup_netrc (*host, user, pass);
	if (!*user)
	    *user = g_strdup ("anonymous");
    }

    /* Look up password in netrc for known user */
    if (use_netrc && *user && pass && !*pass) {
	char *new_user;

	lookup_netrc (*host, &new_user, pass);

	/* If user is different, remove password */
	if (new_user && strcmp (*user, new_user)) {
	    g_free (*pass);
	    *pass = NULL;
	}

	g_free (new_user);
    }

    if (p)
	g_free (p);
}

/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */
static int
get_reply (vfs *me, int sock, char *string_buf, int string_len)
{
    char answer[BUF_1K];
    int i;
    
    for (;;) {
        if (!vfs_s_get_line (me, sock, answer, sizeof (answer), '\n')){
	    if (string_buf)
		*string_buf = 0;
	    code = 421;
	    return 4;
	}
	switch (sscanf(answer, "%d", &code)){
	    case 0:
	        if (string_buf) {
		    strncpy (string_buf, answer, string_len - 1);
		    *(string_buf + string_len - 1) = 0;
		}
	        code = 500;
	        return 5;
	    case 1:
 		if (answer[3] == '-') {
		    while (1) {
			if (!vfs_s_get_line (me, sock, answer, sizeof(answer), '\n')){
			    if (string_buf)
				*string_buf = 0;
			    code = 421;
			    return 4;
			}
			if ((sscanf (answer, "%d", &i) > 0) && 
			    (code == i) && (answer[3] == ' '))
			    break;
		    }
		}
	        if (string_buf){
		    strncpy (string_buf, answer, string_len - 1);
		    *(string_buf + string_len - 1) = 0;
		}
		return code / 100;
	}
    }
}

static int
reconnect (vfs *me, vfs_s_super *super)
{
    int sock = ftpfs_open_socket (me, super);
    if (sock != -1){
	char *cwdir = SUP.cwdir;
	close (SUP.sock);
	SUP.sock = sock;
	SUP.cwdir = NULL;
	if (login_server (me, super, SUP.password)){
	    if (!cwdir)
		return 1;
	    sock = ftpfs_chdir_internal (me, super, cwdir);
	    g_free (cwdir);
	    return sock == COMPLETE;
	}
	SUP.cwdir = cwdir;
    }
    return 0;
}

static int
command (vfs *me, vfs_s_super *super, int wait_reply, const char *fmt, ...)
{
    va_list ap;
    char *str, *fmt_str;
    int status;
    int sock = SUP.sock;
    
    va_start (ap, fmt);
    fmt_str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    status = strlen (fmt_str);
    str = g_realloc (fmt_str, status + 3);
    strcpy (str + status, "\r\n");

    if (logfile){
        if (strncmp (str, "PASS ", 5) == 0){
            fputs ("PASS <Password not logged>\r\n", logfile);
        } else
	    fwrite (str, status + 2, 1, logfile);

	fflush (logfile);
    }

    got_sigpipe = 0;
    enable_interrupt_key ();
    status = write (SUP.sock, str, status + 2);
    
    if (status < 0){
	code = 421;

	if (errno == EPIPE){		/* Remote server has closed connection */
	    static int level = 0;	/* login_server() use command() */
	    if (level == 0){  
		level = 1;
		status = reconnect (me, super);
		level = 0;
		if (status && write (SUP.sock, str, status + 2) > 0)
		    goto ok;
	    }
	    got_sigpipe = 1;
	}
	g_free (str);
	disable_interrupt_key ();
	return TRANSIENT;
    }
  ok:
    g_free (str);
    disable_interrupt_key ();
    
    if (wait_reply)
	return get_reply (me, sock, (wait_reply & WANT_STRING) ? reply_str : NULL, sizeof (reply_str)-1);
    return COMPLETE;
}

static void
free_archive (vfs *me, vfs_s_super *super)
{
    if (SUP.sock != -1){
	print_vfs_message (_("ftpfs: Disconnecting from %s"), SUP.host);
	command(me, super, NONE, "QUIT");
	close(SUP.sock);
    }
    g_free (SUP.host);
    g_free (SUP.user);
    g_free (SUP.cwdir);
    g_free (SUP.password);
}

/* some defines only used by changetype */
/* These two are valid values for the second parameter */
#define TYPE_ASCII    0
#define TYPE_BINARY   1

/* This one is only used to initialize bucket->isbinary, don't use it as
   second parameter to changetype. */
#define TYPE_UNKNOWN -1 

static int
changetype (vfs *me, vfs_s_super *super, int binary)
{
    if (binary != SUP.isbinary) {
        if (command (me, super, WAIT_REPLY, "TYPE %c", binary ? 'I' : 'A') != COMPLETE)
		ERRNOR (EIO, -1);
        SUP.isbinary = binary;
    }
    return binary;
}

/* This routine logs the user in */
static int 
login_server (vfs *me, vfs_s_super *super, const char *netrcpass)
{
    char *pass;
    char *op;
    char *name;			/* login user name */
    int  anon = 0;
    char reply_string[BUF_MEDIUM];

    SUP.isbinary = TYPE_UNKNOWN;    
    if (netrcpass)
        op = g_strdup (netrcpass);
    else {
        if (!strcmp (SUP.user, "anonymous") || 
            !strcmp (SUP.user, "ftp")) {
	    if (!ftpfs_anonymous_passwd)
	        ftpfs_init_passwd();
	    op = g_strdup (ftpfs_anonymous_passwd);
	    anon = 1;
         } else {
            char *p;

	    if (!SUP.password){
		p = g_strconcat (_(" FTP: Password required for "), SUP.user, 
				  " ", NULL);
		op = vfs_get_password (p);
		g_free (p);
		if (op == NULL)
			ERRNOR (EPERM, 0);
		SUP.password = g_strdup (op);
	    } else
		op = g_strdup (SUP.password);
        }
    }

    if (!anon || logfile)
	pass = op;
    else {
	pass = g_strconcat ("-", op, NULL);
	wipe_password (op);
    }
    
    /* Proxy server accepts: username@host-we-want-to-connect*/
    if (SUP.proxy){
	name = g_strconcat (SUP.user, "@", 
		SUP.host[0] == '!' ? SUP.host+1 : SUP.host, NULL);
    } else 
	name = g_strdup (SUP.user);
    
    if (get_reply (me, SUP.sock, reply_string, sizeof (reply_string) - 1) == COMPLETE) {
	g_strup (reply_string);
	SUP.remote_is_amiga = strstr (reply_string, "AMIGA") != 0;
	if (logfile) {
	    fprintf (logfile, "MC -- remote_is_amiga =  %d\n", SUP.remote_is_amiga);
	    fflush (logfile);
	}

	print_vfs_message (_("ftpfs: sending login name"));
	code = command (me, super, WAIT_REPLY, "USER %s", name);

	switch (code){
	case CONTINUE:
	    print_vfs_message (_("ftpfs: sending user password"));
            if (command (me, super, WAIT_REPLY, "PASS %s", pass) != COMPLETE)
		break;

	case COMPLETE:
	    print_vfs_message (_("ftpfs: logged in"));
	    wipe_password (pass);
	    g_free (name);
	    return 1;

	default:
	    SUP.failed_on_login = 1;
	    /* my_errno = E; */
	    if (SUP.password)
		wipe_password (SUP.password);
	    SUP.password = 0;
	    
	    goto login_fail;
	}
    }
    message_2s (1, MSG_ERROR, _("ftpfs: Login incorrect for user %s "), SUP.user);
login_fail:
    wipe_password (pass);
    g_free (name);
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
    memset (buffer, 0, sizeof (buffer));
    *ptr++ = IPOPT_LSRR;
    *ptr++ = 3 + 8;
    *ptr++ = 4;			/* pointer */

    /* First hop */
    memcpy (ptr, (char *) &source_route, sizeof (int));
    ptr += 4;

    /* Second hop (ie, final destination) */
    memcpy (ptr, (char *) &dest, sizeof (int));
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
load_no_proxy_list (void)
{
    /* FixMe: shouldn't be hardcoded!!! */
    char	s[BUF_LARGE]; /* provide for BUF_LARGE characters */
    struct no_proxy_entry *np, *current = 0;
    FILE	*npf;
    int		c;
    char 	*p;
    static char	*mc_file;

    if (mc_file)
	return;

    mc_file = concat_dir_and_file (mc_home, "mc.no_proxy");
    if (exist_file (mc_file) &&
	(npf = fopen (mc_file, "r"))) {
	while (fgets (s, sizeof(s), npf) || !(feof (npf) || ferror (npf))) {
	    if (!(p = strchr (s, '\n'))) {	/* skip bogus entries */ 
		while ((c = fgetc (npf)) != EOF && c != '\n')
		    ;
		continue;
	    }

	    if (p == s)
		continue;

	    *p = '\0';
	    
	    np = g_new (struct no_proxy_entry, 1);
	    np->domain = g_strdup (s);
	    np->next   = NULL;
	    if (no_proxy)
		current->next = np;
	    else
		no_proxy = np;
	    current = np;
	}

	fclose (npf);
    }
    g_free (mc_file);
}

/* Return 1 if FTP proxy should be used for this host, 0 otherwise */
static int
ftpfs_check_proxy (const char *host)
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
	    if (!g_strcasecmp (host, domain))
		return 0;
    }

    return 1;
}

static void
ftpfs_get_proxy_host_and_port (char *proxy, char **host, int *port)
{
    char *user, *dir;

    dir =
	vfs_split_url (proxy, host, &user, port, 0, FTP_COMMAND_PORT,
		       URL_ALLOW_ANON);
    g_free (user);
    g_free (dir);
}

static int
ftpfs_open_socket (vfs *me, vfs_s_super *super)
{
    struct   sockaddr_in server_address;
    struct   hostent *hp;
    int      my_socket;
    char     *host;
    int      port = SUP.port;
    int      free_host = 0;
    
    /* Use a proxy host? */
    host = SUP.host;

    if (!host || !*host){
	print_vfs_message (_("ftpfs: Invalid host name."));
        my_errno = EINVAL;
	return -1;
    }

    /* Hosts to connect to that start with a ! should use proxy */
    if (SUP.proxy){
	ftpfs_get_proxy_host_and_port (ftpfs_proxy_host, &host, &port);
	free_host = 1;
    }
    
    /* Get host address */
    memset ((char *) &server_address, 0, sizeof (server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr (host);
    if (server_address.sin_addr.s_addr == INADDR_NONE) {
	hp = gethostbyname (host);
	if (hp == NULL){
	    print_vfs_message (_("ftpfs: Invalid host address."));
	    my_errno = EINVAL;
	    if (free_host)
		g_free (host);
	    return -1;
	}
	server_address.sin_family = hp->h_addrtype;

	/* We copy only 4 bytes, we can not trust hp->h_length, as it comes from the DNS */
	memcpy ((char *) &server_address.sin_addr, (char *) hp->h_addr, 4);
    }

    server_address.sin_port = htons (port);

    /* Connect */
    if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	my_errno = errno;
        if (free_host)
	    g_free (host);
	return -1;
    }
    setup_source_route (my_socket, server_address.sin_addr.s_addr);
    
    print_vfs_message (_("ftpfs: making connection to %s"), host);
    if (free_host)
	g_free (host);

    enable_interrupt_key (); /* clear the interrupt flag */

    if (connect (my_socket, (struct sockaddr *) &server_address,
	     sizeof (server_address)) < 0){
	my_errno = errno;
	if (errno == EINTR && got_interrupt ())
	    print_vfs_message (_("ftpfs: connection interrupted by user"));
	else
	    print_vfs_message (_("ftpfs: connection to server failed: %s"),
				   unix_error_string(errno));
	disable_interrupt_key();
	close (my_socket);
	return -1;
    }
    disable_interrupt_key();
    return my_socket;
}

static int
open_archive_int (vfs *me, vfs_s_super *super)
{
    int retry_seconds, count_down;

    /* We do not want to use the passive if we are using proxies */
    if (SUP.proxy)
	SUP.use_passive_connection = 0;
    
    retry_seconds = 0;
    do { 
	SUP.failed_on_login = 0;

	SUP.sock = ftpfs_open_socket (me, super);
	if (SUP.sock == -1)
	    return -1;

	if (login_server (me, super, NULL)) {
	    /* Logged in, no need to retry the connection */
	    break;
	} else {
	    if (SUP.failed_on_login){
		/* Close only the socket descriptor */
		close (SUP.sock);
	    } else {
		return -1;
	    } 
	    if (ftpfs_retry_seconds){
		retry_seconds = ftpfs_retry_seconds;
		enable_interrupt_key ();
		for (count_down = retry_seconds; count_down; count_down--){
		    print_vfs_message (_("Waiting to retry... %d (Control-C to cancel)"), count_down);
		    sleep (1);
		    if (got_interrupt ()){
			/* my_errno = E; */
			disable_interrupt_key ();
			return 0;
		    }
		}
		disable_interrupt_key ();
	    }
	}
    } while (retry_seconds);
    
    SUP.cwdir = ftpfs_get_current_directory (me, super);
    if (!SUP.cwdir)
        SUP.cwdir = g_strdup (PATH_SEP_STR);
    return 0;
}

static int
open_archive (vfs *me, vfs_s_super *super, char *archive_name, char *op)
{
    char *host, *user, *password;
    int port;

    ftp_split_url (strchr (op, ':') + 1, &host, &user, &port, &password);

    SUP.host = host;
    SUP.user = user;
    SUP.port = port;
    SUP.cwdir = NULL;
    SUP.proxy = 0;
    if (ftpfs_check_proxy (host))
	SUP.proxy = ftpfs_proxy_host;
    SUP.password = password;
    SUP.use_passive_connection = ftpfs_use_passive_connections | source_route;
    SUP.use_source_route = source_route;
    SUP.strict = ftpfs_use_unix_list_options ? RFC_AUTODETECT : RFC_STRICT;
    SUP.isbinary = TYPE_UNKNOWN;
    SUP.remote_is_amiga = 0;
    super->name = g_strdup("/");
    super->root = vfs_s_new_inode (me, super, vfs_s_default_stat(me, S_IFDIR | 0755)); 

    return open_archive_int (me, super);
}

static int
archive_same(vfs *me, vfs_s_super *super, char *archive_name, char *op, void *cookie)
{	
    char *host, *user;
    int port;

    ftp_split_url (strchr(op, ':') + 1, &host, &user, &port, 0);

    port = ((strcmp (host, SUP.host) == 0) &&
	    (strcmp (user, SUP.user) == 0) &&
	    (port == SUP.port));

    g_free (host);
    g_free (user);

    return port;
}

void
ftpfs_flushdir (void)
{
    force_expiration = 1;
}

static int
dir_uptodate(vfs *me, vfs_s_inode *ino)
{
    struct timeval tim;

    if (force_expiration) {
	force_expiration = 0;
	return 0;
    }
    gettimeofday(&tim, NULL);
    if (tim.tv_sec < ino->u.ftp.timestamp.tv_sec)
	return 1;
    return 0;
}

/* The returned directory should always contain a trailing slash */
static char *
ftpfs_get_current_directory (vfs *me, vfs_s_super *super)
{
    char buf[BUF_8K], *bufp, *bufq;

    if (command (me, super, NONE, "PWD") == COMPLETE &&
        get_reply(me, SUP.sock, buf, sizeof(buf)) == COMPLETE) {
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
			if (*bufp == '/')
			    return g_strdup (bufp);
			else {
			    /* If the remote server is an Amiga a leading slash
			       might be missing. MC needs it because it is used
			       as seperator between hostname and path internally. */
			    return g_strconcat( "/", bufp, 0);
			}
		    } else {
			my_errno = EIO;
			return NULL;
		    }
		}
	    }
    }
    my_errno = EIO;
    return NULL;
}

    
/* Setup Passive ftp connection, we use it for source routed connections */
static int
setup_passive (vfs *me, vfs_s_super *super, int my_socket, struct sockaddr_in *sa)
{
    int xa, xb, xc, xd, xe, xf;
    char n [6];
    char *c = reply_str;
    
    if (command (me, super, WAIT_REPLY | WANT_STRING, "PASV") != COMPLETE)
	return 0;

    /* Parse remote parameters */
    for (c = reply_str + 4; (*c) && (!isdigit ((unsigned char) *c)); c++)
	;
    if (!*c)
	return 0;
    if (!isdigit ((unsigned char) *c))
	return 0;
    if (sscanf (c, "%d,%d,%d,%d,%d,%d", &xa, &xb, &xc, &xd, &xe, &xf) != 6)
	return 0;
    n [0] = (unsigned char) xa;
    n [1] = (unsigned char) xb;
    n [2] = (unsigned char) xc;
    n [3] = (unsigned char) xd;
    n [4] = (unsigned char) xe;
    n [5] = (unsigned char) xf;

    memcpy (&(sa->sin_addr.s_addr), (void *)n, 4);
    memcpy (&(sa->sin_port), (void *)&n[4], 2);
    setup_source_route (my_socket, sa->sin_addr.s_addr);
    if (connect (my_socket, (struct sockaddr *) sa, sizeof (struct sockaddr_in)) < 0)
	return 0;
    return 1;
}

static int
initconn (vfs *me, vfs_s_super *super)
{
    struct sockaddr_in data_addr;
    int data, len = sizeof(data_addr);
    struct protoent *pe;

    getsockname(SUP.sock, (struct sockaddr *) &data_addr, &len);
    data_addr.sin_port = 0;
    
    pe = getprotobyname("tcp");
    if (pe == NULL)
	    ERRNOR (EIO, -1);
    data = socket (AF_INET, SOCK_STREAM, pe->p_proto);
    if (data < 0)
	    ERRNOR (EIO, -1);

    if (SUP.use_passive_connection){
	if ((SUP.use_passive_connection = setup_passive (me, super, data, &data_addr)))
	    return data;

	SUP.use_source_route = 0;
	SUP.use_passive_connection = 0;
	print_vfs_message (_("ftpfs: could not setup passive mode"));
    }

    /* If passive setup fails, fallback to active connections */
    /* Active FTP connection */
    if ((bind (data, (struct sockaddr *)&data_addr, len) == 0) &&
	(getsockname (data, (struct sockaddr *) &data_addr, &len) == 0) && 
	(listen (data, 1) == 0))
    {
	unsigned char *a = (unsigned char *)&data_addr.sin_addr;
	unsigned char *p = (unsigned char *)&data_addr.sin_port;
	
	if (command (me, super, WAIT_REPLY, "PORT %d,%d,%d,%d,%d,%d", a[0], a[1], 
		     a[2], a[3], p[0], p[1]) == COMPLETE)
	    return data;
    }
    close(data);
    my_errno = EIO;
    return -1;
}

static int
open_data_connection (vfs *me, vfs_s_super *super, char *cmd, char *remote, 
		int isbinary, int reget)
{
    struct sockaddr_in from;
    int s, j, data, fromlen = sizeof(from);
    
    if ((s = initconn (me, super)) == -1)
        return -1;
    if (changetype (me, super, isbinary) == -1)
        return -1;
    if (reget > 0){
	j = command (me, super, WAIT_REPLY, "REST %d", reget);
	if (j != CONTINUE)
	    return -1;
    }
    if (remote) {
	char * remote_path = translate_path (me, super, remote);
	j = command (me, super, WAIT_REPLY, "%s /%s", cmd, 
	    /* WarFtpD can't STORE //filename */
	    (*remote_path == '/') ? remote_path + 1 : remote_path);
	g_free (remote_path);
    } else
    	j = command (me, super, WAIT_REPLY, "%s", cmd);
    if (j != PRELIM)
	ERRNOR (EPERM, -1);
    enable_interrupt_key();
    if (SUP.use_passive_connection)
	data = s;
    else {
	data = accept (s, (struct sockaddr *)&from, &fromlen);
	close(s);
	if (data < 0) {
	    my_errno = errno;
	    return -1;
	}
    } 
    disable_interrupt_key();
    return data;
}

#define ABORT_TIMEOUT 5
static void
linear_abort (vfs *me, vfs_s_fh *fh)
{
    vfs_s_super *super = FH_SUPER;
    static unsigned char const ipbuf[3] = { IAC, IP, IAC };
    fd_set mask;
    char buf[1024];
    int dsock = FH_SOCK;
    FH_SOCK = -1;
    SUP.control_connection_buzy = 0;

    print_vfs_message (_("ftpfs: aborting transfer."));
    if (send (SUP.sock, ipbuf, sizeof (ipbuf), MSG_OOB) != sizeof (ipbuf)) {
	print_vfs_message (_("ftpfs: abort error: %s"),
			   unix_error_string (errno));
	if (dsock != -1)
	    close (dsock);
	return;
    }

    if (command (me, super, NONE, "%cABOR", DM) != COMPLETE) {
	print_vfs_message (_("ftpfs: abort failed"));
	if (dsock != -1)
	    close (dsock);
	return;
    }
    if (dsock != -1) {
	FD_ZERO (&mask);
	FD_SET (dsock, &mask);
	if (select (dsock + 1, &mask, NULL, NULL, NULL) > 0) {
	    struct timeval start_tim, tim;
	    gettimeofday (&start_tim, NULL);
	    /* flush the remaining data */
	    while (read (dsock, buf, sizeof (buf)) > 0) {
		gettimeofday (&tim, NULL);
		if (tim.tv_sec > start_tim.tv_sec + ABORT_TIMEOUT) {
		    /* server keeps sending, drop the connection and reconnect */
		    reconnect (me, super);
		    return;
		}
	    }
	}
	close (dsock);
    }
    if ((get_reply (me, SUP.sock, NULL, 0) == TRANSIENT) && (code == 426))
	get_reply (me, SUP.sock, NULL, 0);
}

#if 0
static void
resolve_symlink_without_ls_options(vfs *me, vfs_s_super *super, vfs_s_inode *dir)
{
    struct linklist *flist;
    struct direntry *fe, *fel;
    char tmp[MC_MAXPATHLEN];
    int depth;
    
    dir->symlink_status = FTPFS_RESOLVING_SYMLINKS;
    for (flist = dir->file_list->next; flist != dir->file_list; flist = flist->next) {
        /* flist->data->l_stat is alread initialized with 0 */
        fel = flist->data;
        if (S_ISLNK(fel->s.st_mode) && fel->linkname) {
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
	    for ( depth = 0; depth < 100; depth++) { /* depth protects against recursive symbolic links */
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
	                fel->l_stat = g_new (struct stat, 1);
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
resolve_symlink_with_ls_options(vfs *me, vfs_s_super *super, vfs_s_inode *dir)
{
    char  buffer[2048] = "", *filename;
    int sock;
    FILE *fp;
    struct stat s;
    struct linklist *flist;
    struct direntry *fe;
    int switch_method = 0;
    
    dir->symlink_status = FTPFS_RESOLVED_SYMLINKS;
    if (strchr (dir->remote_path, ' ')) {
        if (ftpfs_chdir_internal (bucket, dir->remote_path) != COMPLETE) {
            print_vfs_message(_("ftpfs: CWD failed."));
	    return;
        }
        sock = open_data_connection (bucket, "LIST -lLa", ".", TYPE_ASCII, 0);
    }
    else
        sock = open_data_connection (bucket, "LIST -lLa", 
                                     dir->remote_path, TYPE_ASCII, 0);

    if (sock == -1) {
	print_vfs_message(_("ftpfs: couldn't resolve symlink"));
	return;
    }
    
    fp = fdopen(sock, "r");
    if (fp == NULL) {
	close(sock);
	print_vfs_message(_("ftpfs: couldn't resolve symlink"));
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
vfs_die("This code should be commented out\n");
	    if (vfs_parse_ls_lga (buffer, &s, &filename, NULL)) {
		int r = strcmp(fe->name, filename);
		g_free(filename);
		if (r == 0) {
                    if (S_ISLNK (s.st_mode)) {
                        /* This server doesn't understand LIST -lLa */
                        switch_method = 1;
                        goto done;
                    }
		    fe->l_stat = g_new (struct stat, 1);
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
    get_reply(me, SUP.sock, NULL, 0);
}

static void
resolve_symlink(vfs *me, vfs_s_super *super, vfs_s_inode *dir)
{
    print_vfs_message(_("Resolving symlink..."));

    if (SUP.strict_rfc959_list_cmd) 
	resolve_symlink_without_ls_options(me, super, dir);
    else
        resolve_symlink_with_ls_options(me, super, dir);
}
#endif

static int
dir_load(vfs *me, vfs_s_inode *dir, char *remote_path)
{
    vfs_s_entry *ent;
    vfs_s_super *super = dir->super;
    int sock, num_entries = 0;
#ifdef FIXME_LATER
    int has_symlinks = 0;
#endif
    char buffer[BUF_8K];
    int cd_first;
    
    cd_first = ftpfs_first_cd_then_ls || (strchr (remote_path, ' ') != NULL)
	|| (SUP.strict == RFC_STRICT);

again:
    print_vfs_message(_("ftpfs: Reading FTP directory %s... %s%s"), remote_path,
		      SUP.strict == RFC_STRICT ? _("(strict rfc959)") : "",
		      cd_first ? _("(chdir first)") : "");

    if (cd_first) {
        char *p;

        p = translate_path (me, super, remote_path);

        if (ftpfs_chdir_internal (me, super, p) != COMPLETE) {
	    g_free (p);
            my_errno = ENOENT;
	    print_vfs_message(_("ftpfs: CWD failed."));
	    return -1;
        }
	g_free (p);
    }

    gettimeofday(&dir->u.ftp.timestamp, NULL);
    dir->u.ftp.timestamp.tv_sec += ftpfs_directory_timeout;

    if (SUP.strict == RFC_STRICT) 
        sock = open_data_connection (me, super, "LIST", 0, TYPE_ASCII, 0);
    else if (cd_first)
	/* Dirty hack to avoid autoprepending / to . */
	/* Wu-ftpd produces strange output for '/' if 'LIST -la .' used */
        sock = open_data_connection (me, super, "LIST -la", 0, TYPE_ASCII, 0);
    else {
	/* Trailing "/." is necessary if remote_path is a symlink */
	char *path = concat_dir_and_file (remote_path, ".");
	sock = open_data_connection (me, super, "LIST -la", path, TYPE_ASCII, 0);
	g_free (path);
    }

    if (sock == -1)
	goto fallback;

    /* Clear the interrupt flag */
    enable_interrupt_key ();

#if 1
    {
      /* added 20001006 by gisburn
       * add dots '.' and '..'. This must be _executed_ before scanning the dir as the 
       * code below may jump directly into error handling code (without executing 
       * remaining code). And C doesn't have try {...} finally {}; :-)
       */
      vfs_s_inode *parent = dir->ent->dir;
      
      if( parent==NULL )
        parent = dir;
 
      ent = vfs_s_generate_entry(me, ".", dir, 0);
      ent->ino->st=dir->st;
      num_entries++;
      vfs_s_insert_entry(me, dir, ent);
      
      ent = vfs_s_generate_entry(me, "..", parent, 0);
      ent->ino->st=parent->st;
      num_entries++;
      vfs_s_insert_entry(me, dir, ent);      
    }
#endif

    while (1) {
	int i;
	int res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), sock);
	if (!res)
	    break;

	if (res == EINTR) {
	    me->verrno = ECONNRESET;
	    close (sock);
	    disable_interrupt_key();
	    get_reply(me, SUP.sock, NULL, 0);
	    print_vfs_message (_("%s: failure"), me->name);
	    return -1;
	}

	if (logfile){
	    fputs (buffer, logfile);
            fputs ("\n", logfile);
	    fflush (logfile);
	}

	ent = vfs_s_generate_entry(me, NULL, dir, 0);
	i = ent->ino->st.st_nlink;
	if (!vfs_parse_ls_lga (buffer, &ent->ino->st, &ent->name, &ent->ino->linkname)) {
	    vfs_s_free_entry (me, ent);
	    continue;
	}
	ent->ino->st.st_nlink = i; /* Ouch, we need to preserve our counts :-( */
	num_entries++;
	if ((!strcmp(ent->name, ".")) || (!strcmp (ent->name, ".."))) {
	    g_free (ent->name);
	    ent->name = NULL;	/* Ouch, vfs_s_free_entry "knows" about . and .. being special :-( */
	    vfs_s_free_entry (me, ent);
	    continue;
	}

	vfs_s_insert_entry(me, dir, ent);
    }

    /*    vfs_s_add_dots(me, dir, NULL);
	  FIXME This really should be here; but we need to provide correct parent.
	  Disabled for now, please fix it. 				Pavel
    */
    close(sock);
    me->verrno = E_REMOTE;
    if ((get_reply (me, SUP.sock, NULL, 0) != COMPLETE) || !num_entries)
        goto fallback;

    if (SUP.strict == RFC_AUTODETECT)
	SUP.strict = RFC_DARING;

#ifdef FIXME_LATER
    if (has_symlinks) {
        if (resolve_symlinks)
	    resolve_symlink(me, super, dcache);
        else
           dcache->symlink_status = FTPFS_UNRESOLVED_SYMLINKS;
    }
#endif
    print_vfs_message (_("%s: done."), me->name);
    return 0;

fallback:
    if (SUP.strict == RFC_AUTODETECT) {
        /* It's our first attempt to get a directory listing from this
	server (UNIX style LIST command) */
	SUP.strict = RFC_STRICT;
	/* I hate goto, but recursive call needs another 8K on stack */
	/* return dir_load (me, dir, remote_path); */
	cd_first = 1;
	goto again;
    }
    print_vfs_message(_("ftpfs: failed; nowhere to fallback to"));
    ERRNOR(-1, EACCES);
}

static int
file_store(vfs *me, vfs_s_fh *fh, char *name, char *localname)
{
    int h, sock, n;
    off_t total;
#ifdef HAVE_STRUCT_LINGER
    struct linger li;
#else
    int flag_one = 1;
#endif
    char buffer[8192];
    struct stat s;
    vfs_s_super *super = FH_SUPER;

    h = open(localname, O_RDONLY);
    if (h == -1)
	    ERRNOR (EIO, -1);
    fstat(h, &s);
    sock = open_data_connection(me, super, fh->u.ftp.append ? "APPE" : "STOR", name, TYPE_BINARY, 0);
    if (sock < 0) {
	close(h);
	return -1;
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
	while ((n = read(h, buffer, sizeof(buffer))) < 0) {
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
	print_vfs_message(_("ftpfs: storing file %lu (%lu)"),
			  (unsigned long) total, (unsigned long) s.st_size);
    }
    disable_interrupt_key();
    close(sock);
    close(h);
    if (get_reply (me, SUP.sock, NULL, 0) != COMPLETE)
	    ERRNOR (EIO, -1);
    return 0;
error_return:
    disable_interrupt_key();
    close(sock);
    close(h);
    get_reply(me, SUP.sock, NULL, 0);
    return -1;
}

static int 
linear_start(vfs *me, vfs_s_fh *fh, int offset)
{
    char *name = vfs_s_fullpath (me, fh->ino);

    if (!name)
	return 0;
    FH_SOCK = open_data_connection(me, FH_SUPER, "RETR", name, TYPE_BINARY, offset);
    g_free (name);
    if (FH_SOCK == -1)
	ERRNOR (EACCES, 0);
    fh->linear = LS_LINEAR_OPEN;
    FH_SUPER->u.ftp.control_connection_buzy = 1;
    fh->u.ftp.append = 0;
    return 1;
}

static int
linear_read (vfs *me, vfs_s_fh *fh, void *buf, int len)
{
    int n;
    vfs_s_super *super = FH_SUPER;

    while ((n = read (FH_SOCK, buf, len))<0) {
        if ((errno == EINTR) && !got_interrupt())
	    continue;
	break;
    }

    if (n<0)
	linear_abort(me, fh);

    if (!n) {
	SUP.control_connection_buzy = 0;
	close (FH_SOCK);
	FH_SOCK = -1;
        if ((get_reply (me, SUP.sock, NULL, 0) != COMPLETE))
	    ERRNOR (E_REMOTE, -1);
	return 0;
    }
    ERRNOR (errno, n);
}

static void
linear_close (vfs *me, vfs_s_fh *fh)
{
    if (FH_SOCK != -1)
        linear_abort(me, fh);
}

static int ftpfs_ctl (void *fh, int ctlop, int arg)
{
    switch (ctlop) {
        case MCCTL_IS_NOTREADY:
	    {
	        int v;
		
		if (!FH->linear)
		    vfs_die ("You may not do this");
		if (FH->linear == LS_LINEAR_CLOSED)
		    return 0;

		v = vfs_s_select_on_two (FH->u.ftp.sock, 0);
		if (((v < 0) && (errno == EINTR)) || v == 0)
		    return 1;
		return 0;
	    }
        default:
	    return 0;
    }
}

/* Warning: filename passed to this command is damaged */
static int
send_ftp_command(vfs *me, char *filename, char *cmd, int flags)
{
    char *rpath, *p;
    vfs_s_super *super;
    int r;
    int flush_directory_cache = (flags & OPT_FLUSH);

    if (!(rpath = vfs_s_get_path_mangle(me, filename, &super, 0)))
	return -1;
    p = translate_path (me, super, rpath);
    r = command (me, super, WAIT_REPLY, cmd, p);
    g_free (p);
    vfs_add_noncurrent_stamps (&vfs_ftpfs_ops, (vfsid) super, NULL);
    if (flags & OPT_IGNORE_ERROR)
	r = COMPLETE;
    if (r != COMPLETE)
	    ERRNOR (EPERM, -1);
    if (flush_directory_cache)
	vfs_s_invalidate(me, super);
    return 0;
}

/* This routine is called as the last step in load_setup */
void
ftpfs_init_passwd(void)
{
    ftpfs_anonymous_passwd = load_anon_passwd ();
    if (ftpfs_anonymous_passwd)
	return;

    /* If there is no anonymous ftp password specified
     * then we'll just use anonymous@
     * We don't send any other thing because:
     * - We want to remain anonymous
     * - We want to stop SPAM
     * - We don't want to let ftp sites to discriminate by the user,
     *   host or country.
     */
    ftpfs_anonymous_passwd = g_strdup ("anonymous@");
}

static int ftpfs_chmod (vfs *me, char *path, int mode)
{
    char buf[BUF_SMALL];

    g_snprintf(buf, sizeof(buf), "SITE CHMOD %4.4o /%%s", mode & 07777);
    return send_ftp_command(me, path, buf, OPT_FLUSH);
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
    return send_ftp_command(me, path, "DELE /%s", OPT_FLUSH);
}

/* Return 1 if path is the same directory as the one we are in now */
static int
is_same_dir (vfs *me, vfs_s_super *super, const char *path)
{
    if (!SUP.cwdir)
	return 0;
    if (strcmp (path, SUP.cwdir) == 0)
	return 1;
    return 0;
}

static int
ftpfs_chdir_internal (vfs *me, vfs_s_super *super, char *remote_path)
{
    int r;
    char *p;
    
    if (!SUP.cwd_defered && is_same_dir (me, super, remote_path))
	return COMPLETE;

    p = translate_path (me, super, remote_path);
    r = command (me, super, WAIT_REPLY, "CWD /%s", p);
    g_free (p);

    if (r != COMPLETE) {
	my_errno = EIO;
    } else {
	g_free(SUP.cwdir);
	SUP.cwdir = g_strdup (remote_path);
	SUP.cwd_defered = 0;
    }
    return r;
}

static int ftpfs_rename (vfs *me, char *path1, char *path2)
{
    send_ftp_command(me, path1, "RNFR /%s", OPT_FLUSH);
    return send_ftp_command(me, path2, "RNTO /%s", OPT_FLUSH);
}

static int ftpfs_mkdir (vfs *me, char *path, mode_t mode)
{
    return send_ftp_command(me, path, "MKD /%s", OPT_FLUSH);
}

static int ftpfs_rmdir (vfs *me, char *path)
{
    return send_ftp_command(me, path, "RMD /%s", OPT_FLUSH);
}

static int ftpfs_fh_open (vfs *me, vfs_s_fh *fh, int flags, int mode)
{
    fh->u.ftp.append = 0;
    /* File will be written only, so no need to retrieve it from ftp server */
    if (((flags & O_WRONLY) == O_WRONLY) && !(flags & (O_RDONLY|O_RDWR))){
#ifdef HAVE_STRUCT_LINGER
	struct linger li;
#else
	int li = 1;
#endif
	char * name;

	/* linear_start() called, so data will be written
	 * to local temporary file and stored to ftp server 
	 * by vfs_s_close later
	 */
	if (FH_SUPER->u.ftp.control_connection_buzy){
	    if (!fh->ino->localname){
		int handle = mc_mkstemps (&fh->ino->localname, me->name, NULL);
		if (handle == -1)
		    return -1;
		close (handle);
		fh->u.ftp.append = flags & O_APPEND;
	    }
	    return 0;
	}
	name = vfs_s_fullpath (me, fh->ino);
	if (!name)
	    return -1;
	fh->handle = open_data_connection(me, fh->ino->super,
		    (flags & O_APPEND) ? "APPE" : "STOR", name, TYPE_BINARY, 0);
	g_free (name);

	if (fh->handle < 0)
	    return -1;
#ifdef HAVE_STRUCT_LINGER
	li.l_onoff = 1;
	li.l_linger = 120;
#endif
	setsockopt(fh->handle, SOL_SOCKET, SO_LINGER, &li, sizeof(li));

	if (fh->ino->localname){
	    unlink (fh->ino->localname);
	    g_free (fh->ino->localname);
	    fh->ino->localname = NULL;
	}
	return 0;
    }

    if (!fh->ino->localname)
	if (vfs_s_retrieve_file (me, fh->ino)==-1)
	    return -1;
    if (!fh->ino->localname)
	vfs_die( "retrieve_file failed to fill in localname" );
    return 0;
}

static int ftpfs_fh_close (vfs *me, vfs_s_fh *fh)
{
    if (fh->handle != -1 && !fh->ino->localname){
	close (fh->handle);
	fh->handle = -1;
	/* File is stored to destination already, so
	 * we prevent MEDATA->file_store() call from vfs_s_close ()
	 */
	fh->changed = 0;
	if (get_reply (me, fh->ino->SUP.sock, NULL, 0) != COMPLETE)
	    ERRNOR (EIO, -1);
	vfs_s_invalidate (me, FH_SUPER);
    }
    return 0;
}

static struct vfs_s_data ftp_data = {
    NULL,
    0,
    0,
    NULL, /* logfile */

    NULL, /* init_inode */
    NULL, /* free_inode */
    NULL, /* init_entry */

    NULL, /* archive_check */
    archive_same,
    open_archive,
    free_archive,

    ftpfs_fh_open, /* fh_open */
    ftpfs_fh_close, /* fh_close */

    vfs_s_find_entry_linear,
    dir_load,
    dir_uptodate,
    file_store,

    linear_start,
    linear_read,
    linear_close
};

static void
ftpfs_fill_names (vfs *me, void (*func)(char *))
{
    struct vfs_s_super * super = ftp_data.supers;
    char *name;
    
    while (super){
	name = g_strconcat ("/#ftp:", SUP.user, "@", SUP.host, "/", SUP.cwdir, NULL);
	(*func)(name);
	g_free (name);
	super = super->next;
    }
}

vfs vfs_ftpfs_ops = {
    NULL,	/* This is place of next pointer */
    "ftpfs",
    F_NET,	/* flags */
    "ftp:",	/* prefix */
    &ftp_data,	/* data */
    0,		/* errno */
    NULL,	/* init */
    NULL,	/* done */
    ftpfs_fill_names,
    NULL,

    vfs_s_open,
    vfs_s_close,
    vfs_s_read,
    vfs_s_write,
    
    vfs_s_opendir,
    vfs_s_readdir,
    vfs_s_closedir,
    vfs_s_telldir,
    vfs_s_seekdir,

    vfs_s_stat,
    vfs_s_lstat,
    vfs_s_fstat,

    ftpfs_chmod,
    ftpfs_chown,	/* not really implemented but returns success */
    NULL,

    vfs_s_readlink,
    NULL,
    NULL,
    ftpfs_unlink,

    ftpfs_rename,
    vfs_s_chdir,
    vfs_s_ferrno,
    vfs_s_lseek,
    NULL,
    
    vfs_s_getid,
    vfs_s_nothingisopen,
    vfs_s_free,
    
    NULL,
    NULL,

    ftpfs_mkdir,
    ftpfs_rmdir,
    ftpfs_ctl,
    vfs_s_setctl

MMAPNULL
};

void ftpfs_set_debug (const char *file)
{
    logfile = fopen (file, "w+");
    if (logfile)
	ftp_data.logfile = logfile;
}

static char buffer[BUF_MEDIUM];
static char *netrc, *netrcp;

/* This should match the keywords[] array below */
typedef enum {
    NETRC_NONE = 0,
    NETRC_DEFAULT,
    NETRC_MACHINE,
    NETRC_LOGIN,
    NETRC_PASSWORD,
    NETRC_PASSWD,
    NETRC_ACCOUNT,
    NETRC_MACDEF,
    NETRC_UNKNOWN
} keyword_t;

static keyword_t netrc_next (void)
{
    char *p;
    keyword_t i;
    static const char *const keywords[] = { "default", "machine",
	"login", "password", "passwd", "account", "macdef", NULL
    };


    while (1) {
	netrcp = skip_separators (netrcp);
	if (*netrcp != '\n')
	    break;
	netrcp++;
    }
    if (!*netrcp)
	return NETRC_NONE;
    p = buffer;
    if (*netrcp == '"') {
	for (netrcp++; *netrcp != '"' && *netrcp; netrcp++) {
	    if (*netrcp == '\\')
		netrcp++;
	    *p++ = *netrcp;
	}
    } else {
	for (; *netrcp != '\n' && *netrcp != '\t' && *netrcp != ' ' &&
	     *netrcp != ',' && *netrcp; netrcp++) {
	    if (*netrcp == '\\')
		netrcp++;
	    *p++ = *netrcp;
	}
    }
    *p = 0;
    if (!*buffer)
	return 0;

    i = NETRC_DEFAULT;
    while (keywords[i - 1]) {
	if (!strcmp (keywords[i - 1], buffer))
	    return i;

	i++;
    }

    return NETRC_UNKNOWN;
}

static int netrc_has_incorrect_mode (char *netrcname, char *netrc)
{
    static int be_angry = 1;
    struct stat mystat;

    if (stat (netrcname, &mystat) >= 0 && (mystat.st_mode & 077)) {
	if (be_angry) {
	    message_1s (1, MSG_ERROR,
			_("~/.netrc file has not correct mode.\n"
			  "Remove password or correct mode."));
	    be_angry = 0;
	}
	return 1;
    }
    return 0;
}

/* Scan .netrc until we find matching "machine" or "default"
 * domain is used for additional matching
 * No search is done after "default" in compliance with "man netrc"
 * Return 0 if found, -1 otherwise */
static int find_machine (const char *host, const char *domain)
{
    keyword_t keyword;

    while ((keyword = netrc_next ()) != NETRC_NONE) {
	if (keyword == NETRC_DEFAULT)
	    return 0;

	if (keyword == NETRC_MACDEF) {
	    /* Scan for an empty line, which concludes "macdef" */
	    do {
		while (*netrcp && *netrcp != '\n')
		    netrcp++;
		if (*netrcp != '\n')
		    break;
		netrcp++;
	    } while (*netrcp && *netrcp != '\n');
	    continue;
	}

	if (keyword != NETRC_MACHINE)
	    continue;

	/* Take machine name */
	if (netrc_next () == NETRC_NONE)
	    break;

	if (g_strcasecmp (host, buffer)) {
	    /* Try adding our domain to short names in .netrc */
	    char *host_domain = strchr (host, '.');
	    if (!host_domain)
		continue;

	    /* Compare domain part */
	    if (g_strcasecmp (host_domain, domain))
		continue;

	    /* Compare local part */
	    if (g_strncasecmp (host, buffer, host_domain - host))
		continue;
	}

	return 0;
    }

    /* end of .netrc */
    return -1;
}

/* Extract login and password from .netrc for the host.
 * pass may be NULL.
 * Returns 0 for success, -1 for error */
static int lookup_netrc (const char *host, char **login, char **pass)
{
    char *netrcname;
    char *tmp_pass = NULL;
    char hostname[MAXHOSTNAMELEN], *domain;
    keyword_t keyword;
    static struct rupcache {
	struct rupcache *next;
	char *host;
	char *login;
	char *pass;
    } *rup_cache = NULL, *rupp;

    /* Initialize *login and *pass */
    if (!login)
	return 0;
    *login = NULL;
    if (pass)
	*pass = NULL;

    /* Look up in the cache first */
    for (rupp = rup_cache; rupp != NULL; rupp = rupp->next) {
	if (!strcmp (host, rupp->host)) {
	    if (rupp->login)
		*login = g_strdup (rupp->login);
	    if (pass && rupp->pass)
		*pass = g_strdup (rupp->pass);
	    return 0;
	}
    }

    /* Load current .netrc */
    netrcname = concat_dir_and_file (home_dir, ".netrc");
    netrcp = netrc = load_file (netrcname);
    if (netrc == NULL) {
	g_free (netrcname);
	return 0;
    }

    /* Find our own domain name */
    if (gethostname (hostname, sizeof (hostname)) < 0)
	*hostname = 0;
    if (!(domain = strchr (hostname, '.')))
	domain = "";

    /* Scan for "default" and matching "machine" keywords */
    find_machine (host, domain);

    /* Scan for keywords following "default" and "machine" */
    while (1) {
	int need_break = 0;
	keyword = netrc_next ();

	switch (keyword) {
	case NETRC_LOGIN:
	    if (netrc_next () == NETRC_NONE) {
		need_break = 1;
		break;
	    }

	    /* We have another name already - should not happen */
	    if (*login) {
		need_break = 1;
		break;
	    }

	    /* We have login name now */
	    *login = g_strdup (buffer);
	    break;

	case NETRC_PASSWORD:
	case NETRC_PASSWD:
	    if (netrc_next () == NETRC_NONE) {
		need_break = 1;
		break;
	    }

	    /* Ignore unsafe passwords */
	    if (strcmp (*login, "anonymous") && strcmp (*login, "ftp")
		&& netrc_has_incorrect_mode (netrcname, netrc)) {
		need_break = 1;
		break;
	    }

	    /* Remember password.  pass may be NULL, so use tmp_pass */
	    if (tmp_pass == NULL)
		tmp_pass = g_strdup (buffer);
	    break;

	case NETRC_ACCOUNT:
	    /* "account" is followed by a token which we ignore */
	    if (netrc_next () == NETRC_NONE) {
		need_break = 1;
		break;
	    }

	    /* Ignore account, but warn user anyways */
	    netrc_has_incorrect_mode (netrcname, netrc);
	    break;

	default:
	    /* Unexpected keyword or end of file */
	    need_break = 1;
	    break;
	}

	if (need_break)
	    break;
    }

    g_free (netrc);
    g_free (netrcname);

    rupp = g_new (struct rupcache, 1);
    rupp->host = g_strdup (host);
    rupp->login = rupp->pass = 0;

    if (*login != NULL) {
	rupp->login = g_strdup (*login);
    }
    if (tmp_pass != NULL)
	rupp->pass = g_strdup (tmp_pass);
    rupp->next = rup_cache;
    rup_cache = rupp;

    if (pass)
	*pass = tmp_pass;

    return 0;
}
