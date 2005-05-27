/* {{{ Copyright */

/* Background support.
   Copyright (C) 1996 The Free Software Foundation
   
   Written by: 1996 Miguel de Icaza

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* }}} */

#include <config.h>

#ifdef WITH_BACKGROUND

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"
#include "background.h"
#include "tty.h"	/* doupdate() */
#include "dialog.h"	/* do_refresh() */
#include "wtools.h"
#include "fileopctx.h"	/* FileOpContext */
#include "key.h"	/* add_select_channel(), delete_select_channel() */

enum ReturnType {
    Return_String,
    Return_Integer
};

/* If true, this is a background process */
int we_are_background = 0;

/* File descriptor for talking to our parent */
static int parent_fd;

#define MAXCALLARGS 4		/* Number of arguments supported */

struct TaskList *task_list = NULL;

static int background_attention (int fd, void *closure);
    
static void
register_task_running (FileOpContext *ctx, pid_t pid, int fd, char *info)
{
    TaskList *new;

    new = g_new (TaskList, 1);
    new->pid   = pid;
    new->info  = info;
    new->state = Task_Running;
    new->next  = task_list;
    new->fd    = fd;
    task_list  = new;

    add_select_channel (fd, background_attention, ctx);
}

void
unregister_task_running (pid_t pid, int fd)
{
    TaskList *p = task_list;
    TaskList *prev = 0;

    while (p){
	if (p->pid == pid){
	    if (prev)
		prev->next = p->next;
	    else
		task_list = p->next;
	    g_free (p->info);
	    g_free (p);
	    break;
	}
	prev = p;
	p = p->next;
    }
    delete_select_channel (fd);
}

/*
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
    int comm[2];		/* control connection stream */
    pid_t pid;

    if (pipe (comm) == -1)
	return -1;

    if ((pid = fork ()) == -1) {
    	int saved_errno = errno;
    	(void) close (comm[0]);
    	(void) close (comm[1]);
    	errno = saved_errno;
	return -1;
    }

    if (pid == 0) {
	int nullfd;

	close (comm[0]);
	parent_fd = comm[1];
	we_are_background = 1;
	current_dlg = NULL;

	/* Make stdin/stdout/stderr point somewhere */
	close (0);
	close (1);
	close (2);

	if ((nullfd = open ("/dev/null", O_RDWR)) != -1) {
	    while (dup2 (nullfd, 0) == -1 && errno == EINTR);
	    while (dup2 (nullfd, 1) == -1 && errno == EINTR);
	    while (dup2 (nullfd, 2) == -1 && errno == EINTR);
	}

	return 0;
    } else {
	close (comm[1]);
	ctx->pid = pid;
	register_task_running (ctx, pid, comm[0], info);
	return 1;
    }
}

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
    void *routine;
    int  argc, i, result, status;
    char *data [MAXCALLARGS];
    ssize_t bytes;
    enum ReturnType type;

    ctx = closure;

    bytes = read (fd, &routine, sizeof (routine));
    if (bytes == -1 || (size_t) bytes < (sizeof (routine))) {
	const char *background_process_error = _(" Background process error ");

	unregister_task_running (ctx->pid, fd);
	if (!waitpid (ctx->pid, &status, WNOHANG)) {
	    /* the process is still running, but it misbehaves - kill it */
	    kill (ctx->pid, SIGTERM);
	    message (1, background_process_error, _(" Unknown error in child "));
	    return 0;
	}

	/* 0 means happy end */
	if (WIFEXITED (status) && (WEXITSTATUS (status) == 0))
	    return 0;

	message (1, background_process_error, _(" Child died unexpectedly "));

	return 0;
    }

    read (fd, &argc, sizeof (argc));
    if (argc > MAXCALLARGS){
	message (1, _(" Background protocol error "),
		 _(" Background process sent us a request for more arguments \n"
		 " than we can handle. \n"));
    }
    read (fd, &type, sizeof (type));
    read (fd, &have_ctx, sizeof (have_ctx));
    if (have_ctx)
	    read (fd, ctx, sizeof (FileOpContext));

    for (i = 0; i < argc; i++){
	int size;

	read (fd, &size, sizeof (size));
	data [i] = g_malloc (size+1);
	read (fd, data [i], size);

	data [i][size] = 0;	/* NULL terminate the blocks (they could be strings) */
    }

    /* Handle the call */
    if (type == Return_Integer){
	if (!have_ctx)
	    switch (argc){
	    case 1:
		result = (*(int (*)(int, char *))routine)(Background, data [0]);
		break;
	    case 2:
		result = (*(int (*)(int, char *, char *))routine)
		    (Background, data [0], data [1]);
		break;
	    case 3:
		result = (*(int (*)(int, char *, char *, char *))routine)
		    (Background, data [0], data [1], data [2]);
		break;
	    case 4:
		result = (*(int (*)(int, char *, char *, char *, char *))routine)
		    (Background, data [0], data [1], data [2], data [3]);
		break;
	    }
	else
	    switch (argc){
	    case 1:
		result = (*(int (*)(FileOpContext *, int, char *))routine)
		    (ctx, Background, data [0]);
		break;
	    case 2:
		result = (*(int (*)(FileOpContext *, int, char *, char *))routine)
		    (ctx, Background, data [0], data [1]);
		break;
	    case 3:
		result = (*(int (*)(FileOpContext *, int, char *, char *, char *))routine)
		    (ctx, Background, data [0], data [1], data [2]);
		break;
	    case 4:
		result = (*(int (*)(FileOpContext *, int, char *, char *, char *, char *))routine)
		    (ctx, Background, data [0], data [1], data [2], data [3]);
		break;
	    }

	/* Send the result code and the value for shared variables */
	write (fd, &result, sizeof (int));
	if (have_ctx)
	    write (fd, ctx, sizeof (FileOpContext));
    } else if (type == Return_String) {
	int len;
	char *resstr = NULL;

	/* FIXME: string routines should also use the Foreground/Background
	 * parameter.  Currently, this is not used here
	 */
	switch (argc){
	case 1:
	    resstr = (*(char * (*)(char *))routine)(data [0]);
	    break;
	case 2:
	    resstr = (*(char * (*)(char *, char *))routine)
		(data [0], data [1]);
	    break;
	case 3:
	    resstr = (*(char * (*)(char *, char *, char *))routine)
		(data [0], data [1], data [2]);
	    break;
	case 4:
	    resstr = (*(char * (*)(char *, char *, char *, char *))routine)
		(data [0], data [1], data [2], data [3]);
	    break;
	default: g_assert_not_reached();
	}
	if (resstr){
	    len = strlen (resstr);
	    write (fd, &len, sizeof (len));
	    if (len){
		write (fd, resstr, len);
		g_free (resstr);
	    }
	} else {
	    len = 0;
	    write (fd, &len, sizeof (len));
	}
    }
    for (i = 0; i < argc; i++)
	g_free (data [i]);

    do_refresh ();
    mc_refresh ();
    doupdate ();
    return 0;
}


/* }}} */

/* {{{ client RPC routines */

/* Sends the header for a call to a routine in the parent process.  If the file
 * operation context is not NULL, then it requests that the first parameter of
 * the call be a file operation context.
 */
static void
parent_call_header (void *routine, int argc, enum ReturnType type, FileOpContext *ctx)
{
    int have_ctx;

    have_ctx = (ctx != NULL);

    write (parent_fd, &routine, sizeof (routine));
    write (parent_fd, &argc, sizeof (int));
    write (parent_fd, &type, sizeof (type));
    write (parent_fd, &have_ctx, sizeof (have_ctx));

    if (have_ctx)
	write (parent_fd, ctx, sizeof (FileOpContext));
}

int
parent_call (void *routine, struct FileOpContext *ctx, int argc, ...)
{
    va_list ap;
    int i;

    va_start (ap, argc);
    parent_call_header (routine, argc, Return_Integer, ctx);
    for (i = 0; i < argc; i++) {
	int  len;
	void *value;

	len   = va_arg (ap, int);
	value = va_arg (ap, void *);
	write (parent_fd, &len, sizeof (int));
	write (parent_fd, value, len);
    }
    read (parent_fd, &i, sizeof (int));
    if (ctx)
	read (parent_fd, ctx, sizeof (FileOpContext));

    return i;
}

char *
parent_call_string (void *routine, int argc, ...)
{
    va_list ap;
    char *str;
    int i;
    
    va_start (ap, argc);
    parent_call_header (routine, argc, Return_String, NULL);
    for (i = 0; i < argc; i++){
	int  len;
	void *value;

	len   = va_arg (ap, int);
	value = va_arg (ap, void *);
	write (parent_fd, &len, sizeof (int));
	write (parent_fd, value, len);
    }
    read (parent_fd, &i, sizeof (int));
    if (!i)
	return NULL;
    str = g_malloc (i + 1);
    read (parent_fd, str, i);
    str [i] = 0;
    return str;
}

#endif				/* WITH_BACKGROUND */
