/*
 * GNU Midnight Commander/GNOME edition: Pulldown menu code
 *
 * Copyright (C) 1997 the Free Software Foundation.
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 */

#include <config.h>
#include <string.h>
#include <malloc.h>
#include "main.h"
#include "mad.h"
#include "menu.h"
#include "x.h"

/* Unused but required by the rest of the code */
int menubar_visible = 1;

/* We assume that menu titles are only one word length */
Menu create_menu (char *name, menu_entry *entries, int count)
{
    Menu menu;

    menu = (Menu) xmalloc (sizeof (*menu), "create_menu");
    menu->count = count;
    menu->max_entry_len = 0;
    menu->entries = entries;
    menu->name = name;
    return menu;
}

static int menubar_callback (Dlg_head *h, WMenu *menubar, int msg, int par)
{
    if (msg == WIDGET_FOCUS)
	return 0;
    
    return default_proc (h, msg, par);
}

int menubar_event (Gpm_Event *event, WMenu *menubar)
{
    return MOU_NORMAL;
}

static void menubar_destroy (WMenu *menubar)
{
    /* nothing yet */
}

WMenu *menubar_new (int y, int x, int cols, Menu menu [], int items)
{
    WMenu *menubar = (WMenu *) xmalloc (sizeof (WMenu), "menubar_new");
    GtkWidget *g_menubar;
    int i, j;
    
    init_widget (&menubar->widget, y, x, 1, cols, (callback_fn) menubar_callback,
		 (destroy_fn)  menubar_destroy, (mouse_h) menubar_event, NULL);
    menubar->menu = menu;
    menubar->active = 0;
    menubar->dropped = 0;
    menubar->items = items;
    menubar->selected = 0;
    widget_want_cursor (menubar->widget, 0);
    
    g_menubar = gtk_menu_bar_new ();
    gtk_widget_show (g_menubar);
    menubar->widget.wdata = (unsigned long) g_menubar;

    for (i = 0; i < items; i++){
	    GtkWidget *child;

	    child = gtk_menu_new ();
	    for (j = 0; j < menu [i]->count; j++){
		    GtkWidget *entry;
		    
		    entry = gtk_label_new (menu [i]->entries->text);
		    gtk_object_set_data (GTK_OBJECT(entry), "callback", &menu [i]->entries);
		    gtk_widget_show (entry);
		    gtk_menu_append (GTK_MENU (child), entry);

		    /* FIXME: gtk_signal_connect (.... "activate", blah blah, */
	    }
	    gtk_menu_bar_append (GTK_MENU_BAR(g_menubar), child);
    }
    return menubar;
}

void
menubar_arrange(WMenu* menubar)
{
}

void
destroy_menu (Menu menu)
{
}
