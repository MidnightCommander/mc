/*
   Header file for pulldown menu engine for Midnignt Commander
 */

/** \file menu.h
 *  \brief Header: pulldown menu code
 */

#ifndef MC__WIDGET_MENU_H
#define MC__WIDGET_MENU_H

/*** typedefs(not structures) and defined constants **********************************************/

#define MENUBAR(x) ((WMenuBar *)(x))

#define menu_separator_create() NULL

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct menu_entry_t;
typedef struct menu_entry_t menu_entry_t;

struct menu_t;
typedef struct menu_t menu_t;

/* The button bar menu */
typedef struct WMenuBar
{
    Widget widget;

    gboolean is_visible;        /* If the menubar is visible */
    gboolean is_active;         /* If the menubar is in use */
    gboolean is_dropped;        /* If the menubar has dropped */
    GList *menu;                /* The actual menus */
    size_t selected;            /* Selected menu on the top bar */
    unsigned long previous_widget;      /* Selected widget ID before activating menu */
} WMenuBar;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

menu_entry_t *menu_entry_create (const char *name, unsigned long command);
void menu_entry_free (menu_entry_t * me);

menu_t *create_menu (const char *name, GList * entries, const char *help_node);
void menu_set_name (menu_t * menu, const char *name);
void destroy_menu (menu_t * menu);

WMenuBar *menubar_new (int y, int x, int cols, GList * menu);
void menubar_set_menu (WMenuBar * menubar, GList * menu);
void menubar_add_menu (WMenuBar * menubar, menu_t * menu);
void menubar_arrange (WMenuBar * menubar);

WMenuBar *find_menubar (const WDialog * h);

/*** inline functions ****************************************************************************/

static inline void
menubar_set_visible (WMenuBar * menubar, gboolean visible)
{
    menubar->is_visible = visible;
}

#endif /* MC__WIDGET_MENU_H */
