/*
   Various utilities - Unix variants

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2007, 2011, 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1996
   Janne Kukonlehto, 1994, 1995, 1996
   Dugan Porter, 1994, 1995, 1996
   Jakub Jelinek, 1994, 1995, 1996
   Mauricio Plaza, 1994, 1995, 1996

   The mc_realpath routine is mostly from uClibc package, written
   by Rick Sladkey <jrs@world.std.com>

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file utilunix.c
 *  \brief Source: various utilities - Unix variant
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_GET_PROCESS_STATS
#include <sys/procstats.h>
#endif
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"        /* VFS_ENCODING_PREFIX */
#include "lib/strutil.h"        /* str_move() */
#include "lib/util.h"
#include "lib/widget.h"         /* message() */
#include "lib/vfs/xdirentry.h"

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "utilunix.h"

/*** global variables ****************************************************************************/

struct sigaction startup_handler;

/*** file scope macro definitions ****************************************************************/

#define UID_CACHE_SIZE 200
#define GID_CACHE_SIZE 30

/* Pipes are guaranteed to be able to hold at least 4096 bytes */
/* More than that would be unportable */
#define MAX_PIPE_SIZE 4096

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int index;
    char *string;
} int_cache;

typedef enum
{
    FORK_ERROR = -1,
    FORK_CHILD,
    FORK_PARENT,
} my_fork_state_t;

typedef struct
{
    struct sigaction intr;
    struct sigaction quit;
    struct sigaction stop;
} my_system_sigactions_t;

/*** file scope variables ************************************************************************/

static int_cache uid_cache[UID_CACHE_SIZE];
static int_cache gid_cache[GID_CACHE_SIZE];

static int error_pipe[2];       /* File descriptors of error pipe */
static int old_error;           /* File descriptor of old standard error */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
i_cache_match (int id, int_cache * cache, int size)
{
    int i;

    for (i = 0; i < size; i++)
        if (cache[i].index == id)
            return cache[i].string;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
i_cache_add (int id, int_cache * cache, int size, char *text, int *last)
{
    g_free (cache[*last].string);
    cache[*last].string = g_strdup (text);
    cache[*last].index = id;
    *last = ((*last) + 1) % size;
}

/* --------------------------------------------------------------------------------------------- */

static my_fork_state_t
my_fork (void)
{
    pid_t pid;

    pid = fork ();

    if (pid < 0)
    {
        fprintf (stderr, "\n\nfork () = -1\n");
        return FORK_ERROR;
    }

    if (pid == 0)
        return FORK_CHILD;

    while (TRUE)
    {
        int status = 0;

        if (waitpid (pid, &status, 0) > 0)
            return WEXITSTATUS (status) == 0 ? FORK_PARENT : FORK_ERROR;

        if (errno != EINTR)
            return FORK_ERROR;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
my_system__save_sigaction_handlers (my_system_sigactions_t * sigactions)
{
    struct sigaction ignore;

    ignore.sa_handler = SIG_IGN;
    sigemptyset (&ignore.sa_mask);
    ignore.sa_flags = 0;

    sigaction (SIGINT, &ignore, &sigactions->intr);
    sigaction (SIGQUIT, &ignore, &sigactions->quit);

    /* Restore the original SIGTSTP handler, we don't want ncurses' */
    /* handler messing the screen after the SIGCONT */
    sigaction (SIGTSTP, &startup_handler, &sigactions->stop);
}

/* --------------------------------------------------------------------------------------------- */

static void
my_system__restore_sigaction_handlers (my_system_sigactions_t * sigactions)
{
    sigaction (SIGINT, &sigactions->intr, NULL);
    sigaction (SIGQUIT, &sigactions->quit, NULL);
    sigaction (SIGTSTP, &sigactions->stop, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
my_system_make_arg_array (int flags, const char *shell, char **execute_name)
{
    GPtrArray *args_array;

    args_array = g_ptr_array_new ();

    if ((flags & EXECUTE_AS_SHELL) != 0)
    {
        g_ptr_array_add (args_array, g_strdup (shell));
        g_ptr_array_add (args_array, g_strdup ("-c"));
        *execute_name = g_strdup (shell);
    }
    else
    {
        char *shell_token;

        shell_token = shell != NULL ? strchr (shell, ' ') : NULL;
        if (shell_token == NULL)
            *execute_name = g_strdup (shell);
        else
            *execute_name = g_strndup (shell, (gsize) (shell_token - shell));

        g_ptr_array_add (args_array, g_strdup (shell));
    }
    return args_array;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
get_owner (int uid)
{
    struct passwd *pwd;
    static char ibuf[10];
    char *name;
    static int uid_last;

    name = i_cache_match (uid, uid_cache, UID_CACHE_SIZE);
    if (name != NULL)
        return name;

    pwd = getpwuid (uid);
    if (pwd != NULL)
    {
        i_cache_add (uid, uid_cache, UID_CACHE_SIZE, pwd->pw_name, &uid_last);
        return pwd->pw_name;
    }
    else
    {
        g_snprintf (ibuf, sizeof (ibuf), "%d", uid);
        return ibuf;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
get_group (int gid)
{
    struct group *grp;
    static char gbuf[10];
    char *name;
    static int gid_last;

    name = i_cache_match (gid, gid_cache, GID_CACHE_SIZE);
    if (name != NULL)
        return name;

    grp = getgrgid (gid);
    if (grp != NULL)
    {
        i_cache_add (gid, gid_cache, GID_CACHE_SIZE, grp->gr_name, &gid_last);
        return grp->gr_name;
    }
    else
    {
        g_snprintf (gbuf, sizeof (gbuf), "%d", gid);
        return gbuf;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* Since ncurses uses a handler that automatically refreshes the */
/* screen after a SIGCONT, and we don't want this behavior when */
/* spawning a child, we save the original handler here */

void
save_stop_handler (void)
{
    sigaction (SIGTSTP, NULL, &startup_handler);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Wrapper for _exit() system call.
 * The _exit() function has gcc's attribute 'noreturn', and this is reason why we can't
 * mock the call.
 *
 * @param status exit code
 */

void
my_exit (int status)
{
    _exit (status);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Call external programs.
 *
 * @parameter flags   addition conditions for running external programs.
 * @parameter shell   shell (if flags contain EXECUTE_AS_SHELL), command to run otherwise.
 *                    Shell (or command) will be found in paths described in PATH variable
 *                    (if shell parameter doesn't begin from path delimiter)
 * @parameter command Command for shell (or first parameter for command, if flags contain EXECUTE_AS_SHELL)
 * @return 0 if successfull, -1 otherwise
 */

int
my_system (int flags, const char *shell, const char *command)
{
    return my_systeml (flags, shell, command, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Call external programs with various parameters number.
 *
 * @parameter flags addition conditions for running external programs.
 * @parameter shell shell (if flags contain EXECUTE_AS_SHELL), command to run otherwise.
 *                  Shell (or command) will be found in pathes described in PATH variable
 *                  (if shell parameter doesn't begin from path delimiter)
 * @parameter ...   Command for shell with addition parameters for shell
 *                  (or parameters for command, if flags contain EXECUTE_AS_SHELL).
 *                  Should be NULL terminated.
 * @return 0 if successfull, -1 otherwise
 */


int
my_systeml (int flags, const char *shell, ...)
{
    GPtrArray *args_array;
    int status = 0;
    va_list vargs;
    char *one_arg;

    args_array = g_ptr_array_new ();

    va_start (vargs, shell);
    while ((one_arg = va_arg (vargs, char *)) != NULL)
          g_ptr_array_add (args_array, one_arg);
    va_end (vargs);

    g_ptr_array_add (args_array, NULL);
    status = my_systemv_flags (flags, shell, (char *const *) args_array->pdata);

    g_ptr_array_free (args_array, TRUE);

    return status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Call external programs with array of strings as parameters.
 *
 * @parameter command command to run. Command will be found in paths described in PATH variable
 *                    (if command parameter doesn't begin from path delimiter)
 * @parameter argv    Array of strings (NULL-terminated) with parameters for command
 * @return 0 if successfull, -1 otherwise
 */

int
my_systemv (const char *command, char *const argv[])
{
    my_fork_state_t fork_state;
    int status = 0;
    my_system_sigactions_t sigactions;

    my_system__save_sigaction_handlers (&sigactions);

    fork_state = my_fork ();
    switch (fork_state)
    {
    case FORK_ERROR:
        status = -1;
        break;
    case FORK_CHILD:
        {
            signal (SIGINT, SIG_DFL);
            signal (SIGQUIT, SIG_DFL);
            signal (SIGTSTP, SIG_DFL);
            signal (SIGCHLD, SIG_DFL);

            execvp (command, argv);
            my_exit (127);      /* Exec error */
        }
        break;
    default:
        status = 0;
        break;
    }
    my_system__restore_sigaction_handlers (&sigactions);

    return status;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Call external programs with flags and with array of strings as parameters.
 *
 * @parameter flags   addition conditions for running external programs.
 * @parameter command shell (if flags contain EXECUTE_AS_SHELL), command to run otherwise.
 *                    Shell (or command) will be found in paths described in PATH variable
 *                    (if shell parameter doesn't begin from path delimiter)
 * @parameter argv    Array of strings (NULL-terminated) with parameters for command
 * @return 0 if successfull, -1 otherwise
 */

int
my_systemv_flags (int flags, const char *command, char *const argv[])
{
    char *execute_name = NULL;
    GPtrArray *args_array;
    int status = 0;

    args_array = my_system_make_arg_array (flags, command, &execute_name);

    for (; argv != NULL && *argv != NULL; argv++)
        g_ptr_array_add (args_array, *argv);

    g_ptr_array_add (args_array, NULL);
    status = my_systemv (execute_name, (char *const *) args_array->pdata);

    g_free (execute_name);
    g_ptr_array_free (args_array, TRUE);

    return status;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Perform tilde expansion if possible.
 *
 * @param directory pointer to the path
 *
 * @return newly allocated string, even if it's unchanged.
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

    /* d = "~" or d = "~/" */
    if (!(*p) || (*p == PATH_SEP))
    {
        passwd = getpwuid (geteuid ());
        q = (*p == PATH_SEP) ? p + 1 : "";
    }
    else
    {
        q = strchr (p, PATH_SEP);
        if (!q)
        {
            passwd = getpwnam (p);
        }
        else
        {
            name = g_strndup (p, q - p);
            passwd = getpwnam (name);
            q++;
            g_free (name);
        }
    }

    /* If we can't figure the user name, leave tilde unexpanded */
    if (!passwd)
        return g_strdup (directory);

    return g_strconcat (passwd->pw_dir, PATH_SEP_STR, q, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Creates a pipe to hold standard error for a later analysis.
 * The pipe can hold 4096 bytes. Make sure no more is written
 * or a deadlock might occur.
 */

void
open_error_pipe (void)
{
    if (pipe (error_pipe) < 0)
    {
        message (D_NORMAL, _("Warning"), _("Pipe failed"));
    }
    old_error = dup (2);
    if (old_error < 0 || close (2) || dup (error_pipe[1]) != 2)
    {
        message (D_NORMAL, _("Warning"), _("Dup failed"));

        close (error_pipe[0]);
        error_pipe[0] = -1;
    }
    else
    {
        /*
         * Settng stderr in nonblocking mode as we close it earlier, than
         * program stops. We try to read some error at program startup,
         * but we should not block on it.
         *
         * TODO: make piped stdin/stderr poll()/select()able to get rid
         * of following hack.
         */
        int fd_flags;
        fd_flags = fcntl (error_pipe[0], F_GETFL, NULL);
        if (fd_flags != -1)
        {
            fd_flags |= O_NONBLOCK;
            if (fcntl (error_pipe[0], F_SETFL, fd_flags) == -1)
            {
                /* TODO: handle it somehow */
            }
        }
    }
    /* we never write there */
    close (error_pipe[1]);
    error_pipe[1] = -1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Close a pipe
 *
 * @param error '-1' - ignore errors, '0' - display warning, '1' - display error
 * @param text is prepended to the error message from the pipe
 *
 * @return not 0 if an error was displayed
 */

int
close_error_pipe (int error, const char *text)
{
    const char *title;
    char msg[MAX_PIPE_SIZE];
    int len = 0;

    /* already closed */
    if (error_pipe[0] == -1)
        return 0;

    if (error < 0 || (error > 0 && (error & D_ERROR) != 0))
        title = MSG_ERROR;
    else
        title = _("Warning");
    if (old_error >= 0)
    {
        if (dup2 (old_error, 2) == -1)
        {
            if (error < 0)
                error = D_ERROR;

            message (error, MSG_ERROR, _("Error dup'ing old error pipe"));
            return 1;
        }
        close (old_error);
        len = read (error_pipe[0], msg, MAX_PIPE_SIZE - 1);

        if (len >= 0)
            msg[len] = 0;
        close (error_pipe[0]);
        error_pipe[0] = -1;
    }
    if (error < 0)
        return 0;               /* Just ignore error message */
    if (text == NULL)
    {
        if (len <= 0)
            return 0;           /* Nothing to show */

        /* Show message from pipe */
        message (error, title, "%s", msg);
    }
    else
    {
        /* Show given text and possible message from pipe */
        message (error, title, "%s\n%s", text, msg);
    }
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Canonicalize path, and return a new path.  Do everything in place.
 * The new path differs from path in:
 *      Multiple `/'s are collapsed to a single `/'.
 *      Leading `./'s and trailing `/.'s are removed.
 *      Trailing `/'s are removed.
 *      Non-leading `../'s and trailing `..'s are handled by removing
 *      portions of the path.
 * Well formed UNC paths are modified only in the local part.
 */

void
custom_canonicalize_pathname (char *path, CANON_PATH_FLAGS flags)
{
    char *p, *s;
    size_t len;
    char *lpath = path;         /* path without leading UNC part */
    const size_t url_delim_len = strlen (VFS_PATH_URL_DELIMITER);

    /* Detect and preserve UNC paths: //server/... */
    if ((flags & CANON_PATH_GUARDUNC) && path[0] == PATH_SEP && path[1] == PATH_SEP)
    {
        p = path + 2;
        while (p[0] && p[0] != '/')
            p++;
        if (p[0] == '/' && p > path + 2)
            lpath = p;
    }

    if (!lpath[0] || !lpath[1])
        return;

    if (flags & CANON_PATH_JOINSLASHES)
    {
        /* Collapse multiple slashes */
        p = lpath;
        while (*p)
        {
            if (p[0] == PATH_SEP && p[1] == PATH_SEP && (p == lpath || *(p - 1) != ':'))
            {
                s = p + 1;
                while (*(++s) == PATH_SEP);
                str_move (p + 1, s);
            }
            p++;
        }
    }

    if (flags & CANON_PATH_JOINSLASHES)
    {
        /* Collapse "/./" -> "/" */
        p = lpath;
        while (*p)
        {
            if (p[0] == PATH_SEP && p[1] == '.' && p[2] == PATH_SEP)
                str_move (p, p + 2);
            else
                p++;
        }
    }

    if (flags & CANON_PATH_REMSLASHDOTS)
    {
        /* Remove trailing slashes */
        p = lpath + strlen (lpath) - 1;
        while (p > lpath && *p == PATH_SEP)
        {
            if (p >= lpath - (url_delim_len + 1)
                && strncmp (p - url_delim_len + 1, VFS_PATH_URL_DELIMITER, url_delim_len) == 0)
                break;
            *p-- = 0;
        }

        /* Remove leading "./" */
        if (lpath[0] == '.' && lpath[1] == PATH_SEP)
        {
            if (lpath[2] == 0)
            {
                lpath[1] = 0;
                return;
            }
            else
            {
                str_move (lpath, lpath + 2);
            }
        }

        /* Remove trailing "/" or "/." */
        len = strlen (lpath);
        if (len < 2)
            return;
        if (lpath[len - 1] == PATH_SEP
            && (len < url_delim_len
                || strncmp (lpath + len - url_delim_len, VFS_PATH_URL_DELIMITER,
                            url_delim_len) != 0))
        {
            lpath[len - 1] = '\0';
        }
        else
        {
            if (lpath[len - 1] == '.' && lpath[len - 2] == PATH_SEP)
            {
                if (len == 2)
                {
                    lpath[1] = '\0';
                    return;
                }
                else
                {
                    lpath[len - 2] = '\0';
                }
            }
        }
    }

    if (flags & CANON_PATH_REMDOUBLEDOTS)
    {
#ifdef HAVE_CHARSET
        const size_t enc_prefix_len = strlen (VFS_ENCODING_PREFIX);
#endif /* HAVE_CHARSET */

        /* Collapse "/.." with the previous part of path */
        p = lpath;
        while (p[0] && p[1] && p[2])
        {
            if ((p[0] != PATH_SEP || p[1] != '.' || p[2] != '.') || (p[3] != PATH_SEP && p[3] != 0))
            {
                p++;
                continue;
            }

            /* search for the previous token */
            s = p - 1;
            if (s >= lpath + url_delim_len - 2
                && strncmp (s - url_delim_len + 2, VFS_PATH_URL_DELIMITER, url_delim_len) == 0)
            {
                s -= (url_delim_len - 2);
                while (s >= lpath && *s-- != PATH_SEP);
            }

            while (s >= lpath)
            {
                if (s - url_delim_len > lpath
                    && strncmp (s - url_delim_len, VFS_PATH_URL_DELIMITER, url_delim_len) == 0)
                {
                    char *vfs_prefix = s - url_delim_len;
                    struct vfs_class *vclass;

                    while (vfs_prefix > lpath && *--vfs_prefix != PATH_SEP);
                    if (*vfs_prefix == PATH_SEP)
                        vfs_prefix++;
                    *(s - url_delim_len) = '\0';

                    vclass = vfs_prefix_to_class (vfs_prefix);
                    *(s - url_delim_len) = *VFS_PATH_URL_DELIMITER;

                    if (vclass != NULL)
                    {
                        struct vfs_s_subclass *sub = (struct vfs_s_subclass *) vclass->data;
                        if (sub != NULL && sub->flags & VFS_S_REMOTE)
                        {
                            s = vfs_prefix;
                            continue;
                        }
                    }
                }

                if (*s == PATH_SEP)
                    break;

                s--;
            }

            s++;

            /* If the previous token is "..", we cannot collapse it */
            if (s[0] == '.' && s[1] == '.' && s + 2 == p)
            {
                p += 3;
                continue;
            }

            if (p[3] != 0)
            {
                if (s == lpath && *s == PATH_SEP)
                {
                    /* "/../foo" -> "/foo" */
                    str_move (s + 1, p + 4);
                }
                else
                {
                    /* "token/../foo" -> "foo" */
#ifdef HAVE_CHARSET
                    if ((strncmp (s, VFS_ENCODING_PREFIX, enc_prefix_len) == 0)
                        && (is_supported_encoding (s + enc_prefix_len)))
                        /* special case: remove encoding */
                        str_move (s, p + 1);
                    else
#endif /* HAVE_CHARSET */
                        str_move (s, p + 4);
                }
                p = (s > lpath) ? s - 1 : s;
                continue;
            }

            /* trailing ".." */
            if (s == lpath)
            {
                /* "token/.." -> "." */
                if (lpath[0] != PATH_SEP)
                {
                    lpath[0] = '.';
                }
                lpath[1] = 0;
            }
            else
            {
                /* "foo/token/.." -> "foo" */
                if (s == lpath + 1)
                    s[0] = 0;
#ifdef HAVE_CHARSET
                else if ((strncmp (s, VFS_ENCODING_PREFIX, enc_prefix_len) == 0)
                         && (is_supported_encoding (s + enc_prefix_len)))
                {
                    /* special case: remove encoding */
                    s[0] = '.';
                    s[1] = '.';
                    s[2] = '\0';

                    /* search for the previous token */
                    /* s[-1] == PATH_SEP */
                    p = s - 1;
                    while (p >= lpath && *p != PATH_SEP)
                        p--;

                    if (p != NULL)
                        continue;
                }
#endif /* HAVE_CHARSET */
                else
                {
                    if (s >= lpath + url_delim_len
                        && strncmp (s - url_delim_len, VFS_PATH_URL_DELIMITER, url_delim_len) == 0)
                        *s = '\0';
                    else
                        s[-1] = '\0';
                }
                break;
            }

            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

void
canonicalize_pathname (char *path)
{
    custom_canonicalize_pathname (path, CANON_PATH_ALL);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_GET_PROCESS_STATS
int
gettimeofday (struct timeval *tp, void *tzp)
{
    return get_process_stats (tp, PS_SELF, 0, 0);
}
#endif /* HAVE_GET_PROCESS_STATS */

/* --------------------------------------------------------------------------------------------- */

#ifndef HAVE_REALPATH
char *
mc_realpath (const char *path, char *resolved_path)
{
    char copy_path[PATH_MAX];
    char link_path[PATH_MAX];
    char got_path[PATH_MAX];
    char *new_path = got_path;
    char *max_path;
    int readlinks = 0;
    int n;

    /* Make a copy of the source path since we may need to modify it. */
    if (strlen (path) >= PATH_MAX - 2)
    {
        errno = ENAMETOOLONG;
        return NULL;
    }
    strcpy (copy_path, path);
    path = copy_path;
    max_path = copy_path + PATH_MAX - 2;
    /* If it's a relative pathname use getwd for starters. */
    if (*path != '/')
    {

        new_path = g_get_current_dir ();
        if (new_path == NULL)
        {
            strcpy (got_path, "");
        }
        else
        {
            g_snprintf (got_path, PATH_MAX, "%s", new_path);
            g_free (new_path);
            new_path = got_path;
        }

        new_path += strlen (got_path);
        if (new_path[-1] != '/')
            *new_path++ = '/';
    }
    else
    {
        *new_path++ = '/';
        path++;
    }
    /* Expand each slash-separated pathname component. */
    while (*path != '\0')
    {
        /* Ignore stray "/". */
        if (*path == '/')
        {
            path++;
            continue;
        }
        if (*path == '.')
        {
            /* Ignore ".". */
            if (path[1] == '\0' || path[1] == '/')
            {
                path++;
                continue;
            }
            if (path[1] == '.')
            {
                if (path[2] == '\0' || path[2] == '/')
                {
                    path += 2;
                    /* Ignore ".." at root. */
                    if (new_path == got_path + 1)
                        continue;
                    /* Handle ".." by backing up. */
                    while ((--new_path)[-1] != '/');
                    continue;
                }
            }
        }
        /* Safely copy the next pathname component. */
        while (*path != '\0' && *path != '/')
        {
            if (path > max_path)
            {
                errno = ENAMETOOLONG;
                return NULL;
            }
            *new_path++ = *path++;
        }
#ifdef S_IFLNK
        /* Protect against infinite loops. */
        if (readlinks++ > MAXSYMLINKS)
        {
            errno = ELOOP;
            return NULL;
        }
        /* See if latest pathname component is a symlink. */
        *new_path = '\0';
        n = readlink (got_path, link_path, PATH_MAX - 1);
        if (n < 0)
        {
            /* EINVAL means the file exists but isn't a symlink. */
            if (errno != EINVAL)
            {
                /* Make sure it's null terminated. */
                *new_path = '\0';
                strcpy (resolved_path, got_path);
                return NULL;
            }
        }
        else
        {
            /* Note: readlink doesn't add the null byte. */
            link_path[n] = '\0';
            if (*link_path == '/')
                /* Start over for an absolute symlink. */
                new_path = got_path;
            else
                /* Otherwise back up over this component. */
                while (*(--new_path) != '/');
            /* Safe sex check. */
            if (strlen (path) + n >= PATH_MAX - 2)
            {
                errno = ENAMETOOLONG;
                return NULL;
            }
            /* Insert symlink contents into path. */
            strcat (link_path, path);
            strcpy (copy_path, link_path);
            path = copy_path;
        }
#endif /* S_IFLNK */
        *new_path++ = '/';
    }
    /* Delete trailing slash but don't whomp a lone slash. */
    if (new_path != got_path + 1 && new_path[-1] == '/')
        new_path--;
    /* Make sure it's null terminated. */
    *new_path = '\0';
    strcpy (resolved_path, got_path);
    return resolved_path;
}
#endif /* HAVE_REALPATH */

/* --------------------------------------------------------------------------------------------- */
/**
 * Return the index of the permissions triplet
 *
 */

int
get_user_permissions (struct stat *st)
{
    static gboolean initialized = FALSE;
    static gid_t *groups;
    static int ngroups;
    static uid_t uid;
    int i;

    if (!initialized)
    {
        uid = geteuid ();

        ngroups = getgroups (0, NULL);
        if (ngroups == -1)
            ngroups = 0;        /* ignore errors */

        /* allocate space for one element in addition to what
         * will be filled by getgroups(). */
        groups = g_new (gid_t, ngroups + 1);

        if (ngroups != 0)
        {
            ngroups = getgroups (ngroups, groups);
            if (ngroups == -1)
                ngroups = 0;    /* ignore errors */
        }

        /* getgroups() may or may not return the effective group ID,
         * so we always include it at the end of the list. */
        groups[ngroups++] = getegid ();

        initialized = TRUE;
    }

    if (st->st_uid == uid || uid == 0)
        return 0;

    for (i = 0; i < ngroups; i++)
    {
        if (st->st_gid == groups[i])
            return 1;
    }

    return 2;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Build filename from arguments.
 * Like to g_build_filename(), but respect VFS_PATH_URL_DELIMITER
 */

char *
mc_build_filenamev (const char *first_element, va_list args)
{
    gboolean absolute;
    const char *element = first_element;
    GString *path;
    char *ret;

    if (element == NULL)
        return NULL;

    path = g_string_new ("");

    absolute = (*first_element != '\0' && *first_element == PATH_SEP);

    do
    {
        if (*element == '\0')
            element = va_arg (args, char *);
        else
        {
            char *tmp_element;
            size_t len;
            const char *start;

            tmp_element = g_strdup (element);

            element = va_arg (args, char *);

            canonicalize_pathname (tmp_element);
            len = strlen (tmp_element);
            start = (tmp_element[0] == PATH_SEP) ? tmp_element + 1 : tmp_element;

            g_string_append (path, start);
            if (tmp_element[len - 1] != PATH_SEP && element != NULL)
                g_string_append_c (path, PATH_SEP);

            g_free (tmp_element);
        }
    }
    while (element != NULL);

    if (absolute)
        g_string_prepend_c (path, PATH_SEP);

    ret = g_string_free (path, FALSE);
    canonicalize_pathname (ret);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Build filename from arguments.
 * Like to g_build_filename(), but respect VFS_PATH_URL_DELIMITER
 */

char *
mc_build_filename (const char *first_element, ...)
{
    va_list args;
    char *ret;

    if (first_element == NULL)
        return NULL;

    va_start (args, first_element);
    ret = mc_build_filenamev (first_element, args);
    va_end (args);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
