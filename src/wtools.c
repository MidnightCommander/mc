/* Widget based utility functions.
   Copyright (C) 1994, 1995 the Free Software Foundation
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "global.h"
#include "tty.h"
#include "color.h"		/* dialog_colors */
#include "dialog.h"
#include "widget.h"
#include "wtools.h"
#include "key.h"		/* mi_getch() */
#include "complete.h"		/* INPUT_COMPLETE_CD */
#include "background.h"		/* parent_call */


Listbox *
create_listbox_window (int cols, int lines, const char *title, const char *help)
{
    int xpos, ypos, len;
    Listbox *listbox = g_new (Listbox, 1);
    const char *cancel_string = _("&Cancel");

    /* Adjust sizes */
    lines = (lines > LINES - 6) ? LINES - 6 : lines;

    if (title && (cols < (len = strlen (title) + 2)))
	cols = len;

    /* no &, but 4 spaces around button for brackets and such */
    if (cols < (len = strlen (cancel_string) + 3))
	cols = len;

    cols = cols > COLS - 6 ? COLS - 6 : cols;
    xpos = (COLS - cols) / 2;
    ypos = (LINES - lines) / 2 - 2;

    /* Create components */
    listbox->dlg =
	create_dlg (ypos, xpos, lines + 6, cols + 4, dialog_colors, NULL,
		    help, title, DLG_CENTER | DLG_REVERSE);

    listbox->list = listbox_new (2, 2, cols, lines, 0);

    add_widget (listbox->dlg,
		button_new (lines + 3, (cols / 2 + 2) - len / 2, B_CANCEL,
			    NORMAL_BUTTON, const_cast(char *, cancel_string), 0));
    add_widget (listbox->dlg, listbox->list);

    return listbox;
}

/* Returns the number of the item selected */
int run_listbox (Listbox *l)
{
    int val;
    
    run_dlg (l->dlg);
    if (l->dlg->ret_value == B_CANCEL)
	val = -1;
    else
	val = l->list->pos;
    destroy_dlg (l->dlg);
    g_free (l);
    return val;
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
    int xpos, ypos;
    int cols, lines;
    char *cur_name;
    static const int *query_colors;

    /* set dialog colors */
    if (flags & D_ERROR)
	query_colors = alarm_colors;
    else
	query_colors = dialog_colors;

    if (header == MSG_ERROR)
	header = _("Error");

    if (count > 0) {
	va_start (ap, count);
	for (i = 0; i < count; i++) {
	    char *cp = va_arg (ap, char *);
	    win_len += strlen (cp) + 6;
	    if (strchr (cp, '&') != NULL)
		win_len--;
	}
	va_end (ap);
    }

    /* count coordinates */
    cols = 6 + max (win_len, max ((int) strlen (header), msglen (text, &lines)));
    lines += 4 + (count > 0 ? 2 : 0);
    xpos = COLS / 2 - cols / 2;
    ypos = LINES / 3 - (lines - 3) / 2;

    /* prepare dialog */
    query_dlg =
	create_dlg (ypos, xpos, lines, cols, query_colors, NULL,
		    "[QueryBox]", header, DLG_NONE);

    if (count > 0) {
	cols = (cols - win_len - 2) / 2 + 2;
	va_start (ap, count);
	for (i = 0; i < count; i++) {
	    cur_name = va_arg (ap, char *);
	    xpos = strlen (cur_name) + 6;
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

	if (defbutton)
	    dlg_select_widget (query_dlg, defbutton);

	/* run dialog and make result */
	run_dlg (query_dlg);
	switch (query_dlg->ret_value) {
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
    p = g_strconcat ("\n", text, "\n", NULL);
    query_dialog (title, p, flags, 0);
    d = last_query_dlg;
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
    mi_getch ();
    dlg_run_done (d);
    destroy_dlg (d);
}


/* Show message box from background */
#ifdef WITH_BACKGROUND
static void
bg_message (int dummy, int *flags, char *title, const char *text)
{
    title = g_strconcat (_("Background process:"), " ", title, NULL);
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

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);

    if (title == MSG_ERROR)
	title = _("Error");

#ifdef WITH_BACKGROUND
    if (we_are_background) {
	parent_call ((void *) bg_message, NULL, 3, sizeof (flags), &flags,
		     strlen (title), title, strlen (p), p);
    } else
#endif				/* WITH_BACKGROUND */
	fg_message (flags, title, p);

    g_free (p);
}


/* {{{ Quick dialog routines */

#define I18N(x) (do_int && *x ? (x = _(x)): x)

int
quick_dialog_skip (QuickDialog *qd, int nskip)
{
    Dlg_head *dd;
    void *widget;
    WRadio *r;
    int xpos;
    int ypos;
    int return_val;
    WInput *input;
    QuickWidget *qw;
    int do_int;
    int count = 0;		/* number of quick widgets */
    int curr_widget;		/* number of the current quick widget */
    Widget **widgets;		/* table of corresponding widgets */

    if (!qd->i18n) {
	qd->i18n = 1;
	do_int = 1;
	if (*qd->title)
	    qd->title = _(qd->title);
    } else
	do_int = 0;

    if (qd->xpos == -1)
	dd = create_dlg (0, 0, qd->ylen, qd->xlen, dialog_colors, NULL,
			 qd->help, qd->title,
			 DLG_CENTER | DLG_TRYUP | DLG_REVERSE);
    else
	dd = create_dlg (qd->ypos, qd->xpos, qd->ylen, qd->xlen,
			 dialog_colors, NULL, qd->help, qd->title,
			 DLG_REVERSE);

    /* We pass this to the callback */
    dd->cols = qd->xlen;
    dd->lines = qd->ylen;

    /* Count widgets */
    for (qw = qd->widgets; qw->widget_type; qw++) {
	count++;
    }

    widgets = (Widget **) g_new (Widget *, count);

    for (curr_widget = 0, qw = qd->widgets; qw->widget_type; qw++) {
	xpos = (qd->xlen * qw->relative_x) / qw->x_divisions;
	ypos = (qd->ylen * qw->relative_y) / qw->y_divisions;

	switch (qw->widget_type) {
	case quick_checkbox:
	    widget = check_new (ypos, xpos, *qw->result, I18N (qw->text));
	    break;

	case quick_radio:
	    r = radio_new (ypos, xpos, qw->hotkey_pos, const_cast(const char **, qw->str_result), 1);
	    r->pos = r->sel = qw->value;
	    widget = r;
	    break;

	case quick_button:
	    widget =
		button_new (ypos, xpos, qw->value,
			    (qw->value ==
			     B_ENTER) ? DEFPUSH_BUTTON : NORMAL_BUTTON,
			    I18N (qw->text), 0);
	    break;

	    /* We use the hotkey pos as the field length */
	case quick_input:
	    input =
		input_new (ypos, xpos, INPUT_COLOR, qw->hotkey_pos,
			   qw->text, qw->histname);
	    input->is_password = qw->value == 1;
	    input->point = 0;
	    if (qw->value & 2)
		input->completion_flags |= INPUT_COMPLETE_CD;
	    widget = input;
	    break;

	case quick_label:
	    widget = label_new (ypos, xpos, I18N (qw->text));
	    break;

	default:
	    widget = 0;
	    fprintf (stderr, "QuickWidget: unknown widget type\n");
	    break;
	}
	widgets[curr_widget++] = widget;
	add_widget (dd, widget);
    }

    while (nskip--)
	dd->current = dd->current->next;

    run_dlg (dd);

    /* Get the data if we found something interesting */
    if (dd->ret_value != B_CANCEL) {
	for (curr_widget = 0, qw = qd->widgets; qw->widget_type; qw++) {
	    Widget *w = widgets[curr_widget++];

	    switch (qw->widget_type) {
	    case quick_checkbox:
		*qw->result = ((WCheck *) w)->state & C_BOOL;
		break;

	    case quick_radio:
		*qw->result = ((WRadio *) w)->sel;
		break;

	    case quick_input:
		if (qw->value & 2)
		    *qw->str_result =
			tilde_expand (((WInput *) w)->buffer);
		else
		    *qw->str_result = g_strdup (((WInput *) w)->buffer);
		break;
	    }
	}
    }
    return_val = dd->ret_value;
    destroy_dlg (dd);
    g_free (widgets);

    return return_val;
}

int quick_dialog (QuickDialog *qd)
{
    return quick_dialog_skip (qd, 0);
}

/* }}} */

/* {{{ Input routines */

#define INPUT_INDEX 2

/* Show dialog, not background safe */
static char *
fg_input_dialog_help (const char *header, const char *text, const char *help,
			const char *def_text)
{
    QuickDialog Quick_input;
    QuickWidget quick_widgets[] = {
	{quick_button, 6, 10, 1, 0, N_("&Cancel"), 0, B_CANCEL, 0, 0,
	 NULL},
	{quick_button, 3, 10, 1, 0, N_("&OK"), 0, B_ENTER, 0, 0, NULL},
	{quick_input, 4, 80, 0, 0, "", 58, 0, 0, 0, NULL},
	{quick_label, 4, 80, 2, 0, "", 0, 0, 0, 0, NULL},
	NULL_QuickWidget
    };

    int len;
    int i;
    int lines;
    int ret;
    char *my_str;
    char histname[64] = "inp|";

    /* we need a unique name for histname because widget.c:history_tool()
       needs a unique name for each dialog - using the header is ideal */
    strncpy (histname + 3, header, 60);
    histname[63] = '\0';
    quick_widgets[2].histname = histname;

    len = max ((int) strlen (header), msglen (text, &lines)) + 4;
    len = max (len, 64);

    /* The special value of def_text is used to identify password boxes
       and hide characters with "*".  Don't save passwords in history! */
    if (def_text == INPUT_PASSWORD) {
	quick_widgets[INPUT_INDEX].value = 1;
	histname[3] = 0;
	def_text = "";
    } else {
	quick_widgets[INPUT_INDEX].value = 0;
    }

#ifdef ENABLE_NLS
    /* 
     * An attempt to place buttons symmetrically, based on actual i18n
     * length of the string. It looks nicer with i18n (IMO) - alex
     */
    quick_widgets[0].relative_x = len / 2 + 4;
    quick_widgets[1].relative_x =
	len / 2 - (strlen (_(quick_widgets[1].text)) + 9);
    quick_widgets[0].x_divisions = quick_widgets[1].x_divisions = len;
#endif				/* ENABLE_NLS */

    Quick_input.xlen = len;
    Quick_input.xpos = -1;
    Quick_input.title = const_cast(char *, header);
    Quick_input.help = const_cast(char *, help);
    Quick_input.i18n = 0;
    quick_widgets[INPUT_INDEX + 1].text = g_strstrip (g_strdup (text));
    quick_widgets[INPUT_INDEX].text = const_cast(char *, def_text);

    for (i = 0; i < 4; i++)
	quick_widgets[i].y_divisions = lines + 6;
    Quick_input.ylen = lines + 6;

    for (i = 0; i < 3; i++)
	quick_widgets[i].relative_y += 2 + lines;

    quick_widgets[INPUT_INDEX].str_result = &my_str;

    Quick_input.widgets = quick_widgets;
    ret = quick_dialog (&Quick_input);
    g_free (quick_widgets[INPUT_INDEX + 1].text);

    if (ret != B_CANCEL) {
	return *(quick_widgets[INPUT_INDEX].str_result);
    } else
	return 0;
}

/* Show input dialog, background safe */
char *
input_dialog_help (const char *header, const char *text, const char *help, const char *def_text)
{
#ifdef WITH_BACKGROUND
    if (we_are_background)
	return parent_call_string ((void *) fg_input_dialog_help, 4,
				   strlen (header), header, strlen (text),
				   text, strlen (help), help,
				   strlen (def_text), def_text);
    else
#endif				/* WITH_BACKGROUND */
	return fg_input_dialog_help (header, text, help, def_text);
}

/* Show input dialog with default help, background safe */
char *input_dialog (const char *header, const char *text, const char *def_text)
{
    return input_dialog_help (header, text, "[Input Line Keys]", def_text);
}

char *
input_expand_dialog (const char *header, const char *text, const char *def_text)
{
    char *result;
    char *expanded;

    result = input_dialog (header, text, def_text);
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
