/* Various utilities - Unix variants
   Copyright (C) 1994, 1995, 1996 the Free Software Foundation.
   Written 1994, 1995, 1996 by:
   Miguel de Icaza, Janne Kukonlehto, Dugan Porter,
   Jakub Jelinek, Mauricio Plaza.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <signal.h>		/* struct sigaction */
#include <limits.h>		/* INT_MAX */
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>		/* errno */
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#endif

#include "global.h"
#include "fsusage.h"
#include "mountlist.h"
#include "wtools.h"		/* message() */

struct sigaction startup_handler;

/* uid of the MC user */
static uid_t current_user_uid = -1;
/* List of the gids of the user */
static GTree *current_user_gid = NULL;

/* Helper function to compare 2 gids */
static gint
mc_gid_compare (gconstpointer v, gconstpointer v2)
{
    return ((GPOINTER_TO_UINT(v) > GPOINTER_TO_UINT(v2)) ? 1 :
	    (GPOINTER_TO_UINT(v) < GPOINTER_TO_UINT(v2)) ? -1 : 0);
}

/* Helper function to delete keys of the gids tree */
static gint
mc_gid_destroy (gpointer key, gpointer value, gpointer data)
{
    g_free (value);
    
    return FALSE;
}

/* This function initialize global GTree with the gids of groups,
   to which user belongs. Tree also store corresponding string 
   with the name of the group.
   FIXME: Do we need this names at all? If not, we can simplify
   initialization by eliminating g_strdup's.
*/
void init_groups (void)
{
    int i;
    struct passwd *pwd;
    struct group *grp;

    current_user_uid = getuid ();
    pwd = getpwuid (current_user_uid);
    g_return_if_fail (pwd != NULL);

    current_user_gid = g_tree_new (mc_gid_compare);

    /* Put user's primary group first. */
    if ((grp = getgrgid (pwd->pw_gid)) != NULL) {
	g_tree_insert (current_user_gid,
		       GUINT_TO_POINTER ((int) grp->gr_gid),
		       g_strdup (grp->gr_name));
    }

    setgrent ();
    while ((grp = getgrent ()) != NULL) {
	for (i = 0; grp->gr_mem[i]; i++) {
	    if (!strcmp (pwd->pw_name, grp->gr_mem[i]) &&
		!g_tree_lookup (current_user_gid,
				GUINT_TO_POINTER ((int) grp->gr_gid))) {
		g_tree_insert (current_user_gid,
			       GUINT_TO_POINTER ((int) grp->gr_gid),
			       g_strdup (grp->gr_name));
		break;
	    }
	}
    }
    endgrent ();
}

/* Return the index of the permissions triplet */
int
get_user_permissions (struct stat *buf) {

    if (buf->st_uid == current_user_uid || current_user_uid == 0)
       return 0;
    
    if (current_user_gid && g_tree_lookup (current_user_gid,
					   GUINT_TO_POINTER((int) buf->st_gid)))
       return 1;

    return 2;
}

/* Completely destroys the gids tree */
void
destroy_groups (void)
{
    g_tree_traverse (current_user_gid, mc_gid_destroy, G_POST_ORDER, NULL);
    g_tree_destroy (current_user_gid);
}

#define UID_CACHE_SIZE 200
#define GID_CACHE_SIZE 30

typedef struct {
    int  index;
    char *string;
} int_cache;

static int_cache uid_cache [UID_CACHE_SIZE];
static int_cache gid_cache [GID_CACHE_SIZE];

static char *i_cache_match (int id, int_cache *cache, int size)
{
    int i;

    for (i = 0; i < size; i++)
	if (cache [i].index == id)
	    return cache [i].string;
    return 0;
}

static void i_cache_add (int id, int_cache *cache, int size, char *text,
			 int *last)
{
    if (cache [*last].string)
	g_free (cache [*last].string);
    cache [*last].string = g_strdup (text);
    cache [*last].index = id;
    *last = ((*last)+1) % size;
}

char *get_owner (int uid)
{
    struct passwd *pwd;
    static char ibuf [10];
    char   *name;
    static int uid_last;
    
    if ((name = i_cache_match (uid, uid_cache, UID_CACHE_SIZE)) != NULL)
	return name;
    
    pwd = getpwuid (uid);
    if (pwd){
	i_cache_add (uid, uid_cache, UID_CACHE_SIZE, pwd->pw_name, &uid_last);
	return pwd->pw_name;
    }
    else {
	g_snprintf (ibuf, sizeof (ibuf), "%d", uid);
	return ibuf;
    }
}

char *get_group (int gid)
{
    struct group *grp;
    static char gbuf [10];
    char *name;
    static int  gid_last;
    
    if ((name = i_cache_match (gid, gid_cache, GID_CACHE_SIZE)) != NULL)
	return name;
    
    grp = getgrgid (gid);
    if (grp){
	i_cache_add (gid, gid_cache, GID_CACHE_SIZE, grp->gr_name, &gid_last);
	return grp->gr_name;
    } else {
	g_snprintf (gbuf, sizeof (gbuf), "%d", gid);
	return gbuf;
    }
}

/* Since ncurses uses a handler that automatically refreshes the */
/* screen after a SIGCONT, and we don't want this behavior when */
/* spawning a child, we save the original handler here */
void save_stop_handler (void)
{
    sigaction (SIGTSTP, NULL, &startup_handler);
}

int my_system (int flags, const char *shell, const char *command)
{
    struct sigaction ignore, save_intr, save_quit, save_stop;
    pid_t pid;
    int status = 0;
    
    ignore.sa_handler = SIG_IGN;
    sigemptyset (&ignore.sa_mask);
    ignore.sa_flags = 0;
    
    sigaction (SIGINT, &ignore, &save_intr);    
    sigaction (SIGQUIT, &ignore, &save_quit);

    /* Restore the original SIGTSTP handler, we don't want ncurses' */
    /* handler messing the screen after the SIGCONT */
    sigaction (SIGTSTP, &startup_handler, &save_stop);

    if ((pid = fork ()) < 0){
	fprintf (stderr, "\n\nfork () = -1\n");
	return -1;
    }
    if (pid == 0){
	signal (SIGINT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGTSTP, SIG_DFL);
	signal (SIGCHLD, SIG_DFL);

	if (flags & EXECUTE_AS_SHELL)
	    execl (shell, shell, "-c", command, NULL);
	else
	    execlp (shell, shell, command, NULL);

	_exit (127);		/* Exec error */
    } else {
	while (waitpid (pid, &status, 0) < 0)
	    if (errno != EINTR){
		status = -1;
		break;
	    }
    }
    sigaction (SIGINT,  &save_intr, NULL);
    sigaction (SIGQUIT, &save_quit, NULL);
    sigaction (SIGTSTP, &save_stop, NULL);

#ifdef SCO_FLAVOR 
	waitpid(-1, NULL, WNOHANG);
#endif /* SCO_FLAVOR */

    return WEXITSTATUS(status);
}

/* Returns a newly allocated string, if directory does not exist, return 0 */
char *tilde_expand (const char *directory)
{
    struct passwd *passwd;
    const char *p;
    char *name;
    
    if (*directory != '~')
	return g_strdup (directory);

    directory++;
    
    p = strchr (directory, PATH_SEP);
    
    /* d = "~" or d = "~/" */
    if (!(*directory) || (*directory == PATH_SEP)){
	passwd = getpwuid (geteuid ());
	p = (*directory == PATH_SEP) ? directory+1 : "";
    } else {
	if (!p){
	    passwd = getpwnam (directory);
	} else {
	    name = g_malloc (p - directory + 1);
	    strncpy (name, directory, p - directory);
	    name [p - directory] = 0;
	    passwd = getpwnam (name);
	    g_free (name);
	}
    }

    /* If we can't figure the user name, return NULL */
    if (!passwd)
	return 0;

    return g_strconcat (passwd->pw_dir, PATH_SEP_STR, p, NULL);
}


/*
 * Return the directory where mc should keep its temporary files.
 * This directory is (in Bourne shell terms) "${TMPDIR=/tmp}-$USER"
 * When called the first time, the directory is created if needed.
 * The first call should be done early, since we are using fprintf()
 * and not message() to report possible problems.
 */
const char *
mc_tmpdir (void)
{
    static char buffer[64];
    static const char *tmpdir;
    const char *sys_tmp;
    struct passwd *pwd;
    struct stat st;
    const char *error = NULL;

    /* Check if already initialized */
    if (tmpdir)
	return tmpdir;

    sys_tmp = getenv ("TMPDIR");
    if (!sys_tmp) {
	sys_tmp = TMPDIR_DEFAULT;
    }

    pwd = getpwuid (getuid ());
    g_snprintf (buffer, sizeof (buffer), "%s/mc-%s", sys_tmp,
		pwd->pw_name);
    canonicalize_pathname (buffer);

    if (lstat (buffer, &st) == 0) {
	/* Sanity check for existing directory */
	if (!S_ISDIR (st.st_mode))
	    error = _("%s is not a directory\n");
	else if (st.st_uid != getuid ())
	    error = _("Directory %s is not owned by you\n");
	else if (((st.st_mode & 0777) != 0700)
		 && (chmod (buffer, 0700) != 0))
	    error = _("Cannot set correct permissions for directory %s\n");
    } else {
	/* Need to create directory */
	if (mkdir (buffer, S_IRWXU) != 0) {
	    fprintf (stderr,
		     _("Cannot create temporary directory %s: %s\n"),
		     buffer, unix_error_string (errno));
	    error = "";
	}
    }

    if (!error) {
	tmpdir = buffer;
    } else {
	int test_fd;
	char *test_fn;
	int fallback_ok = 0;

	if (*error)
	    fprintf (stderr, error, buffer);

	/* Test if sys_tmp is suitable for temporary files */
	tmpdir = sys_tmp;
	test_fd = mc_mkstemps (&test_fn, "mctest", NULL);
	if (test_fd != -1) {
	    close (test_fd);
	    test_fd = open (test_fn, O_RDONLY);
	    if (test_fd != -1) {
		close (test_fd);
		unlink (test_fn);
		fallback_ok = 1;
	    }
	}

	if (fallback_ok) {
	    fprintf (stderr, _("Temporary files will be created in %s\n"),
		     sys_tmp);
	} else {
	    fprintf (stderr, _("Temporary files will not be created\n"));
	    tmpdir = "/dev/null/";
	}

	fprintf (stderr, "%s\n", _("Press any key to continue..."));
	getc (stdin);
    }

    return tmpdir;
}


/* Pipes are guaranteed to be able to hold at least 4096 bytes */
/* More than that would be unportable */
#define MAX_PIPE_SIZE 4096

static int error_pipe[2];	/* File descriptors of error pipe */
static int old_error;		/* File descriptor of old standard error */

/* Creates a pipe to hold standard error for a later analysis. */
/* The pipe can hold 4096 bytes. Make sure no more is written */
/* or a deadlock might occur. */
void open_error_pipe (void)
{
    if (pipe (error_pipe) < 0){
	message (0, _("Warning"), _(" Pipe failed "));
    }
    old_error = dup (2);
    if(old_error < 0 || close(2) || dup (error_pipe[1]) != 2){
	message (0, _("Warning"), _(" Dup failed "));
	close (error_pipe[0]);
	close (error_pipe[1]);
    }
    close (error_pipe[1]);
}

/*
 * Returns true if an error was displayed
 * error: -1 - ignore errors, 0 - display warning, 1 - display error
 * text is prepended to the error message from the pipe
 */
int
close_error_pipe (int error, char *text)
{
    char *title;
    char msg[MAX_PIPE_SIZE];
    int len = 0;

    if (error)
	title = MSG_ERROR;
    else
	title = _("Warning");
    if (old_error >= 0){
	close (2);
	dup (old_error);
	close (old_error);
	len = read (error_pipe[0], msg, MAX_PIPE_SIZE);

	if (len >= 0)
	    msg[len] = 0;
	close (error_pipe[0]);
    }
    if (error < 0)
	return 0;	/* Just ignore error message */
    if (text == NULL){
	if (len <= 0)
	    return 0;	/* Nothing to show */

	/* Show message from pipe */
	message (error, title, "%s", msg);
    } else {
	/* Show given text and possible message from pipe */
	message (error, title, " %s \n %s ", text, msg);
    }
    return 1;
}

/* Checks for messages in the error pipe,
 * closes the pipe and displays an error box if needed
 */
void check_error_pipe (void)
{
    char error[MAX_PIPE_SIZE];
    int len = 0;
    if (old_error >= 0){
	while (len < MAX_PIPE_SIZE)
	{
            fd_set select_set;
            struct timeval timeout;
            FD_ZERO (&select_set);
            FD_SET (error_pipe[0], &select_set);
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            select (error_pipe[0] + 1, &select_set, 0, 0, &timeout);
            if (!FD_ISSET (error_pipe[0], &select_set))
		break;
	    read (error_pipe[0], error + len, 1);
	    len ++;
	}
	error[len] = 0;
	close (error_pipe[0]);
    }
    if (len > 0)
        message (0, _("Warning"), "%s", error);
}

static struct sigaction ignore, save_intr, save_quit, save_stop;

/* INHANDLE is a result of some mc_open call to any vfs, this function
   returns a normal handle (to be used with read) of a pipe for reading
   of the output of COMMAND with arguments ... (must include argv[0] as
   well) which gets as its input at most INLEN bytes from the INHANDLE
   using mc_read. You have to call mc_doublepclose to close the returned
   handle afterwards. If INLEN is -1, we read as much as we can :) */
int mc_doublepopen (int inhandle, int inlen, pid_t *the_pid, char *command, ...)
{
    int pipe0 [2], pipe1 [2];
    pid_t pid;

#define closepipes() close(pipe0[0]);close(pipe0[1]);close(pipe1[0]);close(pipe1[1])
    
    pipe (pipe0); pipe (pipe1);
    ignore.sa_handler = SIG_IGN;
    sigemptyset (&ignore.sa_mask);
    ignore.sa_flags = 0;
        
    sigaction (SIGINT, &ignore, &save_intr);
    sigaction (SIGQUIT, &ignore, &save_quit);
    sigaction (SIGTSTP, &startup_handler, &save_stop);

    switch (pid = fork ()) {
    case -1:
            closepipes ();
	    return -1;
    case 0: {
	    sigaction (SIGINT, &save_intr, NULL);
	    sigaction (SIGQUIT, &save_quit, NULL);
	    switch (pid = fork ()) {
	    	case -1:
	    	    closepipes ();
	    	    _exit (1);
	    	case 0: {
#define MAXARGS 16
		     int argno;
		     char *args[MAXARGS];
		     va_list ap;
		     int nulldevice;

		     nulldevice = open ("/dev/null", O_WRONLY);
		     close (0);
		     dup (pipe0 [0]);
		     close (1);
		     dup (pipe1 [1]);
		     close (2);
		     dup (nulldevice);
		     close (nulldevice);
		     closepipes ();
		     va_start (ap, command);
		     argno = 0;
		     while ((args[argno++] = va_arg(ap, char *)) != NULL)
		         if (argno == (MAXARGS - 1)) {
			     args[argno] = NULL;
			     break;
		         }
		     va_end (ap);
		     execvp (command, args);

		     /* If we are here exec has failed */
		     _exit (0);
		}
	    	default:
	    	    {
	    	    	char buffer [8192];
	    	    	int i;

	    	    	close (pipe0 [0]);
	    	    	close (pipe1 [0]);
	    	    	close (pipe1 [1]);
	    	    	while ((i = mc_read (inhandle, buffer,
                                             (inlen == -1 || inlen > 8192) 
                                             ? 8192 : inlen)) > 0) {
	    	    	    write (pipe0 [1], buffer, i);
	    	    	    if (inlen != -1) {
			        inlen -= i;
			        if (!inlen)
				    break;
			    }
	    	    	}
			close (inhandle);
	    	    	close (pipe0 [1]);
	   		while (waitpid (pid, &i, 0) < 0)
			    if (errno != EINTR)
				break;

	   		_exit (i);
	    	    }
	    }
    }
    default:
	*the_pid = pid;
	break;
    }
    close (pipe0 [0]);
    close (pipe0 [1]);
    close (pipe1 [1]);
    return pipe1 [0];
}

int mc_doublepclose (int pipe, pid_t pid)
{
    int status = 0;
    
    close (pipe);
    waitpid (pid, &status, 0);
#ifdef SCO_FLAVOR
    waitpid (-1, NULL, WNOHANG);
#endif /* SCO_FLAVOR */
    sigaction (SIGINT, &save_intr, NULL);
    sigaction (SIGQUIT, &save_quit, NULL);
    sigaction (SIGTSTP, &save_stop, NULL);

    return status;	
}

/* Canonicalize path, and return a new path. Do everything in situ.
   The new path differs from path in:
	Multiple `/'s are collapsed to a single `/'.
	Leading `./'s and trailing `/.'s are removed.
	Trailing `/'s are removed.
	Non-leading `../'s and trailing `..'s are handled by removing
	portions of the path. */
char *canonicalize_pathname (char *path)
{
    int i, start;
    char stub_char;

    if (!*path)
	return path;

    stub_char = (*path == PATH_SEP) ? PATH_SEP : '.';

    /* Walk along path looking for things to compact. */
    i = 0;
    while (path[i]) {

	while (path[i] && path[i] != PATH_SEP)
	    i++;

	start = i++;

	/* If we didn't find any slashes, then there is nothing left to do. */
	if (!path[start])
	    break;

#if defined(__QNX__) && !defined(__QNXNTO__)
	/*
	   ** QNX accesses the directories of nodes on its peer-to-peer
	   ** network (Qnet) by prefixing their directories with "//[nid]".
	   ** We don't want to collapse two '/'s if they're at the start
	   ** of the path, followed by digits, and ending with a '/'.
	 */
	if (start == 0 && path[1] == PATH_SEP) {
	    char *p = path + 2;
	    char *q = strchr (p, PATH_SEP);

	    if (q > p) {
		*q = 0;
		if (!strcspn (p, "0123456789")) {
		    start = q - path;
		    i = start + 1;
		}
		*q = PATH_SEP;
	    }
	}
#endif

	/* Handle multiple `/'s in a row. */
	while (path[i] == PATH_SEP)
	    i++;

	if ((start + 1) != i) {
	    strcpy (path + start + 1, path + i);
	    i = start + 1;
	}

	/* Check for trailing `/'. */
	if (start && !path[i]) {
	  zero_last:
	    path[--i] = '\0';
	    break;
	}

	/* Check for `../', `./' or trailing `.' by itself. */
	if (path[i] == '.') {
	    /* Handle trailing `.' by itself. */
	    if (!path[i + 1])
		goto zero_last;

	    /* Handle `./'. */
	    if (path[i + 1] == PATH_SEP) {
		strcpy (path + i, path + i + 1);
		i = start;
		continue;
	    }

	    /* Handle `../' or trailing `..' by itself. 
	       Remove the previous ?/ part with the exception of
	       ../, which we should leave intact. */
	    if (path[i + 1] == '.'
		&& (path[i + 2] == PATH_SEP || !path[i + 2])) {
		while (--start > -1 && path[start] != PATH_SEP);
		if (!strncmp (path + start + 1, "../", 3))
		    continue;
		strcpy (path + start + 1, path + i + 2);
		i = start;
		continue;
	    }
	}
    }

    if (!*path) {
	*path = stub_char;
	path[1] = '\0';
    }
    return path;
}

#ifdef SCO_FLAVOR
int gettimeofday( struct timeval * tv, struct timezone * tz)
{
    struct timeb tb;
    struct tm * l;
    
    ftime( &tb );
    if (errno == EFAULT)
	return -1;
    l = localtime(&tb.time);
    tv->tv_sec = l->tm_sec;
    tv->tv_usec = (long) tb.millitm;
    return 0;
}
#endif /* SCO_FLAVOR */

#ifdef HAVE_GET_PROCESS_STATS
#    include <sys/procstats.h>

int gettimeofday (struct timeval *tp, void *tzp)
{
  return get_process_stats(tp, PS_SELF, 0, 0);
}
#endif /* HAVE_GET_PROCESS_STATS */

#ifndef HAVE_PUTENV

/* The following piece of code was copied from the GNU C Library */
/* And is provided here for nextstep who lacks putenv */

extern char **environ;

#ifndef	HAVE_GNU_LD
#define	__environ	environ
#endif


/* Put STRING, which is of the form "NAME=VALUE", in the environment.  */
int
putenv (const char *string)
{
    const char *const name_end = strchr (string, '=');
    register size_t size;
    register char **ep;
    
    if (name_end == NULL){
	/* Remove the variable from the environment.  */
	size = strlen (string);
	for (ep = __environ; *ep != NULL; ++ep)
	    if (!strncmp (*ep, string, size) && (*ep)[size] == '='){
		while (ep[1] != NULL){
		    ep[0] = ep[1];
		    ++ep;
		}
		*ep = NULL;
		return 0;
	    }
    }
    
    size = 0;
    for (ep = __environ; *ep != NULL; ++ep)
	if (!strncmp (*ep, string, name_end - string) &&
	    (*ep)[name_end - string] == '=')
	    break;
	else
	    ++size;
    
    if (*ep == NULL){
	static char **last_environ = NULL;
	char **new_environ = g_new (char *, size + 2);
	if (new_environ == NULL)
	    return -1;
	(void) memcpy ((void *) new_environ, (void *) __environ,
		       size * sizeof (char *));
	new_environ[size] = (char *) string;
	new_environ[size + 1] = NULL;
	if (last_environ != NULL)
	    g_free ((void *) last_environ);
	last_environ = new_environ;
	__environ = new_environ;
    }
    else
	*ep = (char *) string;
    
    return 0;
}
#endif /* !HAVE_PUTENV */

#ifdef SCO_FLAVOR
/* Define this only for SCO */
#ifdef USE_NETCODE
#ifndef HAVE_SOCKETPAIR

/*
 The code for s_pipe function is adapted from Section 7.9
 of the "UNIX Network Programming" by W. Richard Stevens,
 published by Prentice Hall, ISBN 0-13-949876-1
 (c) 1990 by P T R Prentice Hall

 It is used to implement socketpair function for SVR3 systems
 that lack it.
*/

#include <sys/types.h>
#include <sys/stream.h>  /* defines queue_t */
#include <stropts.h>     /* defines struct strtdinsert */

#define SPX_DEVICE "/dev/spx"
#define S_PIPE_HANDLE_ERRNO 1
/* if the above is defined to 1, we will attempt to
   save and restore errno to indicate failure
   reason to the caller;
   Please note that this will not work in environments
   where errno is not just an integer
*/

#if S_PIPE_HANDLE_ERRNO
#include <errno.h>
/* This is for "extern int errno;" */
#endif

 /* s_pipe returns 0 if OK, -1 on error */
 /* two file descriptors are returned   */
static int s_pipe(int fd[2])
{
   struct strfdinsert  ins;  /* stream I_FDINSERT ioctl format */
   queue_t             *pointer;
   #if S_PIPE_HANDLE_ERRNO
   int err_save;
   #endif
   /*
    * First open the stream clone device "dev/spx" twice,
    * obtaining the two file descriptors
    */

   if ( (fd[0] = open(SPX_DEVICE, O_RDWR)) < 0)
      return -1;
   if ( (fd[1] = open(SPX_DEVICE, O_RDWR)) < 0) {
      #if S_PIPE_HANDLE_ERRNO
      err_save = errno;
      #endif
      close(fd[0]);
      #if S_PIPE_HANDLE_ERRNO
      errno = err_save;
      #endif
      return -1;
   }
   
   /*
    * Now link these two stream together with an I_FDINSERT ioctl.
    */
   
   ins.ctlbuf.buf     = (char *) &pointer;   /* no control information, just the pointer */
   ins.ctlbuf.maxlen  = sizeof pointer;
   ins.ctlbuf.len     = sizeof pointer;
   ins.databuf.buf    = (char *) 0;   /* no data to be sent */
   ins.databuf.maxlen = 0;
   ins.databuf.len    = -1;  /* magic: must be -1 rather than 0 for stream pipes */

   ins.fildes = fd[1];  /* the fd to connect with fd[0] */
   ins.flags  = 0;      /* nonpriority message */
   ins.offset = 0;      /* offset of pointer in control buffer */

   if (ioctl(fd[0], I_FDINSERT, (char *) &ins) < 0) {
      #if S_PIPE_HANDLE_ERRNO
      err_save = errno;
      #endif
      close(fd[0]);
      close(fd[1]);
      #if S_PIPE_HANDLE_ERRNO
      errno = err_save;
      #endif
      return -1;
   }
   /* all is OK if we came here, indicate success */
   return 0;
}

int socketpair(int dummy1, int dummy2, int dummy3, int fd[2])
{
   return s_pipe(fd);
}

#endif /* ifndef HAVE_SOCKETPAIR */
#endif /* ifdef USE_NETCODE */
#endif /* SCO_FLAVOR */
