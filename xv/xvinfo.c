/* XView info stuff.
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
#include <xview/canvas.h>
#include <xview/svrimage.h>
#include <xview/cms.h>
#include <xview/font.h>
#include <xview/scrollbar.h>
#include <X11/xpm.h>

#include "fs.h"
#include "dir.h"
#include "panel.h"
#include "dlg.h"
#include "widget.h"
#include "info.h"
#include "main.h"
#include "util.h"
#include "win.h"
#include "xvmain.h"

#include "mc.icon"

extern Dlg_head *midnight_dlg;
extern int defaultDepth;	/* xvmain.c */
extern Display *dpy;		/* xvmain.c */
extern Frame mcframe;		/* ??? xvmain.c, but is this what we want?? */

static Cms infocms;
Server_image mcimage;

#include "mc_icon.xpm"

void x_setup_info (void)
{
    Pixmap pixmap, shapemask = 0;
    
    infocms = xv_create (XV_NULL, CMS,
	CMS_SIZE, sizeof (mc_colors) / sizeof (Xv_singlecolor) + CMS_CONTROL_COLORS,
        CMS_CONTROL_CMS, TRUE,
        CMS_NAME, "infocms",
        CMS_COLORS, &mc_colors,
        NULL);
	
    XpmCreatePixmapFromData (dpy, (Drawable)xv_get(mcframe, XV_XID),
			     mc_icon_xpm, &pixmap, &shapemask, NULL);
			     
    mcimage = xv_create (XV_NULL, SERVER_IMAGE,
        XV_WIDTH, mc_width,
        XV_HEIGHT, mc_height,
        SERVER_IMAGE_COLORMAP, "infocms",
        SERVER_IMAGE_DEPTH, defaultDepth,
/*        SERVER_IMAGE_X_BITS, mc_bits, */
        SERVER_IMAGE_PIXMAP, pixmap,
        NULL);
}

#if 0
static void x_info_recalculate (Columnizer tile)
{
    int i, j, top = xv_get (tile, COLUMNIZER_N_COLUMNS);
    int count = xv_get (tile, COLUMNIZER_N_ROWS), max, w;

    for (i = 0; i < top; i++) {
        max = 0;
        for (j = 0; j < count; j++) {
            w = xv_get ((Rectobj) xv_get (tile, COLUMNIZER_POSITION, i, j),
                XV_WIDTH);
            if (w > max)
            	max = w;
        }
        for (j = 0; j < count; j++) {
            xv_set ((Rectobj) xv_get (tile, COLUMNIZER_POSITION, i, j),
                XV_WIDTH, max,
                NULL);
        }
    }
}
#endif

void x_show_info (WInfo *info, struct my_statfs *s, struct stat *b)
{
#if 0
    Canvas_shell canvas = (Canvas_shell) xv_get ((Bag) (info->widget.wdata),
        XV_OWNER);
    Columnizer tile = (Columnizer) xv_get ((Bag) (info->widget.wdata),
        RECTOBJ_NTH_CHILD, 3); 
    char buffer [100];
    char *text;
    char *p;
    int i;
    Xv_opaque win;
    int w, h;
    Scrollbar scrollbar;

    xv_set (canvas,
        CANVAS_SHELL_DELAY_REPAINT, TRUE,
        NULL);
        
    for (i = 0; i < 14; i++) {
        text = buffer;
        switch (i + 3) {
    	    case 16:
		if (s->nfree >0 || s->nodes > 0)
	    	    sprintf (buffer, "%d (%d%%) of %d",
		        s->nfree,
		        s->total
		        ? 100 * s->nfree / s->nodes : 0,
		        s->nodes);
	        else
	            text = "No node information";
	        break;
	
	    case 15:
		if (s->avail > 0 || s->total > 0){
	    	    sprint_bytesize (buffer, s->avail, 1);
	    	    p = strchr (buffer, 0);
	    	    sprintf (p, " (%d%%) of ", s->total
		        ? 100 * s->avail / s->total : 0);
		    p = strchr (buffer, 0);
	            sprint_bytesize (p, s->total, 1);
	        } else
	            text = "No space information";
	        break;

            case 14:
		sprintf (buffer, "%s ", s->typename);
		p = strchr (buffer, 0);
	        if ((unsigned) s->type != 0xffff && 
	            (unsigned) s->type != 0xffffffff)
	            sprintf (p, " (%Xh)", s->type);
	        break;

    	    case 13:
		text = s->device;
		break;
		
    	    case 12:
    	        text = s->mpoint;
    	        break;

    	    case 11:
		text = file_date (b->st_atime);
		break;
	
    	    case 10:
		text = file_date (b->st_mtime);
		break;
	
	    case 9:
		text = file_date (b->st_ctime);
		break;
	
    	    case 8:
#ifdef HAVE_ST_RDEV
		if (b->st_rdev) {
	    	    sprintf (buffer, "major: %d, minor: %d",
		        b->st_rdev >> 8, b->st_rdev & 0xff);
		    xv_set (xv_get (tile, COLUMNIZER_POSITION, 0, i),
		        DRAWTEXT_STRING, "Inode dev:",
		        NULL);
		} else {
		    xv_set (xv_get (tile, COLUMNIZER_POSITION, 0, i),
		        DRAWTEXT_STRING, "Size:",
		        NULL);
#endif
	    	    sprint_bytesize (buffer, b->st_size, 0);
	    	    p = strchr (buffer, 0);
#ifdef HAVE_ST_BLOCKS
	    	    sprintf (p, " (%d blocks)", (int) b->st_blocks);
#endif
#ifdef HAVE_ST_RDEV	    	    
		}
#endif
		break;
		
	    case 7:
		sprintf (buffer, "%s/%s", get_owner (b->st_uid), 
		    get_group (b->st_gid));
		break;
	
	    case 6:
		sprintf (buffer, "%d", b->st_nlink);
		break;
	
	    case 5:
		sprintf (buffer, "%s (%o)",
		    string_perm (b->st_mode), b->st_mode & 07777);
		break;
	
	    case 4:
		sprintf (buffer, "%Xh:%Xh", (int) b->st_dev, (int) b->st_ino);
		break;
	
	    case 3:
	    	text = cpanel->dir.list [cpanel->selected].fname;
	    	break;
	}
	xv_set (xv_get (tile, COLUMNIZER_POSITION, 1, i),
	    DRAWTEXT_STRING, text,
	    NULL);
    }
    
    x_info_recalculate (tile);
    
    w = xv_get ((Bag) (info->widget.wdata), XV_WIDTH);
    h = xv_get ((Bag) (info->widget.wdata), XV_HEIGHT);
    
    	xv_set (canvas,
            CANVAS_MIN_PAINT_WIDTH, w,
            CANVAS_MIN_PAINT_HEIGHT, h,
            NULL);

        OPENWIN_EACH_VIEW (canvas, win)
            scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_HORIZONTAL_SCROLLBAR, win);
            if (scrollbar != XV_NULL)
                xv_set (scrollbar,
                    SCROLLBAR_OBJECT_LENGTH, w,
                    SCROLLBAR_PIXELS_PER_UNIT, 1,
                    NULL);
            scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_VERTICAL_SCROLLBAR, win);
            if (scrollbar != XV_NULL)
                xv_set (scrollbar,
                    SCROLLBAR_OBJECT_LENGTH, h,
                    SCROLLBAR_PIXELS_PER_UNIT, 1,
                    NULL);
        OPENWIN_END_EACH

    xv_set (canvas,
        CANVAS_SHELL_DELAY_REPAINT, FALSE,
        NULL);
#endif        
}

#ifndef VERSION
#   define VERSION "undefined"
#endif

#if 0
void backevent_proc_seal (Xv_window, Event *, Canvas_shell, Rectobj);
#endif
int x_create_info (Dlg_head *h, widget_data parent, WInfo *info)
{
#if 0
    Canvas canvas = 
        (Canvas) x_get_parent_object ((Widget *) info, parent);
    Bag tile;
    Xv_opaque previous;
    char buffer [50];
    Drawtext text;
    Columnizer tile1;
    int i, height;
    static char *names [] = { "File:", "Location:", "Mode:", "Links:", "Owner:",
        "Size:", "Created:", "Modified:", "Accessed:", "Filesystem:", "Device:",
        "Type:", "Free space:", "Free nodes:" };
    
    previous = xv_get (canvas, RECTOBJ_NTH_CHILD, 0);
    
    if (previous != XV_NULL)
    	xv_destroy_safe (previous);
    
    tile = xv_create (canvas, BAG,
	XV_KEY_DATA, KEY_DATA_PANEL, info,
	XV_KEY_DATA, KEY_DATA_PANEL_CONTAINER, xv_get (canvas, XV_OWNER),
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
	XV_X, 5, 
	XV_Y, 5,
	XV_KEY_DATA, KEY_DATA_BOLD_FONT,
	    xv_find (canvas, FONT,
	        FONT_FAMILY, xv_get (xv_get (canvas, CANVAS_SHELL_FONT),
	            FONT_FAMILY),
	        FONT_SIZE, xv_get (xv_get (canvas, CANVAS_SHELL_FONT),
	            FONT_SIZE),
	        FONT_STYLE, FONT_STYLE_BOLD,
	        NULL),
	NULL);
    xv_set (canvas, 
        XV_KEY_DATA, KEY_DATA_TILE, tile,
        RECTOBJ_ACCEPTS_DROP, FALSE,
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
        CANVAS_PAINTWINDOW_ATTRS,
            WIN_IGNORE_EVENTS,
                KBD_DONE, KBD_USE, WIN_ASCII_EVENTS, WIN_LEFT_KEYS, WIN_TOP_KEYS,
                WIN_RIGHT_KEYS, WIN_META_EVENTS, WIN_EDIT_KEYS, WIN_MOTION_KEYS,
                WIN_TEXT_KEYS, 
                NULL,
            NULL,
        CANVAS_SHELL_EVENT_PROC, NULL,
        WIN_CMS, infocms,
        NULL);
    xv_create (tile, DRAWIMAGE,
        XV_X, 10,
        XV_Y, 10,
        XV_WIDTH, mc_width,
        XV_HEIGHT, mc_height,
        DRAWIMAGE_IMAGE1, mcimage,
        RECTOBJ_SELECTABLE, FALSE,
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
        NULL);
    sprintf (buffer, "Midnight X Commander, version %s", VERSION);
    text = (Drawtext) xv_create (tile, DRAWTEXT,
        XV_X, 10,
        XV_Y, mc_height + 15,
        DRAWTEXT_STRING, buffer,
        DRAWTEXT_FONT, xv_get (tile, XV_KEY_DATA, KEY_DATA_BOLD_FONT),
        RECTOBJ_SELECTABLE, FALSE,
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
        NULL);
    height = xv_get (text, XV_HEIGHT) + mc_height + 15;
    xv_create (tile, DRAWRECT,
        XV_X, 10,
        XV_Y, height + 2,
        XV_WIDTH, xv_get (text, XV_WIDTH),
        XV_HEIGHT, 4,
        DRAWRECT_BORDER2, 1,
        RECTOBJ_SELECTABLE, FALSE,
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
        NULL);
    tile1 = xv_create (tile, COLUMNIZER,
        XV_X, 10,
        XV_Y, height + 10,
        COLUMNIZER_N_COLUMNS, 2,
        COLUMNIZER_N_ROWS, 14,
        COLUMNIZER_COLUMN_GAP, 10,
        COLUMNIZER_ROW_GAP, 0,
	RECTOBJ_EVENT_PROC, backevent_proc_seal,
        NULL);
    for (i = 0; i < 14; i++) {
        xv_create (tile1, DRAWTEXT,
	    DRAWTEXT_STRING, names [i],
            DRAWTEXT_FONT, xv_get (tile, XV_KEY_DATA, KEY_DATA_BOLD_FONT),
            RECTOBJ_SELECTABLE, FALSE,
	    RECTOBJ_EVENT_PROC, backevent_proc_seal,
            NULL);
        xv_create (tile1, DRAWTEXT,
            RECTOBJ_SELECTABLE, FALSE,
	    RECTOBJ_EVENT_PROC, backevent_proc_seal,
            NULL);
    }
    info->widget.wdata = (widget_data) tile;
    info_show_info (info);
    x_info_recalculate (tile1);
#endif
    return 1;
}

