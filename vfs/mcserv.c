/* Server for the Midnight Commander Virtual File System.
   
   Copyright (C) 1995, 1996, 1997 The Free Software Foundation

   Written by: 
      Miguel de Icaza, 1995, 1997, 
      Andrej Borsenkow 1996.
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   TODO:
   opendir instead of keeping its table of file handles could return
   the pointer and expect the client to send a proper value back each
   time :-)

   We should use syslog to register login/logout.
   
    */

/* {{{ Includes and global variables */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_LIMITS_H
#    include <limits.h>
#endif
#ifndef NGROUPS_MAX
#    include <sys/param.h>
#    ifdef NGROUPS
#        define NGROUPS_MAX NGROUPS
#    endif
#endif
#ifdef HAVE_GRP_H
#    include <grp.h>
#endif
#ifdef HAVE_SHADOW_H
#    include <shadow.h>
#else
#    ifdef HAVE_SHADOW_SHADOW_H
#        include <shadow/shadow.h>
#    endif
#endif
#ifdef HAVE_CRYPT_H
#    include <crypt.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#ifdef SCO_FLAVOR
#     include <sys/timeb.h>	/* alex: for struct timeb definition */
#endif /* SCO_FLAVOR */
#include <time.h>
#include <utime.h>

/* Network include files */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifdef HAVE_PMAP_SET
#    include <rpc/rpc.h>
#    include <rpc/pmap_prot.h>
#    ifdef HAVE_RPC_PMAP_CLNT_H
#        include <rpc/pmap_clnt.h>
#    endif
#endif

/* Authentication include files */
#include <pwd.h>
#ifdef HAVE_PAM
#    include <security/pam_misc.h>
#    ifndef PAM_ESTABLISH_CRED
#        define PAM_ESTABLISH_CRED PAM_CRED_ESTABLISH
#    endif
#endif

#include "../src/fs.h"
#include "../src/mem.h"
#include "mcfs.h"
#include "tcputil.h"

/* The socket from which we accept commands */
int msock;

/* Requested version number from client */
static int clnt_version;

/* If non zero, we accept further commands */
int logged_in = 0;

/* Home directory */
char *home_dir = NULL;

char *up_dir = NULL;

/* Were we started from inetd? */
int  inetd_started = 0;

/* Are we running as a daemon? */
int  isDaemon = 0;

/* guess */
int verbose = 0;

/* ftp auth */
int ftp = 0;

/* port number in which we listen to connections,
 * if zero, we try to contact the portmapper to get a port, and
 * if it's not possible, then we use a hardcoded value
 */
int portnum = 0;

/* if the server will use rcmd based authentication (hosts.equiv .rhosts) */
int r_auth = 0;

#define OPENDIR_HANDLES 8

#define DO_QUIT_VOID() \
do { \
  quit_server = 1; \
  return_code = 1; \
  return; \
} while (0)

/* Only used by get_port_number */
#define DO_QUIT_NONVOID(a) \
do { \
  quit_server = 1; \
  return_code = 1; \
  return (a); \
} while (0)

char buffer [4096];
int debug = 1;
static int quit_server;
static int return_code;

/* }}} */

/* {{{ Misc routines */

void send_status (int status, int errno_number)
{
    rpc_send (msock, RPC_INT, status, RPC_INT, errno_number, RPC_END);
    errno = 0;
}

/* }}} */

/* {{{ File with handle operations */

void do_open (void)
{
    int handle, flags, mode;
    char *arg;

    rpc_get (msock, RPC_STRING, &arg, RPC_INT, &flags, RPC_INT, &mode,RPC_END);
    
    handle = open (arg, flags, mode);
    send_status (handle, errno);
    free (arg);
}

void do_read (void)
{
    int handle, count, n;
    void *data;
   
    rpc_get (msock, RPC_INT, &handle, RPC_INT, &count, RPC_END);
    data = malloc (count);
    if (!data){
	send_status (-1, ENOMEM);
	return;
    }
    if (verbose) printf ("count=%d\n", count);
    n = read (handle, data, count);
    if (verbose) printf ("result=%d\n", n);
    if (n < 0){
	send_status (-1, errno);
	return;
    }
    send_status (n, 0);
    rpc_send (msock, RPC_BLOCK, n, data, RPC_END);
    
    free (data);
}

void do_write (void)
{
    int handle, count, status;
    char buffer [8192];
    
    rpc_get (msock, RPC_INT, &handle, RPC_INT, &count, RPC_END);
    status = 0;
    while (count){
	int nbytes = count > 8192 ? 8192 : count;
	
	rpc_get (msock, RPC_BLOCK, nbytes, buffer, RPC_END);
	status = write (handle, buffer, nbytes);
	count -= nbytes;
    }
    send_status (status, errno);
}

void do_lseek (void)
{
    int handle, offset, whence, status;

    rpc_get (msock,
	     RPC_INT, &handle,
	     RPC_INT, &offset,
	     RPC_INT, &whence, RPC_END);
    status = lseek (handle, offset, whence);
    send_status (status, errno);
}

void do_close (void)
{
    int handle, status;
    
    rpc_get (msock, RPC_INT, &handle, RPC_END);
    status = close (handle);
    send_status (status, errno);
}

/* }}} */

/* {{{ Stat family routines */

void send_time (int sock, time_t time)
{
    if (clnt_version == 1) {
	char   *ct;
	int    month;
    
	ct = ctime (&time);
	ct [3] = ct [10] = ct [13] = ct [16] = ct [19] = 0;
	
	/* Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec */
	if (ct [4] == 'J'){
	    if (ct [5] == 'a'){
		month = 0;
	    } else
		month = (ct [6] == 'n') ? 5 : 6;
	} else if (ct [4] == 'F'){
	    month = 1;
	} else if (ct [4] == 'M'){
	    month = (ct [6] == 'r') ? 2 : 5;
	} else if (ct [4] == 'A'){
	    month = (ct [5] == 'p') ? 3 : 7;
	} else if (ct [4] == 'S'){
	    month = 8;
	} else if (ct [4] == 'O'){
	    month = 9;
	} else if (ct [4] == 'N'){
	    month = 10;
	} else
	month = 11;
	rpc_send (msock,
		  RPC_INT, atoi (&ct [17]),  	/* sec */
		  RPC_INT, atoi (&ct [14]),         /* min */
		  RPC_INT, atoi (&ct [11]),         /* hour */
		  RPC_INT, atoi (&ct [8]),          /* mday */
		  RPC_INT, atoi (&ct [20]),         /* year */
		  RPC_INT, month,	                /* month */
		  RPC_END);
    } else {
	long ltime = (long) time;
	char buf[2*sizeof(long) + 1];

	sprintf (buf, "%lx", ltime);
	rpc_send (msock,
		  RPC_STRING, buf,
		  RPC_END);
    }
}

void send_stat_info (struct stat *st)
{
    int mylong;
    int blocks =
#ifdef HAVE_ST_BLOCKS
	st->st_blocks;
#else
        st->st_size / 1024;
#endif
    
    mylong = st->st_dev;
    rpc_send (msock, RPC_INT, (long) st->st_dev, 
	      RPC_INT, (long) st->st_ino,
	      RPC_INT, (long) st->st_mode,
	      RPC_INT, (long) st->st_nlink,
	      RPC_INT, (long) st->st_uid,
	      RPC_INT, (long) st->st_gid,
	      RPC_INT, (long) st->st_size,
	      RPC_INT, (long) blocks, RPC_END);
    send_time (msock, st->st_atime);
    send_time (msock, st->st_mtime);
    send_time (msock, st->st_ctime);
}

void do_lstat ()
{
    struct stat st;
    char   *file;
    int    n;

    rpc_get (msock, RPC_STRING, &file, RPC_END);
    n = lstat (file, &st);
    send_status (n, errno);
    if (n >= 0)
	send_stat_info (&st);
    free (file);
}

void do_fstat (void)
{
    int handle;
    int n;
    struct stat st;
   
    rpc_get (msock, RPC_INT, &handle, RPC_END);
    n = fstat (handle, &st);
    send_status (n, errno);
    if (n < 0)
	return;

    send_stat_info (&st);
}

void do_stat ()
{
    struct stat st;
    int    n;
    char   *file;

    rpc_get (msock, RPC_STRING, &file, RPC_END);

    n = stat (file, &st);
    send_status (n, errno);
    if (n >= 0)
	send_stat_info (&st);
    free (file);
}

/* }}} */

/* {{{ Directory lookup operations */

static struct {
    int used;
    DIR *dirs [OPENDIR_HANDLES];
    char *names [OPENDIR_HANDLES];
} mcfs_DIR;

void close_handle (int handle)
{
    if (mcfs_DIR.used > 0) mcfs_DIR.used--;
    if (mcfs_DIR.dirs [handle])
	closedir (mcfs_DIR.dirs [handle]);
    if (mcfs_DIR.names [handle])
	free (mcfs_DIR.names [handle]);
    mcfs_DIR.dirs [handle] = 0;
    mcfs_DIR.names [handle] = 0;
}

void do_opendir (void)
{
    int handle, i;
    char *arg;
    DIR *p;

    rpc_get (msock, RPC_STRING, &arg, RPC_END);

    if (mcfs_DIR.used == OPENDIR_HANDLES){
	send_status (-1, ENFILE);	/* Error */
	free (arg);
	return;
    }
    
    handle = -1;
    for (i = 0; i < OPENDIR_HANDLES; i++){
	if (mcfs_DIR.dirs [i] == 0){
	    handle = i;
	    break;
	}
    }

    if (handle == -1){
	send_status (-1, EMFILE);
	free (arg);
	if (!inetd_started)
	    fprintf (stderr, "OOPS! you have found a bug in mc - do_opendir()!\n");
	return;
    }

    if (verbose) printf ("handle=%d\n", handle);
    p = opendir (arg);
    if (p){
	mcfs_DIR.dirs [handle] = p;
	mcfs_DIR.names [handle] = arg;
	    mcfs_DIR.used ++;

	/* Because 0 is an error value */
	rpc_send (msock, RPC_INT, handle+1, RPC_INT, 0, RPC_END);

    } else {
	send_status (-1, errno);
	free (arg);
    }
}

/* Sends the complete directory listing, as well as the stat information */
void do_readdir (void)
{
    struct dirent *dirent;
    struct stat st;
    int    handle, n, dnamelen;
    char   *fname = 0;

    rpc_get (msock, RPC_INT, &handle, RPC_END);
    
    if (!handle){
	rpc_send (msock, RPC_INT, 0, RPC_END);
	return;
    }

    /* We incremented it in opendir */
    handle --;
    dnamelen = strlen (mcfs_DIR.names [handle]);

    while ((dirent = readdir (mcfs_DIR.dirs [handle]))){
	int length = NLENGTH (dirent);

	rpc_send (msock, RPC_INT, length, RPC_END);
	rpc_send (msock, RPC_BLOCK, length, dirent->d_name, RPC_END);
	fname = malloc (dnamelen + length + 2);
	strcat (strcat (strcpy (fname, mcfs_DIR.names [handle]), "/"), dirent->d_name);
	n = lstat (fname, &st);
	send_status (n, errno);
	free (fname);
	if (n >= 0)
	    send_stat_info (&st);
    }
    rpc_send (msock, RPC_INT, 0, RPC_END);
}

void do_closedir (void)
{
    int handle;

    rpc_get (msock, RPC_INT, &handle, RPC_END);
    close_handle (handle-1);
}

/* }}} */

/* {{{ Operations with one and two file name argument */

void do_chdir (void)
{
    char *file;
    int  status;

    rpc_get (msock, RPC_STRING, &file, RPC_END);
    
    status = chdir (file);
    send_status (status, errno);
    free (file);
}

void do_rmdir (void)
{
    char *file;
    int  status;

    rpc_get (msock, RPC_STRING, &file, RPC_END);
    
    status = rmdir (file);
    send_status (status, errno);
    free (file);
}

void do_mkdir (void)
{
    char *file;
    int  mode, status;

    rpc_get (msock, RPC_STRING, &file, RPC_INT, &mode, RPC_END);
    
    status = mkdir (file, mode);
    send_status (status, errno);
    free (file);
}

void do_mknod (void)
{
    char *file;
    int  mode, dev, status;

    rpc_get (msock, RPC_STRING, &file, RPC_INT, &mode, RPC_INT, &dev, RPC_END);
    
    status = mknod (file, mode, dev);
    send_status (status, errno);
    free (file);
}

void do_readlink (void)
{
    char buffer [2048];
    char *file;
    int  n;

    rpc_get (msock, RPC_STRING, &file, RPC_END);
    n = readlink (file, buffer, 2048);
    send_status (n, errno);
    if (n >= 0) {
        buffer [n] = 0;
	rpc_send (msock, RPC_STRING, buffer, RPC_END);
    }
    free (file);
}

void do_unlink (void)
{
    char *file;
    int  status;
    
    rpc_get (msock, RPC_STRING, &file, RPC_END);
    status = unlink (file);
    send_status (status, errno);
    free (file);
}

void do_rename (void)
{
    char *f1, *f2;
    int  status;
    
    rpc_get (msock, RPC_STRING, &f1, RPC_STRING, &f2, RPC_END);
    status = rename (f1, f2);
    send_status (status, errno);
    free (f1); free (f2);
}

void do_symlink (void)
{
    char *f1, *f2;
    int  status;
    
    rpc_get (msock, RPC_STRING, &f1, RPC_STRING, &f2, RPC_END);
    status = symlink (f1, f2);
    send_status (status, errno);
    free (f1); free (f2);
}

void do_link (void)
{
    char *f1, *f2;
    int  status;
    
    rpc_get (msock, RPC_STRING, &f1, RPC_STRING, &f2, RPC_END);
    status = link (f1, f2);
    send_status (link (f1, f2), errno);
    free (f1); free (f2);
}


/* }}} */

/* {{{ Misc commands */

void do_gethome (void)
{
    rpc_send (msock, RPC_STRING, (home_dir) ? home_dir : "/", RPC_END);
}

void do_getupdir (void)
{
    rpc_send (msock, RPC_STRING, (up_dir) ? up_dir : "/", RPC_END);
}

void do_chmod (void)
{
    char *file;
    int  mode, status;
    
    rpc_get (msock, RPC_STRING, &file, RPC_INT, &mode, RPC_END);
    status = chmod (file, mode);
    send_status (status, errno);
    free (file);
}

void do_chown (void)
{
    char *file;
    int  owner, group, status;
    
    rpc_get (msock, RPC_STRING, &file,RPC_INT, &owner, RPC_INT,&group,RPC_END);
    status = chown (file, owner, group);
    send_status (status, errno);
    free (file);
}

void do_utime (void)
{
    char *file;
    int  status;
    long atime;
    long mtime;
    char *as;
    char *ms;
    struct utimbuf times;
    
    rpc_get (msock, RPC_STRING, &file,
		    RPC_STRING, &as,
		    RPC_STRING, &ms,
		    RPC_END);
    sscanf (as, "%lx", &atime);
    sscanf (ms, "%lx", &mtime);
    if (verbose) printf ("Got a = %s, m = %s, comp a = %ld, m = %ld\n",
			  as, ms, atime, mtime);
    free (as);
    free (ms);
    times.actime  = (time_t) atime;
    times.modtime = (time_t) mtime;
    status = utime (file, &times);
    send_status (status, errno);
    free (file);
}

void do_quit ()
{
    quit_server = 1;
}

#ifdef HAVE_PAM

struct user_pass {
    char *username;
    char *password;
};

int
mc_pam_conversation (int messages, const struct pam_message **msg,
		     struct pam_response **resp, void *appdata_ptr)
{
    struct pam_response *r;
    struct user_pass *up = appdata_ptr;
    int status;

    r = (struct pam_response *) malloc (sizeof (struct pam_response) * messages);
    if (!r)
	return PAM_CONV_ERR;
    *resp = r;
    
    for (status = PAM_SUCCESS; messages--; msg++, r++){
	switch ((*msg)->msg_style){

	case PAM_PROMPT_ECHO_ON: 
	    r->resp = strdup (up->username);
	    r->resp_retcode  = PAM_SUCCESS;
	    break;

	case PAM_PROMPT_ECHO_OFF:
	    r->resp = strdup (up->password);
	    r->resp_retcode = PAM_SUCCESS;
	    break;

	case PAM_ERROR_MSG:
	    r->resp = NULL;
	    r->resp_retcode = PAM_SUCCESS;
	    break;

	case PAM_TEXT_INFO:
	    r->resp = NULL;
	    r->resp_retcode = PAM_SUCCESS;
	    break;
	}
    }
    return status;
}

static struct pam_conv conv = { &mc_pam_conversation, NULL };
			     

/* Return 0 if authentication failed, 1 otherwise */
int
mc_pam_auth (char *username, char *password)
{
    pam_handle_t *pamh;
    struct user_pass up;
    int status;
    
    up.username = username;
    up.password = password;
    conv.appdata_ptr = &up;
    
    if ((status = pam_start("mcserv", username, &conv, &pamh)) != PAM_SUCCESS)
	goto failed_pam;
    if ((status = pam_authenticate (pamh, 0)) != PAM_SUCCESS)
	goto failed_pam;
    if ((status = pam_acct_mgmt (pamh, 0)) != PAM_SUCCESS)
	goto failed_pam;
    if ((status = pam_setcred (pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
	goto failed_pam;
    pam_end (pamh, status);
    return 0;
    
failed_pam:
    pam_end (pamh, status);
    return 1;
}

#else /* Code for non-PAM authentication */
    
/* Keep reading until we find a \n */
static int next_line (int socket)
{
    char c;

    while (1){
	if (read (socket, &c, 1) <= 0)
	    return 0;
	if (c == '\n')
	    return 1;
    }
}

static int ftp_answer (int sock, char *text)
{
    char answer [4];

    next_line (sock);
    socket_read_block (sock, answer, 3);
    answer [3] = 0;
    if (strcmp (answer, text) == 0)
	return 1;
    return 0;
}

int do_ftp_auth (char *username, char *password)
{
    struct   sockaddr_in local_address;
    unsigned long inaddr;
    int      my_socket;
    char     answer [4];

    bzero ((char *) &local_address, sizeof (local_address));
    
    local_address.sin_family = AF_INET;
    /* FIXME: extract the ftp port with the proper function */
    local_address.sin_port   = htons (21);

    /*  Convert localhost to usable format */
    if ((inaddr = inet_addr ("127.0.0.1")) != -1)
	bcopy ((char *) &inaddr, (char *) &local_address.sin_addr,
	       sizeof (inaddr));
    
    if ((my_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0){
	if (!isDaemon) fprintf (stderr, "do_auth: can't create socket\n");
	return 0;
    }
    if (connect (my_socket, (struct sockaddr *) &local_address,
	     sizeof (local_address)) < 0){
	fprintf (stderr,
		 "do_auth: can't connect to ftp daemon for authentication\n");
	close (my_socket);
	return 0;
    }
    send_string (my_socket, "user ");
    send_string (my_socket, username);
    send_string (my_socket, "\r\n");
    if (!ftp_answer (my_socket, "331")){
	send_string (my_socket, "quit\r\n");
	close (my_socket);
	return 0;
    }
    next_line (my_socket);	/* Eat all the line */
    send_string (my_socket, "pass ");
    send_string (my_socket, password);
    send_string (my_socket, "\r\n");
    socket_read_block (my_socket, answer, 3);
    answer [3] = 0;
    send_string (my_socket, "\r\n");
    send_string (my_socket, "quit\r\n");
    close (my_socket);
    if (strcmp (answer, "230") == 0)
	return 1;
    return 0;
}

int do_classic_auth (char *username, char *password)
{
    struct passwd *this;
    int ret;
#ifdef LINUX_SHADOW
    struct spwd *spw;
    extern char *pw_encrypt (char *, char *);
#else
#ifdef NEED_CRYPT_PROTOTYPE
    extern char *crypt (const char *, const char *);
#endif
#endif
    
    if ((this = getpwnam (username)) == 0)
	return 0;

#ifdef LINUX_SHADOW
    if ((spw = getspnam (username)) == NULL)
        this->pw_passwd = "*";
    else
        this->pw_passwd = spw->sp_pwdp;
    if (strcmp (pw_encrypt (password, this->pw_passwd), this->pw_passwd) == 0)
        ret = 1;
    else
        ret = 0;
#else	
#ifdef HAVE_CRYPT    
    if (strcmp (crypt (password, this->pw_passwd), this->pw_passwd) == 0){
	ret = 1;
    } else
#endif
    {
	ret = 0;
    }
#endif
    endpwent ();
    return ret;
}
#endif /* non-PAM authentication */

/* Try to authenticate the user based on:
   - PAM if the system has it, else it checks:
   - pwdauth if the system supports it.
   - conventional auth (check salt on /etc/passwd, crypt, and compare
   - try to contact the local ftp server and login (if -f flag used)
*/
int
do_auth (char *username, char *password)
{
    int auth = 0;
    struct passwd *this;
    
    if (strcmp (username, "anonymous") == 0)
	username = "ftp";

#ifdef HAVE_PAM
    if (mc_pam_auth (username, password) == 0)
	auth = 1;
#else /* if there is no pam */
#ifdef HAVE_PWDAUTH
    if (pwdauth (username, password) == 0)
	auth = 1;
    else
#endif
    if (do_classic_auth (username, password))
	auth = 1;
    else if (ftp)
	auth = do_ftp_auth (username, password);
#endif /* not pam */
    
    if (!auth)
        return 0;
	
    this = getpwnam (username);
    if (this == 0)
	return 0;

    if (chdir (this->pw_dir) == -1)
        return 0;
    
    if (this->pw_dir [strlen (this->pw_dir) - 1] == '/')
        home_dir = strdup (this->pw_dir);
    else {
        home_dir = malloc (strlen (this->pw_dir) + 2);
        if (home_dir) {
            strcpy (home_dir, this->pw_dir);
            strcat (home_dir, "/");
        } else
            home_dir = "/";
    }
    	
    
    if (setgid (this->pw_gid) == -1)
        return 0;
    
#ifdef HAVE_INITGROUPS
#ifdef NGROUPS_MAX
    if (NGROUPS_MAX > 1 && initgroups (this->pw_name, this->pw_gid))
        return 0;
#endif
#endif    

#ifndef BSD
    if (setuid (this->pw_uid))
#else
    if (setreuid (this->pw_uid, this->pw_uid))
#endif
        return 0;

    /* If the setuid call failed, then deny access */
    /* This should fix the problem on those machines with strange setups */
    if (getuid () != this->pw_uid)
	return 0;
    
    if (strcmp (username, "ftp") == 0)
	chroot (this->pw_dir);

    endpwent ();
    return auth;
}

#if 0
int do_rauth (int socket)
{
    struct sockaddr_in from;
    struct hostent *hp;
    
    if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0)
	return 0;
    from.sin_port = ntohs ((unsigned short) from.sin_port);

    /* Strange, this should not happend */
    if (from.sin_family != AF_INET)
	return 0;

    hp = gethostbyaddr((char *)&fromp.sin_addr, sizeof (struct in_addr),
		       fromp.sin_family);
    
}
#endif

int do_rauth (int msock)
{
    return 0;
}

void login_reply (int logged_in)
{
    rpc_send (msock, RPC_INT,
	      logged_in ? MC_LOGINOK : MC_INVALID_PASS,
	      RPC_END);
}

/* FIXME: Implement the anonymous login */
void do_login ()
{
    char *username;
    char *password;
    int  result;
    
    rpc_get (msock, RPC_LIMITED_STRING, &up_dir, RPC_LIMITED_STRING, &username, RPC_END);
    if (verbose) printf ("username: %s\n", username);
    
    if (r_auth){
	logged_in = do_rauth (msock);
	if (logged_in){
	    login_reply (logged_in);
	    return;
	}
    }
    rpc_send (msock, RPC_INT, MC_NEED_PASSWORD, RPC_END);
    rpc_get (msock, RPC_INT, &result, RPC_END);
    if (result == MC_QUIT)
	DO_QUIT_VOID ();
    if (result != MC_PASS){
	if (verbose) printf ("do_login: Unknown response: %d\n", result);
	DO_QUIT_VOID ();
    }
    rpc_get (msock, RPC_LIMITED_STRING, &password, RPC_END);
    logged_in = do_auth (username, password);
    endpwent ();
    login_reply (logged_in);
}

/* }}} */

/* {{{ Server and dispatching functions */

/* This structure must be kept in synch with mcfs.h enums */

struct _command {
    char *command;
    void (*callback)(void);
} commands [] = {
    { "open",       do_open },
    { "close",      do_close },
    { "read",       do_read },
    { "write",      do_write },
    { "opendir",    do_opendir }, 
    { "readdir",    do_readdir },
    { "closedir",   do_closedir },
    { "stat ",      do_stat },
    { "lstat ",     do_lstat },
    { "fstat",      do_fstat },
    { "chmod",      do_chmod },
    { "chown",      do_chown },
    { "readlink ",  do_readlink },
    { "unlink",     do_unlink },
    { "rename",     do_rename },
    { "chdir ",     do_chdir },
    { "lseek",      do_lseek },
    { "rmdir",      do_rmdir },
    { "symlink",    do_symlink },
    { "mknod",      do_mknod },
    { "mkdir",      do_mkdir },
    { "link",       do_link },
    { "gethome",    do_gethome },
    { "getupdir",   do_getupdir },
    { "login",      do_login },
    { "quit",       do_quit },
    { "utime",      do_utime },
};

static int ncommands = sizeof(commands)/sizeof(struct _command);

void exec_command (int command)
{
    if (command < 0 ||
	command >= ncommands ||
	commands [command].command == 0){
	fprintf (stderr, "Got unknown command: %d\n", command);
	DO_QUIT_VOID ();
    }
    if (verbose) printf ("Command: %s\n", commands [command].command);
    (*commands [command].callback)();
}

void check_version ()
{
    int version;
    
    rpc_get (msock, RPC_INT, &version, RPC_END);
    if (version >= 1 &&
	version <= RPC_PROGVER)
	rpc_send (msock, RPC_INT, MC_VERSION_OK, RPC_END);
    else
	rpc_send (msock, RPC_INT, MC_VERSION_MISMATCH, RPC_END);
    
    clnt_version = version;
}

/* This routine is called by rpc_get/rpc_send when the connection is closed */
void tcp_invalidate_socket (int sock)
{
    if (verbose) printf ("Connection closed\n");
    DO_QUIT_VOID();
}

void server (int sock)
{
    int command;
    
    msock = sock;
    quit_server = 0;

    check_version ();
    do {
	if (rpc_get (sock, RPC_INT, &command, RPC_END) &&
	    (logged_in || command == MC_LOGIN))
	    exec_command (command);
    } while (!quit_server);
}

/* }}} */

/* {{{ Net support code */

char *get_client (int portnum)
{
    int sock, clilen, newsocket;
    struct sockaddr_in client_address, server_address;
    struct hostent *hp;
    char hostname [255];
    int yes = 1;
    
    if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	return "Can't create socket";

    /* Use this to debug: */
    if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof (yes)) < 0)
	return "setsockopt failed";

    gethostname (hostname, 255);
    if (verbose) printf ("hostname=%s\n", hostname);
    hp = gethostbyname (hostname);
    if (hp == 0)
	return "hp = 0!";
    
    bzero ((char *) &server_address, sizeof (server_address));
    server_address.sin_family = hp->h_addrtype;
    server_address.sin_addr.s_addr = htonl (INADDR_ANY);
    server_address.sin_port = htons (portnum);

    if (bind (sock, (struct sockaddr *) &server_address,
	      sizeof (server_address)) < 0)
	return "Can't bind";

    listen (sock, 5);

    for (;;){
	int child;
	
	clilen = sizeof (client_address);
	newsocket = accept (sock, (struct sockaddr *) &client_address,
			    &clilen);

	if (isDaemon && (child = fork())) {
	    int status;
	    
	    close (newsocket);
	    waitpid (child, &status, 0);
	    continue;
	}

	if (isDaemon && fork()) exit (0);

	server (newsocket);
	close (newsocket);
	return 0;
    }
}

#ifdef HAVE_PMAP_SET
void signal_int_handler (int sig)
{
    pmap_unset (RPC_PROGNUM, RPC_PROGVER);
}
#endif

#ifndef IPPORT_RESERVED
#define IPPORT_RESERVED 1024;
#endif

int get_port_number ()
{
    int port = 0;

#ifdef HAVE_RRESVPORT
    int start_port = IPPORT_RESERVED;
    
    port = rresvport (&start_port);
    if (port == -1){
	if (geteuid () == 0){
	    fprintf (stderr, "Could not bind the server on a reserved port\n");
	    DO_QUIT_NONVOID (-1);
	}
	port = 0;
    }
#endif
    if (port)
	return port;
    
    port = mcserver_port;

    return port;
}

void register_port (int portnum, int abort_if_fail)
{
#ifdef HAVE_PMAP_SET
    /* Register our service with the portmapper */
    /* protocol: pmap_set (prognum, versnum, protocol, portp) */
    
    if (pmap_set (RPC_PROGNUM, RPC_PROGVER, IPPROTO_TCP, portnum))
	signal (SIGINT, signal_int_handler);
    else {
	fprintf (stderr, "Could not register service with portmapper\n");
	if (abort_if_fail)
	    exit (1);
    }
#else
    if (abort_if_fail){
	fprintf (stderr,
		 "This system lacks port registration, try using the -p\n"
		 "flag to force installation at a given port");
    }
#endif
}

/* }}} */

int main (int argc, char *argv [])
{
    char *result;
    extern char *optarg;
    int c;

    while ((c = getopt (argc, argv, "fdiqp:v")) != -1){
	switch (c){
	case 'd':
	    isDaemon = 1;
	    verbose = 0;
	    break;

	case 'v':
	    verbose = 1;
	    break;
	    
	case 'f':
	    ftp = 1;
	    break;

	case 'q':
	    verbose = 0;
	    break;

	case 'p':
	    portnum = atoi (optarg);
	    break;

	case 'i':
	    inetd_started = 1;
	    break;

	case 'r':
	    r_auth = 1;
	    break;

	default:
	    fprintf (stderr, "Usage is: mcserv [options] [-p portnum]\n\n"
		    "options are:\n"
		    "-d  become a daemon (sets -q)\n"
		    "-q  quiet mode\n"
		/*    "-r  use rhost based authentication\n" */
#ifndef HAVE_PAM
		    "-f  force ftp authentication\n"
#endif
		    "-v  verbose mode\n"
		    "-p  to specify a port number to listen\n");
	    exit (0);
	    
	}
    }

    if (isDaemon && fork()) exit (0);

    if (portnum == 0)
	portnum = get_port_number ();

    if (portnum != -1) {
	register_port (portnum, 0);
	if (verbose)
	    printf ("Using port %d\n", portnum);
	if ((result = get_client (portnum)))
	    perror (result);
#ifdef HAVE_PMAP_SET
	if (!isDaemon)
	    pmap_unset (RPC_PROGNUM, RPC_PROGVER);
#endif
    }
    exit (return_code);
}

/* This functions are not used */
void message (int is_error, char *text, char *msg)
{
    printf ("%s %s\n", text, msg);
}

char *unix_error_string (int a)
{
    return "none";
}

#ifndef HAVE_MAD
void *do_xmalloc (int size)
{
    void *m = malloc (size);

    if (!m){
	fprintf (stderr, "memory exhausted\n");
	exit (1);
    }
    return m;
}
#endif

#ifndef HAVE_STRDUP
char *strdup (char *s)
{
    char *t = malloc (strlen (s)+1);
    strcpy (t, s);
    return t;
}
#endif

void vfs_die( char *m )
{
    fprintf (stderr,m);
    exit (1);
}
