/* Dialog box features module for the Midnight Commander
   Copyright (C) 1994, 1995 Radek Doulik, Miguel de Icaza

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "global.h"
#include "tty.h"
#include "mouse.h"
#include "help.h"	/* interactive_display() */
#include "key.h"	/* mi_getch() */
#include "dialog.h"
#include "layout.h"	/* winch_flag */
#include "execute.h"	/* suspend_cmd() */
#include "main.h"	/* slow_terminal */

#define waddc(w,y1,x1,c) move (w->y+y1, w->x+x1); addch (c)

/* Primitive way to check if the the current dialog is our dialog */
/* This is needed by async routines like load_prompt */
Dlg_head *current_dlg = 0;

/* A hook list for idle events */
Hook *idle_hook = 0;

static void dlg_broadcast_msg_to (Dlg_head * h, widget_msg_t message,
				  int reverse, int flags);

static void slow_box (Dlg_head *h, int y, int x, int ys, int xs)
{
    move (h->y+y, h->x+x);
    hline (' ', xs);
    vline (' ', ys);
    move (h->y+y, h->x+x+xs-1);
    vline (' ', ys);
    move (h->y+y+ys-1, h->x+x);
    hline (' ', xs);
}

/* draw box in window */
void draw_box (Dlg_head *h, int y, int x, int ys, int xs)
{
    if (slow_terminal){
	slow_box (h, y, x, ys, xs);
	return;
    }

#ifndef HAVE_SLANG
    waddc (h, y, x, ACS_ULCORNER);
    hline (ACS_HLINE, xs - 2);
    waddc (h, y + ys - 1, x, ACS_LLCORNER);
    hline (ACS_HLINE, xs - 2);

    waddc (h, y, x + xs - 1, ACS_URCORNER);
    waddc (h, y + ys - 1, x + xs - 1, ACS_LRCORNER);

    move (h->y+y+1, h->x+x);
    vline (ACS_VLINE, ys - 2);
    move (h->y+y+1, h->x+x+xs-1);
    vline (ACS_VLINE, ys - 2);
#else
    SLsmg_draw_box (h->y+y, h->x+x, ys, xs);
#endif /* HAVE_SLANG */
}

/* draw box in window */
void draw_double_box (Dlg_head *h, int y, int x, int ys, int xs)
{
#ifndef HAVE_SLANG
    draw_box (h, y, x, ys, xs);
#else
    SLsmg_draw_double_box (h->y+y, h->x+x, ys, xs);
#endif /* HAVE_SLANG */
}

void widget_erase (Widget *w)
{
    int x, y;

    for (y = 0; y < w->lines; y++){
	widget_move (w, y, 0);
	for (x = 0; x < w->cols; x++)
	    addch (' ');
    }
}

void dlg_erase (Dlg_head *h)
{
    int x, y;

    for (y = 0; y < h->lines; y++){
	move (y+h->y, h->x);	/* FIXME: should test if ERR */
	for (x = 0; x < h->cols; x++){
	    addch (' ');
	}
    }
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

/* Default callback for widgets */
cb_ret_t
default_proc (widget_msg_t msg, int parm)
{
    switch (msg) {
    case WIDGET_INIT:
    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
    case WIDGET_DESTROY:
    case WIDGET_CURSOR:
    case WIDGET_IDLE:
	return MSG_HANDLED;

    default:
	return MSG_NOT_HANDLED;
    }
}

/* Clean the dialog area, draw the frame and the title */
void
common_dialog_repaint (struct Dlg_head *h)
{
    int space;

    space = (h->flags & DLG_COMPACT) ? 0 : 1;

    attrset (NORMALC);
    dlg_erase (h);
    draw_box (h, space, space, h->lines - 2 * space, h->cols - 2 * space);

    if (h->title) {
	attrset (HOT_NORMALC);
	dlg_move (h, space, (h->cols - strlen (h->title)) / 2);
	addstr (h->title);
    }
}

/* Default dialog callback */
cb_ret_t default_dlg_callback (Dlg_head *h, dlg_msg_t msg, int parm)
{
    if (msg == DLG_DRAW && h->color) {
	common_dialog_repaint (h);
	return MSG_HANDLED;
    }
    if (msg == DLG_IDLE){
	dlg_broadcast_msg_to (h, WIDGET_IDLE, 0, W_WANT_IDLE);
	return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

Dlg_head *
create_dlg (int y1, int x1, int lines, int cols, const int *color_set,
	    dlg_cb_fn callback, const char *help_ctx, const char *title,
	    int flags)
{
    Dlg_head *new_d;

    if (flags & DLG_CENTER) {
	y1 = (LINES - lines) / 2;
	x1 = (COLS - cols) / 2;
    }

    if ((flags & DLG_TRYUP) && (y1 > 3))
	y1 -= 2;

    new_d = g_new0 (Dlg_head, 1);
    new_d->color = color_set;
    new_d->help_ctx = const_cast(char *, help_ctx);
    new_d->callback = callback ? callback : default_dlg_callback;
    new_d->x = x1;
    new_d->y = y1;
    new_d->cols = cols;
    new_d->lines = lines;
    new_d->flags = flags;

    /* Strip existing spaces, add one space before and after the title */
    if (title) {
	char *t;
	t = g_strstrip (g_strdup (title));
	new_d->title = g_strconcat (" ", t, " ", NULL);
	g_free (t);
    }

    return (new_d);
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
add_widget (Dlg_head *h, void *w)
{
    Widget *widget = (Widget *) w;

    /* Don't accept 0 widgets, and running dialogs */
    if (!widget || h->running)
	abort ();

    widget->x += h->x;
    widget->y += h->y;
    widget->parent = h;
    widget->dlg_id = h->count++;

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

enum {
    REFRESH_COVERS_PART,	/* If the refresh fn convers only a part */
    REFRESH_COVERS_ALL		/* If the refresh fn convers all the screen */
};

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
 * the options set to flags.
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
	send_message (wi, message, 0);
    } while (first != p);
}

/* broadcast a message to all the widgets in a dialog */
void
dlg_broadcast_msg (Dlg_head *h, widget_msg_t message, int reverse)
{
    dlg_broadcast_msg_to (h, message, reverse, ~0);
}

int dlg_focus (Dlg_head *h)
{
    if (!h->current)
        return 0;

    if (send_message (h->current, WIDGET_FOCUS, 0)){
	(*h->callback) (h, DLG_FOCUS, 0);
	return 1;
    }
    return 0;
}

static int
dlg_unfocus (Dlg_head *h)
{
    if (!h->current)
        return 0;

    if (send_message (h->current, WIDGET_UNFOCUS, 0)){
	(*h->callback) (h, DLG_UNFOCUS, 0);
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
find_widget_type (Dlg_head *h, void *callback)
{
    Widget *w;
    Widget *item;
    int i;

    if (!h)
	return 0;
    if (!h->current)
	return 0;

    w = 0;
    for (i = 0, item = h->current; i < h->count; i++, item = item->next) {
	if (item->callback == callback) {
	    w = item;
	    break;
	}
    }
    return w;
}

/* Find the widget with the given dialog id in the dialog h and select it */
void
dlg_select_by_id (Dlg_head *h, int id)
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
	dlg_select_widget(h, w_found);
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
    while (h->current != w0) {
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
    }

    if (dlg_overlap (w0, h->current)) {
	send_message (h->current, WIDGET_DRAW, 0);
	send_message (h->current, WIDGET_FOCUS, 0);
    }
}


/*
 * Try to select widget in the dialog.
 */
void
dlg_select_widget (Dlg_head *h, void *w)
{
    do_select_widget (h, w, SELECT_NEXT);
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
    if (!h->current)
         return;
    if (h->current->options & W_WANT_CURSOR)
	send_message (h->current, WIDGET_CURSOR, 0);
    else {
	Widget *p = h->current;

	do {
	    if (p->options & W_WANT_CURSOR)
		if ((*p->callback)(p, WIDGET_CURSOR, 0)){
		    break;
		}
	    p = p->next;
	} while (h->current != p);
    }
}

/* Redraw the widgets in reverse order, leaving the current widget
 * as the last one
 */
void dlg_redraw (Dlg_head *h)
{
    (h->callback)(h, DLG_DRAW, 0);

    dlg_broadcast_msg (h, WIDGET_DRAW, 1);

    update_cursor (h);
}

void dlg_stop (Dlg_head *h)
{
    h->running = 0;
}

static inline void dialog_handle_key (Dlg_head *h, int d_key)
{
    switch (d_key){
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
#ifndef HAVE_SLANG
	/* Use this if the refreshes fail */
	clr_scr ();
	do_refresh ();
#else
	touchwin (stdscr);
#endif /* HAVE_SLANG */
	mc_refresh ();
	doupdate ();
	break;

    case '\n':
    case KEY_ENTER:
	h->ret_value = B_ENTER;
	h->running = 0;
	break;

    case ESC_CHAR:
    case KEY_F (10):
    case XCTRL ('c'):
    case XCTRL ('g'):
	h->ret_value = B_CANCEL;
	dlg_stop (h);
	break;
    }
}

static int
dlg_try_hotkey (Dlg_head *h, int d_key)
{
    Widget *hot_cur;
    int handled, c;

    if (!h->current)
	return 0;

    /*
     * Explanation: we don't send letter hotkeys to other widgets if
     * the currently selected widget is an input line
     */

    if (h->current->options & W_IS_INPUT) {
	if (d_key < 255 && isalpha (d_key))
	    return 0;
    }

    /* If it's an alt key, send the message */
    c = d_key & ~ALT (0);
    if (d_key & ALT (0) && c < 255 && isalpha (c))
	d_key = tolower (c);

    handled = 0;
    if (h->current->options & W_WANT_HOTKEY)
	handled = h->current->callback (h->current, WIDGET_HOTKEY, d_key);

    /* If not used, send hotkey to other widgets */
    if (handled)
	return handled;

    hot_cur = h->current;

    /* send it to all widgets */
    do {
	if (hot_cur->options & W_WANT_HOTKEY)
	    handled |=
		(*hot_cur->callback) (hot_cur, WIDGET_HOTKEY, d_key);

	if (!handled)
	    hot_cur = hot_cur->next;
    } while (h->current != hot_cur && !handled);

    if (!handled)
	return 0;

    do_select_widget (h, hot_cur, SELECT_EXACT);
    return handled;
}

static void
dlg_key_event (Dlg_head *h, int d_key)
{
    int handled;

    if (!h->current)
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
    handled = (*h->callback) (h, DLG_KEY, d_key);

    /* next try the hotkey */
    if (!handled)
	handled = dlg_try_hotkey (h, d_key);

    if (handled)
	(*h->callback) (h, DLG_HOTKEY_HANDLED, 0);

    /* not used - then try widget_callback */
    if (!handled)
	handled = h->current->callback (h->current, WIDGET_KEY, d_key);

    /* not used- try to use the unhandled case */
    if (!handled)
	handled = (*h->callback) (h, DLG_UNHANDLED_KEY, d_key);

    if (!handled)
	dialog_handle_key (h, d_key);

    (*h->callback) (h, DLG_POST_KEY, d_key);
}

static inline int
dlg_mouse_event (Dlg_head * h, Gpm_Event * event)
{
    Widget *item;
    Widget *starting_widget = h->current;
    Gpm_Event new_event;
    int x = event->x;
    int y = event->y;

    item = starting_widget;
    do {
	Widget *widget = item;

	item = item->next;

	if (!((x > widget->x) && (x <= widget->x + widget->cols)
	      && (y > widget->y) && (y <= widget->y + widget->lines)))
	    continue;

	new_event = *event;
	new_event.x -= widget->x;
	new_event.y -= widget->y;

	if (!widget->mouse)
	    return MOU_NORMAL;

	return (*widget->mouse) (&new_event, widget);
    } while (item != starting_widget);

    return MOU_NORMAL;
}

/* Run dialog routines */

/* Init the process */
void init_dlg (Dlg_head *h)
{
    /* Initialize dialog manager and widgets */
    (*h->callback) (h, DLG_INIT, 0);
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
void dlg_run_done (Dlg_head *h)
{
    if (h->current)
	(*h->callback) (h, DLG_END, 0);

    current_dlg = h->parent;
}

void dlg_process_event (Dlg_head *h, int key, Gpm_Event *event)
{
    if (key == EV_NONE){
	if (got_interrupt ())
	    key = XCTRL('g');
	else
	    return;
    }

    if (key == EV_MOUSE)
	h->mouse_status = dlg_mouse_event (h, event);
    else
	dlg_key_event (h, key);
}

static inline void
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
		(*h->callback) (h, DLG_IDLE, 0);

	    /* Allow terminating the dialog from the idle handler */
	    if (!h->running)
		break;
	}

	update_cursor (h);

	/* Clear interrupt flag */
	got_interrupt ();
	d_key = get_event (&event, h->mouse_status == MOU_REPEAT, 1);

	dlg_process_event (h, d_key, &event);

	if (!h->running)
	    (*h->callback) (h, DLG_VALIDATE, 0);
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
	if (h->current) {
	    g_free (h->current);
	}
	h->current = c;
    }
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
}

/* Replace widget old_w for widget new_w in the dialog h */
void
dlg_replace_widget (Dlg_head *h, Widget *old_w, Widget *new_w)
{
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
	dlg_select_widget (h, new_w);

    send_message (new_w, WIDGET_DRAW, 0);
}
