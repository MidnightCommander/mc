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
 *
 * Namespace: fish_vfs_ops exported.
 */

/* Define this if your ssh can take -I option */

#undef HAVE_HACKED_SSH

#include "xdirentry.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/main.h"
#include "../src/mem.h"
#include "vfs.h"
#include "tcputil.h"
#include "container.h"
#include "fish.h"
#include <glib.h>

/*
 * Reply codes.
 */
#define PRELIM		1	/* positive preliminary */
#define COMPLETE	2	/* positive completion */
#define CONTINUE	3	/* positive intermediate */
#define TRANSIENT	4	/* transient negative completion */
#define ERROR		5	/* permanent negative completion */

/* If true, the directory cache is forced to reload */
static int force_expiration = 0;

/* FIXME: prev two variables should be killed */

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02
static char reply_str [80];

static int decode_reply (char *s, int was_garbage)
{
    int code;
    if (!sscanf(s, "%d", &code)) {
	code = 500;
	return 5;
    }
    if (code<100) return was_garbage ? ERROR : (!code ? COMPLETE : PRELIM);
    return code / 100;
}

/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */
static int get_reply (vfs *me, int sock, char *string_buf, int string_len)
{
    char answer[1024];
    int was_garbage = 0;
    
    for (;;) {
        if (!vfs_s_get_line(me, sock, answer, sizeof(answer), '\n')) {
	    if (string_buf)
		*string_buf = 0;
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

#define SUP super->u.fish

static int command (vfs *me, vfs_s_super *super, int wait_reply, char *fmt, ...)
{
    va_list ap;
    char *str;
    int n, status;
    FILE *logfile = MEDATA->logfile;

    va_start (ap, fmt);

    str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (logfile){
        fwrite (str, strlen (str), 1, logfile);
	fflush (logfile);
    }

    enable_interrupt_key();

    status = write(SUP.sockw, str, strlen(str));
    g_free (str);

    disable_interrupt_key();
    if (status < 0)
	return TRANSIENT;
    
    if (wait_reply)
	return get_reply (me, SUP.sockr, (wait_reply & WANT_STRING) ? reply_str : NULL, sizeof (reply_str)-1);
    return COMPLETE;
}

static void
free_archive (vfs *me, vfs_s_super *super)
{
    if ((SUP.sockw != -1) || (SUP.sockr != -1)){
	print_vfs_message ("fish: Disconnecting from %s", super->name?super->name:"???");
	command(me, super, NONE, "#BYE\nlogout\n");
	close(SUP.sockw);
	close(SUP.sockr);
	SUP.sockw = SUP.sockr = -1;
    }
    ifree (SUP.host);
    ifree (SUP.home);
    ifree (SUP.user);
    ifree (SUP.cwdir);
    ifree (SUP.password);
}

static void
pipeopen(vfs_s_super *super, char *path, char *argv[])
{
    int fileset1[2], fileset2[2];
    int res;

    if ((pipe(fileset1)<0) || (pipe(fileset2)<0)) 
	vfs_die("Could not pipe(): %m.");
    
    if ((res = fork())) {
        if (res<0) vfs_die("Could not fork(): %m.");
	/* We are the parent */
	close(fileset1[0]);
	SUP.sockw = fileset1[1];
	close(fileset2[1]);
	SUP.sockr = fileset2[0];
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

/* The returned directory should always contain a trailing slash */
static char *fish_getcwd(vfs *me, vfs_s_super *super)
{
    if (command(me, super, WANT_STRING, "#PWD\npwd; echo '### 200'\n") == COMPLETE)
        return copy_strings(reply_str, "/", NULL);
    ERRNOR (EIO, NULL);
}
static int
open_archive_int (vfs *me, vfs_s_super *super)
{
    char *argv[100];
    char *xsh = (SUP.flags == FISH_FLAG_RSH ? "rsh" : "ssh");
    int i = 0;

    argv[i++] = xsh;
#ifdef HAVE_HACKED_SSH
    argv[i++] = "-I";
#endif
    argv[i++] = "-l";
    argv[i++] = SUP.user;
    argv[i++] = SUP.host;
    if (SUP.flags == FISH_FLAG_COMPRESSED)
        argv[i++] = "-C";
    argv[i++] = "echo FISH:; /bin/sh";
    argv[i++] = NULL;

#if 0
    /* Debugging hack */
    if (!MEDATA->logfile)
	MEDATA->logfile = fopen( "/home/pavel/talk.fish", "w+" ); /* FIXME */
#endif

    pipeopen(super, xsh, argv );

    {
        char answer[2048];
	print_vfs_message( "fish: Waiting for initial line..." );
        if (!vfs_s_get_line(me, SUP.sockr, answer, sizeof(answer), ':'))
	    ERRNOR (E_PROTO, -1);
	print_vfs_message( answer );
	if (strstr(answer, "assword")) {

    /* Currently, this does not work. ssh reads passwords from
       /dev/tty, not from stdin :-(. */

#ifndef HAVE_HACKED_SSH
	    message_1s (1, _(" Error "), _("Sorry, we can not do password authenticated connections for now."));
	    ERRNOR (EPERM, -1);
#endif
	    if (!SUP.password){
		char *p, *op;
		p = copy_strings (" fish: Password required for ", SUP.user, 
				  " ", NULL);
		op = vfs_get_password (p);
		free (p);
		if (op == NULL)
		    ERRNOR (EPERM, -1);
		SUP.password = strdup (op);
		wipe_password(op);
	    }
	    print_vfs_message( "fish: Sending password..." );
	    write(SUP.sockw, SUP.password, strlen(SUP.password));
	    write(SUP.sockw, "\n", 1);
	}
    }

    print_vfs_message( "FISH: Sending initial line..." );
    if (command (me, super, WAIT_REPLY, "#FISH\necho; start_fish_server; echo '### 200'\n") != COMPLETE)
        ERRNOR (E_PROTO, -1);

    print_vfs_message( "FISH: Handshaking version..." );
    if (command (me, super, WAIT_REPLY, "#VER 0.0.0\necho '### 000'\n") != COMPLETE)
        ERRNOR (E_PROTO, -1);

    print_vfs_message( "FISH: Setting up current directory..." );
    SUP.home = fish_getcwd (me, super);
    print_vfs_message( "FISH: Connected, home %s.", SUP.home );
#if 0
    super->name = copy_strings( "/#sh:", SUP.user, "@", SUP.host, "/", NULL );
#endif
    super->name = strdup( "/" );

    super->root = vfs_s_new_inode (me, super, vfs_s_default_stat(me, S_IFDIR | 0755));
    return 0;
}

int
open_archive (vfs *me, vfs_s_super *super, char *archive_name, char *op)
{
    char *host, *user, *password;
    int flags;

    vfs_split_url (strchr(op, ':')+1, &host, &user, &flags, &password, 0, URL_NOSLASH);
    SUP.host = strdup (host);
    SUP.user = strdup (user);
    SUP.flags = flags;
    if (!strncmp( op, "rsh:", 4 ))
	SUP.flags |= FISH_FLAG_RSH;
    SUP.home = NULL;
    if (password)
	SUP.password = strdup (password);
    return open_archive_int (me, super);
}

static int
archive_same(vfs *me, vfs_s_super *super, char *archive_name, char *op, void *cookie)
{	
    char *host, *user, *dummy2;
    int flags;
    vfs_split_url (strchr(op, ':')+1, &host, &user, &flags, &dummy2, 0, URL_NOSLASH);
    return ((strcmp (host, SUP.host) == 0) &&
	    (strcmp (user, SUP.user) == 0) &&
	    (flags == SUP.flags));
}

int
fish_which (vfs *me, char *path)
{
    if (!strncmp (path, "/#sh:", 5))
        return 1;
    if (!strncmp (path, "/#ssh:", 6))
        return 1;
    if (!strncmp (path, "/#rsh:", 6))
        return 1;
    return 0;
}

int
dir_uptodate(vfs *me, vfs_s_inode *ino)
{
    struct timeval tim;

    return 1; /* Timeouting of directories does not work too well :-(. */
    gettimeofday(&tim, NULL);
    if (force_expiration) {
	force_expiration = 0;
	return 0;
    }
    if (tim.tv_sec < ino->u.fish.timestamp.tv_sec)
	return 1;
    return 0;
}

static int
dir_load(vfs *me, vfs_s_inode *dir, char *remote_path)
{
    vfs_s_super *super = dir->super;
    char buffer[8192];
    vfs_s_entry *ent = NULL;
    FILE *logfile;

    logfile = MEDATA->logfile;

    print_vfs_message("fish: Reading directory %s...", remote_path);

    gettimeofday(&dir->u.fish.timestamp, NULL);
    dir->u.fish.timestamp.tv_sec += 10; /* was 360: 10 is good for
					   stressing direntry layer a bit */

    command(me, super, NONE,
    "#LIST /%s\nls -lLa /%s | grep '^[^cbt]' | ( while read p x u g s m d y n; do echo \"P$p $u.$g\n"
    "S$s\nd$m $d $y\n:$n\n\"; done )\n"
    "ls -lLa /%s | grep '^[cb]' | ( while read p x u g a i m d y n; do echo \"P$p $u.$g\n"
    "E$a$i\nd$m $d $y\n:$n\n\"; done ); echo '### 200'\n",
	    remote_path, remote_path, remote_path);

#define SIMPLE_ENTRY vfs_s_generate_entry(me, NULL, dir, 0)
    ent = SIMPLE_ENTRY;
    while (1) {
	int res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), SUP.sockr); 
	if ((!res) || (res == EINTR)) {
	    vfs_s_free_entry(me, ent);
	    me->verrno = ECONNRESET;
	    goto error;
	}
	if (logfile){
	    fputs (buffer, logfile);
            fputs ("\n", logfile);
	    fflush (logfile);
	}
	if (!strncmp(buffer, "### ", 4))
	    break;
	if ((!buffer[0])) {
	    if (ent->name) {
		vfs_s_insert_entry(me, dir, ent);
		ent = SIMPLE_ENTRY;
	    }
	    continue;
	}
	
#define ST ent->ino->st

	switch(buffer[0]) {
	case ':': {
	              char *c;
		      if (!strcmp(buffer+1, ".") || !strcmp(buffer+1, ".."))
			  break;  /* We'll do . and .. ourself */
		      ent->name = strdup(buffer+1); 
		      if ((c=strchr(ent->name, ' ')))
			  *c = 0; /* this is ugly, but we can not handle " " in name */
		      break;
	          }
	case 'S': ST.st_size = atoi(buffer+1); break;
	case 'P': {
	              int i;
		      if ((i = vfs_parse_filetype(buffer[1])) ==-1)
			  break;
		      ST.st_mode = i; 
		      if ((i = vfs_parse_filemode(buffer+2)) ==-1)
			  break;
		      ST.st_mode |= i;
		      if (S_ISLNK(ST.st_mode))
			  ST.st_mode = 0;
	          }
	          break;
	case 'd': {
		      vfs_split_text(buffer+1);
		      if (!vfs_parse_filedate(0, &ST.st_ctime))
			  break;
		      ST.st_atime = ST.st_mtime = ST.st_ctime;
		  }
	          break;
	case 'D': {
	              struct tm tim;
		      if (sscanf(buffer+1, "%d %d %d %d %d %d", &tim.tm_year, &tim.tm_mon, 
				 &tim.tm_mday, &tim.tm_hour, &tim.tm_min, &tim.tm_sec) != 6)
			  break;
		      ST.st_atime = ST.st_mtime = ST.st_ctime = mktime(&tim);
	          }
	          break;
	case 'E': {
	              int maj, min;
	              if (sscanf(buffer+1, "%d,%d", &maj, &min) != 2)
			  break;
#ifdef HAVE_ST_RDEV
		      ST.st_rdev = (maj << 8) | min;
#endif
	          }
	case 'L': ent->ino->linkname = strdup(buffer+1);
	          break;
	}
    }
    
    vfs_s_free_entry (me, ent);
    me->verrno = E_REMOTE;
    if (decode_reply(buffer+4, 0) != COMPLETE)
        goto error;

    print_vfs_message("fish: got listing");
    return 0;

error:
    print_vfs_message("fish: failed");
    return 1;
}

static int
file_store(vfs *me, vfs_s_super *super, char *name, char *localname)
{
    int n, total;
    char buffer[8192];
    struct stat s;
    int was_error = 0;
    int h;

    h = open(localname, O_RDONLY);

    if (fstat(h, &s)<0)
	ERRNOR (EIO, -1);

    /* Use this as stor: ( dd block ; dd smallblock ) | ( cat > file; cat > /dev/null ) */

    print_vfs_message("FISH: store %s: sending command...", name );
    if (command (me, super, WAIT_REPLY, 
		 "#STOR %d /%s\n> /%s; echo '### 001'; ( dd bs=4096 count=%d; dd bs=%d count=1 ) 2>/dev/null | ( cat > /%s; cat > /dev/null ); echo '### 200'\n",
		 s.st_size, name, name,
		 s.st_size / 4096, s.st_size % 4096, name)
	!= PRELIM) 
        ERRNOR(E_REMOTE, -1);

    total = 0;
    
    while (1) {
	while ((n = read(h, buffer, sizeof(buffer))) < 0) {
	    if ((errno == EINTR) && got_interrupt)
	        continue;
	    print_vfs_message("FISH: Local read failed, sending zeros" );
	    close(h);
	    h = open( "/dev/zero", O_RDONLY );
	}
	if (n == 0)
	    break;
    	while (write(SUP.sockw, buffer, n) < 0) {
	    me->verrno = errno;
	    goto error_return;
	}
	disable_interrupt_key();
	total += n;
	print_vfs_message("fish: storing %s %d (%d)", 
			  was_error ? "zeros" : "file", total, s.st_size);
    }
    if ((get_reply (me, SUP.sockr, NULL, 0) != COMPLETE) || was_error)
        ERRNOR (E_REMOTE, 0);
    close(h);
    return 0;
error_return:
    close(h);
    get_reply(me, SUP.sockr, NULL, 0);
    return -1;
}

static int linear_start(vfs *me, vfs_s_fh *fh, int offset)
{
    char *name;
    if (offset)
        ERRNOR (E_NOTSUPP, 0);
/*    fe->local_stat.st_mtime = 0; FIXME: what is this good for? */
    name = vfs_s_fullpath (me, fh->ino);
    if (!name)
	return 0;
    if (command(me, FH_SUPER, WANT_STRING, 
		"#RETR /%s\nls -l /%s | ( read var1 var2 var3 var4 var5 var6; echo $var5 ); echo '### 100'; cat /%s; echo '### 200'\n", 
		name, name, name )
	!= PRELIM) ERRNOR (E_REMOTE, 0);
    fh->linear = LS_LINEAR_OPEN;
    fh->u.fish.got = 0;
    if (sscanf( reply_str, "%d", &fh->u.fish.total )!=1)
	ERRNOR (E_REMOTE, 0);
    return 1;
}

static void
linear_abort (vfs *me, vfs_s_fh *fh)
{
    vfs_s_super *super = FH_SUPER;
    char buffer[8192];
    int n;

    print_vfs_message( "Aborting transfer..." );
    do {
	n = VFS_MIN(8192, fh->u.fish.total - fh->u.fish.got);
	if (n)
	    if ((n = read(SUP.sockr, buffer, n)) < 0)
	        return;
    } while (n);

    if (get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)
        print_vfs_message( "Error reported after abort." );
    else
        print_vfs_message( "Aborted transfer would be successfull." );
}

static int
linear_read (vfs *me, vfs_s_fh *fh, void *buf, int len)
{
    vfs_s_super *super = FH_SUPER;
    int n = 0;
    len = VFS_MIN( fh->u.fish.total - fh->u.fish.got, len );
    disable_interrupt_key();
    while (len && ((n = read (SUP.sockr, buf, len))<0)) {
        if ((errno == EINTR) && !got_interrupt())
	    continue;
	break;
    }
    enable_interrupt_key();

    if (n>0) fh->u.fish.got += n;
    if (n<0) linear_abort(me, fh);
    if ((!n) && ((get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)))
        ERRNOR (E_REMOTE, -1);
    ERRNOR (errno, n);
}

static void
linear_close (vfs *me, vfs_s_fh *fh)
{
    if (fh->u.fish.total != fh->u.fish.got)
        linear_abort(me, fh);
}

static int
fish_ctl (void *fh, int ctlop, int arg)
{
    return 0;
    switch (ctlop) {
        case MCCTL_IS_NOTREADY:
	    {
	        int v;

		if (!FH->linear)
		    vfs_die ("You may not do this");
		if (FH->linear == LS_LINEAR_CLOSED)
		    return 0;

		v = vfs_s_select_on_two (FH_SUPER->u.fish.sockr, 0);
		if (((v < 0) && (errno == EINTR)) || v == 0)
		    return 1;
		return 0;
	    }
        default:
	    return 0;
    }
}

static int
send_fish_command(vfs *me, vfs_s_super *super, char *cmd, int flags)
{
    int r;

    r = command (me, super, WAIT_REPLY, cmd);
    vfs_add_noncurrent_stamps (&vfs_fish_ops, (vfsid) super, NULL);
    if (r != COMPLETE) ERRNOR (E_REMOTE, -1);
    if (flags & OPT_FLUSH)
	vfs_s_invalidate(me, super);
    return 0;
}

#define PREFIX \
    char buf[999]; \
    char *rpath; \
    vfs_s_super *super; \
    if (!(rpath = vfs_s_get_path_mangle(me, path, &super, 0))) \
	return -1;

#define POSTFIX(flags) \
    return send_fish_command(me, super, buf, flags);

static int
fish_chmod (vfs *me, char *path, int mode)
{
    PREFIX
    sprintf(buf, "#CHMOD %4.4o /%s\nchmod %4.4o /%s; echo '### 000'\n", 
	    mode & 07777, rpath,
	    mode & 07777, rpath);
    POSTFIX(OPT_FLUSH);
}

#define FISH_OP(name, chk, string) \
static int fish_##name (vfs *me, char *path1, char *path2) \
{ \
    char buf[1024]; \
    char *rpath1 = NULL, *rpath2 = NULL; \
    vfs_s_super *super1, *super2; \
    if (!(rpath1 = vfs_s_get_path_mangle(me, path1, &super1, 0))) \
	return -1; \
    if (!(rpath2 = vfs_s_get_path_mangle(me, path2, &super2, 0))) \
	return -1; \
    g_snprintf(buf, 1023, string "\n", rpath1, rpath2, rpath1, rpath2 ); \
    return send_fish_command(me, super2, buf, OPT_FLUSH); \
}

#define XTEST if (bucket1 != bucket2) { ERRNOR (EXDEV, -1); }
FISH_OP(rename, XTEST, "#RENAME /%s /%s\nmv /%s /%s; echo '### 000'" );
FISH_OP(link,   XTEST, "#LINK /%s /%s\nln /%s /%s; echo '### 000'" );

static int fish_symlink (vfs *me, char *setto, char *path)
{
    PREFIX
    sprintf(buf, "#SYMLINK %s /%s\nln -s %s /%s; echo '### 000'\n", setto, rpath, setto, rpath);
    POSTFIX(OPT_FLUSH);
}

static int
fish_chown (vfs *me, char *path, int owner, int group)
{
    char *sowner, *sgroup;
    PREFIX
    sowner = getpwuid( owner )->pw_name;
    sgroup = getgrgid( group )->gr_name;
    sprintf(buf, "#CHOWN /%s /%s\nchown /%s /%s; echo '### 000'\n", 
	    sowner, rpath,
	    sowner, rpath);
    send_fish_command(me, super, buf, OPT_FLUSH); 
                  /* FIXME: what should we report if chgrp succeeds but chown fails? */
    sprintf(buf, "#CHGRP /%s /%s\nchgrp /%s /%s; echo '### 000'\n", 
	    sgroup, rpath,
	    sgroup, rpath);
    POSTFIX(OPT_FLUSH)
}

static int fish_unlink (vfs *me, char *path)
{
    PREFIX
    sprintf(buf, "#DELE /%s\nrm -f /%s; echo '### 000'\n", rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int fish_mkdir (vfs *me, char *path, mode_t mode)
{
    PREFIX
    sprintf(buf, "#MKD /%s\nmkdir /%s; echo '### 000'\n", rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int fish_rmdir (vfs *me, char *path)
{
    PREFIX
    sprintf(buf, "#RMD /%s\nrmdir /%s; echo '### 000'\n", rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int retrieve_file(vfs *me, struct vfs_s_inode *ino)
{
    /* If you want reget, you'll have to open file with O_LINEAR */
    int total = 0;
    char buffer[8192];
    int handle, n;
    int stat_size = ino->st.st_size;
    struct vfs_s_fh fh;

    memset(&fh, 0, sizeof(fh));
    
    fh.ino = ino;
    if (!(ino->localname = tempnam (NULL, me->name))) ERRNOR (ENOMEM, 0);

    handle = open(ino->localname, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
    if (handle == -1) {
	me->verrno = errno;
	goto error_4;
    }

    if (!MEDATA->linear_start (me, &fh, 0))
        goto error_3;

    /* Clear the interrupt status */
    
    while (1) {
	n = linear_read(me, &fh, buffer, sizeof(buffer));
	if (n < 0)
	    goto error_1;
	if (!n)
	    break;

	total += n;
	vfs_print_stats (me->name, "Getting file", ino->ent->name, total, stat_size);

        if (write(handle, buffer, n) < 0) {
	    me->verrno = errno;
	    goto error_1;
	}
    }
    linear_close(me, &fh);
    close(handle);

    if (stat (ino->localname, &ino->u.fish.local_stat) < 0)
        ino->u.fish.local_stat.st_mtime = 0;
    
    return 0;
error_1:
    linear_close(me, &fh);
error_3:
    disable_interrupt_key();
    close(handle);
    unlink(ino->localname);
error_4:
    free(ino->localname);
    ino->localname = NULL;
    return -1;
}

static int fish_fh_open (vfs *me, vfs_s_fh *fh, int flags, int mode)
{
    if (IS_LINEAR(mode)) {
	message_1s(1, "Linear mode requested", "?!" );
	fh->linear = LS_LINEAR_CLOSED;
	return 0;
    }
    if (!fh->ino->localname)
	if (retrieve_file (me, fh->ino)==-1)
	    return -1;
    if (!fh->ino->localname)
	vfs_die( "retrieve_file failed to fill in localname" );
    return 0;
}

static struct vfs_s_data fish_data = {
    NULL,
    0,
    0,
    NULL,

    NULL, /* init_inode */
    NULL, /* free_inode */
    NULL, /* init_entry */

    NULL, /* archive_check */
    archive_same,
    open_archive,
    free_archive,

    fish_fh_open, /* fh_open */
    NULL, /* fh_close */

    vfs_s_find_entry_linear,
    dir_load,
    dir_uptodate,
    file_store,

    linear_start,
    linear_read,
    linear_close
};

vfs vfs_fish_ops = {
    NULL,	/* This is place of next pointer */
    "FIles tranferred over SHell",
    F_EXEC,	/* flags */
    "sh:",	/* prefix */
    &fish_data,	/* data */
    0,		/* errno */
    NULL,
    NULL,
    vfs_s_fill_names,
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

    fish_chmod,
    fish_chown,
    NULL,		/* utime */

    vfs_s_readlink,
    fish_symlink,		/* symlink */
    fish_link,			/* link */
    fish_unlink,

    fish_rename,		/* rename */
    vfs_s_chdir,
    vfs_s_ferrno,
    vfs_s_lseek,
    NULL,		/* mknod */
    
    vfs_s_getid,
    vfs_s_nothingisopen,
    vfs_s_free,
    
    NULL, /* vfs_s_getlocalcopy, */
    NULL, /* vfs_s_ungetlocalcopy, */

    fish_mkdir,
    fish_rmdir,
    fish_ctl,
    vfs_s_setctl

MMAPNULL
};
