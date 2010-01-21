/* Dialog box features module for the Midnight Commander
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file dialog.c
 *  \brief Source: dialog box features module
 */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin/skin.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"

#include "help.h"	/* interactive_display() */
#include "dialog.h"
#include "layout.h"
#include "execute.h"	/* suspend_cmd() */
#include "main.h"	/* fast_refresh */
#include "setup.h"	/* mouse_close_dialog */

/* Color styles for normal and error dialogs */
int dialog_colors [4];
int alarm_colors [4];

/* Primitive way to check if the the current dialog is our dialog */
/* This is needed by async routines like load_prompt */
Dlg_head *current_dlg = 0;

/* A hook list for idle events */
Hook *idle_hook = 0;

/* left click outside of dialog closes it */
int mouse_close_dialog = 0;

static void dlg_broadcast_msg_to (Dlg_head * h, widget_msg_t message,
				  int reverse, int flags);

/* draw box in window */
void
draw_box (Dlg_head *h, int y, int x, int ys, int xs)
{
    tty_draw_box (h->y + y, h->x + x, ys, xs);
}

void
widget_erase (Widget *w)
{
    tty_fill_region (w->y, w->x, w->lines, w->cols, ' ');
}

void
dlg_erase (Dlg_head *h)
{
    if (h != NULL)
	tty_fill_region (h->y, h->x, h->lines, h->cols, ' ');
}

void
init_widget (Widget *w, int y, int x, int lines, int cols,
	     callback_fn callback, mouse_h mouse_handler)
{
    w->x = x;
    w->y = y;
    w->cols = cols;
    w->lines = lines;
    w->callback = callback;
    w->mouse = mouse_handler;
    w->parent = 0;

    /* Almost all widgets want to put the cursor in a suitable place */
    w->options = W_WANT_CURSOR;
}

/* Clean the dialog area, draw the frame and the title */
void
common_dialog_repaint (struct Dlg_head *h)
{
    int space;

    space = (h->flags & DLG_COMPACT) ? 0 : 1;

    tty_setcolor (DLG_NORMALC (h));
    dlg_erase (h);
    draw_box (h, space, space, h->lines - 2 * space, h->cols - 2 * space);

    if (h->title) {
	tty_setcolor (DLG_HOT_NORMALC (h));
	dlg_move (h, space, (h->cols - str_term_width1 (h->title)) / 2);
	tty_print_string (h->title);
    }
}

/* this function allows to set dialog position */
void
dlg_set_position (Dlg_head *h, int y1, int x1, int y2, int x2)
{
    /* save old positions, will be used to reposition childs */
    int ox, oy, oc, ol;
    int shift_x, shift_y, scale_x, scale_y;

    /* save old positions, will be used to reposition childs */
    ox = h->x;
    oy = h->y;
    oc = h->cols;
    ol = h->lines;

    h->x = x1;
    h->y = y1;
    h->lines = y2 - y1;
    h->cols = x2 - x1;

    /* values by which controls should be moved */
    shift_x = h->x - ox;
    shift_y = h->y - oy;
    scale_x = h->cols - oc;
    scale_y = h->lines - ol;

    if (h->current == NULL)
	return;

    if ((shift_x != 0) || (shift_y != 0) || (scale_x != 0) || (scale_y != 0)) {
	Widget *c = h->current;

	do {
	    /* there are, mainly, 2 generally possible
	       situations:

	       1. control sticks to one side - it
	       should be moved

	       2. control sticks to two sides of
	       one direction - it should be sized */

	    int x = c->x;
	    int y = c->y;
	    int cols = c->cols;
	    int lines = c->lines;

	    if ((c->pos_flags & WPOS_KEEP_LEFT) && (c->pos_flags & WPOS_KEEP_RIGHT)) {
		x += shift_x;
		cols += scale_x;
	    } else if (c->pos_flags & WPOS_KEEP_LEFT)
		x += shift_x;
	    else if (c->pos_flags & WPOS_KEEP_RIGHT)
		x += shift_x + scale_x;

	    if ((c->pos_flags & WPOS_KEEP_TOP) && (c->pos_flags & WPOS_KEEP_BOTTOM)) {
		y += shift_y;
		lines += scale_y;
	    } else if (c->pos_flags & WPOS_KEEP_TOP)
		y += shift_y;
	    else if (c->pos_flags & WPOS_KEEP_BOTTOM)
		y += shift_y + scale_y;

	    widget_set_size (c, y, x, lines, cols);

	    c = c->next;
	} while (h->current != c);
    }
}

/* this function sets only size, leaving positioning to automatic methods */
void
dlg_set_size (Dlg_head *h, int lines, int cols)
{
    int x = h->x;
    int y = h->y;

    if (h->flags & DLG_CENTER) {
        y = (LINES - lines) / 2;
        x = (COLS - cols) / 2;
    }

    if ((h->flags & DLG_TRYUP) && (y > 3))
        y -= 2;

    dlg_set_position (h, y, x, y + lines, x + cols);
}

/* Default dialog callback */
cb_ret_t
default_dlg_callback (Dlg_head *h, Widget *sender,
			dlg_msg_t msg, int parm, void *data)
{
    (void) sender;
    (void) parm;
    (void) data;

    switch (msg) {
    case DLG_DRAW:
	if (h->color != NULL) {
	    common_dialog_repaint (h);
	    return MSG_HANDLED;
	}
	return MSG_NOT_HANDLED;

    case DLG_IDLE:
	dlg_broadcast_msg_to (h, WIDGET_IDLE, 0, W_WANT_IDLE);
	return MSG_HANDLED;

    case DLG_RESIZE:
	/* this is default resizing mechanism */
	/* the main idea of this code is to resize dialog
	  according to flags (if any of flags require automatic
	  resizing, like DLG_CENTER, end after that reposition
	  controls in dialog according to flags of widget) */
	dlg_set_size (h, h->lines, h->cols);
	return MSG_HANDLED;

    default:
	break;
    }

    return MSG_NOT_HANDLED;
}

Dlg_head *
create_dlg (int y1, int x1, int lines, int cols, const int *color_set,
	    dlg_cb_fn callback, const char *help_ctx, const char *title,
	    int flags)
{
    Dlg_head *new_d;

    new_d = g_new0 (Dlg_head, 1);
    if (color_set != NULL) {
	new_d->color = g_new (int, DLG_COLOR_NUM);
	memmove (new_d->color, color_set, sizeof (int) * DLG_COLOR_NUM);
    }
    new_d->help_ctx = help_ctx;
    new_d->callback = (callback != NULL) ? callback : default_dlg_callback;
    new_d->x = x1;
    new_d->y = y1;
    new_d->flags = flags;
    new_d->data = NULL;

    dlg_set_size (new_d, lines, cols);

    /* Strip existing spaces, add one space before and after the title */
    if (title) {
	char *t;

	t = g_strstrip (g_strdup (title));
	if (*t != '\0')
	    new_d->title = g_strdup_printf (" %s ", t);
	g_free (t);
    }

    return new_d;
}

void
dlg_set_default_colors (void)
{
    dialog_colors [0] = COLOR_NORMAL;
    dialog_colors [1] = COLOR_FOCUS;
    dialog_colors [2] = COLOR_HOT_NORMAL;
    dialog_colors [3] = COLOR_HOT_FOCUS;

    alarm_colors [0] = ERROR_COLOR;
    alarm_colors [1] = REVERSE_COLOR;
    alarm_colors [2] = ERROR_HOT_NORMAL;
    alarm_colors [3] = ERROR_HOT_FOCUS;
}

void
set_idle_proc (Dlg_head *d, int enable)
{
    if (enable)
	d->flags |= DLG_WANT_IDLE;
    else
	d->flags &= ~DLG_WANT_IDLE;
}

/*
 * Insert widget to dialog before current widget.  For dialogs populated
 * from the bottom, make the widget current.  Return widget number.
 */
int
add_widget_autopos (Dlg_head *h, void *w, widget_pos_flags_t pos_flags)
{
    Widget *widget = (Widget *) w;

    /* Don't accept 0 widgets, and running dialogs */
    if (!widget || h->running)
	abort ();

    widget->x += h->x;
    widget->y += h->y;
    widget->parent = h;
    widget->dlg_id = h->count++;
    widget->pos_flags = pos_flags;

    if (h->current) {
	widget->next = h->current;
	widget->prev = h->current->prev;
	h->current->prev->next = widget;
	h->current->prev = widget;
    } else {
	widget->prev = widget;
	widget->next = widget;
    }

    if ((h->flags & DLG_REVERSE) || !h->current)
	h->current = widget;

    return widget->dlg_id;
}

/* wrapper to simply add lefttop positioned controls */
int
add_widget (Dlg_head *h, void *w)
{
    return add_widget_autopos (h, w, WPOS_KEEP_LEFT | WPOS_KEEP_TOP);
}

static void
do_complete_refresh (Dlg_head *dlg)
{
    if (!dlg->fullscreen && dlg->parent)
	do_complete_refresh (dlg->parent);

    dlg_redraw (dlg);
}

void
do_refresh (void)
{
    if (!current_dlg)
	return;

    if (fast_refresh)
	dlg_redraw (current_dlg);
    else {
	do_complete_refresh (current_dlg);
    }
}

/* broadcast a message to all the widgets in a dialog that have
 * the options set to flags. If flags is zero, the message is sent
 * to all widgets.
 */
static void
dlg_broadcast_msg_to (Dlg_head *h, widget_msg_t message, int reverse,
		      int flags)
{
    Widget *p, *first, *wi;

    if (!h->current)
	return;

    if (reverse)
	first = p = h->current->prev;
    else
	first = p = h->current->next;

    do {
	wi = p;
	if (reverse)
	    p = p->prev;
	else
	    p = p->next;
	if (flags == 0 || (flags & wi->options))
	    send_message (wi, message, 0);
    } while (first != p);
}

/* broadcast a message to all the widgets in a dialog */
void
dlg_broadcast_msg (Dlg_head *h, widget_msg_t message, int reverse)
{
    dlg_broadcast_msg_to (h, message, reverse, 0);
}

int
dlg_focus (Dlg_head *h)
{
    if ((h->current != NULL)
	&& (send_message (h->current, WIDGET_FOCUS, 0) == MSG_HANDLED)) {
	    h->callback (h, h->current, DLG_FOCUS, 0, NULL);
	    return 1;
	}
    return 0;
}

static int
dlg_unfocus (Dlg_head *h)
{
    if ((h->current != NULL)
	&& (send_message (h->current, WIDGET_UNFOCUS, 0) == MSG_HANDLED)) {
	    h->callback (h, h->current, DLG_UNFOCUS, 0, NULL);
	    return 1;
	}
    return 0;
}


/* Return true if the windows overlap */
int dlg_overlap (Widget *a, Widget *b)
{
    if ((b->x >= a->x + a->cols)
	|| (a->x >= b->x + b->cols)
	|| (b->y >= a->y + a->lines)
	|| (a->y >= b->y + b->lines))
	return 0;
    return 1;
}


/* Find the widget with the given callback in the dialog h */
Widget *
find_widget_type (const Dlg_head *h, callback_fn callback)
{
    Widget *w = NULL;

    if ((h != NULL) && (h->current != NULL)) {
	int i;
	Widget *item;

	for (i = 0, item = h->current; i < h->count; i++, item = item->next) {
	    if (item->callback == callback) {
		w = item;
		break;
	    }
	}
    }

    return w;
}

/* Find the widget with the given dialog id in the dialog h and select it */
void
dlg_select_by_id (const Dlg_head *h, int id)
{
    Widget *w, *w_found;

    if (!h->current)
	return;

    w = h->current;
    w_found = NULL;

    do {
	if (w->dlg_id == id) {
	    w_found = w;
	    break;
	}
	w = w->next;
    } while (w != h->current);

    if (w_found)
	dlg_select_widget(w_found);
}


/* What to do if the requested widget doesn't take focus */
typedef enum {
    SELECT_NEXT,		/* go the the next widget */
    SELECT_PREV,		/* go the the previous widget */
    SELECT_EXACT		/* use current widget */
} select_dir_t;

/*
 * Try to select another widget.  If forward is set, follow tab order.
 * Otherwise go to the previous widget.
 */
static void
do_select_widget (Dlg_head *h, Widget *w, select_dir_t dir)
{
    Widget *w0 = h->current;

    if (!dlg_unfocus (h))
	return;

    h->current = w;
    do {
	if (dlg_focus (h))
	    break;

	switch (dir) {
	case SELECT_NEXT:
	    h->current = h->current->next;
	    break;
	case SELECT_PREV:
	    h->current = h->current->prev;
	    break;
	case SELECT_EXACT:
	    h->current = w0;
	    dlg_focus (h);
	    return;
	}
    } while (h->current != w);

    if (dlg_overlap (w0, h->current)) {
	send_message (h->current, WIDGET_DRAW, 0);
	send_message (h->current, WIDGET_FOCUS, 0);
    }
}


/*
 * Try to select widget in the dialog.
 */
void
dlg_select_widget (void *w)
{
    do_select_widget (((Widget *) w)->parent, w, SELECT_NEXT);
}


/* Try to select previous widget in the tab order */
void
dlg_one_up (Dlg_head *h)
{
    if (h->current)
	do_select_widget (h, h->current->prev, SELECT_PREV);
}


/* Try to select next widget in the tab order */
void
dlg_one_down (Dlg_head *h)
{
    if (h->current)
	do_select_widget (h, h->current->next, SELECT_NEXT);
}


void update_cursor (Dlg_head *h)
{
    Widget *p = h->current;

    if (p != NULL) {
	if (p->options & W_WANT_CURSOR)
	    send_message (p, WIDGET_CURSOR, 0);
	else
	    while ((p = p->next) != h->current)
		if (p->options & W_WANT_CURSOR)
		    if (send_message (p, WIDGET_CURSOR, 0) == MSG_HANDLED)
			break;
    }
}

/* Redraw the widgets in reverse order, leaving the current widget
 * as the last one
 */
void
dlg_redraw (Dlg_head *h)
{
    h->callback (h, NULL, DLG_DRAW, 0, NULL);
    dlg_broadcast_msg (h, WIDGET_DRAW, 1);
    update_cursor (h);
}

void
dlg_stop (Dlg_head *h)
{
    h->running = 0;
}

static void
dialog_handle_key (Dlg_head *h, int d_key)
{
    if (is_abort_char (d_key)) {
	h->ret_value = B_CANCEL;
	dlg_stop (h);
	return;
    }

    switch (d_key) {
    case '\n':
    case KEY_ENTER:
	h->ret_value = B_ENTER;
	dlg_stop (h);
	break;

    case KEY_LEFT:
    case KEY_UP:
	dlg_one_up (h);
	break;

    case KEY_RIGHT:
    case KEY_DOWN:
	dlg_one_down (h);
	break;

    case KEY_F(1):
	interactive_display (NULL, h->help_ctx);
	do_refresh ();
	break;

    case XCTRL('z'):
	suspend_cmd ();
	/* Fall through */

    case XCTRL('l'):
#ifdef HAVE_SLANG
	tty_touch_screen ();
	mc_refresh ();
#else
	/* Use this if the refreshes fail */
	clr_scr ();
	repaint_screen ();
#endif /* HAVE_SLANG */
	break;

    default:
	break;
    }
}

static cb_ret_t
dlg_try_hotkey (Dlg_head *h, int d_key)
{
    Widget *hot_cur;
    cb_ret_t handled;
    int c;

    if (h->current == NULL)
	return MSG_NOT_HANDLED;

    /*
     * Explanation: we don't send letter hotkeys to other widgets if
     * the currently selected widget is an input line
     */

    if (h->current->options & W_IS_INPUT) {
        /* skip ascii control characters, anything else can valid character in
         * some encoding */
	if (d_key >= 32 && d_key < 256)
	    return MSG_NOT_HANDLED;
    }

    /* If it's an alt key, send the message */
    c = d_key & ~ALT (0);
    if (d_key & ALT (0) && g_ascii_isalpha (c))
	d_key = g_ascii_tolower (c);

    handled = MSG_NOT_HANDLED;
    if (h->current->options & W_WANT_HOTKEY)
	handled = send_message (h->current, WIDGET_HOTKEY, d_key);

    /* If not used, send hotkey to other widgets */
    if (handled == MSG_HANDLED)
	return MSG_HANDLED;

    hot_cur = h->current->next;

    /* send it to all widgets */
    while (h->current != hot_cur && handled == MSG_NOT_HANDLED) {
	if (hot_cur->options & W_WANT_HOTKEY)
	    handled = send_message (hot_cur, WIDGET_HOTKEY, d_key);

	if (handled == MSG_NOT_HANDLED)
	    hot_cur = hot_cur->next;
    }

    if (handled == MSG_HANDLED)
	do_select_widget (h, hot_cur, SELECT_EXACT);

    return handled;
}

static void
dlg_key_event (Dlg_head *h, int d_key)
{
    cb_ret_t handled;

    if (h->current == NULL)
	return;

    /* TAB used to cycle */
    if (!(h->flags & DLG_WANT_TAB)) {
	if (d_key == '\t') {
	    dlg_one_down (h);
	    return;
	} else if (d_key == KEY_BTAB) {
	    dlg_one_up (h);
	    return;
	}
    }

    /* first can dlg_callback handle the key */
    handled = h->callback (h, NULL, DLG_KEY, d_key, NULL);

    /* next try the hotkey */
    if (handled == MSG_NOT_HANDLED)
	handled = dlg_try_hotkey (h, d_key);

    if (handled == MSG_HANDLED)
	h->callback (h, NULL, DLG_HOTKEY_HANDLED, 0, NULL);
    else
	/* not used - then try widget_callback */
	handled = send_message (h->current, WIDGET_KEY, d_key);

    /* not used- try to use the unhandled case */
    if (handled == MSG_NOT_HANDLED)
	handled = h->callback (h, NULL, DLG_UNHANDLED_KEY, d_key, NULL);

    if (handled == MSG_NOT_HANDLED)
	dialog_handle_key (h, d_key);

    h->callback (h, NULL, DLG_POST_KEY, d_key, NULL);
}

static int
dlg_mouse_event (Dlg_head * h, Gpm_Event * event)
{
    Widget *item;
    Widget *starting_widget = h->current;
    Gpm_Event new_event;
    int x = event->x;
    int y = event->y;

    /* close the dialog by mouse click out of dialog area */
    if (mouse_close_dialog && !h->fullscreen
	&& ((event->buttons & GPM_B_LEFT) != 0) && ((event->type & GPM_DOWN) != 0)  /* left click */
	&& !((x > h->x) && (x <= h->x + h->cols) && (y > h->y) && (y <= h->y + h->lines))) {
	h->ret_value = B_CANCEL;
	dlg_stop (h);
	return MOU_NORMAL;
    }

    item = starting_widget;
    do {
	Widget *widget;

	widget = item;
	item = item->next;

	if ((x > widget->x) && (x <= widget->x + widget->cols)
		&& (y > widget->y) && (y <= widget->y + widget->lines)) {
	    new_event = *event;
	    new_event.x -= widget->x;
	    new_event.y -= widget->y;

	    if (widget->mouse != NULL)
		return widget->mouse (&new_event, widget);
	}
    } while (item != starting_widget);

    return MOU_NORMAL;
}

/* Run dialog routines */

/* Init the process */
void
init_dlg (Dlg_head *h)
{
    /* Initialize dialog manager and widgets */
    h->callback (h, NULL, DLG_INIT, 0, NULL);
    dlg_broadcast_msg (h, WIDGET_INIT, 0);

    if (h->x == 0 && h->y == 0 && h->cols == COLS && h->lines == LINES)
	h->fullscreen = 1;

    h->parent = current_dlg;
    current_dlg = h;

    /* Initialize the mouse status */
    h->mouse_status = MOU_NORMAL;

    /* Select the first widget that takes focus */
    while (!dlg_focus (h) && h->current)
	h->current = h->current->next;

    /* Redraw the screen */
    dlg_redraw (h);

    h->ret_value = 0;
    h->running = 1;
}

/* Shutdown the run_dlg */
void
dlg_run_done (Dlg_head *h)
{
    if (h->current != NULL)
	h->callback (h, h->current, DLG_END, 0, NULL);

    current_dlg = h->parent;
}

void
dlg_process_event (Dlg_head *h, int key, Gpm_Event *event)
{
    if (key == EV_NONE){
	if (tty_got_interrupt ())
	    key = XCTRL('g');
	else
	    return;
    }

    if (key == EV_MOUSE)
	h->mouse_status = dlg_mouse_event (h, event);
    else
	dlg_key_event (h, key);
}

static void
frontend_run_dlg (Dlg_head *h)
{
    int d_key;
    Gpm_Event event;

    event.x = -1;
    while (h->running) {
	if (winch_flag)
	    change_screen_size ();

	if (is_idle ()) {
	    if (idle_hook)
		execute_hooks (idle_hook);

	    while ((h->flags & DLG_WANT_IDLE) && is_idle ())
		h->callback (h, NULL, DLG_IDLE, 0, NULL);

	    /* Allow terminating the dialog from the idle handler */
	    if (!h->running)
		break;
	}

	update_cursor (h);

	/* Clear interrupt flag */
	tty_got_interrupt ();
	d_key = tty_get_event (&event, h->mouse_status == MOU_REPEAT, TRUE);

	dlg_process_event (h, d_key, &event);

	if (!h->running)
	    h->callback (h, NULL, DLG_VALIDATE, 0, NULL);
    }
}

/* Standard run dialog routine
 * We have to keep this routine small so that we can duplicate it's
 * behavior on complex routines like the file routines, this way,
 * they can call the dlg_process_event without rewriting all the code
 */
int run_dlg (Dlg_head *h)
{
    init_dlg (h);
    frontend_run_dlg (h);
    dlg_run_done (h);
    return h->ret_value;
}

void
destroy_dlg (Dlg_head *h)
{
    int i;
    Widget *c;

    dlg_broadcast_msg (h, WIDGET_DESTROY, 0);
    c = h->current;
    for (i = 0; i < h->count; i++) {
	c = c->next;
	g_free (h->current);
	h->current = c;
    }
    g_free (h->color);
    g_free (h->title);
    g_free (h);

    do_refresh ();
}

void widget_set_size (Widget *widget, int y, int x, int lines, int cols)
{
    widget->x = x;
    widget->y = y;
    widget->cols = cols;
    widget->lines = lines;
    send_message (widget, WIDGET_RESIZED, 0 /* unused */);
}

/* Replace widget old_w for widget new_w in the dialog */
void
dlg_replace_widget (Widget *old_w, Widget *new_w)
{
    Dlg_head *h = old_w->parent;
    int should_focus = 0;

    if (!h->current)
	return;

    if (old_w == h->current)
	should_focus = 1;

    new_w->parent = h;
    new_w->dlg_id = old_w->dlg_id;

    if (old_w == old_w->next) {
	/* just one widget */
	new_w->prev = new_w;
	new_w->next = new_w;
    } else {
	new_w->prev = old_w->prev;
	new_w->next = old_w->next;
	old_w->prev->next = new_w;
	old_w->next->prev = new_w;
    }

    if (should_focus)
	h->current = new_w;

    send_message (old_w, WIDGET_DESTROY, 0);
    send_message (new_w, WIDGET_INIT, 0);

    if (should_focus)
	dlg_select_widget (new_w);

    send_message (new_w, WIDGET_DRAW, 0);
}
