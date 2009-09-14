/* editor initialisation and callback handler.

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

/** \file
 *  \brief Source: editor initialisation and callback handler
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "../src/global.h"

#include "../src/tty/tty.h"		/* LINES, COLS */
#include "../src/tty/key.h"		/* is_idle() */

#include "edit-impl.h"
#include "edit-widget.h"

#include "../src/widget.h"		/* buttonbar_redraw() */
#include "../src/menu.h"		/* menubar_new() */

WEdit *wedit;
struct WMenu *edit_menubar;

int column_highlighting = 0;

static cb_ret_t edit_callback (Widget *, widget_msg_t msg, int parm);

static int
edit_event (Gpm_Event *event, void *data)
{
    WEdit *edit = (WEdit *) data;

    /* Unknown event type */
    if (!(event->type & (GPM_DOWN | GPM_DRAG | GPM_UP)))
	return MOU_NORMAL;

    /* rest of the upper frame, the menu is invisible - call menu */
    if ((event->type & GPM_DOWN) && (event->y == 1))
	return edit_menubar->widget.mouse (event, edit_menubar);

    edit_update_curs_row (edit);
    edit_update_curs_col (edit);

    /* Outside editor window */
    if (event->y <= 1 || event->x <= 0
	|| event->x > edit->num_widget_columns
	|| event->y > edit->num_widget_lines + 1)
	return MOU_NORMAL;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN)) {
	edit_move_up (edit, 2, 1);
	goto update;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN)) {
	edit_move_down (edit, 2, 1);
	goto update;
    }

    /* A lone up mustn't do anything */
    if (edit->mark2 != -1 && event->type & (GPM_UP | GPM_DRAG))
	return MOU_NORMAL;

    if (event->type & (GPM_DOWN | GPM_UP))
	edit_push_key_press (edit);

    if (option_cursor_beyond_eol) {
        long line_len = edit_move_forward3 (edit, edit_bol (edit, edit->curs1), 0,
                                            edit_eol(edit, edit->curs1));
        if ( event->x > line_len ) {
            edit->over_col = event->x - line_len  - option_line_state_width - 1;
            edit->prev_col = line_len;
        } else {
            edit->over_col = 0;
            edit->prev_col = event->x - option_line_state_width - 1;
        }
    } else {
        edit->prev_col = event->x - edit->start_col - option_line_state_width - 1;
    }

    if (--event->y > (edit->curs_row + 1))
	edit_move_down (edit, event->y - (edit->curs_row + 1), 0);
    else if (event->y < (edit->curs_row + 1))
	edit_move_up (edit, (edit->curs_row + 1) - event->y, 0);
    else
	edit_move_to_prev_col (edit, edit_bol (edit, edit->curs1));

    if (event->type & GPM_DOWN) {
	edit_mark_cmd (edit, 1);	/* reset */
	edit->highlight = 0;
    }

    if (!(event->type & GPM_DRAG))
	edit_mark_cmd (edit, 0);

  update:
    edit_find_bracket (edit);
    edit->force |= REDRAW_COMPLETELY;
    edit_update_curs_row (edit);
    edit_update_curs_col (edit);
    edit_update_screen (edit);

    return MOU_NORMAL;
}

static void
edit_adjust_size (Dlg_head *h)
{
    WEdit *edit;
    WButtonBar *edit_bar;

    edit = (WEdit *) find_widget_type (h, edit_callback);
    edit_bar = find_buttonbar (h);

    widget_set_size (&edit->widget, 0, 0, LINES - 1, COLS);
    widget_set_size ((Widget *) edit_bar, LINES - 1, 0, 1, COLS);
    widget_set_size (&edit_menubar->widget, 0, 0, 1, COLS);

#ifdef RESIZABLE_MENUBAR
    menubar_arrange (edit_menubar);
#endif
}

/* Callback for the edit dialog */
static cb_ret_t
edit_dialog_callback (Dlg_head *h, dlg_msg_t msg, int parm)
{
    WEdit *edit;

    switch (msg) {
    case DLG_RESIZE:
	edit_adjust_size (h);
	return MSG_HANDLED;

    case DLG_VALIDATE:
	edit = (WEdit *) find_widget_type (h, edit_callback);
	if (!edit_ok_to_exit (edit)) {
	    h->running = 1;
	}
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

int
edit_file (const char *_file, int line)
{
    static int made_directory = 0;
    Dlg_head *edit_dlg;
    WButtonBar *edit_bar;

    if (!made_directory) {
	char *dir = concat_dir_and_file (home_dir, EDIT_DIR);
	made_directory = (mkdir (dir, 0700) != -1 || errno == EEXIST);
	g_free (dir);
    }

    if (!(wedit = edit_init (NULL, LINES - 2, COLS, _file, line))) {
	return 0;
    }

    /* Create a new dialog and add it widgets to it */
    edit_dlg =
	create_dlg (0, 0, LINES, COLS, NULL, edit_dialog_callback,
		    "[Internal File Editor]", NULL, DLG_WANT_TAB);

    init_widget (&(wedit->widget), 0, 0, LINES - 1, COLS,
		 edit_callback, edit_event);

    widget_want_cursor (wedit->widget, 1);

    edit_bar = buttonbar_new (1);

    edit_menubar = edit_create_menu ();

    add_widget (edit_dlg, edit_bar);
    add_widget (edit_dlg, wedit);
    add_widget (edit_dlg, edit_menubar);

    run_dlg (edit_dlg);

    edit_done_menu (edit_menubar);		/* editmenu.c */

    destroy_dlg (edit_dlg);

    return 1;
}

const char *
edit_get_file_name (const WEdit *edit)
{
    return edit->filename;
}

static void edit_my_define (Dlg_head * h, int idx, const char *text,
			    void (*fn) (WEdit *), WEdit * edit)
{
    text = edit->labels[idx - 1]? edit->labels[idx - 1] : text;
    /* function-cast ok */
    buttonbar_set_label_data (h, idx, text, (buttonbarfn) fn, edit);
}


static void cmd_F1 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (1));
}

static void cmd_F2 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (2));
}

static void cmd_F3 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (3));
}

static void cmd_F4 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (4));
}

static void cmd_F5 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (5));
}

static void cmd_F6 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (6));
}

static void cmd_F7 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (7));
}

static void cmd_F8 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (8));
}

#if 0
static void cmd_F9 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (9));
}
#endif

static void cmd_F10 (WEdit * edit)
{
    send_message ((Widget *) edit, WIDGET_KEY, KEY_F (10));
}

static void
edit_labels (WEdit *edit)
{
    Dlg_head *h = edit->widget.parent;

    edit_my_define (h, 1, _("Help"), cmd_F1, edit);
    edit_my_define (h, 2, _("Save"), cmd_F2, edit);
    edit_my_define (h, 3, _("Mark"), cmd_F3, edit);
    edit_my_define (h, 4, _("Replac"), cmd_F4, edit);
    edit_my_define (h, 5, _("Copy"), cmd_F5, edit);
    edit_my_define (h, 6, _("Move"), cmd_F6, edit);
    edit_my_define (h, 7, _("Search"), cmd_F7, edit);
    edit_my_define (h, 8, _("Delete"), cmd_F8, edit);
    edit_my_define (h, 9, _("PullDn"), edit_menu_cmd, edit);
    edit_my_define (h, 10, _("Quit"), cmd_F10, edit);

    buttonbar_redraw (h);
}

void edit_update_screen (WEdit * e)
{
    edit_scroll_screen_over_cursor (e);

    edit_update_curs_col (e);
    edit_status (e);

/* pop all events for this window for internal handling */

    if (!is_idle ()) {
	e->force |= REDRAW_PAGE;
	return;
    }
    if (e->force & REDRAW_COMPLETELY)
	e->force |= REDRAW_PAGE;
    edit_render_keypress (e);
}

static cb_ret_t
edit_callback (Widget *w, widget_msg_t msg, int parm)
{
    WEdit *e = (WEdit *) w;

    switch (msg) {
    case WIDGET_INIT:
	e->force |= REDRAW_COMPLETELY;
	edit_labels (e);
	return MSG_HANDLED;

    case WIDGET_DRAW:
	e->force |= REDRAW_COMPLETELY;
	e->num_widget_lines = LINES - 2;
	e->num_widget_columns = COLS;
	/* fallthrough */

    case WIDGET_FOCUS:
	edit_update_screen (e);
	return MSG_HANDLED;

    case WIDGET_KEY:
	{
	    int cmd, ch;

	    /* The user may override the access-keys for the menu bar. */
	    if (edit_translate_key (e, parm, &cmd, &ch)) {
		edit_execute_key_command (e, cmd, ch);
		edit_update_screen (e);
		return MSG_HANDLED;
	    } else  if (edit_drop_hotkey_menu (e, parm)) {
		return MSG_HANDLED;
	    } else {
		return MSG_NOT_HANDLED;
	    }
	}

    case WIDGET_CURSOR:
	widget_move (&e->widget, e->curs_row + EDIT_TEXT_VERTICAL_OFFSET,
		     e->curs_col + e->start_col + e->over_col +
		     EDIT_TEXT_HORIZONTAL_OFFSET + option_line_state_width);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	edit_clean (e);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}
