/* Pulldown menu code.
   Copyright (C) 1995 Jakub Jelinek.
   
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
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>

#include <string.h>
#include <malloc.h>
#include "mad.h"
#include "util.h"
#include "menu.h"
#include "dialog.h"
#include "xvmain.h"

extern Frame mcframe;
extern Frame menubarframe;
int menubar_visible = 1; /* We do not use this */
extern int is_right;
extern Dlg_head *midnight_dlg;

static void menu_notify_proc (Menu menu, Menu_item menu_item)
{
    void (*callback)(void *) = (void (*)(void *)) xv_get (menu_item, 
        MENU_CLIENT_DATA);
        
/*    is_right = strcmp ((char *) xv_get (menu, XV_KEY_DATA, MENU_CLIENT_DATA),
        "Left");*/
    
    xv_post_proc (midnight_dlg, callback, NULL);
}

Menu create_menu (char *name, menu_entry *entries, int count)
{
    Menu menu;
    int i;

    menu = (Menu) xv_create ((Frame)NULL, MENU,
        MENU_CLIENT_DATA, name,
        NULL);
    for (i = 0; i < count; i++)
        if (*(entries [i].text))
            xv_set (menu, 
                MENU_ITEM,
                    MENU_STRING, entries [i].text,
                    MENU_NOTIFY_PROC, menu_notify_proc,
                    MENU_CLIENT_DATA, entries [i].call_back,
                    NULL,
                NULL);
        else
            xv_set (menu,
                MENU_ITEM,
                    MENU_STRING, "",
                    MENU_FEEDBACK, FALSE,
                    NULL,
                NULL);
    return menu;
}

void destroy_menu (Menu menu)
{
}

int quit_cmd (void);

void menu_done_proc (Frame frame)
{
    xv_post_proc (midnight_dlg, (void (*)(void *))quit_cmd, NULL);
}

int create_menubar (WMenu *menubar)
{
    int i;

    Panel menubarpanel;

    menubarframe = (Frame) xv_create (mcframe, FRAME,
        XV_X, 10,
        XV_Y, 10,
        XV_WIDTH, 10000,
        XV_HEIGHT, 300,
        FRAME_LABEL, "The Midnight X Commander",
        FRAME_SHOW_FOOTER, FALSE,
        FRAME_DONE_PROC, menu_done_proc,
        NULL);
        
    menubarpanel = (Panel) xv_create (menubarframe, PANEL, 
        PANEL_LAYOUT, PANEL_HORIZONTAL,
        NULL);
    
    for (i = 0; i < menubar->items; i++)
        xv_create (menubarpanel, PANEL_BUTTON,
            PANEL_LABEL_STRING, 
                xv_get (menubar->menu [i], 
                    MENU_CLIENT_DATA,
                    NULL),
            PANEL_ITEM_MENU, menubar->menu [i],
            NULL);
    window_fit (menubarpanel);
    window_fit (menubarframe);
    xv_set (menubarframe, 
        XV_SHOW, TRUE, 
        NULL);
    menubar->widget.wdata = (widget_data) menubarframe;
    return 1;
}

static int menubar_callback (Dlg_head *h, WMenu *menubar, int msg, int par)
{
    switch (msg) {
    	case WIDGET_INIT: return create_menubar (menubar);
    }
    return default_proc (h, msg, par);
}

int menubar_event (Gpm_Event *event, WMenu *menubar)
{
    return MOU_NORMAL;
}

static void menubar_destroy (WMenu *menubar)
{
    xv_destroy_safe ((Frame)(menubar->widget.wdata));
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
