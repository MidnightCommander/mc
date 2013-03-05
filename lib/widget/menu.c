/*
   Pulldown menu code

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009, 2011, 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2012, 2013

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

#define MENUENTRY(x) ((menu_entry_t *)(x))
#define MENU(x) ((menu_t *)(x))

/*** file scope type declarations ****************************************************************/

struct menu_entry_t
{
    unsigned char first_letter;
    hotkey_t text;
    unsigned long command;
    char *shortcut;
};

struct menu_t
{
    int start_x;                /* position relative to menubar start */
    hotkey_t text;
    GList *entries;
    size_t max_entry_len;       /* cached max length of entry texts (text + shortcut) */
    size_t max_hotkey_len;      /* cached max length of shortcuts */
    unsigned int selected;      /* pointer to current menu entry */
    char *help_node;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
menu_arrange (menu_t * menu, dlg_shortcut_str get_shortcut)
{
    if (menu != NULL)
    {
        GList *i;
        size_t max_shortcut_len = 0;

        menu->max_entry_len = 1;
        menu->max_hotkey_len = 1;

        for (i = menu->entries; i != NULL; i = g_list_next (i))
        {
            menu_entry_t *entry = MENUENTRY (i->data);

            if (entry != NULL)
            {
                size_t len;

                len = (size_t) hotkey_width (entry->text);
                menu->max_hotkey_len = max (menu->max_hotkey_len, len);

                if (get_shortcut != NULL)
                    entry->shortcut = get_shortcut (entry->command);

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
    Widget *w = WIDGET (menubar);
    const menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const menu_entry_t *entry = MENUENTRY (g_list_nth_data (menu->entries, idx));
    const int y = 2 + idx;
    int x = menu->start_x;

    if (x + menu->max_entry_len + 4 > (gsize) w->cols)
        x = w->cols - menu->max_entry_len - 4;

    if (entry == NULL)
    {
        /* menu separator */
        tty_setcolor (MENU_ENTRY_COLOR);

        widget_move (w, y, x - 1);
        tty_print_alt_char (ACS_LTEE, FALSE);
        tty_draw_hline (w->y + y, w->x + x, ACS_HLINE, menu->max_entry_len + 3);
        widget_move (w, y, x + menu->max_entry_len + 3);
        tty_print_alt_char (ACS_RTEE, FALSE);
    }
    else
    {
        int yt, xt;

        /* menu text */
        tty_setcolor (color);
        widget_move (w, y, x);
        tty_print_char ((unsigned char) entry->first_letter);
        tty_getyx (&yt, &xt);
        tty_draw_hline (yt, xt, ' ', menu->max_entry_len + 2);  /* clear line */
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
            widget_move (w, y, x + menu->max_hotkey_len + 3);
            tty_print_string (entry->shortcut);
        }

        /* move cursor to the start of entry text */
        widget_move (w, y, x + 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_draw_drop (WMenuBar * menubar)
{
    Widget *w = WIDGET (menubar);
    const menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const unsigned int count = g_list_length (menu->entries);
    int column = menu->start_x - 1;
    unsigned int i;

    if (column + menu->max_entry_len + 5 > (gsize) w->cols)
        column = w->cols - menu->max_entry_len - 5;

    tty_setcolor (MENU_ENTRY_COLOR);
    draw_box (w->owner, w->y + 1, w->x + column, count + 2, menu->max_entry_len + 5, FALSE);

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
    Widget *w = WIDGET (menubar);
    GList *i;

    /* First draw the complete menubar */
    tty_setcolor (menubar->is_active ? MENU_ENTRY_COLOR : MENU_INACTIVE_COLOR);
    tty_draw_hline (w->y, w->x, ' ', w->cols);

    /* Now each one of the entries */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        menu_t *menu = MENU (i->data);
        gboolean is_selected = (menubar->selected == (gsize) g_list_position (menubar->menu, i));

        menubar_set_color (menubar, is_selected, FALSE);
        widget_move (w, 0, menu->start_x);

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
        widget_move (w, 0, MENU (g_list_nth_data (menubar->menu, menubar->selected))->start_x);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_remove (WMenuBar * menubar)
{
    WDialog *h;

    if (!menubar->is_dropped)
        return;

    /* HACK: before refresh the dialog, change the current widget to keep the order
       of overlapped widgets. This is useful in multi-window editor.
       In general, menubar should be a special object, not an ordinary widget
       in the current dialog. */
    h = WIDGET (menubar)->owner;
    h->current = g_list_find (h->widgets, dlg_find_by_id (h, menubar->previous_widget));

    menubar->is_dropped = FALSE;
    do_refresh ();
    menubar->is_dropped = TRUE;

    /* restore current widget */
    h->current = g_list_find (h->widgets, menubar);
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
    Widget *w = WIDGET (menubar);

    menubar->is_dropped = FALSE;
    menubar->is_active = FALSE;
    w->lines = 1;
    widget_want_hotkey (w, 0);

    dlg_select_by_id (w->owner, menubar->previous_widget);
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
    const menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const menu_entry_t *entry = MENUENTRY (g_list_nth_data (menu->entries, menu->selected));

    if ((entry != NULL) && (entry->command != CK_IgnoreKey))
    {
        Widget *w = WIDGET (menubar);

        mc_global.widget.is_right = (menubar->selected != 0);
        menubar_finish (menubar);
        send_message (w->owner, w, MSG_ACTION, entry->command, NULL);
        do_refresh ();
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_down (WMenuBar * menubar)
{
    menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    do
    {
        menu->selected = (menu->selected + 1) % len;
        entry = MENUENTRY (g_list_nth_data (menu->entries, menu->selected));
    }
    while ((entry == NULL) || (entry->command == CK_IgnoreKey));

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_up (WMenuBar * menubar)
{
    menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    do
    {
        if (menu->selected == 0)
            menu->selected = len - 1;
        else
            menu->selected--;
        entry = MENUENTRY (g_list_nth_data (menu->entries, menu->selected));
    }
    while ((entry == NULL) || (entry->command == CK_IgnoreKey));

    menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

static void
menubar_first (WMenuBar * menubar)
{
    menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    menu_entry_t *entry;

    if (menu->selected == 0)
        return;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    menu->selected = 0;

    while (TRUE)
    {
        entry = MENUENTRY (g_list_nth_data (menu->entries, menu->selected));

        if ((entry == NULL) || (entry->command == CK_IgnoreKey))
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
    menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    const unsigned int len = g_list_length (menu->entries);
    menu_entry_t *entry;

    if (menu->selected == len - 1)
        return;

    menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);

    menu->selected = len;

    do
    {
        menu->selected--;
        entry = MENUENTRY (g_list_nth_data (menu->entries, menu->selected));
    }
    while ((entry == NULL) || (entry->command == CK_IgnoreKey));

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
                    MENU (g_list_nth_data (menubar->menu, menubar->selected))->help_node;
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
            menu_t *menu = MENU (i->data);

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
        menu_t *menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
        GList *i;

        /* execute menu command by hotkey */
        for (i = menu->entries; i != NULL; i = g_list_next (i))
        {
            const menu_entry_t *entry = MENUENTRY (i->data);

            if ((entry != NULL) && (entry->command != CK_IgnoreKey)
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
menubar_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WMenuBar *menubar = MENUBAR (w);

    switch (msg)
    {
        /* We do not want the focus unless we have been activated */
    case MSG_FOCUS:
        if (!menubar->is_active)
            return MSG_NOT_HANDLED;

        /* Trick to get all the mouse events */
        w->lines = LINES;

        /* Trick to get all of the hotkeys */
        widget_want_hotkey (w, 1);
        menubar_draw (menubar);
        return MSG_HANDLED;

        /* We don't want the buttonbar to activate while using the menubar */
    case MSG_HOTKEY:
    case MSG_KEY:
        if (menubar->is_active)
        {
            menubar_handle_key (menubar, parm);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_CURSOR:
        /* Put the cursor in a suitable place */
        return MSG_NOT_HANDLED;

    case MSG_UNFOCUS:
        return menubar->is_active ? MSG_NOT_HANDLED : MSG_HANDLED;

    case MSG_DRAW:
        if (menubar->is_visible)
        {
            menubar_draw (menubar);
            return MSG_HANDLED;
        }
        /* fall through */

    case MSG_RESIZE:
        /* try show menu after screen resize */
        send_message (w, sender, MSG_FOCUS, 0, data);
        return MSG_HANDLED;


    case MSG_DESTROY:
        menubar_set_menu (menubar, NULL);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
menubar_event (Gpm_Event * event, void *data)
{
    WMenuBar *menubar = MENUBAR (data);
    Widget *w = WIDGET (data);
    gboolean was_active = TRUE;
    int left_x, right_x, bottom_y;
    menu_t *menu;
    Gpm_Event local;

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    /* ignore unsupported events */
    if ((event->type & (GPM_UP | GPM_DOWN | GPM_DRAG)) == 0)
        return MOU_NORMAL;

    /* ignore wheel events if menu is inactive */
    if (!menubar->is_active && ((event->buttons & (GPM_B_MIDDLE | GPM_B_UP | GPM_B_DOWN)) != 0))
        return MOU_NORMAL;

    local = mouse_get_local (event, w);

    if (local.y == 1 && (local.type & GPM_UP) != 0)
        return MOU_NORMAL;

    if (!menubar->is_dropped)
    {
        if (local.y > 1)
        {
            /* mouse click below menubar -- close menu and send focus to widget under mouse */
            menubar_finish (menubar);
            return MOU_UNHANDLED;
        }

        menubar->previous_widget = dlg_get_current_widget_id (w->owner);
        menubar->is_active = TRUE;
        menubar->is_dropped = TRUE;
        was_active = FALSE;
    }

    /* Mouse operations on the menubar */
    if (local.y == 1 || !was_active)
    {
        /* wheel events on menubar */
        if ((local.buttons & GPM_B_UP) != 0)
            menubar_left (menubar);
        else if ((local.buttons & GPM_B_DOWN) != 0)
            menubar_right (menubar);
        else
        {
            const unsigned int len = g_list_length (menubar->menu);
            unsigned int new_selection = 0;

            while ((new_selection < len)
                   && (local.x > MENU (g_list_nth_data (menubar->menu, new_selection))->start_x))
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

    if (!menubar->is_dropped || (local.y < 2))
        return MOU_NORMAL;

    /* middle click -- everywhere */
    if (((local.buttons & GPM_B_MIDDLE) != 0) && ((local.type & GPM_DOWN) != 0))
    {
        menubar_execute (menubar);
        return MOU_NORMAL;
    }

    /* the mouse operation is on the menus or it is not */
    menu = MENU (g_list_nth_data (menubar->menu, menubar->selected));
    left_x = menu->start_x;
    right_x = left_x + menu->max_entry_len + 3;
    if (right_x > w->cols)
    {
        left_x = w->cols - menu->max_entry_len - 3;
        right_x = w->cols;
    }

    bottom_y = g_list_length (menu->entries) + 3;

    if ((local.x >= left_x) && (local.x <= right_x) && (local.y <= bottom_y))
    {
        int pos = local.y - 3;
        const menu_entry_t *entry = MENUENTRY (g_list_nth_data (menu->entries, pos));

        /* mouse wheel */
        if ((local.buttons & GPM_B_UP) != 0 && (local.type & GPM_DOWN) != 0)
        {
            menubar_up (menubar);
            return MOU_NORMAL;
        }
        if ((local.buttons & GPM_B_DOWN) != 0 && (local.type & GPM_DOWN) != 0)
        {
            menubar_down (menubar);
            return MOU_NORMAL;
        }

        /* ignore events above and below dropped down menu */
        if ((pos < 0) || (pos >= bottom_y - 3))
            return MOU_NORMAL;

        if ((entry != NULL) && (entry->command != CK_IgnoreKey))
        {
            menubar_paint_idx (menubar, menu->selected, MENU_ENTRY_COLOR);
            menu->selected = pos;
            menubar_paint_idx (menubar, menu->selected, MENU_SELECTED_COLOR);

            if ((event->type & GPM_UP) != 0)
                menubar_execute (menubar);
        }
    }
    else if (((local.type & GPM_DOWN) != 0) && ((local.buttons & (GPM_B_UP | GPM_B_DOWN)) == 0))
    {
        /* use click not wheel to close menu */
        menubar_finish (menubar);
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

menu_entry_t *
menu_entry_create (const char *name, unsigned long command)
{
    menu_entry_t *entry;

    entry = g_new (menu_entry_t, 1);
    entry->first_letter = ' ';
    entry->text = parse_hotkey (name);
    entry->command = command;
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

menu_t *
create_menu (const char *name, GList * entries, const char *help_node)
{
    menu_t *menu;

    menu = g_new (menu_t, 1);
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
menu_set_name (menu_t * menu, const char *name)
{
    release_hotkey (menu->text);
    menu->text = parse_hotkey (name);
}

/* --------------------------------------------------------------------------------------------- */

void
destroy_menu (menu_t * menu)
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
    Widget *w;

    menubar = g_new0 (WMenuBar, 1);
    w = WIDGET (menubar);
    init_widget (w, y, x, 1, cols, menubar_callback, menubar_event);

    menubar->is_visible = TRUE; /* by default */
    widget_want_cursor (w, FALSE);
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
menubar_add_menu (WMenuBar * menubar, menu_t * menu)
{
    if (menu != NULL)
    {
        menu_arrange (menu, WIDGET (menubar)->owner->get_shortcut);
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

    gap = WIDGET (menubar)->cols - 2;

    /* First, calculate gap between items... */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        menu_t *menu = MENU (i->data);

        /* preserve length here, to be used below */
        menu->start_x = hotkey_width (menu->text) + 2;
        gap -= menu->start_x;
    }

    if (g_list_next (menubar->menu) == NULL)
        gap = 1;
    else
        gap /= (g_list_length (menubar->menu) - 1);

    if (gap <= 0)
    {
        /* We are out of luck - window is too narrow... */
        gap = 1;
    }
    else if (gap >= 3)
        gap = 3;

    /* ...and now fix start positions of menubar items */
    for (i = menubar->menu; i != NULL; i = g_list_next (i))
    {
        menu_t *menu = MENU (i->data);
        int len = menu->start_x;

        menu->start_x = start_x;
        start_x += len + gap;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Find MenuBar widget in the dialog */

WMenuBar *
find_menubar (const WDialog * h)
{
    return MENUBAR (find_widget_type (h, menubar_callback));
}

/* --------------------------------------------------------------------------------------------- */
