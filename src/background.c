/* {{{ Copyright */

/* Background support.

   Copyright (C) 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007,
   2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1996

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

/* }}} */

/** \file background.c
 *  \brief Source: Background support
 */

#include <config.h>

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>           /* waitpid() */
#include <unistd.h>
#include <fcntl.h>

#include "lib/global.h"
#include "lib/tty/key.h"        /* add_select_channel(), delete_select_channel() */
#include "lib/widget.h"         /* message() */
#include "lib/event-types.h"

#include "filemanager/fileopctx.h"      /* FileOpContext */

#include "background.h"

/*** global variables ****************************************************************************/

#define MAXCALLARGS 4           /* Number of arguments supported */

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

enum ReturnType
{
    Return_String,
    Return_Integer
};

/*** file scope variables ************************************************************************/

/* File descriptor for talking to our parent */
static int parent_fd;

/* File descriptor for messages from our parent */
static int from_parent_fd;

struct TaskList *task_list = NULL;

static int background_attention (int fd, void *closure);

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
register_task_running (FileOpContext * ctx, pid_t pid, int fd, int to_child, char *info)
{
    TaskList *new;

    new = g_new (TaskList, 1);
    new->pid = pid;
    new->info = info;
    new->state = Task_Running;
    new->next = task_list;
    new->fd = fd;
    new->to_child_fd = to_child;
    task_list = new;

    add_select_channel (fd, background_attention, ctx);
}

/* --------------------------------------------------------------------------------------------- */

static int
destroy_task_and_return_fd (pid_t pid)
{
    TaskList *p = task_list;
    TaskList *prev = 0;

    while (p)
    {
        if (p->pid == pid)
        {
            if (prev)
                prev->next = p->next;
            else
                task_list = p->next;
            g_free (p->info);
            g_free (p);
            return p->fd;
        }
        prev = p;
        p = p->next;
    }

    /* pid not found */
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/* {{{ Parent handlers */

/* Parent/child protocol
 *
 * the child (the background) process send the following:
 * void *routine -- routine to be invoked in the parent
 * int  nargc    -- number of arguments
 * int  type     -- Return argument type.
 *
 * If the routine is zero, then it is a way to tell the parent
 * that the process is dying.
 *
 * nargc arguments in the following format:
 * int size of the coming block
 * size bytes with the block
 *
 * Now, the parent loads all those and then invokes
 * the routine with pointers to the information passed
 * (we just support pointers).
 *
 * If the return type is integer:
 *
 *     the parent then writes an int to the child with
 *     the return value from the routine and the values
 *     of any global variable that is modified in the parent
 *     currently: do_append and recursive_result.
 *
 * If the return type is a string:
 *
 *     the parent writes the resulting string length
 *     if the result string was NULL or the empty string,
 *     then the length is zero.
 *     The parent then writes the string length and frees
 *     the result string.
 */
/*
 * Receive requests from background process and invoke the
 * specified routine
 */

static int
background_attention (int fd, void *closure)
{
    FileOpContext *ctx;
    int have_ctx;
    union
    {
        int (*have_ctx0) (int);
        int (*have_ctx1) (int, char *);
        int (*have_ctx2) (int, char *, char *);
        int (*have_ctx3) (int, char *, char *, char *);
        int (*have_ctx4) (int, char *, char *, char *, char *);

        int (*non_have_ctx0) (FileOpContext *, int);
        int (*non_have_ctx1) (FileOpContext *, int, char *);
        int (*non_have_ctx2) (FileOpContext *, int, char *, char *);
        int (*non_have_ctx3) (FileOpContext *, int, char *, char *, char *);
        int (*non_have_ctx4) (FileOpContext *, int, char *, char *, char *, char *);

        char *(*ret_str0) ();
        char *(*ret_str1) (char *);
        char *(*ret_str2) (char *, char *);
        char *(*ret_str3) (char *, char *, char *);
        char *(*ret_str4) (char *, char *, char *, char *);

        void *pointer;
    } routine;
    /*    void *routine; */
    int argc, i, result, status;
    char *data[MAXCALLARGS];
    ssize_t bytes, ret;
    struct TaskList *p;
    int to_child_fd = -1;
    enum ReturnType type;

    ctx = closure;

    bytes = read (fd, &routine.pointer, sizeof (routine));
    if (bytes == -1 || (size_t) bytes < (sizeof (routine)))
    {
        const char *background_process_error = _("Background process error");

        unregister_task_running (ctx->pid, fd);
        if (!waitpid (ctx->pid, &status, WNOHANG))
        {
            /* the process is still running, but it misbehaves - kill it */
            kill (ctx->pid, SIGTERM);
            message (D_ERROR, background_process_error, _("Unknown error in child"));
            return 0;
        }

        /* 0 means happy end */
        if (WIFEXITED (status) && (WEXITSTATUS (status) == 0))
            return 0;

        message (D_ERROR, background_process_error, _("Child died unexpectedly"));

        return 0;
    }

    if ((read (fd, &argc, sizeof (argc)) != sizeof (argc)) ||
        (read (fd, &type, sizeof (type)) != sizeof (type)) ||
        (read (fd, &have_ctx, sizeof (have_ctx)) != sizeof (have_ctx)))
    {
        message (D_ERROR, _("Background protocol error"), _("Reading failed"));
        return 0;
    }

    if (argc > MAXCALLARGS)
    {
        message (D_ERROR, _("Background protocol error"),
                 _("Background process sent us a request for more arguments\n"
                   "than we can handle."));
    }

    if (have_ctx)
    {
        if (read (fd, ctx, sizeof (FileOpContext)) != sizeof (FileOpContext))
        {
            message (D_ERROR, _("Background protocol error"), _("Reading failed"));
            return 0;
        }
    }

    for (i = 0; i < argc; i++)
    {
        int size;

        if (read (fd, &size, sizeof (size)) != sizeof (size))
        {
            message (D_ERROR, _("Background protocol error"), _("Reading failed"));
            return 0;
        }
        data[i] = g_malloc (size + 1);
        if (read (fd, data[i], size) != size)
        {
            message (D_ERROR, _("Background protocol error"), _("Reading failed"));
            return 0;
        }
        data[i][size] = 0;      /* NULL terminate the blocks (they could be strings) */
    }

    /* Find child task info by descriptor */
    /* Find before call, because process can destroy self after */
    for (p = task_list; p; p = p->next)
    {
        if (p->fd == fd)
            break;
    }

    if (p)
        to_child_fd = p->to_child_fd;

    if (to_child_fd == -1)
        message (D_ERROR, _("Background process error"), _("Unknown error in child"));

    /* Handle the call */
    if (type == Return_Integer)
    {
        if (!have_ctx)
            switch (argc)
            {
            case 0:
                result = routine.have_ctx0 (Background);
                break;
            case 1:
                result = routine.have_ctx1 (Background, data[0]);
                break;
            case 2:
                result = routine.have_ctx2 (Background, data[0], data[1]);
                break;
            case 3:
                result = routine.have_ctx3 (Background, data[0], data[1], data[2]);
                break;
            case 4:
                result = routine.have_ctx4 (Background, data[0], data[1], data[2], data[3]);
                break;
            }
        else
            switch (argc)
            {
            case 0:
                result = routine.non_have_ctx0 (ctx, Background);
                break;
            case 1:
                result = routine.non_have_ctx1 (ctx, Background, data[0]);
                break;
            case 2:
                result = routine.non_have_ctx2 (ctx, Background, data[0], data[1]);
                break;
            case 3:
                result = routine.non_have_ctx3 (ctx, Background, data[0], data[1], data[2]);
                break;
            case 4:
                result =
                    routine.non_have_ctx4 (ctx, Background, data[0], data[1], data[2], data[3]);
                break;
            }

        /* Send the result code and the value for shared variables */
        ret = write (to_child_fd, &result, sizeof (int));
        if (have_ctx && to_child_fd != -1)
            ret = write (to_child_fd, ctx, sizeof (FileOpContext));
    }
    else if (type == Return_String)
    {
        int len;
        char *resstr = NULL;

        /* FIXME: string routines should also use the Foreground/Background
         * parameter.  Currently, this is not used here
         */
        switch (argc)
        {
        case 0:
            resstr = routine.ret_str0 ();
            break;
        case 1:
            resstr = routine.ret_str1 (data[0]);
            break;
        case 2:
            resstr = routine.ret_str2 (data[0], data[1]);
            break;
        case 3:
            resstr = routine.ret_str3 (data[0], data[1], data[2]);
            break;
        case 4:
            resstr = routine.ret_str4 (data[0], data[1], data[2], data[3]);
            break;
        default:
            g_assert_not_reached ();
        }
        if (resstr)
        {
            len = strlen (resstr);
            ret = write (to_child_fd, &len, sizeof (len));
            if (len != 0)
                ret = write (to_child_fd, resstr, len);
            g_free (resstr);
        }
        else
        {
            len = 0;
            ret = write (to_child_fd, &len, sizeof (len));
        }
    }
    for (i = 0; i < argc; i++)
        g_free (data[i]);

    repaint_screen ();
    (void) ret;
    return 0;
}


/* --------------------------------------------------------------------------------------------- */
/* }}} */

/* {{{ client RPC routines */

/* Sends the header for a call to a routine in the parent process.  If the file
 * operation context is not NULL, then it requests that the first parameter of
 * the call be a file operation context.
 */

static void
parent_call_header (void *routine, int argc, enum ReturnType type, FileOpContext * ctx)
{
    int have_ctx;
    ssize_t ret;

    have_ctx = (ctx != NULL);

    ret = write (parent_fd, &routine, sizeof (routine));
    ret = write (parent_fd, &argc, sizeof (int));
    ret = write (parent_fd, &type, sizeof (type));
    ret = write (parent_fd, &have_ctx, sizeof (have_ctx));

    if (have_ctx)
        ret = write (parent_fd, ctx, sizeof (FileOpContext));
    (void) ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
parent_va_call (void *routine, gpointer data, int argc, va_list ap)
{
    int i;
    ssize_t ret;
    struct FileOpContext *ctx = (struct FileOpContext *) data;

    parent_call_header (routine, argc, Return_Integer, ctx);
    for (i = 0; i < argc; i++)
    {
        int len;
        void *value;

        len = va_arg (ap, int);
        value = va_arg (ap, void *);
        ret = write (parent_fd, &len, sizeof (int));
        ret = write (parent_fd, value, len);
    }

    ret = read (from_parent_fd, &i, sizeof (int));
    if (ctx)
        ret = read (from_parent_fd, ctx, sizeof (FileOpContext));

    (void) ret;
    return i;
}

/* --------------------------------------------------------------------------------------------- */

static char *
parent_va_call_string (void *routine, int argc, va_list ap)
{
    char *str;
    int i;

    parent_call_header (routine, argc, Return_String, NULL);
    for (i = 0; i < argc; i++)
    {
        int len;
        void *value;

        len = va_arg (ap, int);
        value = va_arg (ap, void *);
        if ((write (parent_fd, &len, sizeof (int)) != sizeof (int)) ||
            (write (parent_fd, value, len) != len))
        {
            return NULL;
        }
    }

    if (read (from_parent_fd, &i, sizeof (int)) != sizeof (int))
        return NULL;
    if (!i)
        return NULL;
    str = g_malloc (i + 1);
    if (read (from_parent_fd, str, i) != i)
    {
        g_free (str);
        return NULL;
    }
    str[i] = 0;
    return str;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
unregister_task_running (pid_t pid, int fd)
{
    destroy_task_and_return_fd (pid);
    delete_select_channel (fd);
}

/* --------------------------------------------------------------------------------------------- */

void
unregister_task_with_pid (pid_t pid)
{
    int fd = destroy_task_and_return_fd (pid);
    if (fd != -1)
        delete_select_channel (fd);
}


/* --------------------------------------------------------------------------------------------- */
/**
 * Try to make the Midnight Commander a background job
 *
 * Returns:
 *  1 for parent
 *  0 for child
 * -1 on failure
 */
int
do_background (struct FileOpContext *ctx, char *info)
{
    int comm[2];                /* control connection stream */
    int back_comm[2];           /* back connection */
    pid_t pid;

    if (pipe (comm) == -1)
        return -1;

    if (pipe (back_comm) == -1)
        return -1;

    pid = fork ();
    if (pid == -1)
    {
        int saved_errno = errno;

        (void) close (comm[0]);
        (void) close (comm[1]);
        (void) close (back_comm[0]);
        (void) close (back_comm[1]);
        errno = saved_errno;
        return -1;
    }

    if (pid == 0)
    {
        int nullfd;

        parent_fd = comm[1];
        from_parent_fd = back_comm[0];

        mc_global.we_are_background = TRUE;
        top_dlg = NULL;

        /* Make stdin/stdout/stderr point somewhere */
        close (0);
        close (1);
        close (2);

        nullfd = open ("/dev/null", O_RDWR);
        if (nullfd != -1)
        {
            while (dup2 (nullfd, 0) == -1 && errno == EINTR)
                ;
            while (dup2 (nullfd, 1) == -1 && errno == EINTR)
                ;
            while (dup2 (nullfd, 2) == -1 && errno == EINTR)
                ;
        }

        return 0;
    }
    else
    {
        ctx->pid = pid;
        register_task_running (ctx, pid, comm[0], back_comm[1], info);
        return 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

int
parent_call (void *routine, struct FileOpContext *ctx, int argc, ...)
{
    int ret;
    va_list ap;

    va_start (ap, argc);
    ret = parent_va_call (routine, (gpointer) ctx, argc, ap);
    va_end (ap);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

char *
parent_call_string (void *routine, int argc, ...)
{
    va_list ap;
    char *str;

    va_start (ap, argc);
    str = parent_va_call_string (routine, argc, ap);
    va_end (ap);

    return str;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
background_parent_call (const gchar * event_group_name, const gchar * event_name,
                        gpointer init_data, gpointer data)
{
    ev_background_parent_call_t *event_data = (ev_background_parent_call_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    event_data->ret.i =
        parent_va_call (event_data->routine, event_data->ctx, event_data->argc, event_data->ap);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
background_parent_call_string (const gchar * event_group_name, const gchar * event_name,
                               gpointer init_data, gpointer data)
{
    ev_background_parent_call_t *event_data = (ev_background_parent_call_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    event_data->ret.s =
        parent_va_call_string (event_data->routine, event_data->argc, event_data->ap);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
