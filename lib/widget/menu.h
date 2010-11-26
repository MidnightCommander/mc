/* Header file for pulldown menu engine for Midnignt Commander
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2009 Free Software Foundation, Inc.

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

/** \file menu.h
 *  \brief Header: pulldown menu code
 */

#ifndef MC__WIDGET_MENU_H
#define MC__WIDGET_MENU_H

/*** typedefs(not structures) and defined constants **********************************************/

#define menu_separator_create() NULL

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct menu_entry_t
{
    unsigned char first_letter;
    hotkey_t text;
    unsigned long command;
    char *shortcut;
} menu_entry_t;

typedef struct Menu
{
    int start_x;                /* position relative to menubar start */
    hotkey_t text;
    GList *entries;
    size_t max_entry_len;       /* cached max length of entry texts (text + shortcut) */
    size_t max_hotkey_len;      /* cached max length of shortcuts */
    unsigned int selected;      /* pointer to current menu entry */
    char *help_node;
} Menu;

/* The button bar menu */
typedef struct WMenuBar
{
    Widget widget;

    gboolean is_visible;        /* If the menubar is visible */
    gboolean is_active;         /* If the menubar is in use */
    gboolean is_dropped;        /* If the menubar has dropped */
    GList *menu;                /* The actual menus */
    size_t selected;            /* Selected menu on the top bar */
    int previous_widget;        /* Selected widget ID before activating menu */
} WMenuBar;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

menu_entry_t *menu_entry_create (const char *name, unsigned long command);
void menu_entry_free (menu_entry_t * me);

Menu *create_menu (const char *name, GList * entries, const char *help_node);
void menu_set_name (Menu * menu, const char *name);
void destroy_menu (Menu * menu);

WMenuBar *menubar_new (int y, int x, int cols, GList * menu);
void menubar_set_menu (WMenuBar * menubar, GList * menu);
void menubar_add_menu (WMenuBar * menubar, Menu * menu);
void menubar_arrange (WMenuBar * menubar);

WMenuBar *find_menubar (const Dlg_head * h);

/*** inline functions ****************************************************************************/

static inline void
menubar_set_visible (WMenuBar * menubar, gboolean visible)
{
    menubar->is_visible = visible;
}

#endif /* MC__WIDGET_MENU_H */
