#ifndef __MENU_H
#define __MENU_H

#include "widget.h"

typedef void (*callfn) (void);

typedef struct {
    char first_letter;
    const char *text;
    int  hot_key;
    callfn call_back;
} menu_entry;

typedef struct Menu {
    const char   *name;
    int    count;
    int    max_entry_len;
    int    selected;
    int    hotkey;
    menu_entry *entries;
    int    start_x;		/* position relative to menubar start */
    const char   *help_node;
} Menu;

extern int menubar_visible;

/* The button bar menu */
typedef struct WMenu {
    Widget widget;

    int    active;		/* If the menubar is in use */
    int    dropped;		/* If the menubar has dropped */
    Menu   **menu;		/* The actual menus */
    int    items;
    int    selected;		/* Selected menu on the top bar */
    int    subsel;		/* Selected entry on the submenu */
    int    max_entry_len;	/* Cache value for the columns in a box */
    int    previous_widget;	/* Selected widget ID before activating menu */
} WMenu;

Menu  *create_menu     (const char *name, menu_entry *entries, int count,
			const char *help_node);
void   destroy_menu    (Menu *menu);
WMenu *menubar_new     (int y, int x, int cols, Menu *menu[], int items);
void   menubar_arrange (WMenu *menubar);

#endif /* __MENU_H */

