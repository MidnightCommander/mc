/* XView viewer routines.
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

#include <X11/Xlib.h>
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/svrimage.h>
#include <xview/canvas.h>
#include <xview/scrollbar.h>
#include <xview/cms.h>
#include <xview/xv_xrect.h>
#include <xview/textsw.h>

#include <stdio.h>
#include "fs.h"
#include "dlg.h"
#define WANT_WIDGETS
#include "view.h"
#include "../src/util.h"
#include "../src/mad.h"
#include "xvmain.h"

extern Frame mcframe;
Frame viewframe;
Panel viewpanel;
Textsw viewcanvas;
Cms viewcms;

extern void view_destroy(WView *);

static void x_quit_cmd (Panel_button_item);

/*static struct {
    unsigned short *bits;
    void (*callback) (void);
} buttons [] = { 
index_bits, x_index_cmd,
back_bits, x_back_cmd,
previous_bits, x_previous_cmd,
next_bits, x_next_cmd,
search_bits, x_search_cmd };*/

static void fetch_content (Textsw textsw, WView *view)
{
    char *p;
    int n;

    if (view->growing_buffer) {
        p = xmalloc (2048, "view growing");
        if (view->stdfile != (FILE *)-1)
            n = fread (p, 1, 2048, view->stdfile);
        else
            n = mc_read (view->file, p, 2048);
        textsw_insert (textsw, p, n);
        free (p);
    } else {
        textsw_insert (textsw, view->data, view->bytes_read);
    }
}

static void textsw_notify (Textsw textsw, Attr_attribute *attrs)
{
    WView *view = (WView *) xv_get (textsw, XV_KEY_DATA, KEY_DATA_VIEW);
    Rect *from, *to;
    
    for (; *attrs; attrs = attr_next (attrs)) {
        switch (*attrs) {
            case TEXTSW_ACTION_SCROLLED:
                from = (Rect *) attrs [1];
                to = (Rect *) attrs [2];
                if (view->growing_buffer) {
                    if (view->stdfile != (FILE *) -1) {
                        xv_get (textsw, TEXTSW_FIRST_LINE);
                    } else {
                        xv_get (textsw, TEXTSW_FIRST_LINE);
                    }
                }
                break;
            default:
            	break;
        }
    }
}

static void create_frame (WView *view)
{
    Panel_button_item button;
    /*    int i;*/

    viewcms = (Cms) xv_create (XV_NULL, CMS,
        CMS_CONTROL_CMS, TRUE,
        CMS_SIZE, CMS_CONTROL_COLORS + 5,
        CMS_NAMED_COLORS, "black", "red4", "green4", "black", NULL,
        NULL);
        
    viewframe = xv_create (mcframe, FRAME, 
        FRAME_LABEL, "View",
        WIN_CMS, viewcms,
        FRAME_INHERIT_COLORS, TRUE,
        FRAME_SHOW_FOOTER, TRUE,
        XV_KEY_DATA, KEY_DATA_VIEW, view,
        NULL);
    
    viewpanel = xv_create (viewframe, PANEL,
    	XV_KEY_DATA, KEY_DATA_VIEW, view,
        NULL);
/*        
    xv_create (viewpanel, PANEL_BUTTON,
        PANEL_LABEL_STRING, "Help",
        PANEL_ITEM_MENU, xv_create (XV_NULL, MENU,
            MENU_ITEM,
                MENU_STRING, "Help on Help",
                MENU_NOTIFY_PROC, x_view_on_view,
                NULL,
            MENU_ITEM,
                MENU_STRING, "Index",
                MENU_NOTIFY_PROC, x_index_cmd,
                NULL,
            MENU_ITEM,
                MENU_STRING, "Back",
                MENU_NOTIFY_PROC, x_back_cmd,
                NULL,
            MENU_ITEM,
                MENU_STRING, "Previous",
                MENU_NOTIFY_PROC, x_previous_cmd,
                NULL,
            MENU_ITEM,
                MENU_STRING, "Next",
                MENU_NOTIFY_PROC, x_next_cmd,
                NULL,
            MENU_ITEM,
                MENU_STRING, "Search",
                MENU_NOTIFY_PROC, x_search_cmd,
                NULL,
            NULL),
        NULL);
    button = (Panel_button_item) xv_create (viewpanel, PANEL_BUTTON,
        PANEL_LABEL_STRING, "Quit",
        PANEL_NOTIFY_PROC, x_quit_cmd,
        NULL);
    for (i = 0; i < sizeof (buttons) / sizeof (buttons [0]); i++) {
        button = (Panel_button_item) xv_create (viewpanel, PANEL_BUTTON,
            PANEL_LABEL_IMAGE, xv_create (XV_NULL, SERVER_IMAGE,
                SERVER_IMAGE_DEPTH, 1,
                XV_WIDTH, 48,
                XV_HEIGHT, 48,
                SERVER_IMAGE_BITS, buttons [i].bits,
                NULL),
            PANEL_NOTIFY_PROC, buttons [i].callback,
            NULL);
        if (!i)
            xv_set (button,
        	PANEL_NEXT_ROW, -1,
		NULL);
    }
*/
    button = (Panel_button_item) xv_create (viewpanel, PANEL_BUTTON,
        PANEL_LABEL_STRING, "Quit",
	XV_KEY_DATA, KEY_DATA_VIEW, view,
	PANEL_CLIENT_DATA, viewframe,
        PANEL_NOTIFY_PROC, x_quit_cmd,
        NULL);

    window_fit (viewpanel);
    
    xv_set (viewpanel,
        XV_WIDTH, WIN_EXTEND_TO_EDGE,
        NULL);
        
    xv_set (viewframe,
    	XV_KEY_DATA, KEY_DATA_PANEL, viewpanel,
    	NULL);
        
    viewcanvas = xv_create (viewframe, TEXTSW,
        WIN_BELOW, viewpanel,
        XV_KEY_DATA, KEY_DATA_VIEW, view,
        TEXTSW_BROWSING, TRUE,
        TEXTSW_DISABLE_CD, TRUE,
        TEXTSW_DISABLE_LOAD, TRUE,
        TEXTSW_NOTIFY_PROC, textsw_notify,
        TEXTSW_NOTIFY_LEVEL, TEXTSW_NOTIFY_ALL,
        TEXTSW_IGNORE_LIMIT, TEXTSW_INFINITY,
        TEXTSW_INSERT_MAKES_VISIBLE, TEXTSW_IF_AUTO_SCROLL,
        TEXTSW_LOWER_CONTEXT, -1,
        TEXTSW_UPPER_CONTEXT, -1,
        NULL);
        
    xv_create (viewcanvas, SCROLLBAR,
        SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
        SCROLLBAR_SPLITTABLE, TRUE,
        NULL);

    fetch_content (viewcanvas, view);
/*    xv_create (viewcanvas, SCROLLBAR,
        SCROLLBAR_DIRECTION, SCROLLBAR_HORIZONTAL,
        SCROLLBAR_SPLITTABLE, TRUE,
        NULL);*/
        
    window_fit (viewframe);
    
    xv_set (viewframe,
    	XV_KEY_DATA, KEY_DATA_CANVAS, viewcanvas,
    	NULL);
        
    xv_set (viewframe,
        XV_SHOW, TRUE,
        NULL);
        
}

void x_view (WView *view)
{
    create_frame (view);
}

static void x_quit_cmd (Panel_button_item button)
{
    view_destroy ((WView *)xv_get(button, XV_KEY_DATA, KEY_DATA_VIEW));
    xv_destroy_safe ((Frame)xv_get(button, PANEL_CLIENT_DATA));
    xv_dispatch_a_bit();
}
