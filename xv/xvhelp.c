/* XView help viewer routines.
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
#include <X11/cursorfont.h>
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/svrimage.h>
#include <xview/canvas.h>
#include <xview/scrollbar.h>
#include <xview/cms.h>
#include <xview/xv_xrect.h>
#include "help.h"
#include "../src/mad.h"
#include "../src/util.h"
#include "xvmain.h"

#include "help.icons"

#define TOPMARGIN 10
#define LEFTMARGIN 10
#define RIGHTMARGIN 10
#define BOTTOMMARGIN 10

extern Frame mcframe;
extern Server_image mcimage; /* xvinfo.c */
Frame helpframe = XV_NULL;
Panel helppanel;
Canvas helpcanvas;
Cms helpcms;
extern Display *dpy;
typedef struct {
    Xv_Font font;
    GC gc;
    int height;
} fontstruct;

extern char *currentpoint, *startpoint;
static void repaint (void);
static void clear_link_areas (void);

static void x_index_cmd (void)
{
    help_index_cmd ();
    repaint ();
}

static void x_back_cmd (void)
{
}

static void x_previous_cmd (void)
{
}

static void x_next_cmd (void)
{
}

static void x_search_cmd (void)
{
}

static void x_help_on_help (void)
{
    help_help_cmd ();
    repaint ();
}

static void x_quit_cmd (void)
{
    xv_destroy_safe (helpframe);
    clear_link_areas ();
    helpframe = XV_NULL;
    interactive_display_finish ();
    xv_dispatch_a_bit ();
}

fontstruct getfont (char *name, int fg)
{
    fontstruct f;
    XGCValues gcvalues;
    
    f.font = (Xv_Font) xv_find (helpcanvas, FONT, FONT_NAME, name, NULL);
    if (f.font == XV_NULL)
        f.font = (Xv_Font) xv_get (helpframe, XV_FONT);
    gcvalues.font = (Font) xv_get (f.font, XV_XID);
    gcvalues.foreground = (unsigned long) xv_get (helpcms, CMS_PIXEL, fg);
    gcvalues.background = (unsigned long) xv_get (helpcms, CMS_BACKGROUND_PIXEL);    
    gcvalues.graphics_exposures = FALSE;
    f.gc = XCreateGC (dpy, RootWindow (dpy, DefaultScreen (dpy)), 
        GCForeground|GCBackground|GCFont|GCGraphicsExposures, &gcvalues);
    f.height = xv_get (f.font, FONT_DEFAULT_CHAR_HEIGHT);
    return f;
}

static char buffer [200];
int x, h, y;
int bufferindex;
fontstruct fonormal, fobold, folink, fotitle;
#define NORMAL &fonormal
#define BOLD &fobold
#define LINK &folink
#define TITLE &fotitle
fontstruct *curfont, *linefont, *wordfont;
int startup;
int paintwidth;
Window xId;
char *linebegin, *wordbegin;
int tabsize = 30;
struct area;
typedef struct area *AREA;
struct area {
    int wx;  /* West x, Northwest y, Southwest y etc. */
    int nwy;
    int swy;
    int ex;
    int ney;
    int sey;
    char *jump;
    AREA next;
};
AREA firstarea = NULL, lastarea = NULL;
int do_link_areas = 1;
static int painting = 1, linepainting = 1;

static void clear_link_areas (void)
{
    AREA p;
    
    while (firstarea != NULL) {
        p = firstarea;
        firstarea = firstarea->next;
        free (p);
    }
    lastarea = NULL;
}

static void start_link_area (char *link, int test)
{
    AREA p;
    
    if (test || !do_link_areas)
        return;
    
    p = (AREA) xmalloc (sizeof (struct area), "Help link area");
    if (firstarea != NULL)
        lastarea->next = p;
    else
    	firstarea = p;
    lastarea = p;
    p->wx = x;
    p->nwy = y - h;
    p->swy = y;
    p->ex = x;
    p->ney = y - h;
    p->sey = y;
    p->jump = link;
    p->next = NULL;
}

static void end_link_area (int test)
{
    if (test || !do_link_areas || !lastarea)
        return;
    
    lastarea->ex = x;
    lastarea->ney = y - h;
    lastarea->sey = y;
}

static AREA is_in_area (int x, int y)
{
    AREA p;
    
    for (p = firstarea; p != (AREA) NULL; p = p->next) {
        if (y < p->nwy || y >= p->sey)
            continue;
        if (y < p->swy) {
            if (x < p->wx)
                continue;
            if (p->nwy == p->ney && x >= p->ex)
                continue;
        } else if (y >= p->ney) {
            if (x >= p->ex)
                continue;
        }
        return p;
    }
    return NULL;
}

static void hparse (char *paint_start, int test);

static void setfont (fontstruct *f)
{
    curfont = f;
}

static void newline (void)
{
    x = LEFTMARGIN;
    y += h;
    curfont = linefont;
    hparse (linebegin, 0);
    h = 0;
    x = LEFTMARGIN;
    linepainting = painting;
    linebegin = wordbegin;
    linefont = curfont;
}

static int flush (char *p, int test)
{
    Font_string_dims dims;

    buffer [bufferindex] = 0;
    xv_get (curfont->font, FONT_STRING_DIMS, buffer, &dims);
    if (x + dims.width > paintwidth - RIGHTMARGIN && x) {
        if (test)
            newline ();
        else
            return 1;
    }
    if (!test) {
        XDrawString (dpy, xId, curfont->gc, x, y, buffer, strlen (buffer));
    }
    x += dims.width;
    if (dims.height > h)
        h = dims.height;
    wordbegin = p + 1;
    bufferindex = 0;
    return 0;
} 

static void hparse (char *paint_start, int test)
{
    char *p;
    int  c;

    if (test) {
        painting = 1;
        linepainting = 1;
    } else
    	painting = linepainting;
    	
    if (test)
        setfont (NORMAL);
    h = 0;
    x = LEFTMARGIN;
    bufferindex = 0;
    if (test) {
        wordbegin = paint_start;
        linebegin = paint_start;
        linefont = curfont;
    }
    
    for (p = paint_start; *p != CHAR_NODE_END; p++){
	c = *p;
	switch (c){
	case CHAR_LINK_START:
	    if (flush (p, test))
	        return;
	    setfont (LINK);
	    start_link_area (p, test);
	    break;
	case CHAR_LINK_POINTER:
	    if (flush (p, test))
	        return;
	    setfont (NORMAL);
	    painting = 0;
	    end_link_area (test);
	    break;
	case CHAR_LINK_END:
	    painting = 1;
	    break;
	case CHAR_ALTERNATE:
	    break;
	case CHAR_NORMAL:
	    break;
	case CHAR_VERSION:
	    strcpy (buffer + bufferindex, VERSION);
	    bufferindex += strlen (VERSION);
	    break;
	case CHAR_BOLD_ON:
	    if (flush (p, test))
	        return;
	    setfont (BOLD);
	    break;
	case CHAR_BOLD_OFF:
	    if (flush (p, test))
	        return;
	    setfont (NORMAL);
	    break;
	case CHAR_MCLOGO:
	    {
	        int mcimage_width = (int) xv_get (mcimage, XV_WIDTH);
	        int mcimage_height = (int) xv_get (mcimage, XV_HEIGHT);
	    
	        if (flush (p, test))
	            return;
	        if (!test) {
	            XCopyArea (dpy, 
	                (Drawable) xv_get (mcimage, SERVER_IMAGE_PIXMAP),
	                xId, curfont->gc, 0,0, mcimage_width, mcimage_height,
	                x, y - mcimage_height);
	        }
	        x += mcimage_width;
	        if (h < mcimage_height)
	            h = mcimage_height;
	        break;
	    }
	case CHAR_TEXTONLY_START:
	    while (*p && *p != CHAR_NODE_END && *p != CHAR_TEXTONLY_END)
	        p++;
	    if (*p != CHAR_TEXTONLY_END)
	        p--;
	    break;
	case CHAR_XONLY_START:
	case CHAR_XONLY_END:
	    break;
	case '\n':
	    if (p [1] == '\n') {
	        if (flush (p, test))
	            return;
	        if (x > LEFTMARGIN) {
	            if (!test)
	                return;
	            else
	                newline ();
	        }
	        break;
	    }
	case ' ':
	    buffer [bufferindex++] = ' ';
	    if (flush (p, test))
	        return;
	    break;
	case '\t':
	    if (flush (p, test))
	        return;
	    x = (x + tabsize - LEFTMARGIN) / tabsize * tabsize + LEFTMARGIN;
	    if (x > paintwidth - RIGHTMARGIN) {
	        if (!test)
	            return;
	        else
	            newline ();
	        x = tabsize + LEFTMARGIN;
	    }
	    break;
	default:
	    if (!painting)
		continue;
	    buffer [bufferindex++] = c;
	    break;
	}
    }
    if (flush (p, test))
       return;
    if (x > LEFTMARGIN) {
        if (!test)
            return;
        else
            newline ();
    }
}

static void canvas_repaint (Canvas canvas, Xv_Window paint)
{
    if (startup)
        return;
    xId = (Window) xv_get (paint, XV_XID);
    y = TOPMARGIN;
    paintwidth = (int) xv_get (xv_get (paint, CANVAS_PAINT_VIEW_WINDOW), XV_WIDTH);
    XClearWindow (dpy, xId);
    hparse (startpoint, 1);
    XFlush (dpy);
}

static void canvas_resize (Canvas canvas, int width, int height)
{
    if (startup)
        return;
    if (width != paintwidth)
        repaint ();
}

static void repaint (void) 
{
    Xv_Window paint, view;
    Scrollbar scroll;

    clear_link_areas ();
    do_link_areas = 1;
    OPENWIN_EACH_VIEW (helpcanvas, view)
        paint = (Xv_Window) xv_get (view, CANVAS_VIEW_PAINT_WINDOW);
        canvas_repaint (helpcanvas, paint);
        if (do_link_areas) {
            do_link_areas = 0;
            xv_set (helpcanvas,
                CANVAS_MIN_PAINT_HEIGHT, y + BOTTOMMARGIN,
                NULL);
        }
        scroll = (Scrollbar) xv_get (helpcanvas, 
            OPENWIN_VERTICAL_SCROLLBAR, view);
            
        if (scroll != XV_NULL) {
            xv_set (scroll,
                SCROLLBAR_VIEW_START, 0,
                NULL);
            xv_set (scroll,
                SCROLLBAR_OBJECT_LENGTH, y + BOTTOMMARGIN,
                NULL);
        }
    OPENWIN_END_EACH
}

static struct {
    unsigned short *bits;
    void (*callback) (void);
} buttons [] = { 
index_bits, x_index_cmd,
back_bits, x_back_cmd,
previous_bits, x_previous_cmd,
next_bits, x_next_cmd,
search_bits, x_search_cmd };
Cursor cursors [2];

static void event_handler (Xv_Window paint, Event *event)
{
    int x, y;
    AREA p;
    static int current_cursor = 0;

    switch (event_action (event)) {
        case ACTION_SELECT:
            x = event_x (event);
            y = event_y (event);
            p = is_in_area (x, y);
            if (p != NULL && p->jump != NULL) {
                currentpoint = startpoint = help_follow_link (startpoint, p->jump);
                repaint ();
            }
            break;
        case LOC_MOVE:
            x = event_x (event);
            y = event_y (event);
            p = is_in_area (x, y);
            if (p != NULL) {
                if (!current_cursor) {
                    XDefineCursor (dpy, xv_get (paint, XV_XID), cursors [1]);
                    current_cursor = 1;
                }
            } else {
                if (current_cursor) {
                    XDefineCursor (dpy, xv_get (paint, XV_XID), cursors [0]);
                    current_cursor = 0;
                }
            }
            break;
    }
}

static void create_frame (void)
{
    Panel_button_item button;
    int i;

    helpcms = (Cms) xv_create (XV_NULL, CMS,
        CMS_CONTROL_CMS, TRUE,
        CMS_SIZE, CMS_CONTROL_COLORS + 5,
        CMS_NAMED_COLORS, "black", "black", "green4", "red4", "black", NULL,
        NULL);
        
    helpframe = xv_create (mcframe, FRAME, 
        FRAME_LABEL, "Midnight X Commander Help",
        WIN_CMS, helpcms,
        FRAME_INHERIT_COLORS, TRUE,
        NULL);
    
    helppanel = xv_create (helpframe, PANEL,
        NULL);
        
    xv_create (helppanel, PANEL_BUTTON,
        PANEL_LABEL_STRING, "Help",
        PANEL_ITEM_MENU, xv_create (XV_NULL, MENU,
            MENU_ITEM,
                MENU_STRING, "Help on Help",
                MENU_NOTIFY_PROC, x_help_on_help,
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
    button = (Panel_button_item) xv_create (helppanel, PANEL_BUTTON,
        PANEL_LABEL_STRING, "Quit",
        PANEL_NOTIFY_PROC, x_quit_cmd,
        NULL);
    for (i = 0; i < sizeof (buttons) / sizeof (buttons [0]); i++) {
        button = (Panel_button_item) xv_create (helppanel, PANEL_BUTTON,
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

    window_fit (helppanel);
    
    xv_set (helpframe,
        XV_WIDTH, xv_get (helppanel, XV_WIDTH),
        NULL);
        
    xv_set (helppanel,
        XV_WIDTH, WIN_EXTEND_TO_EDGE,
        NULL);
        
    startup = 1;
        
    helpcanvas = xv_create (helpframe, CANVAS,
        WIN_BELOW, helppanel,
        CANVAS_REPAINT_PROC, canvas_repaint,
        CANVAS_RESIZE_PROC, canvas_resize,
        CANVAS_X_PAINT_WINDOW, FALSE,
        CANVAS_PAINTWINDOW_ATTRS,
            WIN_CONSUME_EVENTS,
            WIN_MOUSE_BUTTONS, LOC_MOVE, NULL,
            WIN_EVENT_PROC, event_handler,
            NULL,
        NULL);
        
    cursors [0] = (Cursor) xv_get (xv_get (helpcanvas, WIN_CURSOR), XV_XID);
    cursors [1] = XCreateFontCursor (dpy, XC_hand2);
        
    xv_create (helpcanvas, SCROLLBAR,
        SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
        SCROLLBAR_SPLITTABLE, TRUE,
        NULL);
        
    fonormal = getfont ("lucidasans-12", CMS_CONTROL_COLORS);
    fobold = getfont ("lucidasans-bold-12", CMS_CONTROL_COLORS + 1);
    folink = getfont ("lucidasans-12", CMS_CONTROL_COLORS + 2);
    fotitle = getfont ("lucidasans-bold-18", CMS_CONTROL_COLORS + 3);
    
    window_fit (helpframe);
    
    xv_set (helpframe,
        XV_SHOW, TRUE,
        NULL);
        
    startup = 0;
        
    repaint ();
}

void x_interactive_display (void)
{
    if (helpframe == XV_NULL)
        create_frame ();
}
