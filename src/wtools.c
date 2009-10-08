/* Widget based utility functions.
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.

   Authors: 1994, 1995, 1996 Miguel de Icaza
            1994, 1995 Radek Doulik
	    1995  Jakub Jelinek
	    1995  Andrej Borsenkow

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

/** \file wtools.c
 *  \brief Source: widget based utility functions
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/skin/skin.h"		/* INPUT_COLOR */
#include "../src/tty/key.h"		/* tty_getch() */

#include "dialog.h"
#include "widget.h"
#include "wtools.h"
#include "background.h"		/* parent_call */
#include "strutil.h"


Listbox *
create_listbox_window_delta (int delta_x, int delta_y, int cols, int lines,
				const char *title, const char *help)
{
    const int listbox_colors[DLG_COLOR_NUM] =
    {
	MENU_ENTRY_COLOR,
	MENU_SELECTED_COLOR,
	MENU_HOT_COLOR,
	MENU_HOTSEL_COLOR,
    };

    int xpos, ypos, len;
    Listbox *listbox;

    /* Adjust sizes */
    lines = min (lines, LINES - 6);

    if (title != NULL) {
	len = str_term_width1 (title) + 4;
	cols = max (cols, len);
    }

    cols = min (cols, COLS - 6);

    /* adjust position */
    xpos = (COLS - cols + delta_x) / 2;
    ypos = (LINES - lines + delta_y) / 2 - 2;

    listbox = g_new (Listbox, 1);

    listbox->dlg =
	create_dlg (ypos, xpos, lines + 4, cols + 4, listbox_colors, NULL,
		    help, title, DLG_REVERSE);

    listbox->list = listbox_new (2, 2, lines, cols, NULL);
    add_widget (listbox->dlg, listbox->list);

    return listbox;
}

Listbox *
create_listbox_window (int cols, int lines, const char *title, const char *help)
{
    return create_listbox_window_delta (0, 0, cols, lines, title, help);
}

/* Returns the number of the item selected */
int
run_listbox (Listbox *l)
{
    int val = -1;

    if (run_dlg (l->dlg) != B_CANCEL)
	val = l->list->pos;
    destroy_dlg (l->dlg);
    g_free (l);
    return val;
}

/* default query callback, used to reposition query */
static cb_ret_t
default_query_callback (Dlg_head *h, dlg_msg_t msg, gpointer data)
{
    switch (msg) {
    case DLG_RESIZE:
    {
	int xpos = COLS / 2 - h->cols / 2;
	int ypos = LINES / 3 - (h->lines - 3) / 2;

	/* set position */
	dlg_set_position (h, ypos, xpos, ypos + h->lines, xpos + h->cols);
    }
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, data);
    }
}

static Dlg_head *last_query_dlg;

static int sel_pos = 0;

/* Used to ask questions to the user */
int
query_dialog (const char *header, const char *text, int flags, int count, ...)
{
    va_list ap;
    Dlg_head *query_dlg;
    WButton *button;
    WButton *defbutton = NULL;
    int win_len = 0;
    int i;
    int result = -1;
    int cols, lines;
    char *cur_name;
    const int *query_colors = (flags & D_ERROR) ?
				alarm_colors : dialog_colors;

    if (header == MSG_ERROR)
	header = _("Error");

    if (count > 0) {
	va_start (ap, count);
	for (i = 0; i < count; i++) {
	    char *cp = va_arg (ap, char *);
	    win_len += str_term_width1 (cp) + 6;
	    if (strchr (cp, '&') != NULL)
		win_len--;
	}
	va_end (ap);
    }

    /* count coordinates */
    str_msg_term_size (text, &lines, &cols);
    cols = 6 + max (win_len, max (str_term_width1 (header), cols));
    lines += 4 + (count > 0 ? 2 : 0);

    /* prepare dialog */
    query_dlg =
	create_dlg (0, 0, lines, cols, query_colors, default_query_callback,
		    "[QueryBox]", header, DLG_NONE);

    if (count > 0) {
	cols = (cols - win_len - 2) / 2 + 2;
	va_start (ap, count);
	for (i = 0; i < count; i++) {
	    int xpos;

	    cur_name = va_arg (ap, char *);
	    xpos = str_term_width1 (cur_name) + 6;
	    if (strchr (cur_name, '&') != NULL)
		xpos--;

	    button =
		button_new (lines - 3, cols, B_USER + i, NORMAL_BUTTON,
			    cur_name, 0);
	    add_widget (query_dlg, button);
	    cols += xpos;
	    if (i == sel_pos)
		defbutton = button;
	}
	va_end (ap);

	add_widget (query_dlg, label_new (2, 3, text));

	/* do resize before running and selecting any widget */
	default_query_callback (query_dlg, DLG_RESIZE, 0);

	if (defbutton)
	    dlg_select_widget (defbutton);

	/* run dialog and make result */
	switch (run_dlg (query_dlg)) {
	case B_CANCEL:
	    break;
	default:
	    result = query_dlg->ret_value - B_USER;
	}

	/* free used memory */
	destroy_dlg (query_dlg);
    } else {
	add_widget (query_dlg, label_new (2, 3, text));
	add_widget (query_dlg,
		    button_new (0, 0, 0, HIDDEN_BUTTON, "-", 0));
	last_query_dlg = query_dlg;
    }
    sel_pos = 0;
    return result;
}

void query_set_sel (int new_sel)
{
    sel_pos = new_sel;
}


/* Create message dialog */
static struct Dlg_head *
do_create_message (int flags, const char *title, const char *text)
{
    char *p;
    Dlg_head *d;

    /* Add empty lines before and after the message */
    p = g_strconcat ("\n", text, "\n", (char *) NULL);
    query_dialog (title, p, flags, 0);
    d = last_query_dlg;

    /* do resize before initing and running */
    default_query_callback (d, DLG_RESIZE, 0);

    init_dlg (d);
    g_free (p);

    return d;
}


/*
 * Create message dialog.  The caller must call dlg_run_done() and
 * destroy_dlg() to dismiss it.  Not safe to call from background.
 */
struct Dlg_head *
create_message (int flags, const char *title, const char *text, ...)
{
    va_list args;
    Dlg_head *d;
    char *p;

    va_start (args, text);
    p = g_strdup_vprintf (text, args);
    va_end (args);

    d = do_create_message (flags, title, p);
    g_free (p);

    return d;
}


/*
 * Show message dialog.  Dismiss it when any key is pressed.
 * Not safe to call from background.
 */
static void
fg_message (int flags, const char *title, const char *text)
{
    Dlg_head *d;

    d = do_create_message (flags, title, text);
    tty_getch ();
    dlg_run_done (d);
    destroy_dlg (d);
}


/* Show message box from background */
#ifdef WITH_BACKGROUND
static void
bg_message (int dummy, int *flags, char *title, const char *text)
{
    (void) dummy;
    title = g_strconcat (_("Background process:"), " ", title, (char *) NULL);
    fg_message (*flags, title, text);
    g_free (title);
}
#endif				/* WITH_BACKGROUND */


/* Show message box, background safe */
void
message (int flags, const char *title, const char *text, ...)
{
    char *p;
    va_list ap;
    union {
	void *p;
	void (*f) (int, int *, char *, const char *);
    } func;

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);

    if (title == MSG_ERROR)
	title = _("Error");

#ifdef WITH_BACKGROUND
    if (we_are_background) {
	func.f = bg_message;
	parent_call (func.p, NULL, 3, sizeof (flags), &flags,
		     strlen (title), title, strlen (p), p);
    } else
#endif				/* WITH_BACKGROUND */
	fg_message (flags, title, p);

    g_free (p);
}


/* {{{ Quick dialog routines */


int
quick_dialog_skip (QuickDialog *qd, int nskip)
{
#ifdef ENABLE_NLS
#define I18N(x) (x = !qd->i18n && x && *x ? _(x): x)
#else
#define I18N(x) (x = x)
#endif
    Dlg_head *dd;
    QuickWidget *qw;
    WInput *in;
    WRadio *r;
    int return_val;

    I18N (qd->title);

    if ((qd->xpos == -1) || (qd->ypos == -1))
	dd = create_dlg (0, 0, qd->ylen, qd->xlen,
			    dialog_colors, NULL, qd->help, qd->title,
			    DLG_CENTER | DLG_TRYUP | DLG_REVERSE);
    else
	dd = create_dlg (qd->ypos, qd->xpos, qd->ylen, qd->xlen,
			    dialog_colors, NULL, qd->help, qd->title,
			    DLG_REVERSE);

    for (qw = qd->widgets; qw->widget_type != quick_end; qw++) {
	int xpos;
	int ypos;

	xpos = (qd->xlen * qw->relative_x) / qw->x_divisions;
	ypos = (qd->ylen * qw->relative_y) / qw->y_divisions;

	switch (qw->widget_type) {
	case quick_checkbox:
	    qw->widget = (Widget *) check_new (ypos, xpos, *qw->u.checkbox.state, I18N (qw->u.checkbox.text));
	    break;

	case quick_button:
	    qw->widget = (Widget *) button_new (ypos, xpos, qw->u.button.action,
			    (qw->u.button.action == B_ENTER) ? DEFPUSH_BUTTON : NORMAL_BUTTON,
			    I18N (qw->u.button.text), qw->u.button.callback);
	    break;

	case quick_input:
	    in = input_new (ypos, xpos, INPUT_COLOR, qw->u.input.len,
			    qw->u.input.text, qw->u.input.histname, INPUT_COMPLETE_DEFAULT);
	    in->is_password = (qw->u.input.flags == 1);
	    in->point = 0;
	    if ((qw->u.input.flags & 2) != 0)
		in->completion_flags |= INPUT_COMPLETE_CD;
	    qw->widget = (Widget *) in;
	    *qw->u.input.result = NULL;
	    break;

	case quick_label:
	    qw->widget = (Widget *) label_new (ypos, xpos, I18N (qw->u.label.text));
	    break;

	case quick_radio:
	{
	    int i;
	    char **items = NULL;

	    /* create the copy of radio_items to avoid mwmory leak */
	    items = g_new0 (char *, qw->u.radio.count + 1);

	    if (!qd->i18n)
		for (i = 0; i < qw->u.radio.count; i++)
		    items[i] = g_strdup (_(qw->u.radio.items[i]));
	    else
		for (i = 0; i < qw->u.radio.count; i++)
		    items[i] = g_strdup (qw->u.radio.items[i]);

	    r = radio_new (ypos, xpos, qw->u.radio.count, (const char **) items);
	    r->pos = r->sel = *qw->u.radio.value;
	    qw->widget = (Widget *) r;
	    g_strfreev (items);
	    break;
	}

	default:
	    qw->widget = NULL;
	    fprintf (stderr, "QuickWidget: unknown widget type\n");
	    break;
	}

	add_widget (dd, qw->widget);
    }

    while (nskip-- != 0)
	dd->current = dd->current->next;

    return_val = run_dlg (dd);

    /* Get the data if we found something interesting */
    if (return_val != B_CANCEL) {
	for (qw = qd->widgets; qw->widget_type != quick_end; qw++) {
	    switch (qw->widget_type) {
	    case quick_checkbox:
		*qw->u.checkbox.state = ((WCheck *) qw->widget)->state & C_BOOL;
		break;

	    case quick_input:
		if ((qw->u.input.flags & 2) != 0)
		    *qw->u.input.result = tilde_expand (((WInput *) qw->widget)->buffer);
		else
		    *qw->u.input.result = g_strdup (((WInput *) qw->widget)->buffer);
		break;

	    case quick_radio:
		*qw->u.radio.value = ((WRadio *) qw->widget)->sel;
		break;

	    default:
		break;
	    }
	}
    }

    destroy_dlg (dd);

    return return_val;
#undef I18N
}

int
quick_dialog (QuickDialog *qd)
{
    return quick_dialog_skip (qd, 0);
}

/* }}} */

/* {{{ Input routines */

/*
 * Show dialog, not background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of fg_input_dialog_help().
 *
 * The argument "history_name" holds the name of a section
 * in the history file. Data entered in the input field of
 * the dialog box will be stored there.
 *
 */
static char *
fg_input_dialog_help (const char *header, const char *text, const char *help,
		      const char *history_name, const char *def_text)
{
    char *my_str;

    QuickWidget quick_widgets[] = {
	/* 0 */ QUICK_BUTTON (6, 10, 1, 0, N_("&Cancel"), B_CANCEL, NULL),
	/* 1 */ QUICK_BUTTON (3, 10, 1, 0, N_("&OK"),     B_ENTER,  NULL),
	/* 2 */ QUICK_INPUT (4, 80, 0, 0, def_text, 58, 0, NULL, &my_str),
	/* 3 */ QUICK_LABEL (4, 80, 2, 0, ""),
	QUICK_END
    };

    char histname [64] = "inp|";
    int lines, cols;
    int len;
    int i;
    char *p_text;
    int ret;

    if (history_name != NULL && *history_name != '\0') {
	g_strlcpy (histname + 3, history_name, sizeof (histname) - 3);
	quick_widgets[2].u.input.histname = histname;
    }

    msglen (text, &lines, &cols);
    len = max (max (str_term_width1 (header), cols) + 4, 64);

    /* The special value of def_text is used to identify password boxes
       and hide characters with "*".  Don't save passwords in history! */
    if (def_text == INPUT_PASSWORD) {
	quick_widgets[2].u.input.flags = 1;
	histname[3] = '\0';
	quick_widgets[2].u.input.text = "";
    }

#ifdef ENABLE_NLS
    /*
     * An attempt to place buttons symmetrically, based on actual i18n
     * length of the string. It looks nicer with i18n (IMO) - alex
     */
    quick_widgets[0].u.button.text = _(quick_widgets[0].u.button.text);
    quick_widgets[1].u.button.text = _(quick_widgets[1].u.button.text);
    quick_widgets[0].relative_x = len / 2 + 4;
    quick_widgets[1].relative_x =
	len / 2 - (str_term_width1 (quick_widgets[1].u.button.text) + 9);
    quick_widgets[0].x_divisions = quick_widgets[1].x_divisions = len;
#endif				/* ENABLE_NLS */

    p_text = g_strstrip (g_strdup (text));
    quick_widgets[3].u.label.text = p_text;

    {
	QuickDialog Quick_input =
	{
	    len, lines + 6, -1, -1, header,
	    help, quick_widgets, TRUE
	};

	for (i = 0; i < 4; i++)
	    quick_widgets[i].y_divisions = Quick_input.ylen;

	for (i = 0; i < 3; i++)
	    quick_widgets[i].relative_y += 2 + lines;

	ret = quick_dialog (&Quick_input);
    }

    g_free (p_text);

    return (ret != B_CANCEL) ? my_str : NULL;
}

/*
 * Show input dialog, background safe.
 *
 * If the arguments "header" and "text" should be translated,
 * that MUST be done by the caller of these wrappers.
 */
char *
input_dialog_help (const char *header, const char *text, const char *help,
		   const char *history_name, const char *def_text)
{
    union {
	void *p;
	char * (*f) (const char *, const char *, const char *,
		      const char *, const char *);
    } func;
#ifdef WITH_BACKGROUND
    if (we_are_background)
    {
	func.f = fg_input_dialog_help;
	return parent_call_string (func.p, 5,
				   strlen (header), header, strlen (text),
				   text, strlen (help), help,
				   strlen (history_name), history_name,
				   strlen (def_text), def_text);
    }
    else
#endif				/* WITH_BACKGROUND */
	return fg_input_dialog_help (header, text, help, history_name, def_text);
}

/* Show input dialog with default help, background safe */
char *input_dialog (const char *header, const char *text,
		    const char *history_name, const char *def_text)
{
    return input_dialog_help (header, text, "[Input Line Keys]", history_name, def_text);
}

char *
input_expand_dialog (const char *header, const char *text,
		     const char *history_name, const char *def_text)
{
    char *result;
    char *expanded;

    result = input_dialog (header, text, history_name, def_text);
    if (result) {
	expanded = tilde_expand (result);
	g_free (result);
	return expanded;
    }
    return result;
}

/* }}} */

/* }}} */
/*
  Cause emacs to enter folding mode for this file:
  Local variables:
  end:
*/
