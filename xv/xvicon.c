/* XView Onroot Icon from Pixmap creation.
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

#ifdef HAVE_XPM_SHAPE

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/dragdrop.h>
#include <xview/font.h>
#include <xview/defaults.h>
#include "util.h"
#include "mad.h"
#include "ext.h" /* regex_command */

#include "xvmain.h"
#include "xvicon.h"
#include "xvscreen.h"

/* XPM */
static char *file_xpm[] = {
/* width height ncolors chars_per_pixel */
"32 32 5 1",
/* colors */
"` c black",
"a c gray",
"c c black",
"d s none c none",
"e c white",
/* pixels */
"dddd``````````````````dddddddddd",
"dddd`eeeeeeeeeeeeeeee``ddddddddd",
"dddd`eeeeeeeeeeeeeeee`a`dddddddd",
"dddd`eeeeeeeeeeeeeeee`aa`ddddddd",
"dddd`eeeeeeeeeeeeeeee`aaa`dddddd",
"dddd`eeeeeeeeeeeeeeee`aaaa`ddddd",
"dddd`eeeeeeeeeeeeeeee```````dddd",
"dddd`eeeeeeeeeeeeeeeeeaaaaa`cddd",
"dddd`eeeeeeeeeeeeeeeeeeaaaa`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd`eeeeeeeeeeeeeeeeeeeeee`cddd",
"dddd````````````````````````cddd",
"dddddccccccccccccccccccccccccddd"
};

extern Display *dpy;
static Screen *screen;
#define root XRootWindowOfScreen(screen)
static Colormap colormap;
extern Frame mcframe;
extern Dlg_head *midnight_dlg;

static struct {
    Frame frame;
    int btn_down_x, btn_down_y;
    struct timeval prev_time;
} click_info = { XV_NULL, 0, 0, {0, 0}};
static int xv_icon_grab = 0, xv_icon_drag = 0;
static int wait_for_btnup = 0;
static GC xor_gc;
static int last_x, last_y, offset_x, offset_y;
static xv_icondep_frame = XV_NULL;
static char *xv_icondep_free = NULL;

static void draw_borderrect (Window win, Xv_window xv_win, int x, int y)
{
    int width, height;
    
    width = xv_get (xv_win, XV_WIDTH);
    height = xv_get (xv_win, XV_HEIGHT);
    
    XDrawRectangle (dpy, win, xor_gc, x, y, width, height);
}

struct xviconruncommand {
    char *filename;
    char *action;
    char *drops;
};

void xvIconRunCommand (struct xviconruncommand *xirc)
{
    char *droplist [2];
    droplist [0] = xirc->drops;
    droplist [1] = NULL;

    regex_command (x_basename (xirc->filename), xirc->action, droplist, NULL);
    free (xirc->filename);
    free (xirc->action);
    if (xirc->drops != NULL)
    	free (xirc->drops);
    free (xirc);
}

void XvIconRunCommand (Frame frame, char *action, char *drops)
{
    XpmIcon *icon = (XpmIcon *) xv_get (frame, WIN_CLIENT_DATA);
    struct xviconruncommand *xirc = (struct xviconruncommand *)
        xmalloc (sizeof (struct xviconruncommand), "XvIconRunCommand");
    
    xirc->filename = strdup (icon->filename);
    xirc->action = strdup (action);
    if (drops != NULL)
        xirc->drops = strdup (drops);
    else
    	xirc->drops = NULL;
    
    xv_post_proc (midnight_dlg, (void (*)(void *))xvIconRunCommand, 
        (void *) xirc);
}

void XvIconDepMenu (Menu menu, Menu_item item)
{
    char *p = (char *) xv_get (item, MENU_STRING);
    
    if (!strcmp (p, "Close")) {
	DeleteXpmIcon ((XpmIcon *) xv_get (xv_icondep_frame, WIN_CLIENT_DATA));
	xv_destroy_safe (xv_icondep_frame);
	XFlush (dpy);
    } else {
        XvIconRunCommand (xv_icondep_frame, p, NULL);
    }
    if (xv_icondep_free)
        free (xv_icondep_free);
}

void XpmIconEvent (Xv_window frame, Event *event)
{
    if (xv_icon_drag) {
        if (event_action (event) == LOC_DRAG) {
            draw_borderrect (root, frame, last_x, last_y);
            last_x = event_x (event) - offset_x;
            last_y = event_y (event) - offset_y;
            draw_borderrect (root, frame, last_x, last_y);
            return;
        } else if (event_is_button (event)) {
            if (event_is_down (event))
                return;
        } else if (!event_is_iso (event))
            return;
        draw_borderrect (root, frame, last_x, last_y);
        last_x = event_x (event) - offset_x;
        last_y = event_y (event) - offset_y;
        xv_set (frame,
            XV_X, last_x,
            XV_Y, last_y,
            NULL);
        xv_icon_drag = 0;
        xv_icon_grab = 0;
        XFreeGC (dpy, xor_gc);
        return;
    } else if (xv_icon_grab) {
        if (wait_for_btnup) {
            if (event_action (event) == LOC_DRAG) {
                static int threshold;
                static int dist_x, dist_y;
                
                if (threshold == 0)
                    threshold = defaults_get_integer (
                        "mxc.clickmovethreshold",
                        "Mxc.ClickMoveThreshold", 10);
                        
                dist_x = event_x (event) - click_info.btn_down_x;
                if (dist_x < 0)
                    dist_x = - dist_x;
                dist_y = event_y (event) - click_info.btn_down_y;
                if (dist_y < 0)
                    dist_y = - dist_y;
                if (!xv_icon_drag && (dist_x > threshold || dist_y > threshold ||
                    event_ctrl_is_down (event) || event_shift_is_down (event))) {
                        xor_gc = XCreateGC (dpy, xv_get (frame, XV_XID), 0, 0);
                        XSetForeground (dpy, xor_gc, xv_get (frame, WIN_FOREGROUND_COLOR));
                        XSetFunction (dpy, xor_gc, GXxor);
                        XSetLineAttributes (dpy, xor_gc, 2, LineSolid, CapNotLast, JoinRound);
                        last_x = xv_get (frame, XV_X);
                        last_y = xv_get (frame, XV_Y);
                        offset_x = event_x (event) - last_x;
                        offset_y = event_y (event) - last_y;
                        draw_borderrect (root, frame, last_x, last_y);
			xv_icon_drag = 1;
                }
                return;        
            }
            if (event_is_button (event) && event_is_down (event)) {
                return;
            }
            if (event_action (event) == ACTION_SELECT) {
                if (is_dbl_click (&click_info.prev_time, &event_time (event))) {
                    click_info.prev_time.tv_sec = 0;
                    click_info.prev_time.tv_usec = 0;
		    XvIconRunCommand (frame, "Open", NULL);
                } else {
                    click_info.prev_time = event_time (event);
                }
                xv_icon_grab = 0;
                return;
            }
            if (event_action (event) == ACTION_ADJUST) {
                xv_icon_grab = 0;
                return;
            }
            if (!event_is_button (event) && !event_is_iso (event))
                return;
            xv_icon_grab = 0;
            return;
        }
    }
    switch (event_action (event)) {
        case ACTION_DRAG_PREVIEW:
	    return;
        case ACTION_DRAG_COPY:
        case ACTION_DRAG_MOVE:
        case ACTION_DRAG_LOAD:
            {
		Xv_drop_site ds;
		Selection_requestor sel = (Selection_requestor) 
		    xv_get (frame, XV_KEY_DATA, KEY_DATA_SELREQ);
		
		if ((ds = dnd_decode_drop (sel, event)) != XV_ERROR) {
		    int length, format;
		    char *p, *q;
		        
		    xv_set (sel, SEL_TYPE, XA_STRING, NULL);
		    q = (char *) xv_get (sel, SEL_DATA, &length, &format);
		    if (length != SEL_ERROR) {
		    	XvIconRunCommand (frame, "Drop", q);
		    	free (q);
		    }
		    xv_set (sel, SEL_TYPE_NAME, "_SUN_SELECTION_END", 0);
		    xv_get (sel, SEL_DATA, &length, &format);
		    dnd_done (sel);
		}
            }
            return;
        case ACTION_HELP:
            if (event_is_down (event)) {
            }
            return;
        case ACTION_MENU:
            if (event_is_down (event)) {
                static Menu menu = XV_NULL;
                char *p, *q, c;
                char *filename;
                
                if (menu != XV_NULL)
                    xv_destroy (menu);

		filename = ((XpmIcon *) xv_get (frame, WIN_CLIENT_DATA))->filename;
                menu = (Menu) xv_create (XV_NULL, MENU,
                    MENU_NOTIFY_PROC, XvIconDepMenu,
                    MENU_TITLE_ITEM, x_basename (filename),
                    MENU_STRINGS, "Close", "Open", "View", "Edit", NULL,
                    NULL);
                p = regex_command (x_basename (filename), NULL, NULL, NULL);
                if (p != NULL) {
                    for (;;) {
                        while (*p == ' ' || *p == '\t')
                            p++;
                        if (!*p)
                            break;
                        q = p;
                        while (*q && *q != ' ' && *q != '\t')
                            q++;
                        c = *q;
                        *q = 0;
                        xv_set (menu,
                            MENU_ITEM,
                                MENU_STRING, p,
                                NULL,
                            NULL);
                        if (!c)
                            break;
                        p = q + 1;
                    }
                }
                xv_icondep_frame = frame;
                xv_icondep_free = p;
                menu_show (menu, frame, event, NULL);
            }
            return;
        case ACTION_SELECT:
        case ACTION_ADJUST:
            if (event_is_down (event)) {
   		if (click_info.frame != frame) {
                    click_info.frame = frame;
                    click_info.prev_time.tv_sec = 0;
                    click_info.prev_time.tv_usec = 0;
                }
                click_info.btn_down_x = event_x (event);
                click_info.btn_down_y = event_y (event);
                wait_for_btnup = 1;
                xv_icon_grab = 1;
            }
    }
}

XpmIcon *CreateXpmIcon (char *iconname, int x, int y, char *title)
{
    int ErrorStatus;
    Window win;
    Frame frame;
    XpmIcon *view = xmalloc (sizeof (*view), "CreateXpmIcon");
    Rect rect;
    Font font;
    Font_string_dims fontdims;
    XFontStruct *xfs;
    XImage *ximage, *shapeimage;
    GC gc;
    static char *fontname = NULL;
    int width, height;
    int titlelen = strlen (title);

    screen = ScreenOfDisplay (dpy, 
        (int) xv_get ((Xv_Screen) xv_get (mcframe, XV_SCREEN), SCREEN_NUMBER));
        
    colormap = XDefaultColormapOfScreen(screen);
    memset (view, 0, sizeof (*view));
    
    frame = xv_create (mcframe, FRAME,
        WIN_MAP, FALSE,
        XV_X, x,
        XV_Y, y,
        XV_WIDTH, 32,
        XV_HEIGHT, 32,
        WIN_CONSUME_EVENTS, WIN_MOUSE_BUTTONS, WIN_UP_EVENTS, LOC_DRAG, NULL,
        WIN_EVENT_PROC, XpmIconEvent,
        WIN_CLIENT_DATA, view,
        NULL);
        
    win = (Window) xv_get (frame, XV_XID);
        
    view->attributes.valuemask |= XpmReturnInfos;
    view->attributes.valuemask |= XpmReturnPixels | XpmCloseness;
    view->attributes.closeness = 40000;

    ErrorStatus = XpmReadFileToImage(dpy, iconname,
				      &ximage, &shapeimage,
				      &view->attributes);
    if (ErrorStatus != XpmSuccess)
	ErrorStatus = XpmCreateImageFromData(dpy, file_xpm,
					      &ximage, &shapeimage,
					      &view->attributes);
					      
    if (ErrorStatus != XpmSuccess) {
        free (view);
        xv_destroy_safe (frame);
        XFlush (dpy);
        return NULL;
    }
    
    fontname = defaults_get_string ("mxc.iconfont", "Mxc.IconFont", "");
    font = XV_NULL;
    if (*fontname)
        font = xv_find (mcframe, FONT, 
            FONT_NAME, fontname,
            NULL);
    if (font == XV_NULL)
        font = xv_get (mcframe, XV_FONT);
    xv_get (font, FONT_STRING_DIMS, title, &fontdims);
    xfs = (XFontStruct *) xv_get (font, FONT_INFO);
    width = view->attributes.width;
    if (view->attributes.width < fontdims.width)
        view->attributes.width = fontdims.width;
    height = view->attributes.height;
    view->attributes.height += fontdims.height;
    
    view->pixmap = XCreatePixmap (dpy, win, view->attributes.width, 
        view->attributes.height, ximage->depth);
    gc = XCreateGC (dpy, view->pixmap, 0, NULL);
    XSetForeground (dpy, gc, BlackPixelOfScreen (screen));
    XFillRectangle (dpy, view->pixmap, gc, 0, 0, view->attributes.width,
        view->attributes.height);
    XPutImage (dpy, view->pixmap, gc, ximage, 0, 0, 
        (view->attributes.width - width) / 2, 0, width, height);
    XFreeGC (dpy, gc);
    view->mask = XCreatePixmap (dpy, win, view->attributes.width,
    	view->attributes.height, 1);
    gc = XCreateGC (dpy, view->mask, 0, NULL);
    XSetFont (dpy, gc, xv_get (font, XV_XID));
    XSetForeground (dpy, gc, 0);
    XFillRectangle (dpy, view->mask, gc, 0, 0, view->attributes.width,
        view->attributes.height);
    XSetForeground (dpy, gc, 1);
    if (shapeimage != NULL)
        XPutImage (dpy, view->mask, gc, shapeimage, 0, 0, 
            (view->attributes.width - width) / 2, 0, width, height);
    else
    	XFillRectangle (dpy, view->mask, gc, 
            (view->attributes.width - width) / 2, 0, width, height);
    XDrawString (dpy, view->mask, gc, 
        (view->attributes.width - fontdims.width) / 2, height + xfs->ascent,
        title, titlelen);
    XFreeGC (dpy, gc);
    XDestroyImage (ximage);
    if (shapeimage != NULL)
        XDestroyImage (shapeimage);
    
    xv_set (frame,
        XV_X, x - view->attributes.width / 2,
        XV_Y, y - view->attributes.height / 2,
        XV_WIDTH, view->attributes.width, 
        XV_HEIGHT, view->attributes.height,
        NULL);

    XSetWindowBackgroundPixmap(dpy, win, view->pixmap);

    if (view->mask)
	XShapeCombineMask(dpy, win, ShapeBounding, 0, 0,
			  view->mask, ShapeSet);

    XClearWindow(dpy, win);
    
    rect.r_left = 0;
    rect.r_top = 0;
    rect.r_width = view->attributes.width;
    rect.r_height = view->attributes.height;
    
    xv_create (frame, DROP_SITE_ITEM,
        DROP_SITE_EVENT_MASK, DND_ENTERLEAVE,
        DROP_SITE_REGION, &rect,
        NULL);
    xv_set (frame, 
        XV_KEY_DATA, KEY_DATA_SELREQ,
            xv_create (frame, SELECTION_REQUESTOR, NULL),
        NULL);

    xv_set (frame,
        WIN_MAP, TRUE,
        NULL);
        
    view->frame = frame;
    
    return view;
}

void DeleteXpmIcon (XpmIcon *view)
{
    if (view->pixmap) {
	XFreePixmap(dpy, view->pixmap);
	if (view->mask)
	    XFreePixmap(dpy, view->mask);

	XFreeColors(dpy, colormap,
		    view->attributes.pixels, view->attributes.npixels, 0);

	XpmFreeAttributes(&view->attributes);
    }
    if (view->filename)
        free (view->filename);
    free (view);
}

#endif
