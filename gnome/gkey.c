/*
 * Midnight Commander -- GNOME edition
 *
 * Copyright 1997 The Free Software Foundation
 *
 * Author:Miguel de Icaza (miguel@gnu.org)
 */

#include <config.h>
#include "mad.h"
#include "x.h"
#include "key.h"

/* The tty.h file is included since we try to use as much object code
 * from the curses distribution as possible so we need to send back
 * the same constants that the code expects on the non-X version
 * of the code.  The constants may be the ones from curses/ncurses
 * or our macro definitions for slang
 */

struct trampoline {
	select_fn fn;
	void      *fn_closure;
	int       fd;
	int       tag;
};

void
callback_trampoline (gpointer data, gint source, GdkInputCondition condition)
{
	struct trampoline *t = data;

	/* return value is ignored */
	(*t->fn)(source, t->fn_closure);
}

/*
 * We need to keep track of the select callbacks, as we need to release
 * the trampoline closure.
 */

GList *select_list;

void
add_select_channel (int fd, select_fn callback, void *info)
{
	struct trampoline *t;

	t = xmalloc (sizeof (struct trampoline), "add_select_channel");
	t->fn = callback;
	t->fd = fd;
	t->fn_closure = info;
	t->tag = gdk_input_add (fd, GDK_INPUT_READ, callback_trampoline, t);
	g_list_prepend (select_list, t);
}

static struct trampoline *tclosure;

static void
find_select_closure_callback (gpointer data, gpointer user_data)
{
	struct trampoline *t = data;
	int *pfd = (int *) user_data;
	
	if (t->fd == *pfd)
		tclosure = data;
}

void
delete_select_channel (int fd)
{
	tclosure = 0;
	g_list_foreach (select_list, find_select_closure_callback, &fd);
	if (tclosure){
		select_list = g_list_remove (select_list, tclosure);
		gdk_input_remove (tclosure->tag);
		free (tclosure);
	} else {
		fprintf (stderr, "PANIC: could not find closure for %d\n", fd);
	}
}
