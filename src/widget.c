/* Widgets for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   
   Authors: 1994, 1995 Radek Doulik
            1994, 1995 Miguel de Icaza
            1995 Jakub Jelinek
	    1996 Andrej Borsenkow
	    1997 Norbert Warmuth

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

/** \file widget.c
 *  \brief Source: widgets
 */

#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "global.h"
#include "tty.h"
#include "color.h"
#include "mouse.h"
#include "dialog.h"
#include "widget.h"
#include "win.h"
#include "key.h"	/* XCTRL and ALT macros  */
#include "profile.h"	/* for history loading and saving */
#include "wtools.h"	/* For common_dialog_repaint() */
#include "main.h"	/* for `slow_terminal' */

#define HISTORY_FILE_NAME ".mc/history"

struct WButtonBar {
    Widget widget;
    int    visible;		/* Is it visible? */
    struct {
	char   *text;
	enum { BBFUNC_NONE, BBFUNC_VOID, BBFUNC_PTR } tag;
	union {
	    voidfn fn_void;
	    buttonbarfn fn_ptr;
	} u;
	void   *data;
    } labels [10];
};

static int button_event (Gpm_Event *event, void *);

int quote = 0;

static void
widget_selectcolor (Widget *w, gboolean focused, gboolean hotkey)
{
    Dlg_head *h = w->parent;

    attrset (hotkey
	? (focused
	    ? DLG_HOT_FOCUSC (h)
	    : DLG_HOT_NORMALC (h))
	: (focused
	    ? DLG_FOCUSC (h)
	    : DLG_NORMALC (h)));
}

static cb_ret_t
button_callback (Widget *w, widget_msg_t msg, int parm)
{
    WButton *b = (WButton *) w;
    char buf[BUF_SMALL];
    int stop = 0;
    int off = 0;
    Dlg_head *h = b->widget.parent;

    switch (msg) {
    case WIDGET_HOTKEY:
	/*
	 * Don't let the default button steal Enter from the current
	 * button.  This is a workaround for the flawed event model
	 * when hotkeys are sent to all widgets before the key is
	 * handled by the current widget.
	 */
	if (parm == '\n' && h->current == &b->widget) {
	    button_callback (w, WIDGET_KEY, ' ');
	    return MSG_HANDLED;
	}

	if (parm == '\n' && b->flags == DEFPUSH_BUTTON) {
	    button_callback (w, WIDGET_KEY, ' ');
	    return MSG_HANDLED;
	}

	if (b->hotkey == tolower (parm)) {
	    button_callback (w, WIDGET_KEY, ' ');
	    return MSG_HANDLED;
	}

	return MSG_NOT_HANDLED;

    case WIDGET_KEY:
	if (parm != ' ' && parm != '\n')
	    return MSG_NOT_HANDLED;

	if (b->callback)
	    stop = (*b->callback) (b->action);
	if (!b->callback || stop) {
	    h->ret_value = b->action;
	    dlg_stop (h);
	}
	return MSG_HANDLED;

    case WIDGET_CURSOR:
	switch (b->flags) {
	case DEFPUSH_BUTTON:
	    off = 3;
	    break;
	case NORMAL_BUTTON:
	    off = 2;
	    break;
	case NARROW_BUTTON:
	    off = 1;
	    break;
	case HIDDEN_BUTTON:
	default:
	    off = 0;
	    break;
	}
	widget_move (&b->widget, 0, b->hotpos + off);
	return MSG_HANDLED;

    case WIDGET_UNFOCUS:
    case WIDGET_FOCUS:
    case WIDGET_DRAW:
	if (msg == WIDGET_UNFOCUS)
	    b->selected = 0;
	else if (msg == WIDGET_FOCUS)
	    b->selected = 1;

	switch (b->flags) {
	case DEFPUSH_BUTTON:
	    g_snprintf (buf, sizeof (buf), "[< %s >]", b->text);
	    off = 3;
	    break;
	case NORMAL_BUTTON:
	    g_snprintf (buf, sizeof (buf), "[ %s ]", b->text);
	    off = 2;
	    break;
	case NARROW_BUTTON:
	    g_snprintf (buf, sizeof (buf), "[%s]", b->text);
	    off = 1;
	    break;
	case HIDDEN_BUTTON:
	default:
	    buf[0] = '\0';
	    off = 0;
	    break;
	}

	widget_selectcolor (w, b->selected, FALSE);
	widget_move (w, 0, 0);

	addstr (buf);

	if (b->hotpos >= 0) {
	    widget_selectcolor (w, b->selected, TRUE);
	    widget_move (w, 0, b->hotpos + off);
	    addch ((unsigned char) b->text[b->hotpos]);
	}
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	g_free (b->text);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
button_event (Gpm_Event *event, void *data)
{
    WButton *b = data;

    if (event->type & (GPM_DOWN|GPM_UP)){
    	Dlg_head *h=b->widget.parent;
	dlg_select_widget (b);
	if (event->type & GPM_UP){
	    button_callback ((Widget *) data, WIDGET_KEY, ' ');
	    (*h->callback) (h, DLG_POST_KEY, ' ');
	    return MOU_NORMAL;
	}
    }
    return MOU_NORMAL;
}

static int
button_len (const char *text, unsigned int flags)
{
    int ret = strlen (text);
    switch (flags){
	case DEFPUSH_BUTTON:
	    ret += 6;
	    break;
	case NORMAL_BUTTON:
	    ret += 4;
	    break;
	case NARROW_BUTTON:
	    ret += 2;
	    break;
	case HIDDEN_BUTTON:
	default:
	    return 0;
    }
    return ret;
}

/*
 * Locate the hotkey and remove it from the button text.  Assuming that
 * the button text is g_malloc()ed, we can safely change and shorten it.
 */
static void
button_scan_hotkey (WButton *b)
{
    char *cp = strchr (b->text, '&');

    if (cp != NULL && cp[1] != '\0') {
	g_strlcpy (cp, cp + 1, strlen (cp));
	b->hotkey = tolower ((unsigned char) *cp);
	b->hotpos = cp - b->text;
    }
}

WButton *
button_new (int y, int x, int action, int flags, const char *text,
	    bcback callback)
{
    WButton *b = g_new (WButton, 1);

    init_widget (&b->widget, y, x, 1, button_len (text, flags),
		 button_callback, button_event);
    
    b->action = action;
    b->flags  = flags;
    b->selected = 0;
    b->text   = g_strdup (text);
    b->callback = callback;
    widget_want_hotkey (b->widget, 1);
    b->hotkey = 0;
    b->hotpos = -1;

    button_scan_hotkey(b);
    return b;
}

const char *
button_get_text (WButton *b)
{
    return b->text;
}

void
button_set_text (WButton *b, const char *text)
{
    g_free (b->text);
    b->text = g_strdup (text);
    b->widget.cols = button_len (text, b->flags);
    button_scan_hotkey(b);
    dlg_redraw (b->widget.parent);
}


/* Radio button widget */
static int radio_event (Gpm_Event *event, void *);

static cb_ret_t
radio_callback (Widget *w, widget_msg_t msg, int parm)
{
    WRadio *r = (WRadio *) w;
    int i;
    Dlg_head *h = r->widget.parent;

    switch (msg) {
    case WIDGET_HOTKEY:
	{
	    int i, lp = tolower (parm);
	    const char *cp;

	    for (i = 0; i < r->count; i++) {
		cp = strchr (r->texts[i], '&');
		if (cp != NULL && cp[1] != '\0') {
		    int c = tolower ((unsigned char) cp[1]);

		    if (c != lp)
			continue;
		    r->pos = i;

		    /* Take action */
		    radio_callback (w, WIDGET_KEY, ' ');
		    return MSG_HANDLED;
		}
	    }
	}
	return MSG_NOT_HANDLED;

    case WIDGET_KEY:
	switch (parm) {
	case ' ':
	    r->sel = r->pos;
	    (*h->callback) (h, DLG_ACTION, 0);
	    radio_callback (w, WIDGET_FOCUS, ' ');
	    return MSG_HANDLED;

	case KEY_UP:
	case KEY_LEFT:
	    if (r->pos > 0) {
		r->pos--;
		return MSG_HANDLED;
	    }
	    return MSG_NOT_HANDLED;

	case KEY_DOWN:
	case KEY_RIGHT:
	    if (r->count - 1 > r->pos) {
		r->pos++;
		return MSG_HANDLED;
	    }
	}
	return MSG_NOT_HANDLED;

    case WIDGET_CURSOR:
	(*h->callback) (h, DLG_ACTION, 0);
	radio_callback (w, WIDGET_FOCUS, ' ');
	widget_move (&r->widget, r->pos, 1);
	return MSG_HANDLED;

    case WIDGET_UNFOCUS:
    case WIDGET_FOCUS:
    case WIDGET_DRAW:
	for (i = 0; i < r->count; i++) {
	    register const char *cp;
	    const gboolean focused = (i == r->pos && msg == WIDGET_FOCUS);
	    widget_selectcolor (w, focused, FALSE);
	    widget_move (&r->widget, i, 0);

	    tty_printf ("(%c) ", (r->sel == i) ? '*' : ' ');
	    for (cp = r->texts[i]; *cp; cp++) {
		if (*cp == '&') {
		    widget_selectcolor (w, focused, TRUE);
		    addch (*++cp);
		    widget_selectcolor (w, focused, FALSE);
		} else
		    addch (*cp);
	    }
	}
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
radio_event (Gpm_Event *event, void *data)
{
    WRadio *r = data;
    Widget *w = data;

    if (event->type & (GPM_DOWN|GPM_UP)){
    	Dlg_head *h = r->widget.parent;
	
	r->pos = event->y - 1;
	dlg_select_widget (r);
	if (event->type & GPM_UP){
	    radio_callback (w, WIDGET_KEY, ' ');
	    radio_callback (w, WIDGET_FOCUS, 0);
	    (*h->callback) (h, DLG_POST_KEY, ' ');
	    return MOU_NORMAL;
	}
    }
    return MOU_NORMAL;
}

WRadio *
radio_new (int y, int x, int count, const char **texts)
{
    WRadio *r = g_new (WRadio, 1);
    int i, max, m;

    /* Compute the longest string */
    max = 0;
    for (i = 0; i < count; i++){
	m = strlen (texts [i]);
	if (m > max)
	    max = m;
    }

    init_widget (&r->widget, y, x, count, max, radio_callback, radio_event);
    r->state = 1;
    r->pos = 0;
    r->sel = 0;
    r->count = count;
    r->texts = texts;
    widget_want_hotkey (r->widget, 1);
    
    return r;
}


/* Checkbutton widget */

static int check_event (Gpm_Event *event, void *);

static cb_ret_t
check_callback (Widget *w, widget_msg_t msg, int parm)
{
    WCheck *c = (WCheck *) w;
    Dlg_head *h = c->widget.parent;

    switch (msg) {
    case WIDGET_HOTKEY:
	if (c->hotkey == parm
	    || (c->hotkey >= 'a' && c->hotkey <= 'z'
		&& c->hotkey - 32 == parm)) {
	    check_callback (w, WIDGET_KEY, ' ');	/* make action */
	    return MSG_HANDLED;
	}
	return MSG_NOT_HANDLED;

    case WIDGET_KEY:
	if (parm != ' ')
	    return MSG_NOT_HANDLED;
	c->state ^= C_BOOL;
	c->state ^= C_CHANGE;
	(*h->callback) (h, DLG_ACTION, 0);
	check_callback (w, WIDGET_FOCUS, ' ');
	return MSG_HANDLED;

    case WIDGET_CURSOR:
	widget_move (&c->widget, 0, 1);
	return MSG_HANDLED;

    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
	widget_selectcolor (w, msg == WIDGET_FOCUS, FALSE);
	widget_move (&c->widget, 0, 0);
	tty_printf ("[%c] %s", (c->state & C_BOOL) ? 'x' : ' ', c->text);

	if (c->hotpos >= 0) {
	    widget_selectcolor (w, msg == WIDGET_FOCUS, TRUE);
	    widget_move (&c->widget, 0, +c->hotpos + 4);
	    addch ((unsigned char) c->text[c->hotpos]);
	}
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	g_free (c->text);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
check_event (Gpm_Event *event, void *data)
{
    WCheck *c = data;
    Widget *w = data;

    if (event->type & (GPM_DOWN|GPM_UP)){
    	Dlg_head *h = c->widget.parent;
	
	dlg_select_widget (c);
	if (event->type & GPM_UP){
	    check_callback (w, WIDGET_KEY, ' ');
	    check_callback (w, WIDGET_FOCUS, 0);
	    (*h->callback) (h, DLG_POST_KEY, ' ');
	    return MOU_NORMAL;
	}
    }
    return MOU_NORMAL;
}

WCheck *
check_new (int y, int x, int state, const char *text)
{
    WCheck *c =  g_new (WCheck, 1);
    const char *s;
    char *t;
    
    init_widget (&c->widget, y, x, 1, strlen (text),
	check_callback, check_event);
    c->state = state ? C_BOOL : 0;
    c->text = g_strdup (text);
    c->hotkey = 0;
    c->hotpos = -1;
    widget_want_hotkey (c->widget, 1);

    /* Scan for the hotkey */
    for (s = text, t = c->text; *s; s++, t++){
	if (*s != '&'){
	    *t = *s;
	    continue;
	}
	s++;
	if (*s){
	    c->hotkey = tolower ((unsigned char) *s);
	    c->hotpos = t - c->text;
	}
	*t = *s;
    }
    *t = 0;
    return c;
}


/* Label widget */

static cb_ret_t
label_callback (Widget *w, widget_msg_t msg, int parm)
{
    WLabel *l = (WLabel *) w;
    Dlg_head *h = l->widget.parent;

    switch (msg) {
    case WIDGET_INIT:
	return MSG_HANDLED;

	/* We don't want to get the focus */
    case WIDGET_FOCUS:
	return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
	{
	    char *p = l->text, *q, c = 0;
	    int y = 0;

	    if (!l->text)
		return MSG_HANDLED;

	    if (l->transparent)
		attrset (DEFAULT_COLOR);
	    else
		attrset (DLG_NORMALC (h));
	    for (;;) {
		int xlen;

		q = strchr (p, '\n');
		if (q) {
		    c = *q;
		    *q = 0;
		}
		widget_move (&l->widget, y, 0);
		tty_printf ("%s", p);
		xlen = l->widget.cols - strlen (p);
		if (xlen > 0)
		    tty_printf ("%*s", xlen, " ");
		if (!q)
		    break;
		*q = c;
		p = q + 1;
		y++;
	    }
	    return MSG_HANDLED;
	}

    case WIDGET_DESTROY:
	g_free (l->text);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

void
label_set_text (WLabel *label, const char *text)
{
    int newcols = label->widget.cols;
    
    if (label->text && text && !strcmp (label->text, text))
        return; /* Flickering is not nice */

    g_free (label->text);

    if (text){
	label->text = g_strdup (text);
	if (label->auto_adjust_cols) {
	    newcols = strlen (text);
	    if (newcols > label->widget.cols)
	    label->widget.cols = newcols;
	}
    } else
	label->text = 0;
    
    if (label->widget.parent)
	label_callback ((Widget *) label, WIDGET_DRAW, 0);

    if (newcols < label->widget.cols)
        label->widget.cols = newcols;
}

WLabel *
label_new (int y, int x, const char *text)
{
    WLabel *l;
    int width;

    /* Multiline labels are immutable - no need to compute their sizes */
    if (!text || strchr(text, '\n'))
	width = 1;
    else
	width = strlen (text);

    l = g_new (WLabel, 1);
    init_widget (&l->widget, y, x, 1, width, label_callback, NULL);
    l->text = text ? g_strdup (text) : 0;
    l->auto_adjust_cols = 1;
    l->transparent = 0;
    widget_want_cursor (l->widget, 0);
    return l;
}


/* Gauge widget (progress indicator) */
/* Currently width is hardcoded here for text mode */
#define gauge_len 47

static cb_ret_t
gauge_callback (Widget *w, widget_msg_t msg, int parm)
{
    WGauge *g = (WGauge *) w;
    Dlg_head *h = g->widget.parent;

    if (msg == WIDGET_INIT)
	return MSG_HANDLED;
    
    /* We don't want to get the focus */
    if (msg == WIDGET_FOCUS)
	return MSG_NOT_HANDLED;

    if (msg == WIDGET_DRAW){
	widget_move (&g->widget, 0, 0);
	attrset (DLG_NORMALC (h));
	if (!g->shown)
	    tty_printf ("%*s", gauge_len, "");
	else {
	    int percentage, columns;
	    long total = g->max, done = g->current;
	    
	    if (total <= 0 || done < 0) {
	        done = 0;
	        total = 100;
	    }
	    if (done > total)
	        done = total;
	    while (total > 65535) {
	        total /= 256;
	        done /= 256;
	    }
	    percentage = (200 * done / total + 1) / 2;
	    columns = (2 * (gauge_len - 7) * done / total + 1) / 2;
	    addch ('[');
	    attrset (GAUGE_COLOR);
	    tty_printf ("%*s", (int) columns, "");
	    attrset (DLG_NORMALC (h));
	    tty_printf ("%*s] %3d%%", (int)(gauge_len - 7 - columns), "", (int) percentage);
	}
	return MSG_HANDLED;
    }
    return default_proc (msg, parm);
}

void
gauge_set_value (WGauge *g, int max, int current)
{
    if (g->current == current && g->max == max)
    	return; /* Do not flicker */
    if (max == 0)
        max = 1; /* I do not like division by zero :) */

    g->current = current;
    g->max = max;
    gauge_callback ((Widget *) g, WIDGET_DRAW, 0);
}

void
gauge_show (WGauge *g, int shown)
{
    if (g->shown == shown)
        return;
    g->shown = shown;
    gauge_callback ((Widget *) g, WIDGET_DRAW, 0);
}

WGauge *
gauge_new (int y, int x, int shown, int max, int current)
{
    WGauge *g = g_new (WGauge, 1);

    init_widget (&g->widget, y, x, 1, gauge_len, gauge_callback, NULL);
    g->shown = shown;
    if (max == 0)
        max = 1; /* I do not like division by zero :) */
    g->max = max;
    g->current = current;
    widget_want_cursor (g->widget, 0);
    return g;
}


/* Input widget */

/* {{{ history button */
 
#define LARGE_HISTORY_BUTTON 1
 
#ifdef LARGE_HISTORY_BUTTON
#  define HISTORY_BUTTON_WIDTH 3
#else
#  define HISTORY_BUTTON_WIDTH 1
#endif
 
#define should_show_history_button(in) \
	    (in->history && in->field_len > HISTORY_BUTTON_WIDTH * 2 + 1 && in->widget.parent)

static void draw_history_button (WInput * in)
{
    char c;
    c = in->history->next ? (in->history->prev ? '|' : 'v') : '^';
    widget_move (&in->widget, 0, in->field_len - HISTORY_BUTTON_WIDTH);
#ifdef LARGE_HISTORY_BUTTON
    {
	Dlg_head *h;
	h = in->widget.parent;
#if 0
	attrset (NORMALC);	/* button has the same color as other buttons */
	addstr ("[ ]");
	attrset (HOT_NORMALC);
#else
	attrset (NORMAL_COLOR);
	addstr ("[ ]");
	/* Too distracting: attrset (MARKED_COLOR); */
#endif
	widget_move (&in->widget, 0, in->field_len - HISTORY_BUTTON_WIDTH + 1);
	addch (c);
    }
#else
    attrset (MARKED_COLOR);
    addch (c);
#endif
}

/* }}} history button */


/* Input widgets now have a global kill ring */
/* Pointer to killed data */
static char *kill_buffer = 0;

void
update_input (WInput *in, int clear_first)
{
    int has_history = 0;
    int    i, j;
    unsigned char   c;
    int    buf_len = strlen (in->buffer);

    if (should_show_history_button (in))
	has_history = HISTORY_BUTTON_WIDTH;

    if (in->disable_update)
	return;

    /* Make the point visible */
    if ((in->point < in->first_shown) ||
	(in->point >= in->first_shown+in->field_len - has_history)){
	in->first_shown = in->point - (in->field_len / 3);
	if (in->first_shown < 0)
	    in->first_shown = 0;
    }

    /* Adjust the mark */
    if (in->mark > buf_len)
	in->mark = buf_len;
    
    if (has_history)
	draw_history_button (in);

    attrset (in->color);
    
    widget_move (&in->widget, 0, 0);
    for (i = 0; i < in->field_len - has_history; i++)
	addch (' ');
    widget_move (&in->widget, 0, 0);
    
    for (i = 0, j = in->first_shown; i < in->field_len - has_history && in->buffer [j]; i++){
	c = in->buffer [j++];
	c = is_printable (c) ? c : '.';
	if (in->is_password)
	    c = '*';
	addch (c);
    }
    widget_move (&in->widget, 0, in->point - in->first_shown);

    if (clear_first)
	    in->first = 0;
}

void
winput_set_origin (WInput *in, int x, int field_len)
{
    in->widget.x    = x;
    in->field_len = in->widget.cols = field_len;
    update_input (in, 0);
}

/* {{{ history saving and loading */

int num_history_items_recorded = 60;

/*
   This loads and saves the history of an input line to and from the
   widget. It is called with the widgets history name on creation of the
   widget, and returns the GList list. It stores histories in the file
   ~/.mc/history in using the profile code.

   If def_text is passed as INPUT_LAST_TEXT (to the input_new()
   function) then input_new assigns the default text to be the last text
   entered, or "" if not found.
 */

GList *
history_get (const char *input_name)
{
    int i;
    GList *hist;
    char *profile;

    hist = NULL;

    if (!num_history_items_recorded)	/* this is how to disable */
	return NULL;
    if (!input_name)
	return NULL;
    if (!*input_name)
	return NULL;
    profile = concat_dir_and_file (home_dir, HISTORY_FILE_NAME);
    for (i = 0;; i++) {
	char key_name[BUF_TINY];
	char this_entry[BUF_LARGE];
	g_snprintf (key_name, sizeof (key_name), "%d", i);
	GetPrivateProfileString (input_name, key_name, "", this_entry,
				 sizeof (this_entry), profile);
	if (!*this_entry)
	    break;

	hist = list_append_unique (hist, g_strdup (this_entry));
    }
    g_free (profile);

    /* return pointer to the last entry in the list */
    hist = g_list_last (hist);

    return hist;
}

void
history_put (const char *input_name, GList *h)
{
    int i;
    char *profile;

    if (!input_name)
	return;

    if (!*input_name)
	return;

    if (!h)
	return;

    if (!num_history_items_recorded)	/* this is how to disable */
	return;

    profile = concat_dir_and_file (home_dir, HISTORY_FILE_NAME);

    if ((i = open (profile, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) != -1)
	close (i);

    /* Make sure the history is only readable by the user */
    if (chmod (profile, S_IRUSR | S_IWUSR) == -1 && errno != ENOENT) {
	g_free (profile);
	return;
    }

    /* go to end of list */
    h = g_list_last (h);

    /* go back 60 places */
    for (i = 0; i < num_history_items_recorded - 1 && h->prev; i++)
	h = g_list_previous (h);

    if (input_name)
	profile_clean_section (input_name, profile);

    /* dump histories into profile */
    for (i = 0; h; h = g_list_next (h)) {
	char *text;

	text = (char *) h->data;

	/* We shouldn't have null entries, but let's be sure */
	if (text && *text) {
	    char key_name[BUF_TINY];
	    g_snprintf (key_name, sizeof (key_name), "%d", i++);
	    WritePrivateProfileString (input_name, key_name, text,
				       profile);
	}
    }

    g_free (profile);
}

/* }}} history saving and loading */


/* {{{ history display */

static const char *
i18n_htitle (void)
{
    static const char *history_title = NULL;

    if (history_title == NULL)
	history_title = _(" History ");
    return history_title;	
}

static WLEntry *listbox_select_pos (WListbox *l, WLEntry *base, int
				    pos);

static inline cb_ret_t
listbox_fwd (WListbox *l)
{
    if (l->current != l->list->prev){
	listbox_select_entry (l, listbox_select_pos (l, l->list, l->pos+1));
	return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

char *
show_hist (GList *history, int widget_x, int widget_y)
{
    GList *hi, *z;
    size_t maxlen = strlen (i18n_htitle ()), i, count = 0;
    int x, y, w, h;
    char *q, *r = 0;
    Dlg_head *query_dlg;
    WListbox *query_list;

    z = history;
    if (!z)
	return NULL;

    z = g_list_first (history);
    hi = z;
    while (hi) {
	if ((i = strlen ((char *) hi->data)) > maxlen)
	    maxlen = i;
	count++;
	hi = g_list_next (hi);
    }

    y = widget_y;
    h = count + 2;
    if (h <= y || y > LINES - 6) {
	h = min (h, y - 1);
	y -= h;
    } else {
	y++;
	h = min (h, LINES - y);
    }

    if (widget_x > 2)
	x = widget_x - 2;
    else
	x = 0;
    if ((w = maxlen + 4) + x > COLS) {
	w = min (w, COLS);
	x = COLS - w;
    }

    query_dlg =
	create_dlg (y, x, h, w, dialog_colors, NULL, "[History-query]",
		    i18n_htitle (), DLG_COMPACT);
    query_list = listbox_new (1, 1, w - 2, h - 2, 0);
    add_widget (query_dlg, query_list);
    hi = z;
    if (y < widget_y) {
	/* traverse */
	while (hi) {
	    listbox_add_item (query_list, 0, 0, (char *) hi->data, NULL);
	    hi = g_list_next (hi);
	}
	while (listbox_fwd (query_list));
    } else {
	/* traverse backwards */
	hi = g_list_last (history);
	while (hi) {
	    listbox_add_item (query_list, 0, 0, (char *) hi->data, NULL);
	    hi = g_list_previous (hi);
	}
    }
    run_dlg (query_dlg);
    q = NULL;
    if (query_dlg->ret_value != B_CANCEL) {
	listbox_get_current (query_list, &q, NULL);
	if (q)
	    r = g_strdup (q);
    }
    destroy_dlg (query_dlg);
    return r;
}

static void do_show_hist (WInput * in)
{
    char *r;
    r = show_hist (in->history, in->widget.x, in->widget.y);
    if (r) {
	assign_text (in, r);
	g_free (r);
    }
}

/* }}} history display */

static void
input_destroy (WInput *in)
{
    if (!in){
	fprintf (stderr, "Internal error: null Input *\n");
	exit (1);
    }

    new_input (in);

    if (in->history){
	if (!in->is_password)	/* don't save passwords ;-) */
	    history_put (in->history_name, in->history);

	in->history = g_list_first (in->history);
	g_list_foreach (in->history, (GFunc) g_free, NULL);
	g_list_free (in->history);
    }

    g_free (in->buffer);
    free_completions (in);
    g_free (in->history_name);
}

void
input_disable_update (WInput *in)
{
    in->disable_update++;
}

void
input_enable_update (WInput *in)
{
    in->disable_update--;
    update_input (in, 0);
}

#define ELEMENTS(a)    (sizeof(a)/sizeof(a[0]))

int
push_history (WInput *in, const char *text)
{
    static int i18n;
    /* input widget where urls with passwords are entered without any
       vfs prefix */
    static const char *password_input_fields[] = {
	N_(" Link to a remote machine "),
	N_(" FTP to machine "),
	N_(" SMB link to machine ")
    };
    char *t;
    const char *p;
    size_t i;

    if (!i18n) {
	i18n = 1;
	for (i = 0; i < ELEMENTS (password_input_fields); i++)
	    password_input_fields[i] = _(password_input_fields[i]);
    }

    for (p = text; *p == ' ' || *p == '\t'; p++);
    if (!*p)
	return 0;

    if (in->history) {
	/* Avoid duplicated entries */
	in->history = g_list_last (in->history);
	if (!strcmp ((char *) in->history->data, text))
	    return 1;
    }

    t = g_strdup (text);

    if (in->history_name) {
	p = in->history_name + 3;
	for (i = 0; i < ELEMENTS (password_input_fields); i++)
	    if (strcmp (p, password_input_fields[i]) == 0)
		break;
	if (i < ELEMENTS (password_input_fields))
	    strip_password (t, 0);
	else
	    strip_password (t, 1);
    }

    in->history = list_append_unique (in->history, t);
    in->need_push = 0;

    return 2;
}

#undef ELEMENTS

/* Cleans the input line and adds the current text to the history */
void
new_input (WInput *in)
{
    if (in->buffer)
	push_history (in, in->buffer);
    in->need_push = 1;
    in->buffer [0] = 0;
    in->point = 0;
    in->mark = 0;
    free_completions (in);
    update_input (in, 0);
}

static cb_ret_t
insert_char (WInput *in, int c_code)
{
    size_t i;

    if (c_code == -1)
	return MSG_NOT_HANDLED;
    
    in->need_push = 1;
    if (strlen (in->buffer)+1 == (size_t) in->current_max_len){
	/* Expand the buffer */
	char *narea = g_realloc (in->buffer, in->current_max_len + in->field_len);
	if (narea){
	    in->buffer = narea;
	    in->current_max_len += in->field_len;
	}
    }
    if (strlen (in->buffer)+1 < (size_t) in->current_max_len){
	size_t l = strlen (&in->buffer [in->point]);
	for (i = l+1; i > 0; i--)
	    in->buffer [in->point+i] = in->buffer [in->point+i-1];
	in->buffer [in->point] = c_code;
	in->point++;
    }
    return MSG_HANDLED;
}

static void
beginning_of_line (WInput *in)
{
    in->point = 0;
}

static void
end_of_line (WInput *in)
{
    in->point = strlen (in->buffer);
}

static void
backward_char (WInput *in)
{
    if (in->point)
	in->point--;
}

static void
forward_char (WInput *in)
{
    if (in->buffer [in->point])
	in->point++;
}

static void
forward_word (WInput * in)
{
    char *p = in->buffer + in->point;

    while (*p
	   && (isspace ((unsigned char) *p)
	       || ispunct ((unsigned char) *p)))
	p++;
    while (*p && isalnum ((unsigned char) *p))
	p++;
    in->point = p - in->buffer;
}

static void
backward_word (WInput *in)
{
    char *p = in->buffer + in->point;

    while (p - 1 > in->buffer - 1 && (isspace ((unsigned char) *(p - 1))
				      || ispunct ((unsigned char)
						  *(p - 1))))
	p--;
    while (p - 1 > in->buffer - 1 && isalnum ((unsigned char) *(p - 1)))
	p--;
    in->point = p - in->buffer;
}

static void
key_left (WInput *in)
{
    backward_char (in);
}

static void
key_ctrl_left (WInput *in)
{
    backward_word (in);
}

static void
key_right (WInput *in)
{
    forward_char (in);
}

static void
key_ctrl_right (WInput *in)
{
    forward_word (in);
}
static void
backward_delete (WInput *in)
{
    int i;
    
    if (!in->point)
	return;
    for (i = in->point; in->buffer [i-1]; i++)
	in->buffer [i-1] = in->buffer [i];
    in->need_push = 1;
    in->point--;
}

static void
delete_char (WInput *in)
{
    int i;

    for (i = in->point; in->buffer [i]; i++)
	in->buffer [i] = in->buffer [i+1];
    in->need_push = 1;
}

static void
copy_region (WInput *in, int x_first, int x_last)
{
    int first = min (x_first, x_last);
    int last  = max (x_first, x_last);
    
    if (last == first)
	return;
    
    g_free (kill_buffer);

    kill_buffer = g_strndup(in->buffer+first,last-first);
}

static void
delete_region (WInput *in, int x_first, int x_last)
{
   int first = min (x_first, x_last);
   int last  = max (x_first, x_last);
   size_t len = strlen (&in->buffer [last]) + 1;

   in->point = first;
   in->mark  = first;
   memmove (&in->buffer [first], &in->buffer [last], len);
   in->need_push = 1;
}

static void
kill_word (WInput *in)
{
    int old_point = in->point;
    int new_point;

    forward_word (in);
    new_point = in->point;
    in->point = old_point;

    copy_region (in, old_point, new_point);
    delete_region (in, old_point, new_point);
    in->need_push = 1;
}

static void
back_kill_word (WInput *in)
{
    int old_point = in->point;
    int new_point;

    backward_word (in);
    new_point = in->point;
    in->point = old_point;

    copy_region (in, old_point, new_point);
    delete_region (in, old_point, new_point);
    in->need_push = 1;
}

static void
set_mark (WInput *in)
{
    in->mark = in->point;
}

static void
kill_save (WInput *in)
{
    copy_region (in, in->mark, in->point);
}

static void
kill_region (WInput *in)
{
    kill_save (in);
    delete_region (in, in->point, in->mark);
}

static void
yank (WInput *in)
{
    char *p;
    
    if (!kill_buffer)
        return;
    for (p = kill_buffer; *p; p++)
	insert_char (in, *p);
}

static void
kill_line (WInput *in)
{
    g_free (kill_buffer);
    kill_buffer = g_strdup (&in->buffer [in->point]);
    in->buffer [in->point] = 0;
}

void
assign_text (WInput *in, const char *text)
{
    free_completions (in);
    g_free (in->buffer);
    in->buffer = g_strdup (text);	/* was in->buffer->text */
    in->current_max_len = strlen (in->buffer) + 1;
    in->point = strlen (in->buffer);
    in->mark = 0;
    in->need_push = 1;
}

static void
hist_prev (WInput *in)
{
    if (!in->history)
	return;

    if (in->need_push) {
	switch (push_history (in, in->buffer)) {
	case 2:
	    in->history = g_list_previous (in->history);
	    break;
	case 1:
	    if (in->history->prev)
		in->history = g_list_previous (in->history);
	    break;
	case 0:
	    break;
	}
    } else if (in->history->prev)
	in->history = g_list_previous (in->history);
    else
	return;
    assign_text (in, (char *) in->history->data);
    in->need_push = 0;
}

static void
hist_next (WInput *in)
{
    if (in->need_push) {
        switch (push_history (in, in->buffer)) {
         case 2:
            assign_text (in, "");
            return;
         case 0:
            return;
        }
    }
    
    if (!in->history)
	return;

    if (!in->history->next) {
        assign_text (in, "");
	return;
    }
    
    in->history = g_list_next (in->history);
    assign_text (in, (char *) in->history->data);
    in->need_push = 0;
}

static const struct {
    int key_code;
    void (*fn)(WInput *in);
} input_map [] = {
    /* Motion */
    { XCTRL('a'),         beginning_of_line },
    { KEY_HOME,	          beginning_of_line },
    { KEY_A1,	          beginning_of_line },
    { ALT ('<'),          beginning_of_line },
    { XCTRL('e'),         end_of_line },
    { KEY_END,            end_of_line },
    { KEY_C1,             end_of_line },
    { ALT ('>'),          end_of_line },
    { KEY_LEFT,           key_left },
    { KEY_LEFT | KEY_M_CTRL, key_ctrl_left },
    { XCTRL('b'),         backward_char },
    { ALT('b'),           backward_word },
    { KEY_RIGHT,          key_right },
    { KEY_RIGHT | KEY_M_CTRL, key_ctrl_right },
    { XCTRL('f'),         forward_char },
    { ALT('f'),           forward_word },
		          
    /* Editing */         
    { KEY_BACKSPACE,      backward_delete },
    { KEY_DC,             delete_char },
    { ALT('d'),           kill_word },
    { ALT(KEY_BACKSPACE), back_kill_word },
    
    /* Region manipulation */
    { 0,              	  set_mark },
    { XCTRL('w'),     	  kill_region },
    { ALT('w'),       	  kill_save },
    { XCTRL('y'),     	  yank },
    { XCTRL('k'),     	  kill_line },
    		      	  
    /* History */     	  
    { ALT('p'),       	  hist_prev },
    { ALT('n'),       	  hist_next },
    { ALT('h'),       	  do_show_hist },
    
    /* Completion */
    { ALT('\t'),	  complete },
    
    { 0,            0 }
};

/* This function is a test for a special input key used in complete.c */
/* Returns 0 if it is not a special key, 1 if it is a non-complete key
   and 2 if it is a complete key */
int
is_in_input_map (WInput *in, int c_code)
{
    int i;

    (void) in;

    for (i = 0; input_map [i].fn; i++)
	if (c_code == input_map [i].key_code) {
	    if (input_map [i].fn == complete)
	    	return 2;
	    else
	    	return 1;
	}
    return 0;
}

static void
port_region_marked_for_delete (WInput *in)
{
    *in->buffer = 0;
    in->point = 0;
    in->first = 0;
}

cb_ret_t
handle_char (WInput *in, int c_code)
{
    cb_ret_t v;
    int    i;

    v = MSG_NOT_HANDLED;

    if (quote){
    	free_completions (in);
	v = insert_char (in, c_code);
	update_input (in, 1);
	quote = 0;
	return v;
    }

    for (i = 0; input_map [i].fn; i++){
	if (c_code == input_map [i].key_code){
	    if (input_map [i].fn != complete)
	    	free_completions (in);
	    (*input_map [i].fn)(in);
	    v = MSG_HANDLED;
	    break;
	}
    }
    if (!input_map [i].fn){
	if (c_code > 255 || !is_printable (c_code))
	    return MSG_NOT_HANDLED;
	if (in->first){
	    port_region_marked_for_delete (in);
	}
    	free_completions (in);
	v = insert_char (in, c_code);
    }
    update_input (in, 1);
    return v;
}

/* Inserts text in input line */
void
stuff (WInput *in, const char *text, int insert_extra_space)
{
    input_disable_update (in);
    while (*text)
	handle_char (in, *text++);
    if (insert_extra_space)
	handle_char (in, ' ');
    input_enable_update (in);
    update_input (in, 1);
}

void
input_set_point (WInput *in, int pos)
{
    if (pos > in->current_max_len)
	pos = in->current_max_len;
    if (pos != in->point)
    	free_completions (in);
    in->point = pos;
    update_input (in, 1);
}

cb_ret_t
input_callback (Widget *w, widget_msg_t msg, int parm)
{
    WInput *in = (WInput *) w;
    cb_ret_t v;

    switch (msg) {
    case WIDGET_KEY:
	if (parm == XCTRL ('q')) {
	    quote = 1;
	    v = handle_char (in, ascii_alpha_to_cntrl (mi_getch ()));
	    quote = 0;
	    return v;
	}

	/* Keys we want others to handle */
	if (parm == KEY_UP || parm == KEY_DOWN || parm == ESC_CHAR
	    || parm == KEY_F (10) || parm == XCTRL ('g') || parm == '\n')
	    return MSG_NOT_HANDLED;

	/* When pasting multiline text, insert literal Enter */
	if ((parm & ~KEY_M_MASK) == '\n') {
	    quote = 1;
	    v = handle_char (in, '\n');
	    quote = 0;
	    return v;
	}

	return handle_char (in, parm);

    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
	update_input (in, 0);
	return MSG_HANDLED;

    case WIDGET_CURSOR:
	widget_move (&in->widget, 0, in->point - in->first_shown);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	input_destroy (in);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
input_event (Gpm_Event * event, void *data)
{
    WInput *in = data;

    if (event->type & (GPM_DOWN | GPM_DRAG)) {
	dlg_select_widget (in);

	if (event->x >= in->field_len - HISTORY_BUTTON_WIDTH + 1
	    && should_show_history_button (in)) {
	    do_show_hist (in);
	} else {
	    in->point = strlen (in->buffer);
	    if (event->x - in->first_shown - 1 < in->point)
		in->point = event->x - in->first_shown - 1;
	    if (in->point < 0)
		in->point = 0;
	}
	update_input (in, 1);
    }
    return MOU_NORMAL;
}

WInput *
input_new (int y, int x, int color, int len, const char *def_text,
	   const char *histname, INPUT_COMPLETE_FLAGS completion_flags)
{
    WInput *in = g_new (WInput, 1);
    int initial_buffer_len;

    init_widget (&in->widget, y, x, 1, len, input_callback, input_event);

    /* history setup */
    in->history = NULL;
    in->history_name = 0;
    if (histname) {
	if (*histname) {
	    in->history_name = g_strdup (histname);
	    in->history = history_get (histname);
	}
    }

    if (!def_text)
	def_text = "";

    if (def_text == INPUT_LAST_TEXT) {
	def_text = "";
	if (in->history)
	    if (in->history->data)
		def_text = (char *) in->history->data;
    }
    initial_buffer_len = 1 + max ((size_t) len, strlen (def_text));
    in->widget.options |= W_IS_INPUT;
    in->completions = NULL;
    in->completion_flags = completion_flags;
    in->current_max_len = initial_buffer_len;
    in->buffer = g_malloc (initial_buffer_len);
    in->color = color;
    in->field_len = len;
    in->first = 1;
    in->first_shown = 0;
    in->disable_update = 0;
    in->mark = 0;
    in->need_push = 1;
    in->is_password = 0;

    strcpy (in->buffer, def_text);
    in->point = strlen (in->buffer);
    return in;
}


/* Listbox widget */

/* Should draw the scrollbar, but currently draws only
 * indications that there is more information
 */
static int listbox_cdiff (WLEntry *s, WLEntry *e);

static void
listbox_drawscroll (WListbox *l)
{
    int line;
    int i, top;
    int max_line = l->height-1;
    
    /* Are we at the top? */
    widget_move (&l->widget, 0, l->width);
    if (l->list == l->top)
	one_vline ();
    else
	addch ('^');

    /* Are we at the bottom? */
    widget_move (&l->widget, max_line, l->width);
    top = listbox_cdiff (l->list, l->top);
    if ((top + l->height == l->count) || l->height >= l->count)
	one_vline ();
    else
	addch ('v');

    /* Now draw the nice relative pointer */
    if (l->count)
	line = 1+ ((l->pos * (l->height-2)) / l->count);
    else
	line = 0;
    
    for (i = 1; i < max_line; i++){
	widget_move (&l->widget, i, l->width);
	if (i != line)
	    one_vline ();
	else
	    addch ('*');
    }
}
    
static void
listbox_draw (WListbox *l, int focused)
{
    WLEntry *e;
    int i;
    int sel_line;
    Dlg_head *h = l->widget.parent;
    int normalc = DLG_NORMALC (h);
    int selc;
    const char *text; 

    if (focused){
	selc    = DLG_FOCUSC (h);
    } else {
	selc    = DLG_HOT_FOCUSC (h);
    }
    sel_line = -1;

    for (e = l->top, i = 0; (i < l->height); i++){
	
	/* Display the entry */
	if (e == l->current && sel_line == -1){
	    sel_line = i;
	    attrset (selc);
	} else
	    attrset (normalc);

	widget_move (&l->widget, i, 0);

	if ((i > 0 && e == l->list) || !l->list)
	    text = "";
	else {
	    text = e->text;
	    e = e->next;
	}
	tty_printf (" %-*s ", l->width-2, name_trunc (text, l->width-2));
    }
    l->cursor_y = sel_line;
    if (!l->scrollbar)
	return;
    attrset (normalc);
    listbox_drawscroll (l);
}

/* Returns the number of items between s and e,
   must be on the same linked list */
static int
listbox_cdiff (WLEntry *s, WLEntry *e)
{
    int count;

    for (count = 0; s != e; count++)
	s = s->next;
    return count;
}

static WLEntry *
listbox_check_hotkey (WListbox *l, int key)
{
    int i;
    WLEntry *e;

    i = 0;
    e = l->list;
    if (!e)
	return NULL;

    while (1) {

	/* If we didn't find anything, return */
	if (i && e == l->list)
	    return NULL;

	if (e->hotkey == key)
	    return e;

	i++;
	e = e->next;
    }
}

/* Used only for display updating, for avoiding line at a time scroll */
void
listbox_select_last (WListbox *l, int set_top)
{
    if (l->list){
	l->current = l->list->prev;
	l->pos = l->count - 1;
	if (set_top)
	    l->top = l->list->prev;
    }
}

void
listbox_remove_list (WListbox *l)
{
    WLEntry *p, *q;

    if (!l->count)
	return;

    p = l->list;
    
    while (l->count--) {
	q = p->next;
	g_free (p->text);
	g_free (p);
	p = q;
    }
    l->pos = l->count = 0;
    l->list = l->top = l->current = 0;
}

/*
 * bor 30.10.96: added force flag to remove *last* entry as well
 * bor 30.10.96: corrected selection bug if last entry was removed
 */

void
listbox_remove_current (WListbox *l, int force)
{
    WLEntry *p;
    
    /* Ok, note: this won't allow for emtpy lists */
    if (!force && (!l->count || l->count == 1))
	return;

    l->count--;
    p = l->current;

    if (l->count) {
	l->current->next->prev = l->current->prev;
	l->current->prev->next = l->current->next;
	if (p->next == l->list) {
	    l->current = p->prev;
	    l->pos--;
	}	
	else 
	    l->current = p->next;
	
	if (p == l->list)
	    l->list = l->top = p->next;
    } else {
	l->pos = 0;
	l->list = l->top = l->current = 0;
    }

    g_free (p->text);
    g_free (p);
}

/* Makes *e the selected entry (sets current and pos) */
void
listbox_select_entry (WListbox *l, WLEntry *dest)
{
    WLEntry *e;
    int pos;
    int top_seen;
    
    top_seen = 0;
    
    /* Special case */
    for (pos = 0, e = l->list; pos < l->count; e = e->next, pos++){

	if (e == l->top)
	    top_seen = 1;
	
	if (e == dest){
	    l->current = e;
	    if (top_seen){
		while (listbox_cdiff (l->top, l->current) >= l->height)
		    l->top = l->top->next;
	    } else {
		l->top = l->current;
	    }
	    l->pos = pos;
	    return;
	}
    }
    /* If we are unable to find it, set decent values */
    l->current = l->top = l->list;
    l->pos = 0;
}

/* Selects from base the pos element */
static WLEntry *
listbox_select_pos (WListbox *l, WLEntry *base, int pos)
{
    WLEntry *last = l->list->prev;

    if (base == last)
    	return last;
    while (pos--){
	base = base->next;
	if (base == last)
	    break;
    }
    return base;
}

static inline cb_ret_t
listbox_back (WListbox *l)
{
    if (l->pos){
	listbox_select_entry (l, listbox_select_pos (l, l->list, l->pos-1));
	return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

/* Return MSG_HANDLED if we want a redraw */
static cb_ret_t
listbox_key (WListbox *l, int key)
{
    int i;
    int j = 0;

    if (!l->list)
	return MSG_NOT_HANDLED;
    
    switch (key){
    case KEY_HOME:
    case KEY_A1:
    case ALT ('<'):
	l->current = l->top = l->list;
	l->pos = 0;
	return MSG_HANDLED;
	
    case KEY_END:
    case KEY_C1:
    case ALT ('>'):
	l->current = l->top = l->list->prev;
	for (i = min (l->height - 1, l->count - 1); i; i--)
	    l->top = l->top->prev;
	l->pos = l->count - 1;
	return MSG_HANDLED;
	
    case XCTRL('p'):
    case KEY_UP:
	listbox_back (l);
	return MSG_HANDLED;
	
    case XCTRL('n'):
    case KEY_DOWN:
	listbox_fwd (l);
	return MSG_HANDLED;

    case KEY_NPAGE:
    case XCTRL('v'):
	for (i = 0; i < l->height-1; i++)
	    j |= listbox_fwd (l);
	return (j > 0) ? MSG_HANDLED : MSG_NOT_HANDLED;
	
    case KEY_PPAGE:
    case ALT('v'):
	for (i = 0; i < l->height-1; i++)
	    j |= listbox_back (l);
	return (j > 0) ? MSG_HANDLED : MSG_NOT_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

static void
listbox_destroy (WListbox *l)
{
    WLEntry *n, *p = l->list;
    int i;

    for (i = 0; i < l->count; i++){
	n = p->next;
	g_free (p->text);
	g_free (p);
	p = n;
    }
}

static cb_ret_t
listbox_callback (Widget *w, widget_msg_t msg, int parm)
{
    WListbox *l = (WListbox *) w;
    cb_ret_t ret_code;
    WLEntry *e;
    Dlg_head *h = l->widget.parent;

    switch (msg) {
    case WIDGET_INIT:
	return MSG_HANDLED;

    case WIDGET_HOTKEY:
	if ((e = listbox_check_hotkey (l, parm)) != NULL) {
	    int action;

	    listbox_select_entry (l, e);

	    if (l->cback)
		action = (*l->cback) (l);
	    else
		action = LISTBOX_DONE;

	    if (action == LISTBOX_DONE) {
		h->ret_value = B_ENTER;
		dlg_stop (h);
	    }
	    return MSG_HANDLED;
	} else
	    return MSG_NOT_HANDLED;

    case WIDGET_KEY:
	if ((ret_code = listbox_key (l, parm)))
	    listbox_draw (l, 1);
	return ret_code;

    case WIDGET_CURSOR:
	widget_move (&l->widget, l->cursor_y, 0);
	return MSG_HANDLED;

    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
	listbox_draw (l, msg != WIDGET_UNFOCUS);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	listbox_destroy (l);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
listbox_event (Gpm_Event *event, void *data)
{
    WListbox *l = data;
    Widget *w = data;
    int i;

    Dlg_head *h = l->widget.parent;

    /* Single click */
    if (event->type & GPM_DOWN)
	dlg_select_widget (l);
    if (!l->list)
	return MOU_NORMAL;
    if (event->type & (GPM_DOWN | GPM_DRAG)) {
	if (event->x < 0 || event->x >= l->width)
	    return MOU_REPEAT;
	if (event->y < 1)
	    for (i = -event->y; i >= 0; i--)
		listbox_back (l);
	else if (event->y > l->height)
	    for (i = event->y - l->height; i > 0; i--)
		listbox_fwd (l);
	else
	    listbox_select_entry (l,
				  listbox_select_pos (l, l->top,
						      event->y - 1));

	/* We need to refresh ourselves since the dialog manager doesn't */
	/* know about this event */
	listbox_callback (w, WIDGET_DRAW, 0);
	mc_refresh ();
	return MOU_REPEAT;
    }

    /* Double click */
    if ((event->type & (GPM_DOUBLE | GPM_UP)) == (GPM_UP | GPM_DOUBLE)) {
	int action;

	if (event->x < 0 || event->x >= l->width)
	    return MOU_NORMAL;
	if (event->y < 1 || event->y > l->height)
	    return MOU_NORMAL;

	dlg_select_widget (l);
	listbox_select_entry (l,
			      listbox_select_pos (l, l->top,
						  event->y - 1));

	if (l->cback)
	    action = (*l->cback) (l);
	else
	    action = LISTBOX_DONE;

	if (action == LISTBOX_DONE) {
	    h->ret_value = B_ENTER;
	    dlg_stop (h);
	    return MOU_NORMAL;
	}
    }
    return MOU_NORMAL;
}

WListbox *
listbox_new (int y, int x, int width, int height, lcback callback)
{
    WListbox *l = g_new (WListbox, 1);

    init_widget (&l->widget, y, x, height, width,
		 listbox_callback, listbox_event);

    l->list = l->top = l->current = 0;
    l->pos = 0;
    l->width = width;
    if (height <= 0)
	l->height = 1;
    else
	l->height = height;
    l->count = 0;
    l->cback = callback;
    l->allow_duplicates = 1;
    l->scrollbar = slow_terminal ? 0 : 1;
    widget_want_hotkey (l->widget, 1);

    return l;
}

/* Listbox item adding function.  They still lack a lot of functionality */
/* any takers? */
/* 1.11.96 bor: added pos argument to control placement of new entry */
static void
listbox_append_item (WListbox *l, WLEntry *e, enum append_pos pos)
{
    if (!l->list){
	l->list = e;
	l->top = e;
	l->current = e;
	e->next = l->list;
	e->prev = l->list;
    } else if (pos == LISTBOX_APPEND_AT_END) {
	e->next = l->list;
	e->prev = l->list->prev;
	l->list->prev->next = e;
	l->list->prev = e;
    } else if (pos == LISTBOX_APPEND_BEFORE){
	e->next = l->current;
	e->prev = l->current->prev;
	l->current->prev->next = e;
	l->current->prev = e;
	if (l->list == l->current) {	/* move list one position down */
	    l->list = e;
	    l->top =  e;
	}
    } else if (pos == LISTBOX_APPEND_AFTER) {
	e->prev = l->current;
	e->next = l->current->next;
	l->current->next->prev = e;
	l->current->next = e;
    } else if (pos == LISTBOX_APPEND_SORTED) {
	WLEntry *w = l->list;

	while (w->next != l->list && strcmp (e->text, w->text) > 0)
	    w = w->next;
	if (w->next == l->list) {
	    e->prev = w;
	    e->next = l->list;
	    w->next = e;
	    l->list->prev = e;
	} else {
	    e->next = w;
	    e->prev = w->prev;
	    w->prev->next = e;
	    w->prev = e;
	}
    }
    l->count++;
}

char *
listbox_add_item (WListbox *l, enum append_pos pos, int hotkey,
		  const char *text, void *data)
{
    WLEntry *entry;

    if (!l)
	return NULL;

    if (!l->allow_duplicates)
	if (listbox_search_text (l, text))
	    return NULL;
	    
    entry = g_new (WLEntry, 1);
    entry->text = g_strdup (text);
    entry->data = data;
    entry->hotkey = hotkey;

    listbox_append_item (l, entry, pos);
    
    return entry->text;
}

/* Selects the nth entry in the listbox */
void
listbox_select_by_number (WListbox *l, int n)
{
    listbox_select_entry (l, listbox_select_pos (l, l->list, n));
}

WLEntry *
listbox_search_text (WListbox *l, const char *text)
{
    WLEntry *e;

    e = l->list;
    if (!e)
	return NULL;
    
    do {
	if(!strcmp (e->text, text))
	    return e;
	e = e->next;
    } while (e!=l->list);

    return NULL;
}

/* Returns the current string text as well as the associated extra data */
void
listbox_get_current (WListbox *l, char **string, char **extra)
{
    if (!l->current){
	*string = 0;
	*extra  = 0;
    }
    if (string && l->current)
	*string = l->current->text;
    if (extra && l->current)
	*extra = l->current->data;
}

/* returns TRUE if a function has been called, FALSE otherwise. */
static gboolean
buttonbar_call (WButtonBar *bb, int i)
{
    switch (bb->labels[i].tag) {
	case BBFUNC_NONE:
	    break;
	case BBFUNC_VOID:
	    bb->labels[i].u.fn_void ();
	    return TRUE;
	case BBFUNC_PTR:
	    bb->labels[i].u.fn_ptr (bb->labels[i].data);
	    return TRUE;
    }
    return FALSE;
}


static cb_ret_t
buttonbar_callback (Widget *w, widget_msg_t msg, int parm)
{
    WButtonBar *bb = (WButtonBar *) w;
    int i;

    switch (msg) {
    case WIDGET_FOCUS:
	return MSG_NOT_HANDLED;

    case WIDGET_HOTKEY:
	for (i = 0; i < 10; i++) {
	    if (parm == KEY_F (i + 1) && buttonbar_call (bb, i))
		return MSG_HANDLED;
	}
	return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
	if (!bb->visible)
	    return MSG_HANDLED;
	widget_move (&bb->widget, 0, 0);
	attrset (DEFAULT_COLOR);
	tty_printf ("%-*s", bb->widget.cols, "");
	for (i = 0; i < COLS / 8 && i < 10; i++) {
	    widget_move (&bb->widget, 0, i * 8);
	    attrset (DEFAULT_COLOR);
	    tty_printf ("%d", i + 1);
	    attrset (SELECTED_COLOR);
	    tty_printf ("%-*s", ((i + 1) * 8 == COLS ? 5 : 6),
		    bb->labels[i].text ? bb->labels[i].text : "");
	    attrset (DEFAULT_COLOR);
	}
	attrset (SELECTED_COLOR);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	for (i = 0; i < 10; i++)
	    g_free (bb->labels[i].text);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static int
buttonbar_event (Gpm_Event *event, void *data)
{
    WButtonBar *bb = data;
    int button;

    if (!(event->type & GPM_UP))
	return MOU_NORMAL;
    if (event->y == 2)
	return MOU_NORMAL;
    button = event->x / 8;
    if (button < 10)
	buttonbar_call (bb, button);
    return MOU_NORMAL;
}

WButtonBar *
buttonbar_new (int visible)
{
    int i;
    WButtonBar *bb = g_new (WButtonBar, 1);

    init_widget (&bb->widget, LINES-1, 0, 1, COLS,
		 buttonbar_callback, buttonbar_event);
    
    bb->visible = visible;
    for (i = 0; i < 10; i++){
	bb->labels[i].text = NULL;
	bb->labels[i].tag = BBFUNC_NONE;
    }
    widget_want_hotkey (bb->widget, 1);
    widget_want_cursor (bb->widget, 0);

    return bb;
}

static void
set_label_text (WButtonBar * bb, int index, const char *text)
{
    g_free (bb->labels[index - 1].text);

    bb->labels[index - 1].text = g_strdup (text);
}

/* Find ButtonBar widget in the dialog */
WButtonBar *
find_buttonbar (Dlg_head *h)
{
    WButtonBar *bb;

    bb = (WButtonBar *) find_widget_type (h, buttonbar_callback);
    return bb;
}

void
buttonbar_clear_label (Dlg_head *h, int idx)
{
    WButtonBar *bb = find_buttonbar (h);

    if (!bb)
	return;

    set_label_text (bb, idx, "");
    bb->labels[idx - 1].tag = BBFUNC_NONE;
}

void
buttonbar_set_label_data (Dlg_head *h, int idx, const char *text, buttonbarfn cback,
		   void *data)
{
    WButtonBar *bb = find_buttonbar (h);

    if (!bb)
	return;

    assert (cback != (buttonbarfn) 0);
    set_label_text (bb, idx, text);
    bb->labels[idx - 1].tag = BBFUNC_PTR;
    bb->labels[idx - 1].u.fn_ptr = cback;
    bb->labels[idx - 1].data = data;
}

void
buttonbar_set_label (Dlg_head *h, int idx, const char *text, voidfn cback)
{
    WButtonBar *bb = find_buttonbar (h);

    if (!bb)
	return;

    assert (cback != (voidfn) 0);
    set_label_text (bb, idx, text);
    bb->labels[idx - 1].tag = BBFUNC_VOID;
    bb->labels[idx - 1].u.fn_void = cback;
}

void
buttonbar_set_visible (WButtonBar *bb, gboolean visible)
{
    bb->visible = visible;
}

void
buttonbar_redraw (Dlg_head *h)
{
    WButtonBar *bb = find_buttonbar (h);

    if (!bb)
	return;

    send_message ((Widget *) bb, WIDGET_DRAW, 0);
}

static cb_ret_t
groupbox_callback (Widget *w, widget_msg_t msg, int parm)
{
    WGroupbox *g = (WGroupbox *) w;

    switch (msg) {
    case WIDGET_INIT:
	return MSG_HANDLED;

    case WIDGET_FOCUS:
	return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
	attrset (COLOR_NORMAL);
	draw_box (g->widget.parent, g->widget.y - g->widget.parent->y,
		  g->widget.x - g->widget.parent->x, g->widget.lines,
		  g->widget.cols);

	attrset (COLOR_HOT_NORMAL);
	dlg_move (g->widget.parent, g->widget.y - g->widget.parent->y,
		  g->widget.x - g->widget.parent->x + 1);
	addstr (g->title);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	g_free (g->title);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

WGroupbox *
groupbox_new (int x, int y, int width, int height, const char *title)
{
    WGroupbox *g = g_new (WGroupbox, 1);

    init_widget (&g->widget, y, x, height, width, groupbox_callback, NULL);

    g->widget.options &= ~W_WANT_CURSOR;
    widget_want_hotkey (g->widget, 0);

    /* Strip existing spaces, add one space before and after the title */
    if (title) {
	char *t;
	t = g_strstrip (g_strdup (title));
	g->title = g_strconcat (" ", t, " ", (char *) NULL);
	g_free (t);
    }

    return g;
}
