/* Midnight Commander/Tk: Pulldown menu code.
   Copyright (C) 1995 Miguel de Icaza
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include <string.h>
#include <malloc.h>
#include "main.h"
#include "tkmain.h"
#include "mad.h"
#include "menu.h"

/* Unused but required by the rest of the code */
int menubar_visible = 1;

static int
tkmenu_callback (ClientData cd, Tcl_Interp *interp,
		 int argc, char *argv [])
{
    callfn callback = (callfn) cd;

    is_right = (argv [1][0] != '1');
    (*callback)();
    return TCL_OK;
}

/* We assume that menu titles are only one word length */
Menu create_menu (char *name, menu_entry *entries, int count)
{
    int i;
    char *xname = strdup (name), *p;
    static int menu_number;
    
    /* Lower case string, Tk requires this */
    for (p = xname; *p; p++){
	if (*p < 'a')
	    *p |= 0x20;
    }
    /* And strip trailing space            */
    *(p-1) = 0;
    
    /* Strip leading space */
    for (p = xname; *p == ' '; p++)
	;

    menu_number++;
    tk_evalf ("create_menu %s %s", name, p);
    for (i = 0; i < count; i++){
	if (entries [i].text [0]){
	    char cmd_name [20];

	    sprintf (cmd_name, "m%d%s", i, p);
	    Tcl_CreateCommand (interp, cmd_name, tkmenu_callback,
	        entries [i].call_back, NULL);
	    tk_evalf ("create_mentry %s {%s } %s %d", p, entries [i].text,
		      cmd_name, menu_number);
	} else
	    tk_evalf ("add_separator %s", xname);
    }
    free (xname);
    return 0;
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

    init_widget (&menubar->widget, y, x, 1, cols,
                 (callback_fn) menubar_callback,
		 (destroy_fn)  menubar_destroy,
		 (mouse_h)     menubar_event, NULL);
    menubar->menu = menu;
    menubar->active = 0;
    menubar->dropped = 0;
    menubar->items = items;
    menubar->selected = 0;
    widget_want_cursor (menubar->widget, 0);
    
    return menubar;
}

void
menubar_arrange (WMenu* menubar)
{
    /* nothing to do I think (Norbert) */
}

void
destroy_menu (Menu menu)
{
    /* FIXME: need to implement? (Norbert) */
}

