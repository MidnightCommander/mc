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
#include "execute.h"
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

    return WEXITSTATUS(status);
}


/*
 * Perform tilde expansion if possible.
 * Always return a newly allocated string, even if it's unchanged.
 */
char *
tilde_expand (const char *directory)
{
    struct passwd *passwd;
    const char *p, *q;
    char *name;

    if (*directory != '~')
	return g_strdup (directory);

    p = directory + 1;

    q = strchr (p, PATH_SEP);

    /* d = "~" or d = "~/" */
    if (!(*p) || (*p == PATH_SEP)) {
	passwd = getpwuid (geteuid ());
	q = (*p == PATH_SEP) ? p + 1 : "";
    } else {
	if (!q) {
	    passwd = getpwnam (p);
	} else {
	    name = g_malloc (q - p + 1);
	    strncpy (name, p, q - p);
	    name[q - p] = 0;
	    passwd = getpwnam (name);
	    g_free (name);
	}
    }

    /* If we can't figure the user name, leave tilde unexpanded */
    if (!passwd)
	return g_strdup (directory);

    return g_strconcat (passwd->pw_dir, PATH_SEP_STR, q, NULL);
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

    if (pwd)
	g_snprintf (buffer, sizeof (buffer), "%s/mc-%s", sys_tmp,
		pwd->pw_name);
    else
	g_snprintf (buffer, sizeof (buffer), "%s/mc-%d", sys_tmp,
		getuid ());

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

/*
 * Canonicalize path, and return a new path.  Do everything in place.
 * The new path differs from path in:
 *	Multiple `/'s are collapsed to a single `/'.
 *	Leading `./'s and trailing `/.'s are removed.
 *	Trailing `/'s are removed.
 *	Non-leading `../'s and trailing `..'s are handled by removing
 *	portions of the path.
 * Well formed UNC paths are modified only in the local part.
 */
void
canonicalize_pathname (char *path)
{
    char *p, *s;
    int len;
    char *lpath = path;	/* path without leading UNC part */

    /* Detect and preserve UNC paths: //server/... */
    if (path[0] == PATH_SEP && path[1] == PATH_SEP) {
	p = path + 2;
	while (p[0] && p[0] != '/')
	    p++;
	if (p[0] == '/' && p > path + 2)
	    lpath = p;
    }

    if (!lpath[0] || !lpath[1])
	return;

    /* Collapse multiple slashes */
    p = lpath;
    while (*p) {
	if (p[0] == PATH_SEP && p[1] == PATH_SEP) {
	    s = p + 1;
	    while (*(++s) == PATH_SEP);
	    strcpy (p + 1, s);
	}
	p++;
    }

    /* Collapse "/./" -> "/" */
    p = lpath;
    while (*p) {
	if (p[0] == PATH_SEP && p[1] == '.' && p[2] == PATH_SEP)
	    strcpy (p, p + 2);
	else
	    p++;
    }

    /* Remove trailing slashes */
    p = lpath + strlen (lpath) - 1;
    while (p > lpath && *p == PATH_SEP)
	*p-- = 0;

    /* Remove leading "./" */
    if (lpath[0] == '.' && lpath[1] == PATH_SEP) {
	if (lpath[2] == 0) {
	    lpath[1] = 0;
	    return;
	} else {
	    strcpy (lpath, lpath + 2);
	}
    }

    /* Remove trailing "/" or "/." */
    len = strlen (lpath);
    if (len < 2)
	return;
    if (lpath[len - 1] == PATH_SEP) {
	lpath[len - 1] = 0;
    } else {
	if (lpath[len - 1] == '.' && lpath[len - 2] == PATH_SEP) {
	    if (len == 2) {
		lpath[1] = 0;
		return;
	    } else {
		lpath[len - 2] = 0;
	    }
	}
    }

    /* Collapse "/.." with the previous part of path */
    p = lpath;
    while (p[0] && p[1] && p[2]) {
	if ((p[0] != PATH_SEP || p[1] != '.' || p[2] != '.')
	    || (p[3] != PATH_SEP && p[3] != 0)) {
	    p++;
	    continue;
	}

	/* search for the previous token */
	s = p - 1;
	while (s >= lpath && *s != PATH_SEP)
	    s--;

	s++;

	/* If the previous token is "..", we cannot collapse it */
	if (s[0] == '.' && s[1] == '.' && s + 2 == p) {
	    p += 3;
	    continue;
	}

	if (p[3] != 0) {
	    if (s == lpath && *s == PATH_SEP) {
		/* "/../foo" -> "/foo" */
		strcpy (s + 1, p + 4);
	    } else {
		/* "token/../foo" -> "foo" */
		strcpy (s, p + 4);
	    }
	    p = (s > lpath) ? s - 1 : s;
	    continue;
	}

	/* trailing ".." */
	if (s == lpath) {
	    /* "token/.." -> "." */
	    if (lpath[0] != PATH_SEP) {
		lpath[0] = '.';
	    }
	    lpath[1] = 0;
	} else {
	    /* "foo/token/.." -> "foo" */
	    if (s == lpath + 1)
		s[0] = 0;
	    else
		s[-1] = 0;
	    break;
	}

	break;
    }
}

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

