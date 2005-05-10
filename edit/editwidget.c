/* editor initialisation and callback handler.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdlib.h>

#include "../src/global.h"

#include "edit.h"
#include "edit-widget.h"

#include "../src/tty.h"		/* LINES */
#include "../src/widget.h"	/* buttonbar_redraw() */
#include "../src/menu.h"	/* menubar_new() */
#include "../src/key.h"		/* is_idle() */

WEdit *wedit;
struct WMenu *edit_menubar;

int column_highlighting = 0;

static cb_ret_t edit_callback (WEdit *edit, widget_msg_t msg, int parm);

static int
edit_event (WEdit * edit, Gpm_Event * event, int *result)
{
    *result = MOU_NORMAL;
    edit_update_curs_row (edit);
    edit_update_curs_col (edit);

    /* Unknown event type */
    if (!(event->type & (GPM_DOWN | GPM_DRAG | GPM_UP)))
	return 0;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN)) {
	edit_move_up (edit, 2, 1);
	goto update;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN)) {
	edit_move_down (edit, 2, 1);
	goto update;
    }

    /* Outside editor window */
    if (event->y <= 1 || event->x <= 0
	|| event->x > edit->num_widget_columns
	|| event->y > edit->num_widget_lines + 1)
	return 0;

    /* A lone up mustn't do anything */
    if (edit->mark2 != -1 && event->type & (GPM_UP | GPM_DRAG))
	return 1;

    if (event->type & (GPM_DOWN | GPM_UP))
	edit_push_key_press (edit);

    edit_cursor_move (edit, edit_bol (edit, edit->curs1) - edit->curs1);

    if (--event->y > (edit->curs_row + 1))
	edit_cursor_move (edit,
			  edit_move_forward (edit, edit->curs1,
					     event->y - (edit->curs_row +
							 1), 0)
			  - edit->curs1);

    if (event->y < (edit->curs_row + 1))
	edit_cursor_move (edit,
			  edit_move_backward (edit, edit->curs1,
					      (edit->curs_row + 1) -
					      event->y) - edit->curs1);

    edit_cursor_move (edit,
		      (int) edit_move_forward3 (edit, edit->curs1,
						event->x -
						edit->start_col - 1,
						0) - edit->curs1);

    edit->prev_col = edit_get_col (edit);

    if (event->type & GPM_DOWN) {
	edit_mark_cmd (edit, 1);	/* reset */
	edit->highlight = 0;
    }

    if (!(event->type & GPM_DRAG))
	edit_mark_cmd (edit, 0);

  update:
    edit->force |= REDRAW_COMPLETELY;
    edit_update_curs_row (edit);
    edit_update_curs_col (edit);
    edit_update_screen (edit);

    return 1;
}


static int
edit_mouse_event (Gpm_Event *event, void *x)
{
    int result;
    if (edit_event ((WEdit *) x, event, &result))
	return result;
    else
	return (*edit_menubar->widget.mouse) (event, edit_menubar);
}

static void
edit_adjust_size (Dlg_head *h)
{
    WEdit *edit;
    WButtonBar *edit_bar;

    edit = (WEdit *) find_widget_type (h, (callback_fn) edit_callback);
    edit_bar = find_buttonbar (h);

    widget_set_size (&edit->widget, 0, 0, LINES - 1, COLS);
    widget_set_size (&edit_bar->widget, LINES - 1, 0, 1, COLS);
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
	edit = (WEdit *) find_widget_type (h, (callback_fn) edit_callback);
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

    if (option_backup_ext_int != -1) {
	option_backup_ext = g_malloc (sizeof (int) + 1);
	option_backup_ext[sizeof (int)] = '\0';
	memcpy (option_backup_ext, (char *) &option_backup_ext_int,
		sizeof (int));
    }
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
		 (callback_fn) edit_callback,
		 (mouse_h) edit_mouse_event);

    widget_want_cursor (wedit->widget, 1);

    edit_bar = buttonbar_new (1);

    switch (edit_key_emulation) {
    case EDIT_KEY_EMULATION_NORMAL:
	edit_init_menu_normal ();	/* editmenu.c */
	break;
    case EDIT_KEY_EMULATION_EMACS:
	edit_init_menu_emacs ();	/* editmenu.c */
	break;
    }
    edit_menubar = menubar_new (0, 0, COLS, EditMenuBar, N_menus);

    add_widget (edit_dlg, edit_bar);
    add_widget (edit_dlg, wedit);
    add_widget (edit_dlg, edit_menubar);

    run_dlg (edit_dlg);

    edit_done_menu ();		/* editmenu.c */

    destroy_dlg (edit_dlg);

    return 1;
}

static void edit_my_define (Dlg_head * h, int idx, const char *text,
			    void (*fn) (WEdit *), WEdit * edit)
{
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
edit_callback (WEdit *e, widget_msg_t msg, int parm)
{
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

	    /* first check alt-f, alt-e, alt-s, etc for drop menus */
	    if (edit_drop_hotkey_menu (e, parm))
		return MSG_HANDLED;
	    if (!edit_translate_key (e, parm, &cmd, &ch))
		return MSG_NOT_HANDLED;
	    edit_execute_key_command (e, cmd, ch);
	    edit_update_screen (e);
	}
	return MSG_HANDLED;

    case WIDGET_CURSOR:
	widget_move (&e->widget, e->curs_row + EDIT_TEXT_VERTICAL_OFFSET,
		     e->curs_col + e->start_col);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	edit_clean (e);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}
