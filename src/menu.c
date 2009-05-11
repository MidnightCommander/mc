/* Pulldown menu code.
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file menu.c
 *  \brief Source: pulldown menu code
 */

#include <config.h>

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/mouse.h"
#include "../src/tty/key.h"	/* For mi_getch() */
#include "../src/tty/win.h"

#include "menu.h"
#include "help.h"
#include "dialog.h"
#include "main.h"
#include "strutil.h"

int menubar_visible = 1;	/* This is the new default */

Menu *
create_menu (const char *name, menu_entry *entries, int count, const char *help_node)
{
    Menu *menu;

    menu = g_new (Menu, 1);
    menu->count = count;
    menu->max_entry_len = 20;
    menu->entries = entries;
    menu->text = parse_hotkey (name);

    if (entries != (menu_entry*) NULL) {
        int len;
	register menu_entry* mp;
	for (mp = entries; count--; mp++) {
            if (mp->label[0] != '\0') {
                mp->text = parse_hotkey (_(mp->label));
                len = hotkey_width (mp->text);
                menu->max_entry_len = max (len, menu->max_entry_len);
	    }
	}
    }

    menu->start_x = 0;
    menu->help_node = g_strdup (help_node);
    return menu;
}

static void menubar_drop_compute (WMenu *menubar)
{
    menubar->max_entry_len = menubar->menu [menubar->selected]->max_entry_len;
}

static void menubar_paint_idx (WMenu *menubar, int idx, int color)
{
    const Menu *menu = menubar->menu[menubar->selected];
    const int y = 2 + idx;
    int x = menu->start_x;
    const menu_entry *entry = &menu->entries[idx];

    if (x + menubar->max_entry_len + 3 > menubar->widget.cols)
        x = menubar->widget.cols - menubar->max_entry_len - 3;

    if (entry->text.start == NULL) {
        /* menu separator */
        tty_setcolor (SELECTED_COLOR);

        if (!slow_terminal) {
            widget_move (&menubar->widget, y, x - 1);
            tty_print_alt_char (ACS_LTEE);
        }

        tty_print_hline (menubar->widget.y + y, menubar->widget.x + x,
                            menubar->max_entry_len + 2);

        if (!slow_terminal)
            tty_print_alt_char (ACS_RTEE);
    } else {
        /* menu text */
        tty_setcolor (color);
        widget_move (&menubar->widget, y, x);
        addch ((unsigned char) entry->first_letter);
        hline (' ', menubar->max_entry_len + 1); /* clear line */
        addstr (str_term_form (entry->text.start));

        if (entry->text.hotkey != NULL) {
            tty_setcolor (color == MENU_SELECTED_COLOR ?
                        MENU_HOTSEL_COLOR : MENU_HOT_COLOR);
            addstr (str_term_form (entry->text.hotkey));
            tty_setcolor(color);
        }

        if (entry->text.end != NULL)
            addstr (str_term_form (entry->text.end));

        /* move cursor to the start of entry text */
        widget_move (&menubar->widget, y, x + 1);
    }
}

static inline void menubar_draw_drop (WMenu *menubar)
{
    const int count = menubar->menu [menubar->selected]->count;
    int column = menubar->menu [menubar->selected]->start_x - 1;
    int i;

    if (column + menubar->max_entry_len + 4 > menubar->widget.cols)
        column = menubar->widget.cols - menubar->max_entry_len - 4;

    tty_setcolor (SELECTED_COLOR);
    draw_box (menubar->widget.parent,
	      menubar->widget.y + 1, menubar->widget.x + column,
	      count + 2, menubar->max_entry_len + 4);

    /* draw items except selected */
    for (i = 0; i < count; i++)
        if (i != menubar->subsel)
            menubar_paint_idx (menubar, i, MENU_ENTRY_COLOR);

    /* draw selected item at last to move cursot to the nice location */
    menubar_paint_idx (menubar, menubar->subsel, MENU_SELECTED_COLOR);
}

static void menubar_draw (WMenu *menubar)
{
    const int items = menubar->items;
    int   i;

    /* First draw the complete menubar */
    tty_setcolor (SELECTED_COLOR);
    widget_move (&menubar->widget, 0, 0);

    hline (' ', menubar->widget.cols);

    tty_setcolor (SELECTED_COLOR);
    /* Now each one of the entries */
    for (i = 0; i < items; i++){
        tty_setcolor ((menubar->active && i == menubar->selected) ? 
                MENU_SELECTED_COLOR : SELECTED_COLOR);
	widget_move (&menubar->widget, 0, menubar->menu [i]->start_x);

        addstr (str_term_form (menubar->menu[i]->text.start));

        if (menubar->menu[i]->text.hotkey != NULL) {
            tty_setcolor ((menubar->active && i == menubar->selected) ? 
                    MENU_HOTSEL_COLOR : COLOR_HOT_FOCUS);
            addstr (str_term_form (menubar->menu[i]->text.hotkey));
            tty_setcolor ((menubar->active && i == menubar->selected) ? 
                    MENU_SELECTED_COLOR : SELECTED_COLOR);
        }
        if (menubar->menu[i]->text.end != NULL) {
            addstr (str_term_form (menubar->menu[i]->text.end));
        }
    }

    if (menubar->dropped)
	menubar_draw_drop (menubar);
    else
	widget_move (&menubar->widget, 0,
		menubar-> menu[menubar->selected]->start_x);
}

static inline void menubar_remove (WMenu *menubar)
{
    menubar->subsel = 0;
    if (menubar->dropped){
	menubar->dropped = 0;
	do_refresh ();
	menubar->dropped = 1;
    }
}

static void menubar_left (WMenu *menu)
{
    menubar_remove (menu);
    menu->selected = (menu->selected - 1) % menu->items;
    if (menu->selected < 0)
	menu->selected = menu->items -1;
    menubar_drop_compute (menu);
    menubar_draw (menu);
}

static void menubar_right (WMenu *menu)
{
    menubar_remove (menu);
    menu->selected = (menu->selected + 1) % menu->items;
    menubar_drop_compute (menu);
    menubar_draw (menu);
}

static void
menubar_finish (WMenu *menubar)
{
    menubar->dropped = 0;
    menubar->active = 0;
    menubar->widget.lines = 1;
    widget_want_hotkey (menubar->widget, 0);

    dlg_select_by_id (menubar->widget.parent, menubar->previous_widget);
    do_refresh ();
}

static void menubar_drop (WMenu *menubar, int selected)
{
    menubar->dropped = 1;
    menubar->selected = selected;
    menubar->subsel = 0;
    menubar_drop_compute (menubar);
    menubar_draw (menubar);
}

static void menubar_execute (WMenu *menubar, int entry)
{
    const Menu *menu = menubar->menu [menubar->selected];
    const callfn call_back = menu->entries [entry].call_back;
    
    is_right = menubar->selected != 0;

    /* This used to be the other way round, i.e. first callback and 
       then menubar_finish. The new order (hack?) is needed to make 
       change_panel () work which is used in quick_view_cmd () -- Norbert
    */
    menubar_finish (menubar);
    (*call_back) ();
    do_refresh ();
}

static void menubar_move (WMenu *menubar, int step)
{
    const Menu *menu = menubar->menu [menubar->selected];

    menubar_paint_idx (menubar, menubar->subsel, MENU_ENTRY_COLOR);
    do {
	menubar->subsel += step;
	if (menubar->subsel < 0)
	    menubar->subsel = menu->count - 1;
	
	menubar->subsel %= menu->count;
    } while  (!menu->entries [menubar->subsel].call_back);
    menubar_paint_idx (menubar, menubar->subsel, MENU_SELECTED_COLOR);
}

static int menubar_handle_key (WMenu *menubar, int key)
{
    int   i;

    /* Lowercase */
    if (isascii (key)) key = g_ascii_tolower (key);
    
    if (is_abort_char (key)){
	menubar_finish (menubar);
	return 1;
    }

    if (key == KEY_F(1)) {
	if (menubar->dropped) {
	    interactive_display (NULL,
		    (menubar->menu [menubar->selected])->help_node);
	} else {
	    interactive_display (NULL, "[Menu Bar]");
	}
	menubar_draw (menubar);
	return 1;
    }

    if (key == KEY_LEFT || key == XCTRL('b')){
	menubar_left (menubar);
	return 1;
    } else if (key == KEY_RIGHT || key == XCTRL ('f')){
	menubar_right (menubar);
	return 1;
    }

    if (!menubar->dropped){
	const int items = menubar->items;
        for (i = 0; i < items; i++) {
	    const Menu *menu = menubar->menu [i];

            if (menu->text.hotkey != NULL) {
                if (g_ascii_tolower(menu->text.hotkey[0]) == key) {
		menubar_drop (menubar, i);
		return 1; 
	    }
	}
        }
        if (key == KEY_ENTER || key == XCTRL ('n') 
            || key == KEY_DOWN || key == '\n') {
            
	    menubar_drop (menubar, menubar->selected);
	    return 1;
	}
	return 1;
    } else {
	const int selected = menubar->selected;
	const Menu *menu = menubar->menu [selected];
	const int items = menu->count;
	
        for (i = 0; i < items; i++) {
	    if (!menu->entries [i].call_back)
		continue;
	    
            if (menu->entries[i].text.hotkey != NULL) {
                if (key != g_ascii_tolower (menu->entries[i].text.hotkey[0]))
			continue;
	    
	    menubar_execute (menubar, i);
	    return 1;
	}
        }

        if (key == KEY_ENTER || key == '\n') {
	    menubar_execute (menubar, menubar->subsel);
	    return 1;
	}
	
	
	if (key == KEY_DOWN || key == XCTRL ('n'))
	    menubar_move (menubar, 1);
	
	if (key == KEY_UP || key == XCTRL ('p'))
	    menubar_move (menubar, -1);
    }
    return 0;
}

static cb_ret_t
menubar_callback (Widget *w, widget_msg_t msg, int parm)
{
    WMenu *menubar = (WMenu *) w;

    switch (msg) {
	/* We do not want the focus unless we have been activated */
    case WIDGET_FOCUS:
	if (!menubar->active)
	    return MSG_NOT_HANDLED;

	widget_want_cursor (menubar->widget, 1);

	/* Trick to get all the mouse events */
	menubar->widget.lines = LINES;

	/* Trick to get all of the hotkeys */
	widget_want_hotkey (menubar->widget, 1);
	menubar->subsel = 0;
	menubar_drop_compute (menubar);
	menubar_draw (menubar);
	return MSG_HANDLED;

	/* We don't want the buttonbar to activate while using the menubar */
    case WIDGET_HOTKEY:
    case WIDGET_KEY:
	if (menubar->active) {
	    menubar_handle_key (menubar, parm);
	    return MSG_HANDLED;
	} else
	    return MSG_NOT_HANDLED;

    case WIDGET_CURSOR:
	/* Put the cursor in a suitable place */
	return MSG_NOT_HANDLED;

    case WIDGET_UNFOCUS:
	if (menubar->active)
	    return MSG_NOT_HANDLED;
	else {
	    widget_want_cursor (menubar->widget, 0);
	    return MSG_HANDLED;
	}

    case WIDGET_DRAW:
	if (menubar_visible) {
	    menubar_draw (menubar);
	    return MSG_HANDLED;
	}
	/* fall through */

    case WIDGET_RESIZED:
	/* try show menu after screen resize */
	send_message (w, WIDGET_FOCUS, 0);
	return MSG_HANDLED;
	
    default:
	return default_proc (msg, parm);
    }
}

static int
menubar_event    (Gpm_Event *event, void *data)
{
    WMenu *menubar = data;
    int was_active;
    int new_selection;
    int left_x, right_x, bottom_y;

    if (!(event->type & (GPM_UP|GPM_DOWN|GPM_DRAG)))
	return MOU_NORMAL;
    
    if (!menubar->dropped){
	menubar->previous_widget = menubar->widget.parent->current->dlg_id;
	menubar->active = 1;
	menubar->dropped = 1;
	was_active = 0;
    } else
	was_active = 1;

    /* Mouse operations on the menubar */
    if (event->y == 1 || !was_active){
	if (event->type & GPM_UP)
	    return MOU_NORMAL;
    
	new_selection = 0;
	while (new_selection < menubar->items 
		&& event->x > menubar->menu[new_selection]->start_x
	)
		new_selection++;

	if (new_selection) /* Don't set the invalid value -1 */
		--new_selection;
	
	if (!was_active){
	    menubar->selected = new_selection;
	    dlg_select_widget (menubar);
	    menubar_drop_compute (menubar);
	    menubar_draw (menubar);
	    return MOU_NORMAL;
	}
	
	menubar_remove (menubar);

	menubar->selected = new_selection;

	menubar_drop_compute (menubar);
	menubar_draw (menubar);
	return MOU_NORMAL;
    }

    if (!menubar->dropped)
	return MOU_NORMAL;
    
    /* Ignore the events on anything below the third line */
    if (event->y <= 2)
	return MOU_NORMAL;
    
    /* Else, the mouse operation is on the menus or it is not */
	left_x = menubar->menu[menubar->selected]->start_x;
	right_x = left_x + menubar->max_entry_len + 4;
	if (right_x > menubar->widget.cols)
	{
		left_x = menubar->widget.cols - menubar->max_entry_len - 3;
		right_x = menubar->widget.cols - 1;
	}

    bottom_y = (menubar->menu [menubar->selected])->count + 3;

    if ((event->x > left_x) && (event->x < right_x) && (event->y < bottom_y)){
	int pos = event->y - 3;

	if (!menubar->menu [menubar->selected]->entries [pos].call_back)
	    return MOU_NORMAL;
	
	menubar_paint_idx (menubar, menubar->subsel, MENU_ENTRY_COLOR);
	menubar->subsel = pos;
	menubar_paint_idx (menubar, menubar->subsel, MENU_SELECTED_COLOR);

	if (event->type & GPM_UP)
	    menubar_execute (menubar, pos);
    } else
	if (event->type & GPM_DOWN)
	    menubar_finish (menubar);
	 
    return MOU_NORMAL;
}

/*
 * Properly space menubar items. Should be called when menubar is created
 * and also when widget width is changed (i.e. upon xterm resize).
 */
void
menubar_arrange(WMenu* menubar)
{
	register int i, start_x = 1;
	int items = menubar->items;

#ifndef RESIZABLE_MENUBAR
	int gap = 3;

	for (i = 0; i < items; i++)
	{
		int len = hotkey_width (menubar->menu[i]->text);
		menubar->menu[i]->start_x = start_x;
		start_x += len + gap;
	}

#else /* RESIZABLE_MENUBAR */

	int gap = menubar->widget.cols - 2;

	/* First, calculate gap between items... */
	for (i = 0; i < items; i++)
	{
		/* preserve length here, to be used below */
		gap -= (menubar->menu[i]->start_x = hotkey_width (menubar->menu[i]->text));
	}

	gap /= (items - 1);

	if (gap <= 0)
	{
		/* We are out of luck - window is too narrow... */
		gap = 1;
	}

	/* ...and now fix start positions of menubar items */
	for (i = 0; i < items; i++)
	{
		int len = menubar->menu[i]->start_x;
		menubar->menu[i]->start_x = start_x;
		start_x += len + gap;
	}
#endif /* RESIZABLE_MENUBAR */
}

void
destroy_menu (Menu *menu)
{
    release_hotkey (menu->text);
    if (menu->entries != NULL) {
        int me;
        for (me = 0; me < menu->count; me++) {
            if (menu->entries[me].label[0] != '\0') {
                release_hotkey (menu->entries[me].text);
            }
        }
    }

    g_free (menu->help_node);
    g_free (menu);
}

WMenu *
menubar_new (int y, int x, int cols, Menu *menu[], int items)
{
    WMenu *menubar = g_new0 (WMenu, 1);

    init_widget (&menubar->widget, y, x, 1, cols,
		 menubar_callback, menubar_event);
    menubar->menu = menu;
    menubar->active = 0;
    menubar->dropped = 0;
    menubar->items = items;
    menubar->selected = 0;
    menubar->subsel = 0;
    widget_want_cursor (menubar->widget, 0);
    menubar_arrange (menubar);

    return menubar;
}
