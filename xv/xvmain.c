/* XView initialization.
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
#include <stdarg.h>

#include <X11/Xlib.h>
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/screen.h>
#include <xview/xv_xrect.h>
#include <xview/cms.h>
#include <xview/scrollbar.h>
#include <xview/svrimage.h>
#include <xview/cursor.h>
#include <xview/defaults.h>
#include <xview/dragdrop.h>
#include <X11/xpm.h>

#include <stdio.h>

#include "fs.h"
#include "dir.h"
#include "mad.h"
#include "dlg.h"
#include "panel.h"
#include "xvmain.h"
#include "util.h"


Frame mcframe;
Frame menubarframe;
Cursor splitcursor, dropcursor;
Display *dpy;
int fdXConnection;
int displayWidth;
int displayHeight;
int defaultDepth;
int screen_no;
Server_image iconRegular [2];
Server_image iconDirectory [2];
Cms mccms;
Cms cmsicon;
static Xv_singlecolor icon_colors[] = {
	{255, 255, 255},
	{0, 0, 0},
	{170, 68, 0},
	{170, 170, 204},
	{204, 102, 0},
	{204, 204, 204},
	{238, 0, 0},
	{238, 204, 170},
	{238, 238, 238},
	{170, 0, 0},
	{0, 136, 204},
	{238, 153, 153},
	{136, 68, 0},
	{136, 136, 136},
	{170, 102, 0},
	{238, 170, 0},
	{64, 64, 192}, /* the empty piece only */
	{160, 64, 64}, /* the background color only */
  };
  
#include "pictures.h"

int xv_error_handler (Xv_object object, Attr_avlist avlist);
void x_setup_info (void);
void xv_set_current_panel (WPanel *);

extern widget_data containers []; /* defined in main.c */
extern int containers_no;
extern int is_right;
extern Dlg_head *midnight_dlg;

void get_panel_color (Xv_Singlecolor *colors, int i)
{
    static char *defcolors [] = {
        "black", "gray75", /* normal */
        "blue4", "gray75", /* marked */
        "black", "gray50", /* selected */
        "blue4", "gray50", /* marked selected */
    };
    static char *names [] = {"fg", "bg", "markfg", "markbg", 
                             "selfg", "selbg", "markselfg", "markselbg" };
    char *p, *q;
    char buffer [40], buffer2 [40];
    Xv_singlecolor col;
    int red, green, blue;
    XColor xcolor, scolor;
    
    sprintf (buffer, "mxc.panelcolor.%s", names [i]);
    sprintf (buffer2, "Mxc.PanelColor.%s", names [i]);
    p = defaults_get_string (buffer, buffer2, "");
    if (p != NULL) {
        while (*p == ' ' || *p == '\t')
            p++;
    }
    
    if (p != NULL && *p) {
        if (sscanf (p, "%i %i %i", &red, &green, &blue) == 3) {
            col.red = (u_char) red;
            col.green = (u_char) green;
            col.blue = (u_char) blue;
            p = NULL;
        } else {
            for (q = p; *q && *q != ' ' && *q != '\t'; q++);
            *q = 0;
        }
    } else {
        p = defcolors [i];
    }
    
    if (p == NULL) {
      colors->red = col.red;
      colors->green = col.green;
      colors->blue = col.blue;
    }
    else {
       XLookupColor (dpy, DefaultColormap(dpy, screen_no), p,
		     &xcolor, &scolor);
       colors->red = (unsigned char)(xcolor.red >> 8);
       colors->green = (unsigned char)(xcolor.green >> 8);
       colors->blue = (unsigned char)(xcolor.blue >> 8);
    }
}

#include "Directory.xpm"
#include "Regular.xpm"

int xtoolkit_init(int *argc, char **argv)
{
    int i;
    char *mycolors[] = {
      "LawnGreen", "gold", "brown", "sienna",
      "salmon", "coral", "violet", "orchid",
      "cornsilk", "azure", "misty rose", "snow",
      "black", "red4", "green4", "yellow4",
      "blue4", "magenta4", "cyan4", "gray75",
      "gray50", "red", "green", "yellow",
      "blue", "magenta", "cyan", "white", "black" };
    Xv_singlecolor my_xv_colors[29];
    XColor exact_c, screen_c;
    Pixmap pixmap, shapemask;

    xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, 
    	XV_ERROR_PROC, xv_error_handler,
        NULL);
    mcframe = (Frame) xv_create (XV_NULL, FRAME,
        FRAME_LABEL, "The Midnight X Commander",
        NULL);
    if (mcframe == XV_NULL)
    	return -1;
    dpy = (Display *) xv_get (mcframe, XV_DISPLAY);
    screen_no = (int) xv_get ((Xv_Screen) xv_get (mcframe, XV_SCREEN),
        SCREEN_NUMBER);
    displayWidth = DisplayWidth (dpy, screen_no);
    displayHeight = DisplayHeight (dpy, screen_no);
    defaultDepth = DefaultDepth (dpy, screen_no);
    fdXConnection = XConnectionNumber (dpy);
    xv_dispatch_a_bit ();

    /* set up the array of Xv_singlecolors */
    for (i = 0; i < 29; ++i) {
        XLookupColor (dpy, DefaultColormap(dpy, screen_no), mycolors[i],
                      &exact_c, &screen_c);
        my_xv_colors[i].red = (unsigned char)(exact_c.red >> 8);
        my_xv_colors[i].green = (unsigned char)(exact_c.green >> 8);
        my_xv_colors[i].blue = (unsigned char)(exact_c.blue >> 8);
    }

    defaults_load_db (APP_DEFAULTS);
    for (i = 0; i < 8; i++)
      get_panel_color (&my_xv_colors[i], i);
       
    mccms = (Cms) xv_create (XV_NULL, CMS,
        CMS_CONTROL_CMS, TRUE,
	CMS_SIZE, 29 + CMS_CONTROL_COLORS,
	CMS_NAME, "mccms",
	CMS_COLORS, my_xv_colors,
	NULL);
	
	/* this seems never to be used ?? -- BD */	
     cmsicon = (Cms) xv_create(XV_NULL, CMS,
 			CMS_SIZE, 18,
 			CMS_COLORS, icon_colors,
 			CMS_NAME, "iconcms",
 			NULL);
         
    splitcursor = (Cursor) xv_create (XV_NULL, CURSOR,
        CURSOR_XHOT, 15,
        CURSOR_YHOT, 14,
        CURSOR_IMAGE, xv_create (XV_NULL, SERVER_IMAGE,
            XV_WIDTH, 32,
	    XV_HEIGHT, 32,
	    SERVER_IMAGE_X_BITS, splitcursor_bits,
	    NULL),
	NULL);
    dropcursor = (Cursor) xv_create (XV_NULL, CURSOR,
        CURSOR_XHOT, 3,
        CURSOR_YHOT, 15,
        CURSOR_IMAGE, xv_create (XV_NULL, SERVER_IMAGE,
            XV_WIDTH, 32,
	    XV_HEIGHT, 32,
	    SERVER_IMAGE_X_BITS, dropcursor_bits,
	    NULL),
	NULL);

    XpmCreatePixmapFromData (dpy, (Drawable)xv_get(mcframe, XV_XID),
			     Regular_xpm, &pixmap, &shapemask, NULL);

    iconRegular [0] = (Server_image) xv_create (XV_NULL, SERVER_IMAGE,
        XV_WIDTH, 16,
	XV_HEIGHT, 16,
        SERVER_IMAGE_COLORMAP, "mccms",
	  SERVER_IMAGE_DEPTH, defaultDepth,
	  SERVER_IMAGE_PIXMAP, pixmap,
	NULL);

    iconRegular [1] = (Server_image) xv_create (XV_NULL, SERVER_IMAGE,
	XV_WIDTH, 16,
	XV_HEIGHT, 16,
	  SERVER_IMAGE_PIXMAP, shapemask,
	NULL);

    XpmCreatePixmapFromData (dpy, (Drawable)xv_get(mcframe, XV_XID),
			     Directory_xpm, &pixmap, &shapemask, NULL);

     iconDirectory [0] = (Server_image) xv_create (XV_NULL, SERVER_IMAGE,
 	XV_WIDTH, 16,
 	XV_HEIGHT, 16,
 	SERVER_IMAGE_COLORMAP, "mccms",
 	SERVER_IMAGE_DEPTH, defaultDepth,
	SERVER_IMAGE_PIXMAP, pixmap,
     	NULL);
    
     iconDirectory [1] = (Server_image) xv_create (XV_NULL, SERVER_IMAGE,
     	XV_WIDTH, 16,
     	XV_HEIGHT, 16,
        SERVER_IMAGE_PIXMAP, shapemask,
     	NULL);
     
    xv_set (splitcursor,
        XV_INCREMENT_REF_COUNT,
	NULL);
    xv_set (iconRegular [0],
	XV_INCREMENT_REF_COUNT,
	NULL);
    xv_set (iconRegular [1],
	XV_INCREMENT_REF_COUNT,
	NULL);
    xv_set (iconDirectory [0], 
         XV_INCREMENT_REF_COUNT, 
         NULL);
     xv_set (iconDirectory [1],
         XV_INCREMENT_REF_COUNT, 
         NULL);
    xv_dispatch_something ();

    x_setup_info ();    
    xv_dispatch_a_bit ();
    
    return 0;
}

int xtoolkit_end(void)
{
    xv_destroy_safe (splitcursor);
    xv_destroy_safe (iconRegular [0]);
    xv_destroy_safe (iconRegular [1]);
    xv_destroy_safe (iconDirectory [0]);
    xv_destroy_safe (iconDirectory [1]);
    return 0;
}

/* These are only temporary routines. Should go into dlg.c, which should be
   completely rewritten for XView */
   
#include <sys/types.h>
#include <sys/time.h>

Notify_value my_destroy_func (Frame frame, Destroy_status status)
{
    if (status == DESTROY_PROCESS_DEATH || status == DESTROY_CLEANUP) {
        Dlg_head *h = (Dlg_head *) xv_get (frame, 
            XV_KEY_DATA, KEY_DATA_DLG_HEAD);
        h->running = -1;
    }
    return notify_next_destroy_func (frame, status);
}

void my_done_func (Frame frame)
{
    Dlg_head *h = (Dlg_head *) xv_get (frame, 
        XV_KEY_DATA, KEY_DATA_DLG_HEAD);
    h->running = 0;
    h->ret_value = B_CANCEL;
}

struct timeval timeout = { 0, 0 };

/*#define PARALELIZE*/

struct xv_dispatch_proc {
    void (*callback)(void *);
    void *arg;
#ifdef PARALELIZE    
    Dlg_head *h;
#endif
    struct xv_dispatch_proc *next;
};

#ifndef PARALELIZE
struct xv_dispatch_struct {
    Dlg_head *h;
    struct xv_dispatch_struct *next;
    struct xv_dispatch_proc *proc;
};

static struct xv_dispatch_struct *dispatch_current = NULL;
#else
static struct xv_dispatch_proc *dispatch_current = NULL;
#endif

static Dlg_head *previous_dlg = NULL;
int xvrundlg_event (Dlg_head *h)
{
    fd_set readfds;
    struct xv_dispatch_proc *proc, *proc2;
    struct Dlg_head *previous = previous_dlg;
    int    v;
#ifndef PARALELIZE    
    struct xv_dispatch_struct *dispatch_new = xmalloc (
        sizeof (struct xv_dispatch_struct), "XV dispatching");
#else
    struct xv_dispatch_proc *proc4;
#endif

    previous_dlg = h;
    if (previous == midnight_dlg) {
        xv_set (menubarframe,
            FRAME_BUSY, TRUE,
            NULL);
        for (v = 0; v < containers_no; v++) {
            xv_set ((Frame) containers [v],
                FRAME_BUSY, TRUE,
                NULL);
        }
        
    } else if (previous != (Dlg_head *) NULL) {
        xv_set ((Frame) (previous->wdata),
            FRAME_BUSY, TRUE,
            NULL);
    }
    if (dispatch_current == NULL) {
        h->wdata = mcframe;
        xv_set (mcframe,
            XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
            NULL);
    } else {
    	xv_set ((Frame) (h->wdata),
    	    XV_KEY_DATA, KEY_DATA_DLG_HEAD, h,
    	    NULL);
    }
#ifndef PARALELIZE
    dispatch_new->next = dispatch_current;
    dispatch_current = dispatch_new;
    dispatch_current->h = h;
    dispatch_current->proc = NULL;
#endif 
    h->running = 1;
    if (h->wdata == mcframe) {
        xv_post_proc (h, (void (*)(void *))xv_action_icons, (void *) NULL);
    }
    
    notify_interpose_destroy_func ((Frame) (h->wdata), (Notify_func) my_destroy_func);
    XFlush (dpy);
    while (h->running >= 0) {
	FD_ZERO (&readfds);
        FD_SET (fdXConnection, &readfds);
        v = select (FD_SETSIZE, &readfds, NULL, NULL, &timeout);
	if (v > 0){
            notify_dispatch ();
        } else {
	    XFlush (dpy);
        }
#ifndef PARALELIZE        
        if (dispatch_current->proc != NULL) {
            proc = dispatch_current->proc;
            dispatch_current->proc = proc->next;
#else
	if (dispatch_current != NULL) {
	    proc = dispatch_current;
	    dispatch_current = proc->next;
#endif            
	    (*proc->callback)(proc->arg);
	    free (proc);
        }
        if (h->running == 0) {
            xv_destroy_safe (h->wdata);
        }
    }
    if (previous == midnight_dlg) {
        xv_set (menubarframe,
            FRAME_BUSY, FALSE,
            NULL);
        for (v = 0; v < containers_no; v++) {
            xv_set ((Frame) containers [v],
                FRAME_BUSY, FALSE,
                NULL);
        }
        
    } else if (previous != (Dlg_head *) NULL) {
        xv_set ((Frame) (previous->wdata),
            FRAME_BUSY, FALSE,
            NULL);
    }
    previous_dlg = previous;
#ifndef PARALELIZE    
    dispatch_new = dispatch_current;
    dispatch_current = dispatch_current->next;
    for (proc = dispatch_new->proc; proc != NULL; proc = proc2) {
        proc2 = proc->next;
        free (proc);
    }
    free (dispatch_new);
#else
    proc = dispatch_current;
    dispatch_current = NULL;
    proc4 = NULL;
    for (; proc != NULL; proc = proc2) {
        proc2 = proc->next;
        if (proc->h == h)
            free (proc);
        else {
            if (dispatch_current == NULL)
                dispatch_current = proc4 = proc;
            else {
                proc4->next = proc;
                proc4 = proc;
            }
        }
    }
    if (proc4 != NULL)
        proc4->next = NULL;
#endif    
    return 0;
}

void xv_dispatch_a_bit (void)
{
    fd_set readfds;
    struct timeval timeout = { 0, 1 };
    struct timeval zero_timeout = { 0, 0 };
    int i;
    int v;
    Xv_Server server = XV_SERVER_FROM_WINDOW (mcframe);
    
    xv_set (server, SERVER_SYNC_AND_PROCESS_EVENTS, NULL);
    
    XFlush (dpy);
#if 0
    for (i = 0; i < 300; i++) {
	int v;
	
        FD_SET (fdXConnection, &readfds);
        v = select (FD_SETSIZE, &readfds, NULL, NULL, &timeout);
	if (v == -1){
	    fprintf (stderr, "Fatal: fdXConnection lost\n");
	}
	if (v){
            notify_dispatch ();
        } else
       	    break;
    }
#endif
    for (v = 1; v;) {
	FD_ZERO (&readfds);
        FD_SET (fdXConnection, &readfds);
        v = select (FD_SETSIZE, &readfds, NULL, NULL, &zero_timeout);
	if (v == -1){
	    perror ("xv_dispatch\n");
	    exit (1);
	}
	if (v)
            notify_dispatch ();
    }
}

void xv_dispatch_something (void)
{
    fd_set readfds;
    struct timeval timeout = { 0, 1 };
    Xv_Server server = XV_SERVER_FROM_WINDOW (mcframe);
    int i;
    
    xv_set (server, SERVER_SYNC_AND_PROCESS_EVENTS, NULL);
    
    XFlush (dpy);
    for (i = 0; i < 50; i++) {
	FD_ZERO (&readfds);
        FD_SET (fdXConnection, &readfds);
        if (select (FD_SETSIZE, &readfds, NULL, NULL, &timeout)) {
            notify_dispatch ();
        } else
       	    return;
    }
}

widget_data xtoolkit_create_dialog (Dlg_head *h, int with_grid)
{
    Frame frame;
    Panel panel;
    
    frame = (Frame) xv_create (mcframe, FRAME_CMD,
        FRAME_LABEL, "Dialog",
        FRAME_DONE_PROC, my_done_func,
        NULL);
    xv_set (panel = (Panel) xv_get (frame, FRAME_CMD_PANEL),
        XV_KEY_DATA, KEY_DATA_PANEL_LASTITEM, XV_NULL,
	XV_KEY_DATA, KEY_DATA_PANEL_MAXX, 0,
	XV_KEY_DATA, KEY_DATA_PANEL_MAXY, 0,
	NULL);
    return (widget_data) frame;
}

/* Only needed when we run a dialog ourselves :) */
void xtoolkit_kill_dialog (Dlg_head *h)
{
    h->running = 0;
    xv_destroy_safe ((Frame) (h->wdata));
}

void x_set_dialog_title (Dlg_head *h, char *title)
{
    xv_set ((Frame) (h->wdata),
        FRAME_LABEL, title,
	NULL);
}

widget_data xtoolkit_get_main_dialog (Dlg_head *h)
{
    return (widget_data) mcframe;
}

int quit_cmd (void);
static void xv_container_done_proc (Frame frame)
{
    xv_post_proc (midnight_dlg, (void (*)(void *))quit_cmd, NULL);
}

static void xv_division_repaint_proc (Canvas canvas, Xv_Window paint, 
    Rectlist *repaint)
{
    int h;
    Drawable xid = (Drawable) xv_get (paint, XV_XID);
    GC gc = XCreateGC (dpy, xid, 0, 0);
    unsigned long *xc;
    XPoint xp [3];

#define DIVISION_LEFT 2
#define DIVISION_TOP 0
#define DIVISION_RIGHT 8
#define DIVISION_BOTTOM (h - 4)
    h = xv_get (paint, XV_HEIGHT);
    xc = (unsigned long *) xv_get (mccms, CMS_INDEX_TABLE);
    XSetLineAttributes (dpy, gc, 1, LineSolid, JoinRound, CapNotLast);
    xp [0].x = DIVISION_LEFT;
    xp [0].y = DIVISION_BOTTOM;
    xp [1].x = DIVISION_LEFT;
    xp [1].y = DIVISION_TOP;
    xp [2].x = DIVISION_RIGHT - 1;
    xp [2].y = DIVISION_TOP;
    XSetForeground (dpy, gc, xc [2]);
    XDrawLines (dpy, xid, gc, xp, 3, CoordModeOrigin);
    xp [0].x = DIVISION_LEFT + 1;
    xp [0].y = DIVISION_BOTTOM - 1;
    xp [1].x = DIVISION_RIGHT - 1;
    xp [1].y = DIVISION_BOTTOM - 1;
    xp [2].x = DIVISION_RIGHT - 1;
    xp [2].y = DIVISION_TOP;
    XSetForeground (dpy, gc, xc [3]);
    XDrawLines (dpy, xid, gc, xp, 3, CoordModeOrigin);
    XFreeGC (dpy, gc);
}

static void xv_division_resize_proc (Canvas canvas, int w, int h)
{
    xv_division_repaint_proc (canvas, 
        xv_get (canvas, CANVAS_NTH_PAINT_WINDOW, 0), NULL);
}

static void x_container_resize_proc (Frame frame)
{
    int w = xv_get (frame, XV_WIDTH);
    int h = xv_get (frame, XV_HEIGHT);
    int h1, h2, s;
    Panel toppanel, bottompanel;
    Canvas leftcanvas, rightcanvas;
    Canvas division;
    	    	
    toppanel = (Panel) xv_get (frame, XV_KEY_DATA, KEY_DATA_AREA_TOP);
    bottompanel = (Panel) xv_get (frame, XV_KEY_DATA, KEY_DATA_AREA_BOTTOM);
    leftcanvas = (Canvas) xv_get (frame, XV_KEY_DATA, KEY_DATA_AREA_LEFT);
    rightcanvas = (Canvas) xv_get (frame, XV_KEY_DATA, KEY_DATA_AREA_RIGHT);
    division = (Canvas) xv_get (frame, XV_KEY_DATA, KEY_DATA_DIVISION);
    	    	
    h1 = xv_get (bottompanel, XV_HEIGHT);
    h2 = xv_get (toppanel, XV_HEIGHT);
    s = xv_get (frame, XV_KEY_DATA, KEY_DATA_PANEL_SPLITX);
    	    	
    xv_set (toppanel,
    	XV_X, 0,
    	XV_Y, 0,
    	XV_WIDTH, w,
    	XV_HEIGHT, h2,
    	NULL);
    xv_set (bottompanel,
    	XV_X, 0,
    	XV_Y, h - h1,
    	XV_WIDTH, w,
    	XV_HEIGHT, h1,
    	NULL);
    if (s > 30)
        xv_set (leftcanvas,
    	    XV_X, 0,
    	    XV_Y, h2,
    	    XV_HEIGHT, h - h1 - h2,
    	    XV_WIDTH, s,
    	    NULL);
    if (w - s - 12 > 30)
        xv_set (rightcanvas,
    	    XV_X, s + 12,
    	    XV_Y, h2,
    	    XV_HEIGHT, h - h1 - h2,
    	    XV_WIDTH, w - s - 12,
    	    NULL);
    xv_set (division,
    	XV_X, s,
    	XV_Y, h2,
    	XV_WIDTH, 12,
    	XV_HEIGHT, h - h1 - h2,
    	NULL);
}

static Notify_value x_container_interpose_proc (Frame frame, Event *event, 
    Notify_arg arg, Notify_event_type type)
{
    switch (event_action (event)) {
    	case WIN_RESIZE:
    	    x_container_resize_proc (frame);
    	    return XV_OK;
    	default:
    	    return notify_next_event_func (frame, (Notify_event) event, 
    	        arg, type);
    }
}

int x_container_get_id (widget_data wcontainer)
{
    int i;
    
    for (i = 0; i < containers_no; i++)
        if (wcontainer == containers [i])
            return i;
    return 0;
}

static Notify_value x_container_item_interpos (Xv_opaque item, Event *event, 
    Notify_arg arg, Notify_event_type type)
{
    Frame frame = (Frame) xv_get (item, XV_OWNER);

    switch (event_action (event)) {
        case KBD_USE:
	case ACTION_SELECT:
	case ACTION_ADJUST:
	case ACTION_MENU:
	    is_right = (x_container_get_id ((widget_data) frame) != 0);
    	default:
    	    return notify_next_event_func (item, (Notify_event) event, 
    	        arg, type);
    }
}

extern Menu MenuBar []; /* from main.c */
#include "panel_icon.xpm"

widget_data x_create_panel_container (int which)
{
    Frame panelframe;
    Panel toppanel, bottompanel;
    Canvas leftcanvas, rightcanvas;
    Canvas division;
    int panelsplit = 0;
    Pixmap pixmap, mask;
    
    XpmCreatePixmapFromData (dpy, (Drawable)xv_get(mcframe, XV_XID),
			    panel_icon_xpm, &pixmap, &mask, NULL);
    
    panelframe = (Frame) xv_create (mcframe, FRAME,
    	XV_WIDTH, displayWidth * 2 / (3 * containers_no),
    	XV_HEIGHT, displayHeight / 2,
        XV_X, displayWidth / 6 + 
              which * (displayWidth * 2 / (3 * containers_no) + 30),
        XV_Y, displayHeight / 4,
        FRAME_LABEL, "/",
        XV_KEY_DATA, KEY_DATA_PANEL_SPLITX, panelsplit,
	FRAME_ICON, (Icon) xv_create(XV_NULL, ICON,
	    ICON_IMAGE, xv_create(XV_NULL, SERVER_IMAGE,
	        XV_WIDTH, 64,
	        XV_HEIGHT, 64,
	        SERVER_IMAGE_COLORMAP, "iconcms",
	        SERVER_IMAGE_DEPTH, defaultDepth,
	        SERVER_IMAGE_PIXMAP, pixmap,
	        NULL),
	    NULL),	
        NULL);

    rightcanvas = (Canvas) xv_create (
        panelframe, CANVAS,
        WIN_CMS, mccms,
        XV_X, 22,
        XV_Y, 10,
        XV_WIDTH, WIN_EXTEND_TO_EDGE,
        XV_HEIGHT, 30,
        NULL);
        
    leftcanvas = (Canvas) xv_create (
        panelframe, CANVAS,
        WIN_CMS, mccms,
        XV_X, 0,
        XV_Y, 10,
        XV_WIDTH, 10,
        XV_HEIGHT, 30,
        NULL);
        
    toppanel = (Panel) xv_create (panelframe, PANEL,
    	XV_X, 0,
    	XV_Y, 0,
    	XV_WIDTH, WIN_EXTEND_TO_EDGE,
        XV_HEIGHT, 10,
        NULL);

    bottompanel = (Panel) xv_create (panelframe, PANEL,
    	XV_X, 0,
    	XV_Y, 40,
    	XV_WIDTH, WIN_EXTEND_TO_EDGE,
        XV_HEIGHT, WIN_EXTEND_TO_EDGE,
        NULL);

    division = (Canvas) xv_create (panelframe, CANVAS,
    	XV_X, 10,
    	XV_Y, 10,
        XV_WIDTH, 12,
        XV_HEIGHT, 30,
        WIN_CMS, mccms,
        WIN_FOREGROUND_COLOR, 0,

        CANVAS_PAINTWINDOW_ATTRS,
            WIN_CURSOR, splitcursor,
            NULL,
        CANVAS_REPAINT_PROC, xv_division_repaint_proc,
        CANVAS_X_PAINT_WINDOW, FALSE,
        CANVAS_FIXED_IMAGE, FALSE,
        CANVAS_AUTO_SHRINK, TRUE,
        CANVAS_AUTO_EXPAND, TRUE,
        CANVAS_RESIZE_PROC, xv_division_resize_proc,
        CANVAS_VIEW_MARGIN, 0,
        NULL);
        
    if (panelsplit < 30)
        xv_set (leftcanvas,
            XV_SHOW, FALSE,
            NULL);
        
    xv_set (leftcanvas,
        XV_KEY_DATA, KEY_DATA_SELREQ,
            xv_create (leftcanvas, SELECTION_REQUESTOR,
                NULL),
        NULL);

    xv_set (leftcanvas,
        XV_KEY_DATA, KEY_DATA_HSCROLL,
            xv_create (leftcanvas, SCROLLBAR,
                SCROLLBAR_DIRECTION, SCROLLBAR_HORIZONTAL,
                SCROLLBAR_SPLITTABLE, TRUE,
                NULL),
        NULL);

    xv_set (leftcanvas,
        XV_KEY_DATA, KEY_DATA_VSCROLL,
            xv_create (leftcanvas, SCROLLBAR,
                SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
                SCROLLBAR_SPLITTABLE, TRUE,
                NULL),
        NULL);

    xv_set (rightcanvas,
        XV_KEY_DATA, KEY_DATA_SELREQ,
            xv_create (rightcanvas, SELECTION_REQUESTOR,
                NULL),
        NULL);

    xv_set (rightcanvas,
        XV_KEY_DATA, KEY_DATA_HSCROLL,
            xv_create (rightcanvas, SCROLLBAR,
                SCROLLBAR_DIRECTION, SCROLLBAR_HORIZONTAL,
                SCROLLBAR_SPLITTABLE, TRUE,
                NULL),
        NULL);
        
    xv_set (rightcanvas,
        XV_KEY_DATA, KEY_DATA_VSCROLL,
            xv_create (rightcanvas, SCROLLBAR,
                SCROLLBAR_DIRECTION, SCROLLBAR_VERTICAL,
                SCROLLBAR_SPLITTABLE, TRUE,
                NULL),
        NULL);

     xv_set (panelframe,
         XV_KEY_DATA, KEY_DATA_AREA_TOP, toppanel,
         XV_KEY_DATA, KEY_DATA_AREA_LEFT, leftcanvas,
         XV_KEY_DATA, KEY_DATA_AREA_RIGHT, rightcanvas,
         XV_KEY_DATA, KEY_DATA_AREA_BOTTOM, bottompanel,
	 XV_KEY_DATA, KEY_DATA_DIVISION, division,
         NULL);

     notify_interpose_event_func (panelframe, x_container_interpose_proc, 
         NOTIFY_SAFE);
         
     notify_interpose_event_func (toppanel, x_container_item_interpos, 
         NOTIFY_SAFE);
         
     notify_interpose_event_func (bottompanel, x_container_item_interpos, 
         NOTIFY_SAFE);
         
    return (widget_data) panelframe;
}

void x_panel_container_show (widget_data wdata)
{
    window_fit_height 
        ((Panel) xv_get ((Frame) wdata, XV_KEY_DATA, KEY_DATA_AREA_TOP));
    window_fit_height
        ((Panel) xv_get ((Frame) wdata, XV_KEY_DATA, KEY_DATA_AREA_BOTTOM));
    x_container_resize_proc ((Frame) wdata); 
    xv_set ((Frame) wdata,
        XV_SHOW, TRUE,
        NULL);
}

void xv_center_layout (Panel panel)
{
    Panel_item item, item1, item2 = XV_NULL;
    Widget *widget;
    int y, shift;
    
    for (item = (Panel_item) xv_get (panel, PANEL_FIRST_ITEM); item != XV_NULL;
        item = (Panel_item) xv_get (item, PANEL_NEXT_ITEM)) {
        if (xv_get (item, PANEL_ITEM_OWNER))
            continue;
        widget = (Widget *) xv_get (item, PANEL_CLIENT_DATA);
        if (widget->layout == XV_WLAY_CENTERROW) {
            y = xv_get (item, XV_Y);
            for (item1 = item; item1 != XV_NULL && (int) xv_get (item1, XV_Y) == y;
                item1 = (Panel_item) xv_get (item1, PANEL_NEXT_ITEM)) {
                item2 = item1;
            }
            shift = xv_get (panel, XV_WIDTH);
            shift -= xv_get (item2, XV_X) + xv_get (item2, XV_WIDTH);
            shift -= xv_get (item, XV_X);
            shift /= 2;
            for (item1 = item; item1 != XV_NULL; item1 = (Panel_item) 
                xv_get (item1, PANEL_NEXT_ITEM)) {
                xv_set (item1,
                    XV_X, xv_get (item1, XV_X) + shift,
                    NULL);
                if (item1 == item2)
                    break;
            }
            item = item2;
        } else if (widget->layout == XV_WLAY_EXTENDWIDTH) {
            xv_set (item, 
                PANEL_LIST_WIDTH, xv_get (panel, XV_WIDTH) - 2 * xv_get (item, XV_X),
                NULL);
        }
    }
}

void xvdlg_show (Dlg_head *dialog, widget_data wdata)
{
    Panel panel;
    int w, h;

    panel = (Panel) xv_get ((Frame) wdata, FRAME_CMD_PANEL);
    window_fit (panel);
    if (xv_get (panel, XV_WIDTH) < 60)
        xv_set (panel, XV_WIDTH, 60);
    xv_set ((Frame) wdata,
        XV_WIDTH, w = xv_get (panel, XV_WIDTH),
        XV_HEIGHT, h = xv_get (panel, XV_HEIGHT),
        NULL);
    xv_center_layout (panel);
    if (dialog->initfocus != NULL) {
        Panel_item item = (Panel_item) dialog->initfocus->widget->wdata;
       
        if (xv_get (item, PANEL_ACCEPT_KEYSTROKE)) {
	    xv_set (panel, PANEL_CARET_ITEM, item);
	}
    }
    xv_set ((Frame) wdata,
        XV_X, (displayWidth - w) / 2,
        XV_Y, (displayHeight - h) / 2,
        XV_SHOW, TRUE,
        NULL);
}

void xv_post_proc (Dlg_head *h, void (*callback)(void *), void *arg)
{
    struct xv_dispatch_proc *proc, *proc_new;

#ifndef PARALELIZE
    if (dispatch_current != NULL) {
#endif    
        proc_new = (struct xv_dispatch_proc *) xmalloc (
            sizeof (struct xv_dispatch_proc), "XV post proc");
        proc_new->callback = callback;
        proc_new->arg = arg;
        proc_new->next = NULL;
#ifndef PARALELIZE            
        if (dispatch_current->proc != NULL) {
            for (proc = dispatch_current->proc; 
                 proc->next != NULL; proc = proc->next);
            proc->next = proc_new;
        } else
            dispatch_current->proc = proc_new;
    }
#else
	proc_new->h = h;
	if (dispatch_current != NULL) {
	    for (proc = dispatch_current; proc->next != NULL; 
	        proc = proc->next);
	    proc->next = proc_new;
	} else
	    dispatch_current = proc_new;
#endif    
}

widget_data x_get_parent_object (Widget *w, widget_data wdata)
{
    if ((Frame) wdata == mcframe && w->wcontainer != (widget_data) NULL) {
        switch (w->area) {
            case AREA_TOP: 
                return (widget_data) xv_get ((Frame) (w->wcontainer), 
                    XV_KEY_DATA, KEY_DATA_AREA_TOP);
            case AREA_LEFT: 
                return (widget_data) xv_get ((Frame) (w->wcontainer), 
                    XV_KEY_DATA, KEY_DATA_AREA_LEFT);
            case AREA_RIGHT: 
                return (widget_data) xv_get ((Frame) (w->wcontainer), 
                    XV_KEY_DATA, KEY_DATA_AREA_RIGHT);
            case AREA_BOTTOM: 
                return (widget_data) xv_get ((Frame) (w->wcontainer), 
                    XV_KEY_DATA, KEY_DATA_AREA_BOTTOM);
        }
	return (widget_data) NULL;
    } else
    	return (widget_data) xv_get ((Frame) wdata, FRAME_CMD_PANEL);
}

void xv_widget_layout (Panel_item item, WLay layout)
{
    Panel panel = (Panel) xv_get (item, XV_OWNER);
    Panel_item lastitem = (Panel_item) xv_get (panel,
        XV_KEY_DATA, KEY_DATA_PANEL_LASTITEM);
    int maxx = (int) xv_get (panel, XV_KEY_DATA, KEY_DATA_PANEL_MAXX);
    int maxy = (int) xv_get (panel, XV_KEY_DATA, KEY_DATA_PANEL_MAXY);
    int w = (int) xv_get (item, XV_WIDTH);
    int h = (int) xv_get (item, XV_HEIGHT);
    int x = (int) xv_get (panel, PANEL_ITEM_X);
    int y = (int) xv_get (panel, PANEL_ITEM_Y);

    if (lastitem != XV_NULL && layout != XV_WLAY_DONTCARE) {
        switch (layout) {
            case XV_WLAY_RIGHTOF:
            case XV_WLAY_RIGHTDOWN:
                x = (int) xv_get (lastitem, XV_X) +
                    (int) xv_get (lastitem, XV_WIDTH) +
                    (int) xv_get (panel, PANEL_ITEM_X_GAP);
                if (layout == XV_WLAY_RIGHTOF) 
                    y = (int) xv_get (lastitem, XV_Y);
                else {
                    y = (int) xv_get (lastitem, XV_Y) + 
                        (int) xv_get (lastitem, XV_HEIGHT) - h;
                }
                break;
            case XV_WLAY_BELOWOF:
                x = (int) xv_get (lastitem, XV_X);
		y = (int) xv_get (lastitem, XV_Y) +
                    (int) xv_get (lastitem, XV_HEIGHT) +
                    (int) xv_get (panel, PANEL_ITEM_Y_GAP);
                break;
            case XV_WLAY_BELOWCLOSE:
                x = (int) xv_get (lastitem, XV_X);
		y = (int) xv_get (lastitem, XV_Y) +
                    (int) xv_get (lastitem, XV_HEIGHT);
                break;
            case XV_WLAY_NEXTCOLUMN:
               y = (int) xv_get (panel, XV_KEY_DATA, KEY_DATA_PANEL_MINY);
               x = maxx + (int) xv_get (panel, PANEL_ITEM_X_GAP);
               break;
            case XV_WLAY_NEXTROW:
            case XV_WLAY_CENTERROW:
            case XV_WLAY_EXTENDWIDTH:
               x = (int) xv_get (panel, XV_KEY_DATA, KEY_DATA_PANEL_MINX);
               y = maxy + (int) xv_get (panel, PANEL_ITEM_Y_GAP);
               break;
            case XV_WLAY_DONTCARE:
               break;
        }
    	xv_set (item,
               XV_X, x,
               XV_Y, y,
               NULL);
    } else if (lastitem == XV_NULL) {
        xv_set (item,
        	XV_X, x,
        	XV_Y, y,
        	NULL);
        xv_set (panel, 
            XV_KEY_DATA, KEY_DATA_PANEL_MINX, x,
            NULL);
        xv_set (panel, 
            XV_KEY_DATA, KEY_DATA_PANEL_MINY, y,
            NULL);
    }

    if (maxx < x + w)
        xv_set (panel,
            XV_KEY_DATA, KEY_DATA_PANEL_MAXX, x + w,
            NULL);
    if (maxy < y + h)
        xv_set (panel,
            XV_KEY_DATA, KEY_DATA_PANEL_MAXY, y + h,
            NULL);
    xv_set (panel,
        XV_KEY_DATA, KEY_DATA_PANEL_LASTITEM, item,
        NULL);
}

int move (int y, int x)
{
    return 0;
}

void
x_init_dlg (Dlg_head *h)
{
	if (h != midnight_dlg)
		xvdlg_show(h, h->wdata);
	else {
		int i;
		for (i = 0; i < containers_no; i++)
			x_panel_container_show (containers [i]);
	}
}

void
x_destroy_dlg (Dlg_head *h)
{
}

void
x_focus_widget (Widget_Item *p)
{
}

void
edition_post_exec (void)
{
}

void
edition_pre_exec  (void)
{
}
