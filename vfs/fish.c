/* Virtual File System: FISH implementation for transfering files over
   shell connections.

   Copyright (C) 1998 The Free Software Foundation
   
   Written by: 1998 Pavel Machek

   Derived from ftpfs.c.
   
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

/*
 * Read README.fish for protocol specification.
 *
 * Syntax of path is: /#sh:user@host[:Cr]/path
 *	where C means you want compressed connection,
 *	and r means you want to use rsh
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
#include <grp.h>
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
#ifndef SCO_FLAVOR
#	include <sys/time.h>	/* alex: this redefines struct timeval */
#endif /* SCO_FLAVOR */
#include <sys/param.h>

#include "../src/mem.h"
#define WANT_PARSE_LS_LGA
#include "vfs.h"
#include "tcputil.h"
#include "../src/util.h"
#include "../src/dialog.h"
#include "container.h"
#include "fish.h"
#ifndef MAXHOSTNAMELEN
#    define MAXHOSTNAMELEN 64
#endif

/*
 * Reply codes.
 */
#define PRELIM		1	/* positive preliminary */
#define COMPLETE	2	/* positive completion */
#define CONTINUE	3	/* positive intermediate */
#define TRANSIENT	4	/* transient negative completion */
#define ERROR		5	/* permanent negative completion */

#define UPLOAD_ZERO_LENGTH_FILE

static int my_errno;
static int code;

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

static char *fish_get_current_directory(struct connection *bucket);
static void free_bucket (void *data);
static void connection_destructor(void *data);
static void flush_all_directory(struct connection *bucket);
static int get_line (int sock, char *buf, int buf_len, char term);
static char *get_path (struct connection **bucket, char *path);

static char *my_get_host_and_username (char *path, char **host, char **user, int *flags, char **pass)
{
    return get_host_and_username (path, host, user, flags, 0, 0, pass);
}

static int decode_reply (char *s, int was_garbage)
{
    if (!sscanf(s, "%d", &code)) {
	code = 500;
	return 5;
    }
    if (code<100) return was_garbage ? ERROR : (!code ? COMPLETE : PRELIM);
    return code / 100;
}

/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */
static int get_reply (int sock, char *string_buf, int string_len)
{
    char answer[1024];
    int was_garbage = 0;
    
    for (;;) {
        if (!get_line(sock, answer, sizeof(answer), '\n')) {
	    if (string_buf)
		*string_buf = 0;
	    code = 421;
	    return 4;
	}
	if (strncmp(answer, "### ", 4)) {
	    was_garbage = 1;
	    if (string_buf) {
	        strncpy(string_buf, answer, string_len - 1);
		*(string_buf + string_len - 1) = 0;
	    }
	} else return decode_reply(answer+4, was_garbage);
    }
}

int got_sigpipe = 0;

static int command (struct connection *bucket, int wait_reply,
		    char *fmt, ...)
{
    va_list ap;
    char buf[2048]; /* FIXME: buffer exceed ?? */
    int n, status;
    
    va_start (ap, fmt);
    vsprintf (buf, fmt, ap);
    va_end (ap);
    n = strlen(buf);
    buf[n] = 0;

    if (logfile){
        fwrite (buf, strlen (buf), 1, logfile);
	fflush (logfile);
    }

    enable_interrupt_key();
    status = write(qsockw(bucket), buf, strlen(buf));
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
	return get_reply (qsockr(bucket), (wait_reply & WANT_STRING) ? reply_str : NULL, sizeof (reply_str)-1);
    return COMPLETE;
}

static void
connection_close (void *data)
{
    struct connection *bucket = data;

    if ((qsockw (bucket) != -1) || (qsockr (bucket) != -1)){
	print_vfs_message ("fish: Disconnecting from %s", qhost(bucket));
	command(bucket, NONE, "logout\n");
	close(qsockw(bucket));
	close(qsockr(bucket));
    }
}

static void
pipeopen(struct connection *bucket, char *path, char *argv[])
{
    int fileset1[2], fileset2[2];
    FILE *retval;
    int res;

    if (pipe(fileset1)<0) vfs_die("Could not pipe(): %m.");
    if (pipe(fileset2)<0) vfs_die("Could not pipe(): %m.");
    
    if ((res = fork())) {
        if (res<0) vfs_die("Could not fork(): %m.");
	/* We are the parent */
	close(fileset1[0]);
	qsockw(bucket) = fileset1[1];
	close(fileset2[1]);
	qsockr(bucket) = fileset2[0];
	if (!retval) vfs_die( "Could not fdopen(): %m." );
    } else {
        close(0);
	dup(fileset1[0]);
	close(fileset1[0]); close(fileset1[1]);
	close(1); close(2);
	dup(fileset2[1]);
	dup(fileset2[1]);
	close(fileset2[0]); close(fileset2[1]);
	execvp(path, argv);
	vfs_die("Exec failed.");
    }
}


static struct connection *
open_command_connection (char *host, char *user, int flags, char *netrcpass)
{
    struct connection *bucket;
    char *argv[100];
    int i = 0;

    bucket = xmalloc(sizeof(struct connection), 
		     "struct connection");
    
    if (bucket == NULL) ERRNOR (ENOMEM, NULL);

    qhost(bucket) = strdup (host);
    quser(bucket) = strdup (user);
    qcdir(bucket) = NULL;
    qflags(bucket) = flags;
    qlock(bucket) = 0;
    qhome(bucket) = NULL;
    qupdir(bucket)= 0;
    qdcache(bucket)=0;
    bucket->__inode_counter = 0;
    bucket->lock = 0;
    bucket->password = 0;

    my_errno = ENOMEM;
    if ((qdcache(bucket) = linklist_init()) == NULL)
	goto error;
    
#define XSH (flags == FISH_FLAG_RSH ? "rsh" : "ssh")
    argv[i++] = XSH;
    argv[i++] = "-l";
    argv[i++] = user;
    argv[i++] = host;
    if (flags == FISH_FLAG_COMPRESSED)
        argv[i++] = "-C";
    argv[i++] = "echo FISH:; /bin/sh";
    argv[i++] = NULL;

    pipeopen(bucket, XSH, argv );


    {
        char answer[2048];
	print_vfs_message( "FISH: Waiting for initial line..." );
        if (!get_line(qsockr(bucket), answer, sizeof(answer), ':'))
	    goto error_2;
	print_vfs_message( answer );
	if (strstr(answer, "assword")) {

    /* Currently, this does not work. ssh reads passwords from
       /dev/tty, not from stdin :-(. */

	    message_1s (1, MSG_ERROR, _("Sorry, we can not do password authenticated connections for now."));
	    my_errno = EPERM;
	    goto error_2;

	    if (!bucket->password){
	        char *p, *op;
		p = copy_strings (" FISH: Password required for ", quser(bucket), 
				  " ", NULL);
		op = vfs_get_password (p);
		free (p);
		my_errno = EPERM;
		if (op == NULL)
		    goto error_2;
		bucket->password = strdup (op);
		wipe_password(op);
	    }
	    print_vfs_message( "FISH: Sending password..." );
	    write(qsockw(bucket), bucket->password, strlen(bucket->password));
	    write(qsockw(bucket), "\r\n", 2);
	}
    }

    print_vfs_message( "FISH: Sending initial line..." );
    my_errno = ECONNREFUSED;
    if (command (bucket, WAIT_REPLY, "#FISH\necho; start_fish_server; echo '### 200'\n") != COMPLETE)
        goto error_2;

    print_vfs_message( "FISH: Handshaking version..." );
    if (command (bucket, WAIT_REPLY, "#VER 0.0.0\necho '### 000'\n") != COMPLETE)
        goto error_2;

    print_vfs_message( "FISH: Setting up current directory..." );
    qhome(bucket) = fish_get_current_directory (bucket);
    if (!qhome(bucket))
        qhome(bucket) = strdup ("/");
    print_vfs_message( "FISH: Connected." );
    return bucket;

error_2:
    close(qsockr(bucket));
    close(qsockw(bucket));
error:
    free (qhost(bucket));
    free (quser(bucket));
    free (bucket);
    return NULL;
}

/* This routine keeps track of open connections */
/* Returns a connected socket to host */
static struct connection *
open_link (char *host, char *user, int flags, char *netrcpass)
{
    struct connection *bucket;
    struct linklist *lptr;
    
    for (lptr = connections_list->next; 
	 lptr != connections_list; lptr = lptr->next) {
	bucket = lptr->data;
	if ((strcmp (host, qhost(bucket)) == 0) &&
	    (strcmp (user, quser(bucket)) == 0) &&
	    (flags == qflags(bucket)))
	    return bucket;
    }
    bucket = open_command_connection(host, user, flags, netrcpass);
    if (bucket == NULL)
	return NULL;
    if (!linklist_insert(connections_list, bucket)) {
	connection_destructor(bucket);
	ERRNOR (ENOMEM, NULL);
    }
    return bucket;
}

/* The returned directory should always contain a trailing slash */
static char *fish_get_current_directory(struct connection *bucket)
{
    if (command(bucket, WANT_STRING, "#PWD\npwd; echo '### 200'\n") == COMPLETE)
        return copy_strings(reply_str, "/", NULL);
    ERRNOR (EIO, NULL);
}

static void my_forget (char *path)
{
}

#define X "fish"
#define X_myname "/#sh:"
#define X_vfs_ops fish_vfs_ops
#define X_fill_names fish_fill_names
#define X_hint_reread fish_hint_reread
#define X_flushdir fish_flushdir
#define X_done fish_done
#include "shared_ftp_fish.c"

static char*
get_path (struct connection **bucket, char *path)
{
    char *res;
    if ((res = s_get_path (bucket, path, "/#sh:")))
        return res;
    if ((res = s_get_path (bucket, path, "/#ssh:")))
        return res;
    if ((res = s_get_path (bucket, path, "/#rsh:"))) {
        qflags((*bucket)) |= FISH_FLAG_RSH;
        return res;
    }
    return NULL;
}

/*
 * This is the 'new' code
 */
/*
 * Last parameter (resolve_symlinks) is currently not used. Due to
 * the code sharing (file shared_ftp_fish.c) the fish and ftp interface
 * have to be the same (Norbert).
 */

static struct dir *
retrieve_dir(struct connection *bucket, char *remote_path, int resolve_symlinks)
{
    int has_symlinks;
    struct linklist *file_list, *p;
    struct direntry *fe;
    char buffer[8192];
    struct dir *dcache;
    int got_intr = 0;

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
		dir_destructor(dcache);
		free (p);
		break;
	    }
	}
    }

    has_symlinks = 0;
    print_vfs_message("fish: Reading FTP directory...");

    my_errno = ENOMEM;
    if (!(file_list = linklist_init()))
        return NULL;
    if (!(dcache = xmalloc(sizeof(struct dir), "struct dir"))) {
	linklist_destroy(file_list, NULL);
	return NULL;
    }
    gettimeofday(&dcache->timestamp, NULL);
    dcache->timestamp.tv_sec += 360;
    dcache->file_list = file_list;
    dcache->remote_path = strdup(remote_path);
    dcache->count = 1;

    command(bucket, NONE,
    "#LIST %s\nls -lLa %s | grep '^[^cbt]' | ( while read p x u g s m d y n; do echo \"P$p $u.$g\n"
    "S$s\nd$m $d $y\n:$n\n\"; done )\n"
    "ls -lLa %s | grep '^[cb]' | ( while read p x u g a i m d y n; do echo \"P$p $u.$g\n"
    "E$a$i\nd$m $d $y\n:$n\n\"; done ); echo '### 200'\n",
	    remote_path, remote_path, remote_path);

    /* Clear the interrupt flag */
    enable_interrupt_key ();
    
    fe = NULL;
    errno = 0;
    my_errno = ENOMEM;
    while ((got_intr = get_line_interruptible (buffer, sizeof (buffer), qsockr(bucket))) != EINTR){
	int eof = (got_intr == 0);

	if (logfile){
	    fputs (buffer, logfile);
            fputs ("\n", logfile);
	    fflush (logfile);
	}
	if (eof) {
	    if (fe)
	        free(fe);
	    my_errno = ECONNRESET;
	    goto error_1;
	}
	if (!strncmp(buffer, "### ", 4))
	    break;
	if ((!buffer[0]) && fe) {
	    if (!linklist_insert(file_list, fe)) {
		free(fe);
	        goto error_1;
	    }
	    fe = NULL;
	    continue;
	}
	
	if (!fe) {
	    if (!(fe = xmalloc(sizeof(struct direntry), "struct direntry")))
	        goto error_1;
	    bzero(fe, sizeof(struct direntry));
	    fe->count = 1;
	    fe->bucket = bucket;
	    fe->s.st_ino = bucket->__inode_counter++;
	    fe->s.st_nlink = 1;
	}

	switch(buffer[0]) {
	case ':': fe->name = strdup(buffer+1); break;
	case 'S': fe->s.st_size = atoi(buffer+1); break;
	case 'P': {
	              int i;
		      if ((i = parse_filetype(buffer[1])) ==-1)
			  break;
		      fe->s.st_mode = i;
		      if ((i = parse_filemode(buffer+2)) ==-1)
			  break;
		      fe->s.st_mode |= i;
	          }
	          break;
	case 'd': {
		      split_text(buffer+1);
		      if (!parse_filedate(0, &fe->s.st_ctime))
			  break;
		      fe->s.st_atime = fe->s.st_mtime = fe->s.st_ctime;
		  }
	          break;
	case 'D': {
	              struct tm tim;
		      if (sscanf(buffer+1, "%d %d %d %d %d %d", &tim.tm_year, &tim.tm_mon, 
				 &tim.tm_mday, &tim.tm_hour, &tim.tm_min, &tim.tm_sec) != 6)
			  break;
		      fe->s.st_atime = fe->s.st_mtime = fe->s.st_ctime = mktime(&tim);
	          }
	          break;
	case 'E': {
	              int maj, min;
	              if (sscanf(buffer+1, "%d,%d", &maj, &min) != 2)
			  break;
#ifdef HAVE_ST_RDEV
		      fe->s.st_rdev = (maj << 8) | min;
#endif
	          }
	case 'L': fe->linkname = strdup(buffer+1);
	          break;
	}
    }
    disable_interrupt_key();
#if 0
    if (got_intr)
	vfs_die("fish: reading FTP directory interrupted by user");
#endif

    if (decode_reply(buffer+4, 0) != COMPLETE) {
	my_errno = EIO;
        goto error_3;
    }
    if (file_list->next == file_list) {
	my_errno = EACCES;
	goto error_3;
    }
    if (!linklist_insert(qdcache(bucket), dcache)) {
	my_errno = ENOMEM;
        goto error_3;
    }
    print_vfs_message("fish: got listing");
    return dcache;
error_1:
    disable_interrupt_key();
error_3:
    free(dcache->remote_path);
    free(dcache);
    linklist_destroy(file_list, direntry_destructor);
    print_vfs_message("fish: failed");
    return NULL;
}

static int
store_file(struct direntry *fe)
{
    int local_handle, n, total;
    char buffer[8192];
    struct stat s;
    int was_error = 0;

    local_handle = open(fe->local_filename, O_RDONLY);
    unlink (fe->local_filename);
    my_errno = EIO;
    if (local_handle == -1)
	return 0;

    fstat(local_handle, &s);

    /* Use this as stor: ( dd block ; dd malyblock ) | ( cat > file; cat > /dev/null ) */

    print_vfs_message("FISH: store: sending command..." );
    if (command (fe->bucket, WAIT_REPLY, 
		 "#STOR %d %s\n> %s; echo '### 001'; ( dd bs=4096 count=%d; dd bs=%d count=1 ) 2>/dev/null | ( cat > %s; cat > /dev/null ); echo '### 200'\n",
		 s.st_size, fe->remote_filename,
		 fe->remote_filename,
		 s.st_size / 4096, s.st_size % 4096, fe->remote_filename)
	!= PRELIM) 
        return 0;

    total = 0;
    
    enable_interrupt_key();
    while (1) {
	while ((n = read(local_handle, buffer, sizeof(buffer))) < 0) {
	    if ((errno == EINTR) && got_interrupt)
	        continue;
	    print_vfs_message("FISH: Local read failed, sending zeros" );
	    close(local_handle);
	    local_handle = open( "/dev/zero", O_RDONLY );
	}
	if (n == 0)
	    break;
    	while (write(qsockw(fe->bucket), buffer, n) < 0) {
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
	print_vfs_message("fish: storing %s %d (%d)", 
			  was_error ? "zeros" : "file", total, s.st_size);
    }
    disable_interrupt_key();
    close(local_handle);
    if (get_reply (qsockr(fe->bucket), NULL, 0) != COMPLETE)
        ERRNOR (EIO, 0);
    return (!was_error);
error_return:
    disable_interrupt_key();
    close(local_handle);
    get_reply(qsockr(fe->bucket), NULL, 0);
    return 0;
}

static int linear_start(struct direntry *fe)
{
    fe->local_stat.st_mtime = 0;
    if (command(fe->bucket, WANT_STRING, 
		"#RETR %s\nls -l %s | ( read var1 var2 var3 var4 var5 var6; echo $var5 ); echo '### 100'; cat %s; echo '### 200'\n", 
		fe->remote_filename, fe->remote_filename, fe->remote_filename )
	!= PRELIM) ERRNOR (EACCES, 0);

    fe->got = 0;
    fe->total = atoi(reply_str);
    return 1;
}

static void
linear_abort (struct direntry *fe)
{
    char buffer[8192];
    int n;

    print_vfs_message( "Aborting transfer..." );
    do {
	n = VFS_MIN(8192, fe->total - fe->got);
	if (n)
	    if ((n = read(qsockr(fe->bucket), buffer, n)) < 0)
	        return;
    } while (n);

    if (get_reply (qsockr(fe->bucket), NULL, 0) != COMPLETE)
        print_vfs_message( "Error reported after abort." );
    else
        print_vfs_message( "Aborted transfer would be successfull." );
}

static int
linear_read (struct direntry *fe, void *buf, int len)
{
    int n = 0;
    len = VFS_MIN( fe->total - fe->got, len );
    while (len && ((n = read (qsockr(fe->bucket), buf, len))<0)) {
        if ((errno == EINTR) && !got_interrupt())
	    continue;
	break;
    }

    if (n>0) fe->got += n;
    if (n<0) linear_abort(fe);
    if ((!n) && ((get_reply (qsockr (fe->bucket), NULL, 0) != COMPLETE)))
        ERRNOR (EIO, -1);
    ERRNOR (errno, n);
}

static void
linear_close (struct direntry *fe)
{
    if (fe->total != fe->got)
        linear_abort(fe);
}

int fish_ctl (void *data, int ctlop, int arg)
{
    struct filp *fp = data;
    switch (ctlop) {
        case MCCTL_IS_NOTREADY:
	    {
	        int v = select_on_two (qsockr(fp->fe->bucket), 0);
		
		if (((v < 0) && (errno == EINTR)) || v == 0)
		    return 1;
		return 0;
	    }
        default:
	    return 0;
    }
}

static int
send_fish_command(struct connection *bucket, char *cmd, int flags)
{
    int r;
    int flush_directory_cache = (flags & OPT_FLUSH) && (normal_flush > 0);

    r = command (bucket, WAIT_REPLY, cmd);
    vfs_add_noncurrent_stamps (&fish_vfs_ops, (vfsid) bucket, NULL);
    if (r != COMPLETE) ERRNOR (EPERM, -1);
    if (flush_directory_cache)
	flush_all_directory(bucket);
    return 0;
}

int
fish_init (vfs *me)
{
    connections_list = linklist_init();
#if 0
    logfile = fopen ("/tmp/talk.fish", "w+");
#endif
    return 1;
}

#define PREFIX \
    char buf[999]; \
    char *remote_path; \
    struct connection *bucket; \
    if (!(remote_path = get_path(&bucket, path))) \
	return -1;

#define POSTFIX(flags) \
    free(remote_path); \
    return send_fish_command(bucket, buf, flags);

int fish_chmod (vfs *me, char *path, int mode)
{
    PREFIX
    sprintf(buf, "#CHMOD %4.4o %s\nchmod %4.4o %s; echo '### 000'\n", 
	    mode & 07777, remote_path,
	    mode & 07777, remote_path);
    POSTFIX(OPT_FLUSH);
}

#define FISH_OP(name, chk, string) \
int fish_##name (vfs *me, char *path1, char *path2) \
{ \
    char buf[120]; \
    char *remote_path1 = NULL, *remote_path2 = NULL; \
    struct connection *bucket1, *bucket2; \
    if (!(remote_path1 = get_path(&bucket1, path1))) \
	return -1; \
    if (!(remote_path2 = get_path(&bucket2, path2))) { \
	free(remote_path1); \
	return -1; \
    } \
    sprintf(buf, string, path1, path2, path1, path2 ); \
    free(remote_path1); \
    free(remote_path2); \
    return send_fish_command(bucket2, buf, OPT_FLUSH); \
}

#define XTEST if (bucket1 != bucket2) { free(remote_path1); free(remote_path2); ERRNOR (EXDEV, -1); }
FISH_OP(rename, XTEST, "#RENAME %s %s\nmv %s %s; echo '*** 000'" );
FISH_OP(link,   XTEST, "#LINK %s %s\nln %s %s; echo '*** 000'" );
FISH_OP(symlink,     , "#SYMLINK %s %s\nln -s %s %s; echo '*** 000'" );

int fish_chown (vfs *me, char *path, int owner, int group)
{
    char *sowner, *sgroup;
    PREFIX
    sowner = getpwuid( owner )->pw_name;
    sgroup = getgrgid( group )->gr_name;
    sprintf(buf, "#CHOWN %s %s\nchown %s %s; echo '### 000'\n", 
	    sowner, remote_path,
	    sowner, remote_path);
    send_fish_command(bucket, buf, OPT_FLUSH); 
                  /* FIXME: what should we report if chgrp succeeds but chown fails? */
    sprintf(buf, "#CHGRP %s %s\nchgrp %s %s; echo '### 000'\n", 
	    sgroup, remote_path,
	    sgroup, remote_path);
    free(remote_path);
    POSTFIX(OPT_FLUSH)
}

static int fish_unlink (vfs *me, char *path)
{
    PREFIX
    sprintf(buf, "#DELE %s\nrm -f %s; echo '### 000'\n", remote_path, remote_path);
    POSTFIX(OPT_FLUSH);
}

static int fish_mkdir (vfs *me, char *path, mode_t mode)
{
    PREFIX
    sprintf(buf, "#MKD %s\nmkdir %s; echo '### 000'\n", remote_path, remote_path);
    POSTFIX(OPT_FLUSH);
}

static int fish_rmdir (vfs *me, char *path)
{
    PREFIX
    sprintf(buf, "#RMD %s\nrmdir %s; echo '### 000'\n", remote_path, remote_path);
    POSTFIX(OPT_FLUSH);
}

vfs fish_vfs_ops = {
    NULL,	/* This is place of next pointer */
    "FIles tranferred over SHell",
    F_EXEC,	/* flags */
    "sh:",	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    fish_init,
    fish_done,
    fish_fill_names,
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

    fish_chmod,
    fish_chown,	/* not really implemented but returns success */
    NULL,		/* utime */

    s_readlink,
    fish_symlink,		/* symlink */
    fish_link,			/* link */
    fish_unlink,

    fish_rename,		/* rename */
    s_chdir,
    s_errno,
    s_lseek,
    NULL,		/* mknod */
    
    s_getid,
    s_nothingisopen,
    s_free,
    
    s_getlocalcopy,
    s_ungetlocalcopy,

    fish_mkdir,
    fish_rmdir,
    fish_ctl,
    s_setctl
#ifdef HAVE_MMAP
    , NULL,
    NULL
#endif
};
