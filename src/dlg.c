/* Dlg box features module for the Midnight Commander
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
#include "menu.h"
#include "win.h"
#include "color.h"
#include "mouse.h"
#include "help.h"
#include "key.h"	/* For mi_getch() */
#include "dlg.h"
#include "dialog.h"	/* For push_refresh() and pop_refresh() */
#include "layout.h"
#include "execute.h"
#include "main.h"

#define waddc(w,y1,x1,c) move (w->y+y1, w->x+x1); addch (c)

/* Primitive way to check if the the current dialog is our dialog */
/* This is needed by async routines like load_prompt */
Dlg_head *current_dlg = 0;

/* A hook list for idle events */
Hook *idle_hook = 0;

static void dlg_broadcast_msg_to (Dlg_head *h, int message, int reverse, int flags);

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
int default_proc (int Msg, int Par)
{
    switch (Msg){

    case WIDGET_HOTKEY:         /* Didn't use the key */
        return 0;

    case WIDGET_INIT:		/* We could tell if something went wrong */
	return 1;

    case WIDGET_KEY:
	return 0;		/* Didn't use the key */

    case WIDGET_FOCUS:		/* We accept FOCUSes */
	return 1;

    case WIDGET_UNFOCUS:	/* We accept loose FOCUSes */
	return 1;

    case WIDGET_DRAW:
	return 1;

    case WIDGET_DESTROY:
	return 1;

    case WIDGET_CURSOR:
	/* Move the cursor to the default widget position */
	return 1;

    case WIDGET_IDLE:
	return 1;
    }
    printf ("Internal error: unhandled message: %d\n", Msg);
    return 1;
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
	    dlg_cb_fn callback, char *help_ctx, const char *title,
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
    new_d->help_ctx = help_ctx;
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

/* add component to dialog buffer */
int add_widget (Dlg_head *where, void *what)
{
    Widget_Item *back;
    Widget      *widget = (Widget *) what;

    /* Don't accept 0 widgets, this could be from widgets that could not */
    /* initialize properly */
    if (!what)
	return 0;

    widget->x += where->x;
    widget->y += where->y;

    if (where->running){
	    Widget_Item *point = where->current;

	    where->current = g_new (Widget_Item, 1);

	    if (point){
		    where->current->next = point->next;
		    where->current->prev = point;
		    point->next->prev = where->current;
		    point->next = where->current;
	    } else {
		    where->current->next = where->current;
		    where->first = where->current;
		    where->current->prev = where->first;
		    where->last = where->current;
		    where->first->next = where->last;
	    }
    } else {
	    back = where->current;
	    where->current = g_new (Widget_Item, 1);
	    if (back){
		    back->prev = where->current;
		    where->current->next = back;
	    } else {
		    where->current->next = where->current;
		    where->first = where->current;
	    }

	    where->current->prev = where->first;
	    where->last = where->current;
	    where->first->next = where->last;

    }
    where->current->dlg_id = where->count;
    where->current->widget = what;
    where->current->widget->parent = where;

    where->count++;

    /* If the widget is inserted in a running dialog */
    if (where->running) {
	send_message (widget, WIDGET_INIT, 0);
	send_message (widget, WIDGET_DRAW, 0);
    }
    return (where->count - 1);
}

/* broadcast a message to all the widgets in a dialog that have
 * the options set to flags.
 */
static void
dlg_broadcast_msg_to (Dlg_head *h, int message, int reverse, int flags)
{
    Widget_Item *p, *first, *wi;

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
	send_message (wi->widget, message, 0);
    } while (first != p);
}

/* broadcast a message to all the widgets in a dialog */
void dlg_broadcast_msg (Dlg_head *h, int message, int reverse)
{
    dlg_broadcast_msg_to (h, message, reverse, ~0);
}

int dlg_focus (Dlg_head *h)
{
    if (!h->current)
        return 0;

    if (send_message (h->current->widget, WIDGET_FOCUS, 0)){
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

    if (send_message (h->current->widget, WIDGET_UNFOCUS, 0)){
	(*h->callback) (h, DLG_UNFOCUS, 0);
	return 1;
    }
    return 0;
}

static void select_a_widget (Dlg_head *h, int down)
{
    int dir_forward = !(h->flags & DLG_BACKWARD);

    if (!h->current)
       return;

    if (!down)
	dir_forward = !dir_forward;

    do {
	if (dir_forward)
	    h->current = h->current->next;
	else
	    h->current = h->current->prev;
    } while (!dlg_focus (h));
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
    Widget_Item *item;
    int i;

    if (!h)
	return 0;
    if (!h->current)
	return 0;

    w = 0;
    for (i = 0, item = h->current; i < h->count; i++, item = item->next) {
	if (item->widget->callback == callback) {
	    w = item->widget;
	    break;
	}
    }
    return w;
}

void dlg_one_up (Dlg_head *h)
{
    Widget_Item *old;

    old = h->current;

    if (!old)
        return;

    /* If it accepts unFOCUSion */
    if (!dlg_unfocus(h))
	return;

    select_a_widget (h, 0);
    if (dlg_overlap (old->widget, h->current->widget)){
	send_message (h->current->widget, WIDGET_DRAW, 0);
	send_message (h->current->widget, WIDGET_FOCUS, 0);
    }
}

void dlg_one_down (Dlg_head *h)
{
    Widget_Item *old;

    old = h->current;
    if (!old)
        return;

    if (!dlg_unfocus (h))
	return;

    select_a_widget (h, 1);
    if (dlg_overlap (old->widget, h->current->widget)){
	send_message (h->current->widget, WIDGET_DRAW, 0);
	send_message (h->current->widget, WIDGET_FOCUS, 0);
    }
}

int dlg_select_widget (Dlg_head *h, void *w)
{
    if (!h->current)
       return 0;

    if (dlg_unfocus (h)){
	while (h->current->widget != w)
	    h->current = h->current->next;
	while (!dlg_focus (h))
	    h->current = h->current->next;

	return 1;
    }
    return 0;
}

#define callback(h) (h->current->widget->callback)

void update_cursor (Dlg_head *h)
{
    if (!h->current)
         return;
    if (h->current->widget->options & W_WANT_CURSOR)
	send_message (h->current->widget, WIDGET_CURSOR, 0);
    else {
	Widget_Item *p = h->current;

	do {
	    if (p->widget->options & W_WANT_CURSOR)
		if ((*p->widget->callback)(p->widget, WIDGET_CURSOR, 0)){
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

static void
dlg_refresh (void *parameter)
{
    dlg_redraw ((Dlg_head *) parameter);
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

static int dlg_try_hotkey (Dlg_head *h, int d_key)
{
    Widget_Item *hot_cur;
    Widget_Item *previous;
    int    handled, c;

    if (!h->current)
        return 0;

    /*
     * Explanation: we don't send letter hotkeys to other widgets if
     * the currently selected widget is an input line
     */

    if (h->current->widget->options & W_IS_INPUT){
		if(d_key < 255 && isalpha(d_key))
			return 0;
    }

    /* If it's an alt key, send the message */
    c = d_key & ~ALT(0);
    if (d_key & ALT(0) && c < 255 && isalpha(c))
	d_key = tolower(c);

    handled = 0;
    if (h->current->widget->options & W_WANT_HOTKEY)
	handled = callback (h) (h->current->widget, WIDGET_HOTKEY, d_key);

    /* If not used, send hotkey to other widgets */
    if (handled)
	return handled;

    hot_cur = h->current;

    /* send it to all widgets */
    do {
	if (hot_cur->widget->options & W_WANT_HOTKEY)
	    handled |= (*hot_cur->widget->callback)
		(hot_cur->widget, WIDGET_HOTKEY, d_key);

	if (!handled)
	    hot_cur = hot_cur->next;
    } while (h->current != hot_cur && !handled);

    if (!handled)
	return 0;

    previous = h->current;
    if (!dlg_unfocus (h))
	return handled;

    h->current = hot_cur;
    if (!dlg_focus (h)){
	h->current = previous;
	dlg_focus (h);
    }
    return handled;
}

static int
dlg_key_event (Dlg_head *h, int d_key)
{
    int handled;

    if (!h->current)
	return 0;

    /* TAB used to cycle */
    if (!(h->flags & DLG_WANT_TAB)
	&& (d_key == '\t' || d_key == KEY_BTAB)) {
	if (d_key == '\t')
	    dlg_one_down (h);
	else
	    dlg_one_up (h);
    } else {

	/* first can dlg_callback handle the key */
	handled = (*h->callback) (h, DLG_KEY, d_key);

	/* next try the hotkey */
	if (!handled)
	    handled = dlg_try_hotkey (h, d_key);

	if (handled)
	    (*h->callback) (h, DLG_HOTKEY_HANDLED, 0);

	/* not used - then try widget_callback */
	if (!handled)
	    handled = callback (h) (h->current->widget, WIDGET_KEY, d_key);

	/* not used- try to use the unhandled case */
	if (!handled)
	    handled = (*h->callback) (h, DLG_UNHANDLED_KEY, d_key);

	if (!handled)
	    dialog_handle_key (h, d_key);
	(*h->callback) (h, DLG_POST_KEY, d_key);

	return handled;
    }
    return 1;
}

static inline int
dlg_mouse_event (Dlg_head * h, Gpm_Event * event)
{
    Widget_Item *item;
    Widget_Item *starting_widget = h->current;
    Gpm_Event new_event;
    int x = event->x;
    int y = event->y;

    /* kludge for the menubar: start at h->first, not current  */
    /* Must be careful in the insertion order to the dlg list */
    if (y == 1 && (h->flags & DLG_HAS_MENUBAR))
	starting_widget = h->first;

    item = starting_widget;
    do {
	Widget *widget = item->widget;

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
    int refresh_mode;

    /* Initialize dialog manager and widgets */
    (*h->callback) (h, DLG_INIT, 0);
    dlg_broadcast_msg (h, WIDGET_INIT, 0);

    if (h->x == 0 && h->y == 0 && h->cols == COLS && h->lines == LINES)
	refresh_mode = REFRESH_COVERS_ALL;
    else
	refresh_mode = REFRESH_COVERS_PART;

    push_refresh (dlg_refresh, h, refresh_mode);
    h->refresh_pushed = 1;

    /* Initialize direction */
    if (h->flags & DLG_BACKWARD)
	h->current =  h->first;

    if (h->initfocus != NULL)
        h->current = h->initfocus;

    h->previous_dialog = current_dlg;
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

    current_dlg = (Dlg_head *) h->previous_dialog;
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
    Widget_Item *c;

    if (h->refresh_pushed)
	pop_refresh ();

    dlg_broadcast_msg (h, WIDGET_DESTROY, 0);
    c = h->current;
    for (i = 0; i < h->count; i++){
	c = c->next;
	if (h->current){
	    g_free (h->current->widget);
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

/* Replace widget old for widget new in the h dialog */
void dlg_replace_widget (Dlg_head *h, Widget *old, Widget *new)
{
    Widget_Item *p = h->current;
    int should_focus = 0;

    if (!h->current)
        return;

    do {
	if (p->widget == old){

	    if (old == h->current->widget)
		should_focus = 1;

	    /* We found the widget */
	    /* First kill the widget */
	    new->parent  = h;
	    send_message (old, WIDGET_DESTROY, 0);

	    /* We insert the new widget */
	    p->widget = new;
	    send_message (new, WIDGET_INIT, 0);
	    if (should_focus){
		if (dlg_focus (h) == 0)
		    select_a_widget (h, 1);
	    }
	    send_message (new, WIDGET_DRAW, 0);
	    break;
	}
	p = p->next;
    } while (p != h->current);
}

/* Returns the index of h->current from h->first */
int dlg_item_number (Dlg_head *h)
{
    Widget_Item *p;
    int i = 0;

    p = h->first;

    do {
	if (p == h->current)
	    return i;
	i++;
	p = p->next;
    } while (p != h->first);
    fprintf (stderr, "Internal error: current not in dialog list\n\r");
    exit (1);
}

int dlg_select_nth_widget (Dlg_head *h, int n)
{
    Widget_Item *w;
    int i;

    w = h->first;
    for (i = 0; i < n; i++)
	w = w->next;

    return dlg_select_widget (h, w->widget);
}

