/* mousemark.c - mouse stuff marking dragging

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#ifndef GTK
#include "coolwidget.h"
#endif

int just_dropped_something = 0;

#include "edit.h"
#include <X11/Xmd.h>
#include <X11/Xatom.h>
#ifndef GTK
#include "app_glob.c"
#include "coollocal.h"
#endif
#include "editcmddef.h"
#include "xdnd.h"
#ifdef GTK
#include "src/mad.h"
#else
#include "mad.h"
#endif

#ifndef HAVE_DND

Atom **xdnd_typelist_send = 0;
Atom **xdnd_typelist_receive = 0;

#define NUM_SIMPLE_TYPES 10

/* each entry is listed in order of what we would like the RECEIVE */
static char *mime_type_send[NUM_SIMPLE_TYPES][10] =
{
    {0},			/* DndUnknown 0 */
    {0},			/* DndRawData 1 */
    {"url/url", "text/plain", 0},	/* DndFile    2 (input widget) */
    {"text/plain", "text/uri-list", 0},	/* DndFiles   3 (fielded text box) */
    {"text/plain", "application/octet-stream", 0},	/* DndText    4 (editor widget) */
/* don't use these: */
    {0},			/* DndLink    5 */
    {0},			/* DndDir     6 */
    {0},			/* DndExe     7 */
    {0},			/* DndURL     8 */
    {0},			/* DndMIME    9 */
};

static char *mime_type_recieve[NUM_SIMPLE_TYPES][10] =
{
    {0},	/* DndUnknown 0 */
    {0},	/* DndRawData 1 */
    {"url/url", "text/uri-list", "text/plain", 0},	/* DndFile    2 (input widget) */
    {0},	/* DndFiles   3 */
    {"url/url", "text/plain", "application/octet-stream", "text/uri-list", 0},	/* DndText    4 (editor widget) */
/* don't use these: */
    {0},			/* DndLink    5 */
    {0},			/* DndDir     6 */
    {0},			/* DndExe     7 */
    {0},			/* DndURL     8 */
    {0},			/* DndMIME    9 */
};

#ifndef GTK

static char dnd_directory[MAX_PATH_LEN] = "/";

char *striptrailing (char *s, int c)
{
    int i;
    i = strlen (s) - 1;

    while (i >= 0) {
	if (s[i] == c) {
	    s[i--] = 0;
	    continue;
	}
	break;
    }
    return s;
}

/* return the prepending directory (see CDndFileList() below) */
char *CDndDirectory (void)
{
    return dnd_directory;
}

/*
   Sets the directory, must be a null terminated complete path.
   Strips trailing slashes.
 */
void CSetDndDirectory (char *d)
{
    if (!d)
	return;
    strcpy (dnd_directory, d);
    striptrailing (dnd_directory, '/');
    if (*dnd_directory)
	return;
    *dnd_directory = '/';
}

/*
   Takes a newline seperated list of files,
   returns a newline seperated list of complete `file:' path names
   by prepending dnd_directory to each file name.
   returns 0 if no files in list.
   result must always be free'd.
   returns l as the total length of data.
   returns num_files as the number of files in the list.
   Alters t
 */
char *CDndFileList (char *t, int *l, int *num_files)
{
    char *p, *q, *r, *result;
    int i, len, done = 0;

/* strip leading newlines */
    while (*t == '\n')
	t++;

/* strip trailing newlines */
    striptrailing (t, '\n');

    if (!*t)
	return 0;

/* count files */
    for (i = 1, p = t; *p; p++)
	if (*p == '\n')
	    i++;

    *num_files = i;

    len = (unsigned long) p - (unsigned long) t;
    result = CMalloc ((strlen (dnd_directory) + strlen ("file:") + 2) * i + len + 2);

    r = result;
    p = t;
    while (!done) {
	q = strchr (p, '\n');
	if (q)
	    *q = 0;
	else
	    done = 1;
	strcpy (r, "file:");
	if (*p != '/') {
	    strcat (r, dnd_directory);
	    strcat (r, "/");
	}
	strcat (r, p);
	r += strlen (r);
	*r++ = '\n';
	p = ++q;
    }
    *r = 0;
    *l = (unsigned long) r - (unsigned long) result;
    return result;
}

Window get_focus_border_widget (void);

static void widget_apply_leave (DndClass * dnd, Window widgets_window)
{
    CWidget *w;
    w = CWidgetOfWindow (widgets_window);
    if (get_focus_border_widget () == widgets_window)
	destroy_focus_border ();
    if (w)
	CExpose (w->ident);
}

static int widget_insert_drop (DndClass * dnd, unsigned char *data, int length, int remaining, Window into, Window from, Atom type)
{
    CWidget *w;
    char *p;
    w = CWidgetOfWindow (into);
    if (!w)
	return 1;
    if (*w->funcs->insert_drop) {
	int r;
	Window child_return;
	int xd, yd;
	if (!dnd->user_hook1)
	    dnd->user_hook2 = dnd->user_hook1 = CMalloc (length + remaining + 1);
	memcpy (dnd->user_hook2, data, length);
	p = dnd->user_hook2;
	p += length;	/* avoids ansi warning */
	dnd->user_hook2 = p;
	if (remaining)
	    return 0;
	XTranslateCoordinates (CDisplay, CRoot, into, dnd->x, dnd->y, &xd, &yd, &child_return);
	r = (*w->funcs->insert_drop) (w->funcs->data, from, dnd->user_hook1, (unsigned long) dnd->user_hook2 - (unsigned long) dnd->user_hook1, xd, yd, type, dnd->supported_action);
	free (dnd->user_hook1);
	dnd->user_hook1 = dnd->user_hook2 = 0;
	if (get_focus_border_widget () == into)
	    destroy_focus_border ();
	CExpose (w->ident);
	return r;
    }
    return 1;
}

static int array_length (Atom * a)
{
    int n;
    for (n = 0; a[n]; n++);
    return n;
}

static int widget_apply_position (DndClass * dnd, Window widgets_window, Window from,
		      Atom action, int x, int y, Time t, Atom * typelist,
	int *want_position, Atom * supported_action, Atom * desired_type,
				  XRectangle * rectangle)
{
    CWidget *w;
    Window child_return;
    int xt, yt;
    int xd, yd;
    long click;
    int i, j;
    Atom result = 0;

    w = CWidgetOfWindow (widgets_window);
    if (!w)
	return 0;

/* input widgets can't drop to themselves :-( */
    if (w->kind == C_TEXTINPUT_WIDGET && widgets_window == from)
	return 0;

/* look up mime types from my own list of supported types */
    for (j = 0; xdnd_typelist_receive[w->funcs->types][j]; j++) {
	for (i = 0; typelist[i]; i++) {
	    if (typelist[i] == xdnd_typelist_receive[w->funcs->types][j]) {
		result = typelist[i];
		break;
	    }
	}
	if (result)
	    break;
    }

    if (!result && w->funcs->mime_majors) {
	char **names_return;
	names_return = CMalloc ((array_length (typelist) + 1) * sizeof (char *));
	if (XGetAtomNames (CDisplay, typelist, array_length (typelist), names_return)) {
	    for (i = 0; i < array_length (typelist); i++) {
		for (j = 0; w->funcs->mime_majors[j]; j++) {
		    if (!strncmp (w->funcs->mime_majors[j], names_return[i], strlen (w->funcs->mime_majors[j]))) {
			result = typelist[i];
			break;
		    }
		}
		if (result)
		    break;
	    }
	}
    }
/* not supported, so return false */
    if (!result)
	return 0;

    XTranslateCoordinates (CDisplay, CRoot, widgets_window, x, y, &xd, &yd, &child_return);
    if (xd < 0 || yd < 0 || xd >= CWidthOf (w) || yd >= CHeightOf (w))
	return 0;

    if (w->funcs->xy && w->funcs->cp && w->funcs->move) {
	(*w->funcs->xy) (xd, yd, &xt, &yt);
	click = (*w->funcs->cp) (w->funcs->data, xt, yt);
	if (w->funcs->fin_mark)
	    (*w->funcs->fin_mark) (w->funcs->data);
	if (w->funcs->move)
	    (*w->funcs->move) (w->funcs->data, click, yt);
	if (w->funcs->redraw)
	    (*w->funcs->redraw) (w->funcs->data, click);
    }
/* we want more position messages */
    *want_position = 1;

/* we only support copy and move */
    if (action == dnd->XdndActionMove) {
	*supported_action = dnd->XdndActionMove;
    } else {
	*supported_action = dnd->XdndActionCopy;
    }
    *desired_type = result;
    rectangle->x = x;
    rectangle->y = y;
    rectangle->width = rectangle->height = 0;
    if (get_focus_border_widget () != widgets_window) {
	destroy_focus_border ();
	create_focus_border (w, 4);
    }
    CExpose (w->ident);
    return 1;
}

static void widget_get_data (DndClass * dnd, Window window, unsigned char **data, int *length, Atom type)
{
    int t = DndText;
    long start_mark, end_mark;
    CWidget *w;
    w = CWidgetOfWindow (window);
    if (!w) {
	return;
    }
    if (!w->funcs)
	return;
    if ((*w->funcs->marks) (w->funcs->data, &start_mark, &end_mark)) {
	return;
    }
    if (type == XInternAtom (dnd->display, "url/url", False))
	t = DndFile;
    else if (type == XInternAtom (dnd->display, "text/uri-list", False))
	t = DndFiles;
    *data = (unsigned char *) (*w->funcs->get_block) (w->funcs->data, start_mark, end_mark, &t, length);
}

static int widget_exists (DndClass * dnd, Window window)
{
    return (CWidgetOfWindow (window) != 0);
}

static void handle_expose_events (DndClass * dnd, XEvent * xevent)
{
    if (!xevent->xexpose.count)
	render_focus_border (xevent->xexpose.window);
    return;
}


void mouse_init (void)
{
    CDndClass->handle_expose_events = handle_expose_events;
    CDndClass->widget_insert_drop = widget_insert_drop;
    CDndClass->widget_exists = widget_exists;
    CDndClass->widget_apply_position = widget_apply_position;
    CDndClass->widget_get_data = widget_get_data;
    CDndClass->widget_apply_leave = widget_apply_leave;
    CDndClass->options |= XDND_OPTION_NO_HYSTERESIS;
    CDndClass->user_hook1 = CDndClass->user_hook2 = 0;
    if (!xdnd_typelist_receive) {
	int i;
	xdnd_typelist_receive = malloc ((NUM_SIMPLE_TYPES + 1) * sizeof (Atom *));
	xdnd_typelist_send = malloc ((NUM_SIMPLE_TYPES + 1) * sizeof (Atom *));
	for (i = 0; i < NUM_SIMPLE_TYPES; i++) {
	    int j;
	    xdnd_typelist_receive[i] = CMalloc (32 * sizeof (Atom));
	    for (j = 0; mime_type_recieve[i][j]; j++) {
		xdnd_typelist_receive[i][j] = XInternAtom (CDndClass->display, mime_type_recieve[i][j], False);
		xdnd_typelist_receive[i][j + 1] = 0;
	    }
	    xdnd_typelist_receive[i + 1] = 0;
	    xdnd_typelist_send[i] = CMalloc (32 * sizeof (Atom));
	    for (j = 0; mime_type_send[i][j]; j++) {
		xdnd_typelist_send[i][j] = XInternAtom (CDndClass->display, mime_type_send[i][j], False);
		xdnd_typelist_send[i][j + 1] = 0;
	    }
	    xdnd_typelist_send[i + 1] = 0;
	}
    }
}

void mouse_shut (void)
{
    if (xdnd_typelist_receive) {
	int i;
	for (i = 0; xdnd_typelist_send[i]; i++)
	    free (xdnd_typelist_send[i]);
	free (xdnd_typelist_send);
	xdnd_typelist_send = 0;
	for (i = 0; xdnd_typelist_receive[i]; i++)
	    free (xdnd_typelist_receive[i]);
	free (xdnd_typelist_receive);
	xdnd_typelist_receive = 0;
    }
}

#endif  /* !GTK */

struct mouse_funcs *mouse_funcs_new (void *data, struct mouse_funcs *m)
{
    struct mouse_funcs *p;
    p = CMalloc (sizeof (*m));
    memcpy (p, m, sizeof (*m));
    p->data = data;
    return p;
}

#endif /* HAVE_DND */

void mouse_mark (XEvent * event, int double_click, struct mouse_funcs *funcs)
{
    void *data;
    static unsigned long win_press = 0;
    static int state = 0;	/* 0 = button up, 1 = button pressed, 2 = button pressed and dragging */
    static int x_last, y_last;
    long click;
    data = funcs->data;

    if (event->type == ButtonPress) {
	long start_mark, end_mark;
	state = 1;
	win_press = (unsigned long) event->xbutton.window;
	(*funcs->xy) (event->xbutton.x, event->xbutton.y, &x_last, &y_last);
	click = (*funcs->cp) (data, x_last, y_last);
	if (!(*funcs->marks) (data, &start_mark, &end_mark)) {
	    if ((*funcs->range) (data, start_mark, end_mark, click)) {		/* if clicked on highlighted text */
		unsigned char *t;
		int l;
#ifdef HAVE_DND
		int type;
		t = (unsigned char *) (*funcs->get_block) (data, start_mark, end_mark, &type, &l);
		if (t) {
		    just_dropped_something = 1;
		    CDrag (event->xbutton.window, type, t, l, event->xbutton.button == Button1 ? Button1Mask : 0);
		    free (t);
		}
#else
#ifndef GTK
		/* this is just to get the type which depends on the number of lines selected for 
		    the fileselection box: */
		t = (unsigned char *) (*funcs->get_block) (data, start_mark, end_mark, &funcs->types, &l);
		if (t) {
		    free (t);
		    if (xdnd_drag (CDndClass, event->xbutton.window,
				event->xbutton.button == Button1 ? CDndClass->XdndActionCopy : CDndClass->XdndActionMove,
				xdnd_typelist_send[funcs->types]) == CDndClass->XdndActionMove) {
			if (funcs->delete_block)
			    (*funcs->delete_block) (data);
		    }
		}
#endif
#endif
		if (funcs->fin_mark)
		    (*funcs->fin_mark) (data);
		return;
	    }
	}
	just_dropped_something = 0;
	if (funcs->fin_mark)
	    (*funcs->fin_mark) (data);
	(*funcs->move) (data, click, y_last);
	if (double_click && funcs->dclick) {
	    (*funcs->dclick) (data, event);
	    state = 0;
	}
	if (funcs->redraw)
	    (*funcs->redraw) (data, click);
    } else if (event->type == ButtonRelease && state > 0 && win_press == (unsigned long) event->xbutton.window && !double_click) {
	int x, y;
	long start_mark, end_mark;
	(*funcs->xy) (event->xbutton.x, event->xbutton.y, &x, &y);
	click = (*funcs->cp) (data, x, y);
	(*funcs->move) (data, click, y);
	if (state == 2)
	    goto unhighlight;
	if (!(*funcs->marks) (data, &start_mark, &end_mark)) {
	    if ((*funcs->range) (data, start_mark, end_mark, click)) {		/* if clicked on highlighted text */
	      unhighlight:
		if (funcs->release_mark)
		    (*funcs->release_mark) (data, event);
	    }
	}
	state = 0;
	if (funcs->redraw)
	    (*funcs->redraw) (data, click);
    } else if (event->type == MotionNotify && state > 0 && win_press == (unsigned long) event->xbutton.window && event->xbutton.state) {
	int x, y;
	if (!event->xmotion.state)
	    return;
	(*funcs->xy) (event->xbutton.x, event->xbutton.y, &x, &y);
	if (x == x_last && y == y_last && state == 1)
	    return;
	click = (*funcs->cp) (data, x, y);
	if (state == 1) {
	    state = 2;
	    if (funcs->move_mark)
		(funcs->move_mark) (data);
	}
	(*funcs->move) (data, click, y);
	if (funcs->motion)
	    (*funcs->motion) (data, click);
	if (funcs->redraw)
	    (*funcs->redraw) (data, click);
    }
}

