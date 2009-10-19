/* {{{ Copyright */

/* Background support.
   Copyright (C) 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.
   
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

/** \file background.c
 *  \brief Source: Background support
 */

#include <config.h>

#ifdef WITH_BACKGROUND

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>	/* waitpid() */
#include <unistd.h>
#include <fcntl.h>

#include "global.h"
#include "background.h"
#include "wtools.h"
#include "layout.h"	/* repaint_screen() */
#include "fileopctx.h"	/* FileOpContext */
#include "../src/tty/key.h"	/* add_select_channel(), delete_select_channel() */

enum ReturnType {
    Return_String,
    Return_Integer
};

/* If true, this is a background process */
int we_are_background = 0;

/* File descriptor for talking to our parent */
static int parent_fd;

/* File descriptor for messages from our parent */
static int from_parent_fd;

#define MAXCALLARGS 4		/* Number of arguments supported */

struct TaskList *task_list = NULL;

static int background_attention (int fd, void *closure);
    
static void
register_task_running (FileOpContext *ctx, pid_t pid, int fd, int to_child,
					   char *info)
{
    TaskList *new;

    new = g_new (TaskList, 1);
    new->pid   = pid;
    new->info  = info;
    new->state = Task_Running;
    new->next  = task_list;
    new->fd    = fd;
    new->to_child_fd = to_child;
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
    int back_comm[2];	/* back connection */
    pid_t pid;

    if (pipe (comm) == -1)
	return -1;

    if (pipe (back_comm) == -1)
	return -1;

    if ((pid = fork ()) == -1) {
	int saved_errno = errno;
	(void) close (comm[0]);
	(void) close (comm[1]);
	(void) close (back_comm[0]);
	(void) close (back_comm[1]);
	errno = saved_errno;
	return -1;
    }

    if (pid == 0) {
	int nullfd;

	parent_fd = comm[1];
	from_parent_fd = back_comm[0];

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
	ctx->pid = pid;
	register_task_running (ctx, pid, comm[0], back_comm[1], info);
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
    union 
    {
      int (*have_ctx1)(int, char *);
      int (*have_ctx2)(int, char *, char *);
      int (*have_ctx3)(int, char *, char *, char *);
      int (*have_ctx4)(int, char *, char *, char *, char *);

      int (*non_have_ctx1)(FileOpContext *, int, char *);
      int (*non_have_ctx2)(FileOpContext *, int, char *, char *);
      int (*non_have_ctx3)(FileOpContext *, int, char *, char *, char *);
      int (*non_have_ctx4)(FileOpContext *, int, char *, char *, char *, char *);

      char * (*ret_str1)(char *);
      char * (*ret_str2)(char *, char *);
      char * (*ret_str3)(char *, char *, char *);
      char * (*ret_str4)(char *, char *, char *, char *);

      void *pointer;
    } routine;
/*    void *routine;*/
    int  argc, i, result, status;
    char *data [MAXCALLARGS];
    ssize_t bytes;
	struct TaskList *p;
	int to_child_fd;
    enum ReturnType type;

    ctx = closure;

    bytes = read (fd, &routine.pointer, sizeof (routine));
    if (bytes == -1 || (size_t) bytes < (sizeof (routine))) {
	const char *background_process_error = _(" Background process error ");

	unregister_task_running (ctx->pid, fd);
	if (!waitpid (ctx->pid, &status, WNOHANG)) {
	    /* the process is still running, but it misbehaves - kill it */
	    kill (ctx->pid, SIGTERM);
	    message (D_ERROR, background_process_error, _(" Unknown error in child "));
	    return 0;
	}

	/* 0 means happy end */
	if (WIFEXITED (status) && (WEXITSTATUS (status) == 0))
	    return 0;

	message (D_ERROR, background_process_error, _(" Child died unexpectedly "));

	return 0;
    }

    read (fd, &argc, sizeof (argc));
    if (argc > MAXCALLARGS){
	message (D_ERROR, _(" Background protocol error "),
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
		result = routine.have_ctx1 (Background, data [0]);
		break;
	    case 2:
		result = routine.have_ctx2 (Background, data [0], data [1]);
		break;
	    case 3:
		result = routine.have_ctx3 (Background, data [0], data [1], data [2]);
		break;
	    case 4:
		result = routine.have_ctx4 (Background, data [0], data [1], data [2], data [3]);
		break;
	    }
	else
	    switch (argc){
	    case 1:
		result = routine.non_have_ctx1 (ctx, Background, data [0]);
		break;
	    case 2:
		result = routine.non_have_ctx2 (ctx, Background, data [0], data [1]);
		break;
	    case 3:
		result = routine.non_have_ctx3 (ctx, Background, data [0], data [1], data [2]);
		break;
	    case 4:
		result = routine.non_have_ctx4 (ctx, Background, data [0], data [1], data [2], data [3]);
		break;
	    }

	/* Find child task info by descriptor */
	for (p = task_list; p; p = p->next) {
		if (p->fd == fd)
			break;
	}

	to_child_fd = p->to_child_fd;

	/* Send the result code and the value for shared variables */
	write (to_child_fd, &result, sizeof (int));
	if (have_ctx)
	    write (to_child_fd, ctx, sizeof (FileOpContext));
    } else if (type == Return_String) {
	int len;
	char *resstr = NULL;

	/* FIXME: string routines should also use the Foreground/Background
	 * parameter.  Currently, this is not used here
	 */
	switch (argc){
	case 1:
	    resstr = routine.ret_str1 (data [0]);
	    break;
	case 2:
	    resstr = routine.ret_str2 (data [0], data [1]);
	    break;
	case 3:
	    resstr = routine.ret_str3 (data [0], data [1], data [2]);
	    break;
	case 4:
	    resstr = routine.ret_str4 (data [0], data [1], data [2], data [3]);
	    break;
	default: g_assert_not_reached();
	}
	if (resstr){
	    len = strlen (resstr);
	    write (to_child_fd, &len, sizeof (len));
	    if (len){
		write (to_child_fd, resstr, len);
		g_free (resstr);
	    }
	} else {
	    len = 0;
	    write (to_child_fd, &len, sizeof (len));
	}
    }
    for (i = 0; i < argc; i++)
	g_free (data [i]);

    repaint_screen ();
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

    read (from_parent_fd, &i, sizeof (int));
    if (ctx)
	read (from_parent_fd, ctx, sizeof (FileOpContext));

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
    read (from_parent_fd, &i, sizeof (int));
    if (!i)
	return NULL;
    str = g_malloc (i + 1);
    read (from_parent_fd, str, i);
    str [i] = 0;
    return str;
}

#endif				/* WITH_BACKGROUND */
