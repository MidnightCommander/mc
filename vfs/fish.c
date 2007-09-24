/* Virtual File System: FISH implementation for transfering files over
   shell connections.

   Copyright (C) 1998 Free Software Foundation, Inc.
   
   Written by: 1998 Pavel Machek
   Spaces fix: 2000 Michal Svec

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

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

#include <config.h>
#include <errno.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "xdirentry.h"
#include "vfs.h"
#include "vfs-impl.h"
#include "gc.h"		/* vfs_stamp_create */
#include "tcputil.h"
#include "../src/unixcompat.h"
#include "fish.h"

int fish_directory_timeout = 900;

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

#define FISH_FLAG_COMPRESSED 1
#define FISH_FLAG_RSH	     2

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

/*
 * Reply codes.
 */
#define PRELIM		1	/* positive preliminary */
#define COMPLETE	2	/* positive completion */
#define CONTINUE	3	/* positive intermediate */
#define TRANSIENT	4	/* transient negative completion */
#define ERROR		5	/* permanent negative completion */

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02
static char reply_str [80];

static struct vfs_class vfs_fish_ops;

static int
 fish_command (struct vfs_class *me, struct vfs_s_super *super,
	       int wait_reply, const char *fmt, ...)
    __attribute__ ((format (__printf__, 4, 5)));

static int fish_decode_reply (char *s, int was_garbage)
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
static int fish_get_reply (struct vfs_class *me, int sock, char *string_buf, int string_len)
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
	    if (string_buf)
		g_strlcpy(string_buf, answer, string_len);
	} else return fish_decode_reply(answer+4, was_garbage);
    }
}

#define SUP super->u.fish

static int
fish_command (struct vfs_class *me, struct vfs_s_super *super,
	      int wait_reply, const char *fmt, ...)
{
    va_list ap;
    char *str;
    int status;
    FILE *logfile = MEDATA->logfile;

    va_start (ap, fmt);

    str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (logfile) {
	fwrite (str, strlen (str), 1, logfile);
	fflush (logfile);
    }

    enable_interrupt_key ();

    status = write (SUP.sockw, str, strlen (str));
    g_free (str);

    disable_interrupt_key ();
    if (status < 0)
	return TRANSIENT;

    if (wait_reply)
	return fish_get_reply (me, SUP.sockr,
			       (wait_reply & WANT_STRING) ? reply_str :
			       NULL, sizeof (reply_str) - 1);
    return COMPLETE;
}

static void
fish_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    if ((SUP.sockw != -1) || (SUP.sockr != -1)) {
	print_vfs_message (_("fish: Disconnecting from %s"),
			   super->name ? super->name : "???");
	fish_command (me, super, NONE, "#BYE\nexit\n");
	close (SUP.sockw);
	close (SUP.sockr);
	SUP.sockw = SUP.sockr = -1;
    }
    g_free (SUP.host);
    g_free (SUP.user);
    g_free (SUP.cwdir);
    g_free (SUP.password);
}

static void
fish_pipeopen(struct vfs_s_super *super, const char *path, const char *argv[])
{
    int fileset1[2], fileset2[2];
    int res;

    if ((pipe(fileset1)<0) || (pipe(fileset2)<0)) 
	vfs_die("Cannot pipe(): %m.");
    
    if ((res = fork())) {
        if (res<0) vfs_die("Cannot fork(): %m.");
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
	/* stderr to /dev/null */
	open ("/dev/null", O_WRONLY);
	close(fileset2[0]); close(fileset2[1]);
	execvp(path, const_cast(char **, argv));
	_exit(3);
    }
}

/* The returned directory should always contain a trailing slash */
static char *fish_getcwd(struct vfs_class *me, struct vfs_s_super *super)
{
    if (fish_command (me, super, WANT_STRING, "#PWD\npwd; echo '### 200'\n") == COMPLETE)
        return  g_strconcat (reply_str, "/", (char *) NULL);
    ERRNOR (EIO, NULL);
}

static int
fish_open_archive_int (struct vfs_class *me, struct vfs_s_super *super)
{
    {
	const char *argv[10];
	const char *xsh = (SUP.flags == FISH_FLAG_RSH ? "rsh" : "ssh");
	int i = 0;

	argv[i++] = xsh;
	if (SUP.flags == FISH_FLAG_COMPRESSED)
	    argv[i++] = "-C";
	argv[i++] = "-l";
	argv[i++] = SUP.user;
	argv[i++] = SUP.host;
	argv[i++] = "echo FISH:; /bin/sh";
	argv[i++] = NULL;

	fish_pipeopen (super, xsh, argv);
    }
    {
	char answer[2048];
	print_vfs_message (_("fish: Waiting for initial line..."));
	if (!vfs_s_get_line (me, SUP.sockr, answer, sizeof (answer), ':'))
	    ERRNOR (E_PROTO, -1);
	print_vfs_message ("%s", answer);
	if (strstr (answer, "assword")) {

	    /* Currently, this does not work. ssh reads passwords from
	       /dev/tty, not from stdin :-(. */

	    message (1, MSG_ERROR,
		     _
		     ("Sorry, we cannot do password authenticated connections for now."));
	    ERRNOR (EPERM, -1);
	    if (!SUP.password) {
		char *p, *op;
		p = g_strconcat (_(" fish: Password required for "),
				 SUP.user, " ", (char *) NULL);
		op = vfs_get_password (p);
		g_free (p);
		if (op == NULL)
		    ERRNOR (EPERM, -1);
		SUP.password = op;
	    }
	    print_vfs_message (_("fish: Sending password..."));
	    write (SUP.sockw, SUP.password, strlen (SUP.password));
	    write (SUP.sockw, "\n", 1);
	}
    }

    print_vfs_message (_("fish: Sending initial line..."));
    /*
     * Run `start_fish_server'. If it doesn't exist - no problem,
     * we'll talk directly to the shell.
     */
    if (fish_command
	(me, super, WAIT_REPLY,
	 "#FISH\necho; start_fish_server 2>&1; echo '### 200'\n") !=
	COMPLETE)
	ERRNOR (E_PROTO, -1);

    print_vfs_message (_("fish: Handshaking version..."));
    if (fish_command
	(me, super, WAIT_REPLY,
	 "#VER 0.0.0\necho '### 000'\n") != COMPLETE)
	ERRNOR (E_PROTO, -1);

    /* Set up remote locale to C, otherwise dates cannot be recognized */
    if (fish_command
	(me, super, WAIT_REPLY,
	 "LANG=C; LC_ALL=C; LC_TIME=C\n"
	 "export LANG; export LC_ALL; export LC_TIME\n" "echo '### 200'\n")
	!= COMPLETE)
	ERRNOR (E_PROTO, -1);

    print_vfs_message (_("fish: Setting up current directory..."));
    SUP.cwdir = fish_getcwd (me, super);
    print_vfs_message (_("fish: Connected, home %s."), SUP.cwdir);
#if 0
    super->name =
	g_strconcat ("/#sh:", SUP.user, "@", SUP.host, "/", (char *) NULL);
#endif
    super->name = g_strdup (PATH_SEP_STR);

    super->root =
	vfs_s_new_inode (me, super,
			 vfs_s_default_stat (me, S_IFDIR | 0755));
    return 0;
}

static int
fish_open_archive (struct vfs_class *me, struct vfs_s_super *super,
		   const char *archive_name, char *op)
{
    char *host, *user, *password, *p;
    int flags;

    (void) archive_name;

    p = vfs_split_url (strchr (op, ':') + 1, &host, &user, &flags,
		       &password, 0, URL_NOSLASH);

    g_free (p);

    SUP.host = host;
    SUP.user = user;
    SUP.flags = flags;
    if (!strncmp (op, "rsh:", 4))
	SUP.flags |= FISH_FLAG_RSH;
    SUP.cwdir = NULL;
    if (password)
	SUP.password = password;
    return fish_open_archive_int (me, super);
}

static int
fish_archive_same (struct vfs_class *me, struct vfs_s_super *super,
		   const char *archive_name, char *op, void *cookie)
{
    char *host, *user;
    int flags;

    (void) me;
    (void) archive_name;
    (void) cookie;

    op = vfs_split_url (strchr (op, ':') + 1, &host, &user, &flags, 0, 0,
			URL_NOSLASH);

    g_free (op);

    flags = ((strcmp (host, SUP.host) == 0)
	     && (strcmp (user, SUP.user) == 0) && (flags == SUP.flags));
    g_free (host);
    g_free (user);

    return flags;
}

static int
fish_dir_load(struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
{
    struct vfs_s_super *super = dir->super;
    char buffer[8192];
    struct vfs_s_entry *ent = NULL;
    FILE *logfile;
    char *quoted_path;

    logfile = MEDATA->logfile;

    print_vfs_message(_("fish: Reading directory %s..."), remote_path);

    gettimeofday(&dir->timestamp, NULL);
    dir->timestamp.tv_sec += fish_directory_timeout;
    quoted_path = name_quote (remote_path, 0);
    fish_command (me, super, NONE,
	    "#LIST /%s\n"
	    "ls -lLan /%s 2>/dev/null | grep '^[^cbt]' | (\n"
	      "while read p l u g s m d y n; do\n"
	        "echo \"P$p $u.$g\nS$s\nd$m $d $y\n:$n\n\"\n"
	      "done\n"
	    ")\n"
	    "ls -lan /%s 2>/dev/null | grep '^[cb]' | (\n"
	      "while read p l u g a i m d y n; do\n"
	        "echo \"P$p $u.$g\nE$a$i\nd$m $d $y\n:$n\n\"\n"
	      "done\n"
	    ")\n"
	    "echo '### 200'\n",
	    remote_path, quoted_path, quoted_path);
    g_free (quoted_path);
    ent = vfs_s_generate_entry(me, NULL, dir, 0);
    while (1) {
	int res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), SUP.sockr); 
	if ((!res) || (res == EINTR)) {
	    vfs_s_free_entry(me, ent);
	    me->verrno = ECONNRESET;
	    goto error;
	}
	if (logfile) {
	    fputs (buffer, logfile);
            fputs ("\n", logfile);
	    fflush (logfile);
	}
	if (!strncmp(buffer, "### ", 4))
	    break;
	if ((!buffer[0])) {
	    if (ent->name) {
		vfs_s_insert_entry(me, dir, ent);
		ent = vfs_s_generate_entry(me, NULL, dir, 0);
	    }
	    continue;
	}
	
#define ST ent->ino->st

	switch(buffer[0]) {
	case ':': {
		      if (!strcmp(buffer+1, ".") || !strcmp(buffer+1, ".."))
			  break;  /* We'll do . and .. ourself */
		      ent->name = g_strdup(buffer+1); 
		      break;
	          }
	case 'S':
#ifdef HAVE_ATOLL
	    ST.st_size = (off_t) atoll (buffer+1);
#else
	    ST.st_size = (off_t) atof (buffer+1);
#endif
	    break;
	case 'P': {
	    size_t skipped;

	    if (vfs_parse_filemode (buffer + 1, &skipped, &ST.st_mode)) {
		if (S_ISLNK(ST.st_mode))
		    ST.st_mode = 0;
	    }
	    break;
	}
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
#ifdef HAVE_STRUCT_STAT_ST_RDEV
		      ST.st_rdev = makedev (maj, min);
#endif
	          }
	case 'L': ent->ino->linkname = g_strdup(buffer+1);
	          break;
	}
    }
    
    vfs_s_free_entry (me, ent);
    me->verrno = E_REMOTE;
    if (fish_decode_reply(buffer+4, 0) == COMPLETE) {
	g_free (SUP.cwdir);
	SUP.cwdir = g_strdup (remote_path);
	print_vfs_message (_("%s: done."), me->name);
	return 0;
    }

error:
    print_vfs_message (_("%s: failure"), me->name);
    return 1;
}

static int
fish_file_store(struct vfs_class *me, struct vfs_s_fh *fh, char *name, char *localname)
{
    struct vfs_s_super *super = FH_SUPER;
    int n, total;
    char buffer[8192];
    struct stat s;
    int was_error = 0;
    int h;
    char *quoted_name;

    h = open (localname, O_RDONLY);

    if (h == -1)
	ERRNOR (EIO, -1);
    if (fstat(h, &s)<0) {
	close (h);
	ERRNOR (EIO, -1);
    }

    /* First, try this as stor:
     *
     *     ( head -c number ) | ( cat > file; cat >/dev/null )
     *
     *  If `head' is not present on the remote system, `dd' will be used.
     * Unfortunately, we cannot trust most non-GNU `head' implementations
     * even if `-c' options is supported. Therefore, we separate GNU head
     * (and other modern heads?) using `-q' and `-' . This causes another
     * implementations to fail (because of "incorrect options").
     *
     *	Fallback is:
     *
     *	   rest=<number>
     *	   while [ $rest -gt 0 ]
     *	   do
     *	      cnt=`expr \( $rest + 255 \) / 256`
     *	      n=`dd bs=256 count=$cnt | tee -a <target_file> | wc -c`
     *	      rest=`expr $rest - $n`
     *	   done
     *
     *	`dd' was not designed for full filling of input buffers,
     *	and does not report exact number of bytes (not blocks).
     *	Therefore a more complex shell script is needed.
     *
     *	 On some systems non-GNU head writes "Usage:" error report to stdout
     *	instead of stderr. It makes impossible the use of "head || dd"
     *	algorithm for file appending case, therefore just "dd" is used for it.
     */

    print_vfs_message(_("fish: store %s: sending command..."), name );
    quoted_name = name_quote (name, 0);

    /* FIXME: File size is limited to ULONG_MAX */
    if (!fh->u.fish.append)
	n = fish_command (me, super, WAIT_REPLY,
		 "#STOR %lu /%s\n"
		 "echo '### 001'\n"
		 "file=/%s\n"
                 "res=`exec 3>&1\n"
		 "(\n"
		   "head -c %lu -q - || echo DD >&3\n"
		 ") 2>/dev/null | (\n"
		   "cat > \"$file\"\n"
		   "cat > /dev/null\n"
		 ")`; [ \"$res\" = DD ] && {\n"
			"> \"$file\"\n"
			"rest=%lu\n"
			"while [ $rest -gt 0 ]\n"
			"do\n"
			"    cnt=`expr \\( $rest + 255 \\) / 256`\n"
			"    n=`dd bs=256 count=$cnt | tee -a \"$file\" | wc -c`\n"
			"    rest=`expr $rest - $n`\n"
			"done\n"
		 "}; echo '### 200'\n",
		 (unsigned long) s.st_size, name,
		 quoted_name, (unsigned long) s.st_size,
		 (unsigned long) s.st_size);
    else
	n = fish_command (me, super, WAIT_REPLY,
		 "#STOR %lu /%s\n"
		 "echo '### 001'\n"
		 "{\n"
			"file=/%s\n"
			"rest=%lu\n"
			"while [ $rest -gt 0 ]\n"
			"do\n"
			"    cnt=`expr \\( $rest + 255 \\) / 256`\n"
			"    n=`dd bs=256 count=$cnt | tee -a \"$file\" | wc -c`\n"
			"    rest=`expr $rest - $n`\n"
			"done\n"
		 "}; echo '### 200'\n",
		 (unsigned long) s.st_size, name,
		 quoted_name, (unsigned long) s.st_size);

    g_free (quoted_name);
    if (n != PRELIM) {
	close (h);
        ERRNOR(E_REMOTE, -1);
    }
    total = 0;
    
    while (1) {
	int t;
	while ((n = read(h, buffer, sizeof(buffer))) < 0) {
	    if ((errno == EINTR) && got_interrupt())
	        continue;
	    print_vfs_message(_("fish: Local read failed, sending zeros") );
	    close(h);
	    h = open( "/dev/zero", O_RDONLY );
	}
	if (n == 0)
	    break;
    	if ((t = write (SUP.sockw, buffer, n)) != n) {
	    if (t == -1) {
		me->verrno = errno;
	    } else { 
		me->verrno = EIO;
	    }
	    goto error_return;
	}
	disable_interrupt_key();
	total += n;
	print_vfs_message(_("fish: storing %s %d (%lu)"), 
			  was_error ? _("zeros") : _("file"), total,
			  (unsigned long) s.st_size);
    }
    close(h);
    if ((fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE) || was_error)
        ERRNOR (E_REMOTE, -1);
    return 0;
error_return:
    close(h);
    fish_get_reply(me, SUP.sockr, NULL, 0);
    return -1;
}

static int
fish_linear_start (struct vfs_class *me, struct vfs_s_fh *fh, off_t offset)
{
    char *name;
    char *quoted_name;
    if (offset)
        ERRNOR (E_NOTSUPP, 0);
    name = vfs_s_fullpath (me, fh->ino);
    if (!name)
	return 0;
    quoted_name = name_quote (name, 0);
    g_free (name);
    name = quoted_name;
    fh->u.fish.append = 0;
    offset = fish_command (me, FH_SUPER, WANT_STRING,
		"#RETR /%s\n"
		"ls -ln /%s 2>/dev/null | (\n"
		  "read p l u g s r\n"
		  "echo \"$s\"\n"
		")\n"
		"echo '### 100'\n"
		"cat /%s\n"
		"echo '### 200'\n", 
		name, name, name );
    g_free (name);
    if (offset != PRELIM) ERRNOR (E_REMOTE, 0);
    fh->linear = LS_LINEAR_OPEN;
    fh->u.fish.got = 0;
#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64 || (defined _LARGE_FILES && _LARGE_FILES)
    if (sscanf( reply_str, "%llu", &fh->u.fish.total )!=1)
#else
    if (sscanf( reply_str, "%u", &fh->u.fish.total )!=1)
#endif
	ERRNOR (E_REMOTE, 0);
    return 1;
}

static void
fish_linear_abort (struct vfs_class *me, struct vfs_s_fh *fh)
{
    struct vfs_s_super *super = FH_SUPER;
    char buffer[8192];
    int n;

    print_vfs_message( _("Aborting transfer...") );
    do {
	n = MIN(8192, fh->u.fish.total - fh->u.fish.got);
	if (n) {
	    if ((n = read(SUP.sockr, buffer, n)) < 0)
	        return;
	    fh->u.fish.got += n;
	}
    } while (n);

    if (fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)
        print_vfs_message( _("Error reported after abort.") );
    else
        print_vfs_message( _("Aborted transfer would be successful.") );
}

static int
fish_linear_read (struct vfs_class *me, struct vfs_s_fh *fh, void *buf, int len)
{
    struct vfs_s_super *super = FH_SUPER;
    int n = 0;
    len = MIN( fh->u.fish.total - fh->u.fish.got, len );
    disable_interrupt_key();
    while (len && ((n = read (SUP.sockr, buf, len))<0)) {
        if ((errno == EINTR) && !got_interrupt())
	    continue;
	break;
    }
    enable_interrupt_key();

    if (n>0) fh->u.fish.got += n;
    if (n<0) fish_linear_abort(me, fh);
    if ((!n) && ((fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)))
        ERRNOR (E_REMOTE, -1);
    ERRNOR (errno, n);
}

static void
fish_linear_close (struct vfs_class *me, struct vfs_s_fh *fh)
{
    if (fh->u.fish.total != fh->u.fish.got)
	fish_linear_abort(me, fh);
}

static int
fish_ctl (void *fh, int ctlop, void *arg)
{
    (void) arg;
    return 0;
#if 0
    switch (ctlop) {
        case VFS_CTL_IS_NOTREADY:
	    {
	        int v;

		if (!FH->linear)
		    vfs_die ("You may not do this");
		if (FH->linear == LS_LINEAR_CLOSED || FH->linear == LS_LINEAR_PREOPEN)
		    return 0;

		v = vfs_s_select_on_two (FH_SUPER->u.fish.sockr, 0);
		if (((v < 0) && (errno == EINTR)) || v == 0)
		    return 1;
		return 0;
	    }
        default:
	    return 0;
    }
#endif
}

static int
fish_send_command(struct vfs_class *me, struct vfs_s_super *super, const char *cmd, int flags)
{
    int r;

    r = fish_command (me, super, WAIT_REPLY, "%s", cmd);
    vfs_stamp_create (&vfs_fish_ops, super);
    if (r != COMPLETE) ERRNOR (E_REMOTE, -1);
    if (flags & OPT_FLUSH)
	vfs_s_invalidate(me, super);
    return 0;
}

#define PREFIX \
    char buf[BUF_LARGE]; \
    const char *crpath; \
    char *rpath, *mpath = g_strdup (path); \
    struct vfs_s_super *super; \
    if (!(crpath = vfs_s_get_path_mangle (me, mpath, &super, 0))) { \
	g_free (mpath); \
	return -1; \
    } \
    rpath = name_quote (crpath, 0); \
    g_free (mpath);

#define POSTFIX(flags) \
    g_free (rpath); \
    return fish_send_command(me, super, buf, flags);

static int
fish_chmod (struct vfs_class *me, const char *path, int mode)
{
    PREFIX
    g_snprintf(buf, sizeof(buf), "#CHMOD %4.4o /%s\n"
				 "chmod %4.4o \"/%s\" 2>/dev/null\n"
				 "echo '### 000'\n", 
	    mode & 07777, rpath,
	    mode & 07777, rpath);
    POSTFIX(OPT_FLUSH);
}

#define FISH_OP(name, chk, string) \
static int fish_##name (struct vfs_class *me, const char *path1, const char *path2) \
{ \
    char buf[BUF_LARGE]; \
    const char *crpath1, *crpath2; \
    char *rpath1, *rpath2, *mpath1, *mpath2; \
    struct vfs_s_super *super1, *super2; \
    if (!(crpath1 = vfs_s_get_path_mangle (me, mpath1 = g_strdup(path1), &super1, 0))) { \
	g_free (mpath1); \
	return -1; \
    } \
    if (!(crpath2 = vfs_s_get_path_mangle (me, mpath2 = g_strdup(path2), &super2, 0))) { \
	g_free (mpath1); \
	g_free (mpath2); \
	return -1; \
    } \
    rpath1 = name_quote (crpath1, 0); \
    g_free (mpath1); \
    rpath2 = name_quote (crpath2, 0); \
    g_free (mpath2); \
    g_snprintf(buf, sizeof(buf), string "\n", rpath1, rpath2, rpath1, rpath2); \
    g_free (rpath1); \
    g_free (rpath2); \
    return fish_send_command(me, super2, buf, OPT_FLUSH); \
}

#define XTEST if (bucket1 != bucket2) { ERRNOR (EXDEV, -1); }
FISH_OP(rename, XTEST, "#RENAME /%s /%s\n"
		       "mv /%s /%s 2>/dev/null\n"
		       "echo '### 000'" )
FISH_OP(link,   XTEST, "#LINK /%s /%s\n"
		       "ln /%s /%s 2>/dev/null\n"
		       "echo '### 000'" )

static int fish_symlink (struct vfs_class *me, const char *setto, const char *path)
{
    char *qsetto;
    PREFIX
    qsetto = name_quote (setto, 0);
    g_snprintf(buf, sizeof(buf),
            "#SYMLINK %s /%s\n"
	    "ln -s %s /%s 2>/dev/null\n"
	    "echo '### 000'\n",
	    qsetto, rpath, qsetto, rpath);
    g_free (qsetto);
    POSTFIX(OPT_FLUSH);
}

static int
fish_chown (struct vfs_class *me, const char *path, int owner, int group)
{
    char *sowner, *sgroup;
    struct passwd *pw;
    struct group *gr;

    if ((pw = getpwuid (owner)) == NULL)
	return 0;

    if ((gr = getgrgid (group)) == NULL)
	return 0;

    sowner = pw->pw_name;
    sgroup = gr->gr_name;
    {
	PREFIX
	g_snprintf (buf, sizeof(buf),
    	    "#CHOWN /%s /%s\n"
	    "chown %s /%s 2>/dev/null\n"
	    "echo '### 000'\n", 
	    sowner, rpath,
	    sowner, rpath);
	fish_send_command (me, super, buf, OPT_FLUSH); 
	/* FIXME: what should we report if chgrp succeeds but chown fails? */
	g_snprintf (buf, sizeof(buf),
            "#CHGRP /%s /%s\n"
	    "chgrp %s /%s 2>/dev/null\n"
	    "echo '### 000'\n", 
	    sgroup, rpath,
	    sgroup, rpath);
	/* fish_send_command(me, super, buf, OPT_FLUSH); */
	POSTFIX (OPT_FLUSH)
    }
}

static int fish_unlink (struct vfs_class *me, const char *path)
{
    PREFIX
    g_snprintf(buf, sizeof(buf),
            "#DELE /%s\n"
	    "rm -f /%s 2>/dev/null\n"
	    "echo '### 000'\n",
	    rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int fish_mkdir (struct vfs_class *me, const char *path, mode_t mode)
{
    PREFIX

    (void) mode;

    g_snprintf(buf, sizeof(buf),
            "#MKD /%s\n"
	    "mkdir /%s 2>/dev/null\n"
	    "echo '### 000'\n",
	    rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int fish_rmdir (struct vfs_class *me, const char *path)
{
    PREFIX
    g_snprintf(buf, sizeof(buf),
            "#RMD /%s\n"
	    "rmdir /%s 2>/dev/null\n"
	    "echo '### 000'\n",
	    rpath, rpath);
    POSTFIX(OPT_FLUSH);
}

static int
fish_fh_open (struct vfs_class *me, struct vfs_s_fh *fh, int flags,
	      int mode)
{
    (void) mode;

    fh->u.fish.append = 0;
    /* File will be written only, so no need to retrieve it */
    if (((flags & O_WRONLY) == O_WRONLY) && !(flags & (O_RDONLY | O_RDWR))) {
	fh->u.fish.append = flags & O_APPEND;
	if (!fh->ino->localname) {
	    int tmp_handle =
		vfs_mkstemps (&fh->ino->localname, me->name,
			      fh->ino->ent->name);
	    if (tmp_handle == -1)
		return -1;
	    close (tmp_handle);
	}
	return 0;
    }
    if (!fh->ino->localname)
	if (vfs_s_retrieve_file (me, fh->ino) == -1)
	    return -1;
    if (!fh->ino->localname)
	vfs_die ("retrieve_file failed to fill in localname");
    return 0;
}

static void
fish_fill_names (struct vfs_class *me, fill_names_f func)
{
    struct vfs_s_super *super = MEDATA->supers;
    const char *flags;
    char *name;
    
    while (super){
	switch (SUP.flags & (FISH_FLAG_RSH | FISH_FLAG_COMPRESSED)) {
	case FISH_FLAG_RSH:
		flags = ":r";
		break;
	case FISH_FLAG_COMPRESSED:
		flags = ":C";
		break;
	case FISH_FLAG_RSH | FISH_FLAG_COMPRESSED:
		flags = "";
		break;
	default:
		flags = "";
		break;
	}

	name = g_strconcat ("/#sh:", SUP.user, "@", SUP.host, flags,
			    "/", SUP.cwdir, (char *) NULL);
	(*func)(name);
	g_free (name);
	super = super->next;
    }
}

void
init_fish (void)
{
    static struct vfs_s_subclass fish_subclass;

    fish_subclass.flags = VFS_S_REMOTE;
    fish_subclass.archive_same = fish_archive_same;
    fish_subclass.open_archive = fish_open_archive;
    fish_subclass.free_archive = fish_free_archive;
    fish_subclass.fh_open = fish_fh_open;
    fish_subclass.dir_load = fish_dir_load;
    fish_subclass.file_store = fish_file_store;
    fish_subclass.linear_start = fish_linear_start;
    fish_subclass.linear_read = fish_linear_read;
    fish_subclass.linear_close = fish_linear_close;

    vfs_s_init_class (&vfs_fish_ops, &fish_subclass);
    vfs_fish_ops.name = "fish";
    vfs_fish_ops.prefix = "sh:";
    vfs_fish_ops.fill_names = fish_fill_names;
    vfs_fish_ops.chmod = fish_chmod;
    vfs_fish_ops.chown = fish_chown;
    vfs_fish_ops.symlink = fish_symlink;
    vfs_fish_ops.link = fish_link;
    vfs_fish_ops.unlink = fish_unlink;
    vfs_fish_ops.rename = fish_rename;
    vfs_fish_ops.mkdir = fish_mkdir;
    vfs_fish_ops.rmdir = fish_rmdir;
    vfs_fish_ops.ctl = fish_ctl;
    vfs_register_class (&vfs_fish_ops);
}
