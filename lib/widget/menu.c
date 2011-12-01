/*
   Pulldown menu code

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009, 2011
   The Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file menu.c
 *  \brief Source: pulldown menu code
 */

#include <config.h>

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"        /* key macros */
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static cb_ret_t menubar_callback (Widget * w, widget_msg_t msg, int parm);

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
menu_arrange (Menu * menu, dlg_shortcut_str get_shortcut)
{
    if (menu != NULL)
    {
        GList *i;
        size_t max_shortcut_len = 0;

        menu->max_entry_len = 1;
        menu->max_hotkey_len = 1;

        for (i = menu->entries; i != NULL; i = g_list_next (i))
        {
            menu_entry_t *entry = i->data;

            if (entry != NULL)
            {
                size_t len;

                len = (size_t) hotkey_width (entry->text);
                menu->max_hotkey_len = max (menu->max_hotkey_len, len);

                if (get_shortcut != NULL && entry->type == MENU_ENTITY_TYPE_COMMAND)
                    entry->shortcut = get_shortcut (entry->u.command);

                if (entry->shortcut != NULL)
                {
                    len = (size_t) str_term_width1 (entry->shortcut);
                    max_shortcut_len = max (max_shortcut_len, len);
                }
            }
        }

        menu->max_entry_len = menu->max_hotkey_len + max_shortcut_len;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_paint_idx (WMenuBar * menubar, unsigned int idx, int color)
{
    const Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const menu_entry_t *entry = g_list_nth_data (menu->entries, idx);
    const int y = 2 + idx;
    int x = menu->start_x;

    if (x + menu->max_entry_len + 4 > (gsize) menubar->widget.cols)
        x = menubar->widget.cols - menu->max_entry_len - 4;

    if (entry == NULL)
    {
        /* menu separator */
        tty_setcolor (MENU_ENTRY_COLOR);

        widget_move (&menubar->widget, y, x - 1);
        tty_print_alt_char (ACS_LTEE, FALSE);

        tty_draw_hline (menubar->widget.y + y, menubar->widget.x + x,
                        ACS_HLINE, menu->max_entry_len + 3);

        widget_move (&menubar->widget, y, x + menu->max_entry_len + 3);
        tty_print_alt_char (ACS_RTEE, FALSE);
    }
    else
    {
        /* menu text */
        tty_setcolor (color);
        widget_move (&menubar->widget, y, x);
        tty_print_char ((unsigned char) entry->first_letter);
        tty_draw_hline (-1, -1, ' ', menu->max_entry_len + 2);  /* clear line */
        tty_print_string (entry->text.start);

        if (entry->text.hotkey != NULL)
        {
            tty_setcolor (color == MENU_SELECTED_COLOR ? MENU_HOTSEL_COLOR : MENU_HOT_COLOR);
            tty_print_string (entry->text.hotkey);
            tty_setcolor (color);
        }

        if (entry->text.end != NULL)
            tty_print_string (entry->text.end);

        if (entry->shortcut != NULL)
        {
            widget_move (&menubar->widget, y, x + menu->max_hotkey_len + 3);
            tty_print_string (entry->shortcut);
        }

        /* move cursor to the start of entry text */
        widget_move (&menubar->widget, y, x + 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_draw_drop (WMenuBar * menubar)
{
    const Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const unsigned int count = g_list_length (menu->entries);
    int column = menu->start_x - 1;
    unsigned int i;

    if (column + menu->max_entry_len + 5 > (gsize) menubar->widget.cols)
        column = menubar->widget.cols - menu->max_entry_len - 5;

    tty_setcolor (MENU_ENTRY_COLOR);
    draw_box (menubar->widget.owner,
              menubar->widget.y + 1, menubar->widget.x + column,
              count + 2, menu->max_entry_len + 5, FALSE);

    for (i = 0; i < count; i++)
        menubar_paint_idx (menubar, i,
                           i == menu->selected ? MENU_SELECTED_COLOR : MENU_ENTRY_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_set_color (WMenuBar * menubar, gboolean current, gboolean hotkey)
{
    if (!menubar->is_active)
        tty_setcolor (MENU_INACTIVE_COLOR);
    else if (current)
        tty_setcolor (hotkey ? MENU_HOTSEL_COLOR : MENU_SELECTED_COLOR);
    else
        tty_setcolor (hotkey ? MENU_HOT_COLOR : MENU_ENTRY_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_draw (WMenuBar * menubar)
{
    GList *i;

    /* First draw the complete menubar */
    tty_setcolor (menubar->is_active ? MENU_ENTRY_COLOR : MENU_INACTIVE_COLOR);
    tty_draw_hline (menubar->widget.y, menubar->widget.x, ' ', menubar->widget.cols);

    /* Now each one of the entries */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        Menu *menu = i->data;
        gboolean is_selected = (menubar->selected == (gsize) g_list_position (menubar->menu, i));

        menubar_set_color (menubar, is_selected, FALSE);
        widget_move (&menubar->widget, 0, menu->start_x);

        tty_print_char (' ');
        tty_print_string (menu->text.start);

        if (menu->text.hotkey != NULL)
        {
            menubar_set_color (menubar, is_selected, TRUE);
            tty_print_string (menu->text.hotkey);
            menubar_set_color (menubar, is_selected, FALSE);
        }

        if (menu->text.end != NULL)
            tty_print_string (menu->text.end);

        tty_print_char (' ');
    }

    if (menubar->is_dropped)
        menubar_draw_drop (menubar);
    else
        widget_move (&menubar->widget, 0,
                     ((Menu *) g_list_nth_data (menubar->menu, menubar->selected))->start_x);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_remove (WMenuBar * menubar)
{
    if (menubar->is_dropped)
    {
        menubar->is_dropped = FALSE;
        do_refresh ();
        menubar->is_dropped = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_left (WMenuBar * menubar)
{
    menubar_remove (menubar);
    if (menubar->selected == 0)
        menubar->selected = g_list_length (menubar->menu) - 1;
    else
        menubar->selected--;
    menubar_draw (menubar);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_right (WMenuBar * menubar)
{
    menubar_remove (menubar);
    menubar->selected = (menubar->selected + 1) % g_list_length (menubar->menu);
    menubar_draw (menubar);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_finish (WMenuBar * menubar)
{
    menubar->is_dropped = FALSE;
    menubar->is_active = FALSE;
    menubar->widget.lines = 1;
    widget_want_hotkey (menubar->widget, 0);

    dlg_select_by_id (menubar->widget.owner, menubar->previous_widget);
    do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_drop (WMenuBar * menubar, unsigned int selected)
{
    menubar->is_dropped = TRUE;
    menubar->selected = selected;
    menubar_draw (menubar);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_execute (WMenuBar * menubar)
{
    const Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const menu_entry_t *entry = g_list_nth_data (menu->entries, menu->selected);


    if (entry == NULL)
        return;


    switch (entry->type)
    {
    case MENU_ENTITY_TYPE_COMMAND:
        if (entry->u.command != CK_IgnoreKey)
        {
            mc_global.is_right = (menubar->selected != 0);
            menubar_finish (menubar);
            menubar->widget.owner->callback (menubar->widget.owner, &menubar->widget,
                                             DLG_ACTION, entry->u.command, NULL);
            do_refresh ();
        }
        break;
    case MENU_ENTITY_TYPE_EVENT:
        if (entry->u.event.group != NULL && entry->u.event.name != NULL)
        {
            mc_global.is_right = (menubar->selected != 0);
            menubar_finish (menubar);
            mc_event_raise (entry->u.event.group, entry->u.event.name, entry->u.event.data);
            do_refresh ();
        }
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_down (WMenuBar * menubar)
{
    Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    do
    {
        menu->selected = (menu->selected + 1) % len;
        entry = (menu_entry_t *) g_list_nth_data (menu->entries, menu->selected);
    }
    while ((entry == NULL)
           || (entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command == CK_IgnoreKey)
           || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name == NULL));

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_up (WMenuBar * menubar)
{
    Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    do
    {
        if (menu->selected == 0)
            menu->selected = len - 1;
        else
            menu->selected--;
        entry = (menu_entry_t *) g_list_nth_data (menu->entries, menu->selected);
    }
    while ((entry == NULL)
           || (entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command == CK_IgnoreKey)
           || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name == NULL));

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_first (WMenuBar * menubar)
{
    Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    menu_entry_t *entry;

    if (menu->selected == 0)
        return;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    menu->selected = 0;

    while (TRUE)
    {
        entry = (menu_entry_t *) g_list_nth_data (menu->entries, menu->selected);

        if ((entry == NULL)
            || (entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command == CK_IgnoreKey)
            || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name == NULL))
            menu->selected++;
        else
            break;
    }

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_last (WMenuBar * menubar)
{
    Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    if (menu->selected == len - 1)
        return;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    menu->selected = len;

    do
    {
        menu->selected--;
        entry = (menu_entry_t *) g_list_nth_data (menu->entries, menu->selected);
    }
    while ((entry == NULL)
           || (entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command == CK_IgnoreKey)
           || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name == NULL));

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static int
menubar_handle_key (WMenuBar * menubar, int key)
{
    /* Lowercase */
    if (isascii (key))
        key = g_ascii_tolower (key);

    if (is_abort_char (key))
    {
        menubar_finish (menubar);
        return 1;
    }

    /* menubar help or menubar navigation */
    switch (key)
    {
    case KEY_F (1):
        {
            ev_help_t event_data = { NULL, NULL };

            if (menubar->is_dropped)
                event_data.node =
                    ((Menu *) g_list_nth_data (menubar->menu, menubar->selected))->help_node;
            else
                event_data.node = "[Menu Bar]";

            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
            menubar_draw (menubar);
            return 1;
        }
    case KEY_LEFT:
    case XCTRL ('b'):
        menubar_left (menubar);
        return 1;

    case KEY_RIGHT:
    case XCTRL ('f'):
        menubar_right (menubar);
        return 1;
    }

    if (!menubar->is_dropped)
    {
        GList *i;

        /* drop menu by hotkey */
        for (i = menubar->menu; i != NULL; i = g_list_next (i))
        {
            Menu *menu = i->data;

            if ((menu->text.hotkey != NULL) && (key == g_ascii_tolower (menu->text.hotkey[0])))
            {
                menubar_drop (menubar, g_list_position (menubar->menu, i));
                return 1;
            }
        }

        /* drop menu by Enter or Dowwn key */
        if (key == KEY_ENTER || key == XCTRL ('n') || key == KEY_DOWN || key == '\n')
            menubar_drop (menubar, menubar->selected);

        return 1;
    }

    {
        Menu *menu = g_list_nth_data (menubar->menu, menubar->selected);
        GList *i;

        /* execute menu command by hotkey */
        for (i = menu->entries; i != NULL; i = g_list_next (i))
        {
            const menu_entry_t *entry = i->data;

            if ((entry != NULL)
                && ((entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command != CK_IgnoreKey)
                    || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name != NULL))
                && (entry->text.hotkey != NULL) && (key == g_ascii_tolower (entry->text.hotkey[0])))
            {
                menu->selected = g_list_position (menu->entries, i);
                menubar_execute (menubar);
                return 1;
            }
        }

        /* menu execute by Enter or menu navigation */
        switch (key)
        {
        case KEY_ENTER:
        case '\n':
            menubar_execute (menubar);
            return 1;

        case KEY_HOME:
        case ALT ('<'):
            menubar_first (menubar);
            break;

        case KEY_END:
        case ALT ('>'):
            menubar_last (menubar);
            break;

        case KEY_DOWN:
        case XCTRL ('n'):
            menubar_down (menubar);
            break;

        case KEY_UP:
        case XCTRL ('p'):
            menubar_up (menubar);
            break;
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
menubar_callback (Widget * w, widget_msg_t msg, int parm)
{
    WMenuBar *menubar = (WMenuBar *) w;

    switch (msg)
    {
        /* We do not want the focus unless we have been activated */
    case WIDGET_FOCUS:
        if (!menubar->is_active)
            return MSG_NOT_HANDLED;

        /* Trick to get all the mouse events */
        menubar->widget.lines = LINES;

        /* Trick to get all of the hotkeys */
        widget_want_hotkey (menubar->widget, 1);
        menubar_draw (menubar);
        return MSG_HANDLED;

        /* We don't want the buttonbar to activate while using the menubar */
    case WIDGET_HOTKEY:
    case WIDGET_KEY:
        if (menubar->is_active)
        {
            menubar_handle_key (menubar, parm);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case WIDGET_CURSOR:
        /* Put the cursor in a suitable place */
        return MSG_NOT_HANDLED;

    case WIDGET_UNFOCUS:
        return menubar->is_active ? MSG_NOT_HANDLED : MSG_HANDLED;

    case WIDGET_DRAW:
        if (menubar->is_visible)
        {
            menubar_draw (menubar);
            return MSG_HANDLED;
        }
        /* fall through */

    case WIDGET_RESIZED:
        /* try show menu after screen resize */
        send_message (w, WIDGET_FOCUS, 0);
        return MSG_HANDLED;


    case WIDGET_DESTROY:
        menubar_set_menu (menubar, NULL);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
menubar_event (Gpm_Event * event, void *data)
{
    WMenuBar *menubar = data;
    gboolean was_active = TRUE;
    int left_x, right_x, bottom_y;
    Menu *menu;

    /* ignore unsupported events */
    if ((event->type & (GPM_UP | GPM_DOWN | GPM_DRAG)) == 0)
        return MOU_NORMAL;

    /* ignore wheel events if menu is inactive */
    if (!menubar->is_active && ((event->buttons & (GPM_B_MIDDLE | GPM_B_UP | GPM_B_DOWN)) != 0))
        return MOU_NORMAL;

    if (!menubar->is_dropped)
    {
        menubar->previous_widget = dlg_get_current_widget_id (menubar->widget.owner);
        menubar->is_active = TRUE;
        menubar->is_dropped = TRUE;
        was_active = FALSE;
    }

    /* Mouse operations on the menubar */
    if (event->y == 1 || !was_active)
    {
        if ((event->type & GPM_UP) != 0)
            return MOU_NORMAL;

        /* wheel events on menubar */
        if (event->buttons & GPM_B_UP)
            menubar_left (menubar);
        else if (event->buttons & GPM_B_DOWN)
            menubar_right (menubar);
        else
        {
            const unsigned int len = g_list_length (menubar->menu);
            unsigned int new_selection = 0;

            while ((new_selection < len)
                   && (event->x > ((Menu *) g_list_nth_data (menubar->menu,
                                                             new_selection))->start_x))
                new_selection++;

            if (new_selection != 0)     /* Don't set the invalid value -1 */
                new_selection--;

            if (!was_active)
            {
                menubar->selected = new_selection;
                dlg_select_widget (menubar);
            }
            else
            {
                menubar_remove (menubar);
                menubar->selected = new_selection;
            }
            menubar_draw (menubar);
        }
        return MOU_NORMAL;
    }

    if (!menubar->is_dropped || (event->y < 2))
        return MOU_NORMAL;

    /* middle click -- everywhere */
    if (((event->buttons & GPM_B_MIDDLE) != 0) && ((event->type & GPM_DOWN) != 0))
    {
        menubar_execute (menubar);
        return MOU_NORMAL;
    }

    /* the mouse operation is on the menus or it is not */
    menu = (Menu *) g_list_nth_data (menubar->menu, menubar->selected);
    left_x = menu->start_x;
    right_x = left_x + menu->max_entry_len + 3;
    if (right_x > menubar->widget.cols)
    {
        left_x = menubar->widget.cols - menu->max_entry_len - 3;
        right_x = menubar->widget.cols;
    }

    bottom_y = g_list_length (menu->entries) + 3;

    if ((event->x >= left_x) && (event->x <= right_x) && (event->y <= bottom_y))
    {
        int pos = event->y - 3;
        const menu_entry_t *entry = g_list_nth_data (menu->entries, pos);

        /* mouse wheel */
        if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN))
        {
            menubar_up (menubar);
            return MOU_NORMAL;
        }
        if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN))
        {
            menubar_down (menubar);
            return MOU_NORMAL;
        }

        /* ignore events above and below dropped down menu */
        if ((pos < 0) || (pos >= bottom_y - 3))
            return MOU_NORMAL;

        if ((entry != NULL)
            && ((entry->type == MENU_ENTITY_TYPE_COMMAND && entry->u.command != CK_IgnoreKey)
                || (entry->type == MENU_ENTITY_TYPE_EVENT && entry->u.event.name != NULL)))
        {
            menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);
            menu->selected = pos;
            menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);

            if ((event->type & GPM_UP) != 0)
                menubar_execute (menubar);
        }
    }
    else
        /* use click not wheel to close menu */
    if (((event->type & GPM_DOWN) != 0) && ((event->buttons & (GPM_B_UP | GPM_B_DOWN)) == 0))
        menubar_finish (menubar);

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

menu_entry_t *
menu_entry_create (const char *name, unsigned long command)
{
    menu_entry_t *entry;

    entry = g_new0 (menu_entry_t, 1);
    entry->first_letter = ' ';
    entry->type = MENU_ENTITY_TYPE_COMMAND;
    entry->text = parse_hotkey (name);
    entry->u.command = command;
    entry->shortcut = NULL;

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

menu_entry_t *
menu_entry_create_event (const char *name, const char *event_group, const char *event_name,
                         gpointer event_data)
{
    menu_entry_t *entry;

    entry = g_new0 (menu_entry_t, 1);
    entry->first_letter = ' ';
    entry->type = MENU_ENTITY_TYPE_EVENT;
    entry->text = parse_hotkey (name);
    entry->u.event.group = event_group;
    entry->u.event.name = event_name;
    entry->u.event.data = event_data;
    entry->shortcut = NULL;

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

void
menu_entry_free (menu_entry_t * entry)
{
    if (entry != NULL)
    {
        release_hotkey (entry->text);
        g_free (entry->shortcut);
        g_free (entry);
    }
}

/* --------------------------------------------------------------------------------------------- */

Menu *
create_menu (const char *name, GList * entries, const char *help_node)
{
    Menu *menu;

    menu = g_new (Menu, 1);
    menu->start_x = 0;
    menu->text = parse_hotkey (name);
    menu->entries = entries;
    menu->max_entry_len = 1;
    menu->max_hotkey_len = 0;
    menu->selected = 0;
    menu->help_node = g_strdup (help_node);

    return menu;
}

/* --------------------------------------------------------------------------------------------- */

void
menu_set_name (Menu * menu, const char *name)
{
    release_hotkey (menu->text);
    menu->text = parse_hotkey (name);
}

/* --------------------------------------------------------------------------------------------- */

void
destroy_menu (Menu * menu)
{
    release_hotkey (menu->text);
    g_list_foreach (menu->entries, (GFunc) menu_entry_free, NULL);
    g_list_free (menu->entries);
    g_free (menu->help_node);
    g_free (menu);
}

/* --------------------------------------------------------------------------------------------- */

WMenuBar *
menubar_new (int y, int x, int cols, GList * menu)
{
    WMenuBar *menubar;

    menubar = g_new0 (WMenuBar, 1);
    init_widget (&menubar->widget, y, x, 1, cols, menubar_callback, menubar_event);
    widget_want_cursor (menubar->widget, FALSE);
    menubar->is_visible = TRUE; /* by default */
    menubar_set_menu (menubar, menu);

    return menubar;
}

/* --------------------------------------------------------------------------------------------- */

void
menubar_set_menu (WMenuBar * menubar, GList * menu)
{
    /* delete previous menu */
    if (menubar->menu != NULL)
    {
        g_list_foreach (menubar->menu, (GFunc) destroy_menu, NULL);
        g_list_free (menubar->menu);
    }
    /* add new menu */
    menubar->is_active = FALSE;
    menubar->is_dropped = FALSE;
    menubar->menu = menu;
    menubar->selected = 0;
    menubar_arrange (menubar);
}

/* --------------------------------------------------------------------------------------------- */

void
menubar_add_menu (WMenuBar * menubar, Menu * menu)
{
    if (menu != NULL)
    {
        menu_arrange (menu, menubar->widget.owner->get_shortcut);
        menubar->menu = g_list_append (menubar->menu, menu);
    }

    menubar_arrange (menubar);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Properly space menubar items. Should be called when menubar is created
 * and also when widget width is changed (i.e. upon xterm resize).
 */

void
menubar_arrange (WMenuBar * menubar)
{
    int start_x = 1;
    GList *i;
    int gap;

    if (menubar->menu == NULL)
        return;

#ifndef RESIZABLE_MENUBAR
    gap = 3;

    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        Menu *menu = i->data;
        int len = hotkey_width (menu->text) + 2;

        menu->start_x = start_x;
        start_x += len + gap;
    }
#else /* RESIZABLE_MENUBAR */
    gap = menubar->widget.cols - 2;

    /* First, calculate gap between items... */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        Menu *menu = i->data;
        /* preserve length here, to be used below */
        menu->start_x = hotkey_width (menu->text) + 2;
        gap -= menu->start_x;
    }

    gap /= (menubar->menu->len - 1);

    if (gap <= 0)
    {
        /* We are out of luck - window is too narrow... */
        gap = 1;
    }

    /* ...and now fix start positions of menubar items */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        Menu *menu = i->data;
        int len = menu->start_x;

        menu->start_x = start_x;
        start_x += len + gap;
    }
#endif /* RESIZABLE_MENUBAR */
}

/* --------------------------------------------------------------------------------------------- */
/** Find MenuBar widget in the dialog */

WMenuBar *
find_menubar (const Dlg_head * h)
{
    return (WMenuBar *) find_widget_type (h, menubar_callback);
}

/* --------------------------------------------------------------------------------------------- */
