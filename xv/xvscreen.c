/* XView panel stuff.
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
#include <X11/Xatom.h>
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/canvas.h>
#include <xview/scrollbar.h>
#include <xview/svrimage.h>
#include <xview/win_event.h>
#include <xview/cms.h>
#include <xview/dragdrop.h>
#include <xview/cursor.h> 
#include <xview/rect.h>
#include <string.h>

#include "mad.h"
#include "mem.h"
#include "fs.h"
#include "dir.h"
#include "panel.h"
#include "dlg.h"
#include "widget.h"
#include "command.h"
#include "cmd.h"
#include "main.h"
#include "xvmain.h"
#include "xvscreen.h"
#include "util.h"
#include "ext.h" /* regex_command */

void do_enter (WPanel *panel);

extern Server_image iconRegular [2];
extern Server_image iconDirectory [2];

extern WPanel *current_panel;
extern WCommand *cmdline;
extern Cursor dropcursor;
extern Dlg_head *midnight_dlg;
extern Cms mccms;
extern Display *dpy;

extern int is_right;

#define PANELITEMBORDER 2
#define PANELBORDER	4

static void xv_panel_repaint_proc (Canvas canvas, Xv_Window paint_window,
    Rectlist *repaint);
    
void show_dir (WPanel *panel)
{
    /* This routine should update the current directory label */
    xv_set ((Frame) xv_get ((Canvas) panel->widget.wdata, XV_OWNER),
        FRAME_LABEL, panel->cwd,
        NULL);
}

void xv_set_current_panel (WPanel *panel)
{
    if (panel != current_panel)
    	change_panel ();
}

int x_panel_load_index (WPanel *panel, int idx)
{
    return 0;
}

void xv_clear_area (Xv_window xv_win, Rect *rect)
{
    XClearArea (dpy, xv_get (xv_win, XV_XID),
        rect->r_left, rect->r_top,
        rect->r_width, rect->r_height,
        FALSE);
}

void xv_invalidate_rect (Canvas canvas, Rect *rect)
{
    Xv_window xv_win, xv_view;
    Scrollbar scrollbar;
    Rect rec;
    int i, j;
    Rectlist rl;
    Rectnode rn;
    
    CANVAS_EACH_PAINT_WINDOW (canvas, xv_win)
        if (rect == NULL) {
            rec.r_left = 0;
            rec.r_top = 0;
            rec.r_width = xv_get (xv_win, XV_WIDTH);
            rec.r_height = xv_get (xv_win, XV_HEIGHT);
            xv_clear_area (xv_win, &rec);
        } else
            rec = *rect;
        xv_view = xv_get (xv_win, CANVAS_PAINT_VIEW_WINDOW);
        scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_HORIZONTAL_SCROLLBAR, xv_win);
        if (scrollbar != XV_NULL) {
            i = xv_get (scrollbar, SCROLLBAR_VIEW_START);
            j = xv_get (xv_view, XV_WIDTH);
            if (rec.r_left < i)
                rec.r_left = i;
            if (rec.r_left + rec.r_width > i + j)
                rec.r_width = i + j - rec.r_left;
        }
        scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_VERTICAL_SCROLLBAR, xv_win);
        if (scrollbar != XV_NULL) {
            i = xv_get (scrollbar, SCROLLBAR_VIEW_START) * 
                xv_get (scrollbar, SCROLLBAR_PIXELS_PER_UNIT);
            j = xv_get (xv_view, XV_HEIGHT);
            if (rec.r_top < i)
                rec.r_top = i;
            if (rec.r_top + rec.r_height > i + j)
                rec.r_height = i + j - rec.r_top;
        }
        rn.rn_next = NULL;
        rn.rn_rect = rec;
        rl.rl_x = 0;
        rl.rl_y = 0;
        rl.rl_head = rl.rl_tail = &rn;
        rl.rl_bound = rec;
        xv_panel_repaint_proc (canvas, xv_win, &rl);
    CANVAS_END_EACH
}

void xv_invalidate_item (Canvas canvas, int idx)
{
    Rect rect;
    WPanel *panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
    
    rect_construct (&rect, PANELBORDER, PANELBORDER + panel->item_height * idx,
                           panel->total_width, panel->item_height);
    xv_invalidate_rect (canvas, &rect);
}

/* This procedure should be called whenever we change a f.marked file attribute
   as a result of the program action, not by user clicking */
void x_panel_select_item (WPanel *panel, int idx, int value)
{
    Canvas canvas = (Canvas) (panel->widget.wdata);

    xv_invalidate_item (canvas, idx);
}

typedef enum {
    MIP_UP, MIP_DOWN, MIP_PAGEUP, MIP_PAGEDOWN, MIP_HOME, MIP_END,
} mipaction;

static void moveinpanel (Canvas canvas, Xv_window paint, mipaction action)
{
    WPanel *panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
    Scrollbar scroll = (Scrollbar) xv_get (canvas, OPENWIN_VERTICAL_SCROLLBAR,
    	xv_get (paint, CANVAS_PAINT_VIEW_WINDOW));
    int newsel, newstart;
    int start = xv_get (scroll, SCROLLBAR_VIEW_START);
    int length = xv_get (scroll, SCROLLBAR_VIEW_LENGTH);
    int lasttop = panel->count - length;
    
    newstart = start;
    newsel = panel->selected;
    switch (action) {
        case MIP_PAGEUP:
            if (newsel > start)
                newsel = start;
            else {
            	newstart -= length - 1;
            	newsel -= length - 1;
            }
            break;
        case MIP_PAGEDOWN:
	    if (newsel < start + length - 1)
	        newsel = start + length - 1;
	    else {
	        newstart += length - 1;
	        newsel += length - 1;
	    }
	    break;
	case MIP_HOME:
	    newsel = 0;
	    newstart = 0;
	    break;
	case MIP_END:
	    newsel = panel->count - 1;
	    newstart = lasttop;
	    break;
	case MIP_UP:
	    if (newsel <= start)
	        newstart -= length / 2 - 1;
	    newsel--;
	    break;
	case MIP_DOWN:
	    if (newsel >= start + length - 1) {
	        newstart += length / 2 - 1;
	    }
	    newsel++;
	    break;
    }
    if (newsel > panel->count - 1)
        newsel = panel->count - 1;
    if (newsel < 0)
        newsel = 0;
    if (newstart > lasttop)
        newstart = lasttop;
    if (newstart < 0)
        newstart = 0;

    if (newstart != start || newsel != panel->selected) {    
        if (newstart != start)
            xv_set (scroll,
                SCROLLBAR_VIEW_START, newstart,
                NULL);
            
        if (newsel != panel->selected) {
            unselect_item (panel);
            panel->selected = newsel;
            select_item (panel);
        }

    }
}

static WPanel *drag_panel;
struct copy_move_in_panel {
    int copy;
    char *to;
    WPanel *from;
};

static void copymove_in_panel (struct copy_move_in_panel *cmip)
{
    WPanel *panel = current_panel;
    
    xv_set_current_panel (cmip->from);
    copymove_cmd_with_default (cmip->copy, cmip->to);
    free (cmip->to);
    free (cmip);
    xv_set_current_panel (panel);
}

static void delete_cmd_in_panel (WPanel *panel)
{
    xv_set_current_panel (panel);
    delete_cmd ();
}

struct user_drop {
    WPanel *panel;
    char *filename;
    char *drops;
};

static void user_drop (struct user_drop *usdr)
{
    WPanel *panel = current_panel;
    char *droplist[2];
    droplist [0] = usdr->drops;
    droplist [1] = NULL;
    
    xv_set_current_panel (usdr->panel);
    regex_command (usdr->filename, "Drop", droplist, NULL);
    free (usdr->drops);
    free (usdr);
    xv_set_current_panel (panel);
}

static int selection_convert (Selection_owner seln, Atom *type, Xv_opaque *data,
    unsigned long *length, int *format)
{
    Xv_Server server = XV_SERVER_FROM_WINDOW (xv_get (seln, XV_OWNER));
    
    if (*type == (Atom) xv_get (server, SERVER_ATOM, "_SUN_SELECTION_END") ||
        *type == (Atom) xv_get (server, SERVER_ATOM, "_SUN_DRAGDROP_DONE")) {
        *format = 32;
        *length = 0;
        *data = XV_NULL;
        *type = (Atom) xv_get (server, SERVER_ATOM, "NULL");
        return TRUE;
    } else if (*type == (Atom) xv_get (server, SERVER_ATOM, "DELETE")) {
        xv_post_proc (midnight_dlg, (void (*)(void *))delete_cmd_in_panel,
            (void *) drag_panel);
        *format = 32;
        *length = 0;
        *data = XV_NULL;
        *type = (Atom) xv_get (server, SERVER_ATOM, "NULL");
        return TRUE;
    } else
        return sel_convert_proc (seln, type, data, length, format);
}

static void start_drag (Xv_window paint, Event *event, Canvas canvas)
{
    Dnd dnd;
    Selection_item sel_item;
    WPanel *panel;
    char *p, *q;
    int i = (event_shift_is_down (event) || event_ctrl_is_down (event));
    int trailslash;
    Xv_Server server = XV_SERVER_FROM_WINDOW (paint);
    
    drag_panel = panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
    dnd = xv_create (paint, DRAGDROP,
        DND_TYPE, i ? DND_COPY : DND_MOVE,
        SEL_CONVERT_PROC, selection_convert,
        DND_ACCEPT_CURSOR, dropcursor,
        NULL);
    if (i) {
        xv_set (dnd,
            DND_CURSOR, xv_create (XV_NULL, CURSOR,
                CURSOR_SRC_CHAR, OLC_COPY_PTR,
                CURSOR_MASK_CHAR, OLC_COPY_MASK_PTR,
                NULL),
            NULL);
    } else
        xv_set (dnd,
            DND_CURSOR, xv_create (XV_NULL, CURSOR,
                CURSOR_SRC_CHAR, OLC_MOVE_PTR,
                CURSOR_MASK_CHAR, OLC_MOVE_MASK_PTR,
                NULL),
	    NULL);
    p = "";
    trailslash = (panel->cwd [strlen (panel->cwd) - 1] == '/');
    for (i = 0; i < panel->count; i++)
        if (panel->dir.list [i].f.marked) {
            q = p;
            if (trailslash)
                p = copy_strings (q, panel->cwd, panel->dir.list [i].fname, 
                    " ", NULL);
            else
                p = copy_strings (q, panel->cwd, "/", panel->dir.list [i].fname, 
                    " ", NULL);
            if (*q)
                free (q);
        }
    if (!*p) {
        if (trailslash)
            p = copy_strings (panel->cwd, 
                panel->dir.list [panel->selected].fname, NULL);
        else
            p = copy_strings (panel->cwd, "/", 
                panel->dir.list [panel->selected].fname, " ", NULL);
    }
    sel_item = xv_create (dnd, SELECTION_ITEM,
        SEL_TYPE, (Atom) XA_STRING,
        SEL_DATA, p,
        NULL);
    xv_create (dnd, SELECTION_ITEM,
        SEL_TYPE, (Atom) xv_get (server, SERVER_ATOM, "_MXC_PANELID"),
        SEL_FORMAT, sizeof (WPanel *) * 8,
        SEL_LENGTH, 1,
        SEL_DATA, &drag_panel,
        NULL);
    dnd_send_drop (dnd);
    xv_destroy_safe (dnd);
}

static void do_drop (Xv_window paint, Event *event, Canvas canvas, 
    int in, int idx)
{
    Selection_requestor sel;
    Xv_server server;
    WPanel **ppanel;
    WPanel *panel;
    char *p;
    int trailslash;
    struct copy_move_in_panel *cmip;
    struct user_drop *usdr;

    switch (event_action (event)) {
    case ACTION_DRAG_PREVIEW:
        return;
    case ACTION_DRAG_COPY:
    case ACTION_DRAG_MOVE:
    case ACTION_DRAG_LOAD: 
        server = XV_SERVER_FROM_WINDOW (paint);
        sel = (Selection_requestor) 
            xv_get (canvas, XV_KEY_DATA, KEY_DATA_SELREQ);
        if (dnd_decode_drop (sel, event) != XV_ERROR) {
            int length, format;
            char *q;
            
            xv_set (sel, SEL_TYPE, XA_STRING, NULL);
            q = (char *) xv_get (sel, SEL_DATA, &length, &format);
            if (length != SEL_ERROR) {
  		panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
                if (!in || S_ISDIR (panel->dir.list [idx].buf.st_mode)) {
                    xv_set (sel, 
                        SEL_TYPE, (Atom) xv_get (server, SERVER_ATOM, "_MXC_PANELID"),
                        NULL);
  		    ppanel = (WPanel **) xv_get (sel, SEL_DATA, &length, &format);
  		    trailslash = (panel->cwd [strlen (panel->cwd) - 1] == '/');
  		    if (length != SEL_ERROR) {
		        if (!in) {
			    p = strdup (panel->cwd);
		        } else {
		            if (trailslash)
		                p = copy_strings (panel->cwd, 
		            		          panel->dir.list [idx].fname, NULL);
		            else
		                p = copy_strings (panel->cwd, "/",
		            		          panel->dir.list [idx].fname, NULL);
		        }
		        cmip = (struct copy_move_in_panel *)
		            xmalloc (sizeof (struct copy_move_in_panel),
		            "Copy/Move in Panel");
		        cmip->copy = (event_action (event) == ACTION_COPY);
		        cmip->from = *ppanel;
		        cmip->to = p;
		        xv_post_proc(midnight_dlg, 
		                     (void (*) (void *))copymove_in_panel,
		                     (void *) cmip);
		        free ((char *) ppanel);  		    
  		    }
  		    free (q);
  		} else {
		    usdr = (struct user_drop *)
		        xmalloc (sizeof (struct user_drop),
		        "User drop");
		    usdr->panel = panel;
		    usdr->filename = panel->dir.list [idx].fname;
		    usdr->drops = q;
  		    xv_post_proc(midnight_dlg,
  		    		 (void (*) (void *))user_drop,
  		    		 (void *) usdr);
  		}
            }
            xv_set (sel, SEL_TYPE_NAME, "_SUN_SELECTION_END", NULL);
            xv_get (sel, SEL_DATA, &length, &format);
            dnd_done (sel);
        }
    }
}

static int xv_get_text_len (WPanel *panel, char *text)
{
    Font_string_dims dims;
    
    xv_get ((Font) (panel->font), FONT_STRING_DIMS, text, &dims);
    return dims.width;
}

static void xv_insert_panel_item (Canvas canvas, WPanel *panel, int idx)
{
    format_e *format;
    int i, len;
    char *txt;
    file_entry *fe = & panel->dir.list [idx];
    int marked = fe->f.marked;
    int isdir = S_ISDIR (panel->dir.list [idx].buf.st_mode);
    static char buffer [1024];
    char *p = buffer;
    
    for (format = panel->format; format; format = format->next) {
        if (format->string_fn) {
            txt = (*format->string_fn) (fe, 0);
        } else
            txt = " ";
        if (format->string_fn == string_file_type) {
	    format->field_len = 18;
	    p [0] = 3 + ((*txt == '/') ? 1 : 0);
	    p [1] = 0;
	    p += 2;
        } else {
	    len = xv_get_text_len (panel, txt) + 2;
	    if (format->field_len < len)
	        format->field_len = len;
	    p [0] = format->just_mode + 1;

	    strcpy (p + 1, txt);
	    p = index (p + 1, 0) + 1;
	}                
    }
    *(p++) = 0;
    fe->cache = xmalloc (p - buffer, "xview display cache");
    bcopy (buffer, fe->cache, p - buffer);
}

void x_select_item (WPanel *panel)
{
    Canvas canvas = (Canvas) panel->widget.wdata;
    int idx = panel->selected;

    xv_invalidate_item (canvas, idx);
}

void x_unselect_item (WPanel *panel)
{
    Canvas canvas = (Canvas) panel->widget.wdata;
    int idx = panel->selected;

    panel->selected = -1;
    xv_invalidate_item (canvas, idx);
    panel->selected = idx;
}

void x_fill_panel (WPanel *panel)
{
    int i;
    int w, h;
    Xv_Window win, paint;
    Canvas canvas = (Canvas) panel->widget.wdata;
    Scrollbar scrollbar;
    Rect rect;
    format_e *format;

    for (format = panel->format; format; format = format->next)
        format->field_len = 2;
        
    for (i = 0; i < panel->count; i++)
        xv_insert_panel_item (canvas, panel, i);
        
    for (format = panel->format, panel->total_width = 2 * PANELITEMBORDER; 
         format; format = format->next)
        panel->total_width += format->field_len;
        
    w = panel->total_width + 2 * PANELBORDER;
    h = panel->item_height * panel->count + 2 * PANELBORDER;
    
    xv_set (canvas,
        CANVAS_MIN_PAINT_WIDTH, w,
        CANVAS_MIN_PAINT_HEIGHT, h,
        NULL);
        
    i = panel->item_height;
    
    rect.r_left = PANELBORDER;
    rect.r_width = panel->total_width;
    rect.r_top = PANELBORDER;
    rect.r_height = h - 2 * PANELBORDER;
    
    OPENWIN_EACH_VIEW (canvas, win)
        scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_HORIZONTAL_SCROLLBAR, win);
        if (scrollbar != XV_NULL)
            xv_set (scrollbar,
                SCROLLBAR_OBJECT_LENGTH, w,
                NULL);
        scrollbar = (Scrollbar) xv_get (canvas, OPENWIN_VERTICAL_SCROLLBAR, win);
        if (scrollbar != XV_NULL)
            xv_set (scrollbar,
            	SCROLLBAR_PIXELS_PER_UNIT, i,
                SCROLLBAR_OBJECT_LENGTH, panel->count + 1,
                NULL);
        paint = (Xv_window) xv_get (win, CANVAS_VIEW_PAINT_WINDOW);
        xv_set (xv_get (paint, XV_KEY_DATA, KEY_DATA_TILE),
            DROP_SITE_DELETE_REGION, NULL,
            DROP_SITE_REGION, &rect,
            NULL);
    OPENWIN_END_EACH

    xv_invalidate_rect (canvas, NULL);
    
}

static void xv_panel_repaint_item (WPanel *panel, int idx, Xv_Window paint_window)
{
    int selmark = 0;
    file_entry *fe = & panel->dir.list [idx];
    Drawable xid = (Drawable) xv_get (paint_window, XV_XID);
    unsigned long fg, bg, *xc;
    GC gc = (GC) (panel->gc); /* where does this get set? -- BD */
    Rect rect;
    char *cache = fe->cache;
    XPoint xp [3];
    char *p;
    int w, x, h, y;
    Server_image image, mask;
    format_e *format;
    
        
    if (cache == NULL)
        return;
    if (fe->f.marked)
        selmark |= 1;
    if (idx == panel->selected)
        selmark |= 2;
    
    xc = (unsigned long *) xv_get (mccms, CMS_INDEX_TABLE);
    
    fg = xc [selmark * 2 + CMS_CONTROL_COLORS];
    bg = xc [(selmark & 2) ? 1 : 0];
    
    rect_construct (&rect, PANELBORDER, PANELBORDER + idx * panel->item_height,
                           panel->total_width, panel->item_height);
    
    XSetForeground (dpy, gc, bg);
    XFillRectangle (dpy, xid, gc,
        rect.r_left,
        rect.r_top,
        rect.r_width,
        rect.r_height);

    if (selmark & 2) {
        xp [0].x = rect.r_left;
        xp [0].y = rect.r_top + rect.r_height;
        xp [1].x = rect.r_left;
        xp [1].y = rect.r_top;
        xp [2].x = rect.r_left + rect.r_width - 1;
        xp [2].y = rect.r_top;
        XSetForeground (dpy, gc, xc [2]);
        XDrawLines (dpy, xid, gc, xp, 3, CoordModeOrigin);
        xp [0].x = rect.r_left + 1;
        xp [0].y = rect.r_top + rect.r_height - 1;
        xp [1].x = rect.r_left + rect.r_width - 1;
        xp [1].y = rect.r_top + rect.r_height - 1;
        xp [2].x = rect.r_left + rect.r_width - 1;
        xp [2].y = rect.r_top;
        XSetForeground (dpy, gc, xc [3]);
        XDrawLines (dpy, xid, gc, xp, 3, CoordModeOrigin);
    }
    
    XSetForeground (dpy, gc, fg);
    XSetBackground (dpy, gc, bg);
    rect.r_left += PANELITEMBORDER;
    
    for (format=panel->format , p = fe->cache; *p; p = strchr (p, 0) + 1, format=format->next) {
        if (*p <= 2) { /* i.e. either left or right justified text */
            w = xv_get_text_len (panel, p + 1);
            h = panel->ascent + panel->descent;
            y = rect.r_top + panel->ascent + (panel->item_height - h) / 2;
            if (*p == J_LEFT + 1)
                x = rect.r_left + 1;
            else
            	x = rect.r_left + format->field_len - w - 1;
            XDrawString (dpy, xid, gc, x, y, p + 1, strlen (p + 1));
        } else {
            switch (*p) {
                case 3: image = iconRegular [0]; mask = iconRegular [1]; break;
                case 4: image = iconDirectory [0]; mask = iconDirectory [1]; break;
            }
            XSetClipMask (dpy, gc, (Drawable) xv_get (mask, SERVER_IMAGE_PIXMAP));
            x = rect.r_left + 2;
            y = rect.r_top + (panel->item_height - 16) / 2;
            XSetClipOrigin (dpy, gc, x, y);
            XCopyArea (dpy, (Drawable) xv_get (image, SERVER_IMAGE_PIXMAP),
                       xid, gc,
                       0, 0, 
                       16, 16, 
                       x, y);
            XSetClipMask (dpy, gc, None);
        }
        rect.r_left += format->field_len;
    }
}

static void xv_panel_repaint_proc (Canvas canvas, Xv_Window paint_window,
    Rectlist *repaint)
{
    int first, last;
    WPanel *panel;
    int i;
    Rect rect;

    panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
        
/* This stuff has to change as soon as we start supporting big icons display */
    first = (repaint->rl_y + repaint->rl_bound.r_top - PANELBORDER) / panel->item_height;
    last = (repaint->rl_y + repaint->rl_bound.r_top + repaint->rl_bound.r_height
        - PANELBORDER - 1) / panel->item_height;
    if (first < 0) {
        first = 0;
    }
    if (last >= panel->count) {
        last = panel->count - 1;
    }
    
    /* First we clear all the areas around... */

    if (repaint->rl_x + repaint->rl_bound.r_left < PANELBORDER) {
        rect.r_left = repaint->rl_x + repaint->rl_bound.r_left;
        rect.r_width = PANELBORDER - rect.r_left;
        rect.r_top = repaint->rl_y + repaint->rl_bound.r_top;
        rect.r_height = repaint->rl_bound.r_height;
        xv_clear_area (paint_window, &rect);
    }

    if (repaint->rl_x + repaint->rl_bound.r_left + repaint->rl_bound.r_width
        > PANELBORDER + panel->total_width) {
        rect.r_left = PANELBORDER + panel->total_width;
        rect.r_width = repaint->rl_x + repaint->rl_bound.r_left +
            repaint->rl_bound.r_width - rect.r_left;
        rect.r_top = repaint->rl_y + repaint->rl_bound.r_top;
        rect.r_height = repaint->rl_bound.r_height;
        xv_clear_area (paint_window, &rect);
    }
    
    if (repaint->rl_y + repaint->rl_bound.r_top < PANELBORDER) {
        rect.r_left = repaint->rl_x + repaint->rl_bound.r_left;
        rect.r_width = rect.r_left + repaint->rl_bound.r_width;
        if (rect.r_left < PANELBORDER)
            rect.r_left = PANELBORDER;
        if (rect.r_width > panel->total_width + PANELBORDER)
            rect.r_width = panel->total_width + PANELBORDER - rect.r_left;
        else
            rect.r_width -= rect.r_left;
        rect.r_top = repaint->rl_y + repaint->rl_bound.r_top;
        rect.r_height = PANELBORDER - rect.r_top;
        xv_clear_area (paint_window, &rect);
    }

    if (repaint->rl_y + repaint->rl_bound.r_top + repaint->rl_bound.r_height
        > PANELBORDER + panel->count * panel->item_height) {
        rect.r_left = repaint->rl_x + repaint->rl_bound.r_left;
        rect.r_width = rect.r_left + repaint->rl_bound.r_width;
        if (rect.r_left < PANELBORDER)
            rect.r_left = PANELBORDER;
        if (rect.r_width > panel->total_width + PANELBORDER)
            rect.r_width = panel->total_width + PANELBORDER - rect.r_left;
        else
            rect.r_width -= rect.r_left;
        rect.r_top = PANELBORDER + panel->count * panel->item_height;
        rect.r_height = repaint->rl_y + repaint->rl_bound.r_top + 
            repaint->rl_bound.r_height - rect.r_top;
        xv_clear_area (paint_window, &rect);
    }

    for (i = first; i <= last; i++) {
	xv_panel_repaint_item (panel, i, paint_window);
    }
}

static struct {
    Canvas canvas;
    int idx;
    int btn_down_x, btn_down_y;
    struct timeval prev_time;
} click_info = { XV_NULL, -1, 0, 0, {0, 0}};
static int xv_panel_grab = 0;
static int wait_for_btnup = 0;
static int rubber_adjust = 0;
static int lastx, lasty, startx, starty;
static GC xor_gc; 

/* Should select / invert selection of all the idx's inside 
 * lastx, startx, lasty, starty 
 * As a used side effect, setting lastx = startx = -1 should unmark all files
 */
static void xv_rubberband_select (WPanel *panel)
{
    int x1, y1, x2, y2, first, last, i;

    x1 = (startx < lastx) ? startx : lastx;
    y1 = (starty < lasty) ? starty : lasty;
    x2 = (startx > lastx) ? startx : lastx;
    y2 = (starty > lasty) ? starty : lasty;

    /* This has to change because of big icons */    
    if (x2 <= PANELBORDER || x1 >= panel->total_width + PANELBORDER) {
        first = last = -1;
    } else {
        first = (y1 - PANELBORDER) / panel->item_height;
        last = (y2 - PANELBORDER) / panel->item_height;
    }
    for (i = 0; i < panel->count; i++) {
        if (i >= first && i <= last) {
            if (rubber_adjust)
                do_file_mark (panel, i, panel->dir.list [i].f.marked ? 0 : 1);
            else if (!panel->dir.list [i].f.marked)
                do_file_mark (panel, i, 1);
        } else if (!rubber_adjust) {
            if (panel->dir.list [i].f.marked)
                do_file_mark (panel, i, 0);
        }
    }
}

int is_dbl_click (struct timeval *then, struct timeval *now)
{
    struct timeval delta;
    static int multiclicktimeout = 0;
    
    delta.tv_sec = now->tv_sec - then->tv_sec;
    if ((delta.tv_usec = now->tv_usec - then->tv_usec) < 0) {
        delta.tv_usec += 1000000;
        delta.tv_sec--;
    }
    if (multiclicktimeout == 0) {
        multiclicktimeout == defaults_get_integer ("mxc.doubleclicktime",
        					   "Mxc.DoubleClickTime", 5);
        if (multiclicktimeout < 2 || multiclicktimeout > 100)
            multiclicktimeout = 5;
    }   
    if ((delta.tv_sec * 10 + delta.tv_usec / 100000) <= multiclicktimeout)
        return 1;
    return 0;
}

static void draw_rubberband (Canvas canvas)
{
    Xv_window xv_win;
    int x1, y1, x2, y2, width, height;
    
    x1 = (startx < lastx) ? startx : lastx;
    y1 = (starty < lasty) ? starty : lasty;
    x2 = (startx > lastx) ? startx : lastx;
    y2 = (starty > lasty) ? starty : lasty;
    width = x2 - x1;
    height = y2 - y1;
    
    if (!width || !height)
        return;
    
    CANVAS_EACH_PAINT_WINDOW (canvas, xv_win)
         XDrawRectangle (dpy, xv_get (xv_win, XV_XID), xor_gc, x1, y1, 
             width, height);
    CANVAS_END_EACH
}

static void xv_dblclick_proc (WPanel *panel, int idx)
{
    xv_set_current_panel (panel);    
    if (panel->selected != idx)
    	panel->selected = idx; /* Should not happen :) */
    xv_post_proc (midnight_dlg, (void (*)(void *))do_enter, (void *)panel);
}

static WPanel *xv_filedepmenu_panel = NULL;
static int xv_filedepmenu_idx;
static char *xv_filedepmenu_free = NULL;

static void xv_filedepmenu_cmd (char *todo)
{
    char *p;
    
    p = strchr (todo, ' ');
    *p = 0;
    regex_command (p + 1, todo, NULL, NULL);
    free (todo);
}

static void xv_filedepmenu (Menu menu, Menu_item item)
{
    char *q;
    char *p = (char *) xv_get (item, MENU_STRING);
    
    if (!strcmp (p, "Copy"))
        xv_post_proc (midnight_dlg, (void (*)(void *))copy_cmd, (void *)NULL);
    else if (!strcmp (p, "Move"))
        xv_post_proc (midnight_dlg, (void (*)(void *))ren_cmd, (void *)NULL);
    else if (!strcmp (p, "Delete"))
        xv_post_proc (midnight_dlg, (void (*)(void *))delete_cmd, (void *)NULL);
    else {
        q = copy_strings (p, " ", xv_filedepmenu_panel->dir.list [xv_filedepmenu_idx].fname, NULL);
        xv_post_proc (midnight_dlg, (void (*)(void *))xv_filedepmenu_cmd, (void *)q);
    }
    if (xv_filedepmenu_free)
        free (xv_filedepmenu_free);
}

static void xv_mouse_event (Canvas canvas, Xv_window paint, Event *event)
{
    WPanel *panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
    int in, idx;
    
    if (event_x (event) < PANELBORDER || 
        event_x (event) >= PANELBORDER + panel->total_width ||
        event_y (event) < PANELBORDER || 
        event_y (event) >= PANELBORDER + panel->count * panel->item_height) {
        in = 0;
    } else {
        in = 1;
        idx = (event_y (event) - PANELBORDER) / panel->item_height;
    }
    switch (event_action (event)) {
        case ACTION_DRAG_PREVIEW:
        case ACTION_DRAG_COPY:
        case ACTION_DRAG_MOVE:
        case ACTION_DRAG_LOAD:
            do_drop (paint, event, canvas, in, idx);
            return;
    }
    if (xv_panel_grab) {
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
                if (dist_x > threshold || dist_y > threshold ||
                    event_ctrl_is_down (event) || event_shift_is_down (event)) {
                    xv_panel_grab = 0;
                    wait_for_btnup = 0;
                    start_drag (paint, event, canvas);
                }
                return;        
            }
            if (event_is_button (event) && event_is_down (event)) {
                return;
            }
            if (event_action (event) == ACTION_SELECT) {
                if (is_dbl_click (&click_info.prev_time, &event_time (event))) {
                    /* Should we handle tripple clicks? 
                     * Maybe for selecting/unselecting all the files :) */
                    click_info.prev_time.tv_sec = 0;
                    click_info.prev_time.tv_usec = 0;
                    xv_dblclick_proc (panel, click_info.idx);
                } else {
                    click_info.prev_time = event_time (event);
                }
                xv_panel_grab = 0;
                return;
            }
            if (event_action (event) == ACTION_ADJUST) {
                xv_panel_grab = 0;
                return;
            }
            if (!event_is_button (event) && !event_is_iso (event))
                return;
            xv_panel_grab = 0;
            return;
        } else {
            if (event_action (event) == LOC_DRAG) {
                draw_rubberband (canvas);
                lastx = event_x (event);
                lasty = event_y (event);
                draw_rubberband (canvas);
                return;
            }
            if (event_is_button (event)) {
                if (event_is_down (event)) {
                    draw_rubberband (canvas);
                    XFreeGC (dpy, xor_gc);
                    xv_panel_grab = 0;
                    return;
                }
            } else {
                if (event_is_iso (event)) {
                    draw_rubberband (canvas);
                    XFreeGC (dpy, xor_gc);
                    xv_panel_grab = 0;
                }
                return;
            }
            draw_rubberband (canvas);
            xv_panel_grab = 0;
            XFreeGC (dpy, xor_gc);
            xv_rubberband_select (panel);
            return;
        } 
    }
    switch (event_action (event)) {
        case ACTION_HELP:
            if (event_is_down (event)) {
            }
            return;
        case ACTION_MENU:
            if (event_is_down (event)) {
                static Menu menu = XV_NULL;
                char *p, *q, c;
                
                if (menu != XV_NULL)
                    xv_destroy (menu);
                menu = (Menu) xv_create (XV_NULL, MENU,
                    MENU_NOTIFY_PROC, xv_filedepmenu,
                    MENU_TITLE_ITEM, panel->dir.list [idx].fname,
                    MENU_STRINGS, "Open", "View", "Edit", "Copy", "Move", "Delete", NULL,
                    NULL);
                p = regex_command (panel->dir.list [idx].fname, NULL, NULL, NULL);
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
                xv_filedepmenu_panel = panel;
                xv_filedepmenu_idx = idx;
                xv_filedepmenu_free = p;
                menu_show (menu, paint, event, NULL);
            }
            return;
        case ACTION_SELECT:
        case ACTION_ADJUST:
            if (event_is_down (event)) {
                if (in) {
                    if (click_info.idx != idx) {
                        click_info.idx = idx;
                        click_info.prev_time.tv_sec = 0;
                        click_info.prev_time.tv_usec = 0;
                    }
                    click_info.btn_down_x = event_x (event);
                    click_info.btn_down_y = event_y (event);
                    if (event_action (event) == ACTION_SELECT) {
                        if (panel->selected != idx) {
                            unselect_item (panel);
                            panel->selected = idx;
                            select_item (panel);
                        }
                    } else {
                        do_file_mark (panel, idx, 
                            panel->dir.list [idx].f.marked ? 0 : 1);
                    }
                    wait_for_btnup = 1;
                    xv_panel_grab = 1;
                } else {
                    if (event_action (event) == ACTION_SELECT) {
                        rubber_adjust = 0;
                    	lastx = -1;
                    	startx = -1;
                    	xv_rubberband_select (panel);
                    } else
                        rubber_adjust = 1;
                    xor_gc = XCreateGC (dpy, xv_get (paint, XV_XID), 0, 0);
                    XSetForeground (dpy, xor_gc, xv_get (canvas, WIN_FOREGROUND_COLOR));
                    XSetFunction (dpy, xor_gc, GXxor);
                    XSetLineAttributes (dpy, xor_gc, 1, LineSolid, CapNotLast, JoinRound);
                    lastx = startx = event_x (event);
                    lasty = starty = event_y (event);
                    wait_for_btnup = 0;
                    xv_panel_grab = 1;
                }
            }
    }
}

static int xv_panel_event_proc (Xv_window paint, Event *event)
{
    Canvas canvas = (Canvas) xv_get (paint, CANVAS_PAINT_CANVAS_WINDOW);
    WPanel *panel;
    
    panel = (WPanel *) xv_get (canvas, XV_KEY_DATA, KEY_DATA_PANEL);
    
    if (event_is_down (event) && event_is_button (event))
        xv_set_current_panel (panel);
    
    switch (event_action (event)) {
    	case ACTION_SELECT:
    	case ACTION_ADJUST:
    	case LOC_DRAG:
    	case ACTION_MENU:
    	case ACTION_HELP:
    	case ACTION_DRAG_PREVIEW:
    	case ACTION_DRAG_COPY:
    	case ACTION_DRAG_MOVE:
    	case ACTION_DRAG_LOAD:
    	    xv_mouse_event (canvas, paint, event);
    	    return 0;
    }
    
    if (event_is_up (event))
    	return 0;
    
    switch (event_action (event)) {
        case KBD_USE:
            xv_set_current_panel (panel);
            break;
    	case ACTION_GO_COLUMN_FORWARD:
    	    moveinpanel (canvas, paint, MIP_DOWN);
    	    break;
    	case ACTION_GO_COLUMN_BACKWARD:
    	    moveinpanel (canvas, paint, MIP_UP);
    	    break;
    	case ACTION_GO_DOCUMENT_START:
    	    moveinpanel (canvas, paint, MIP_HOME);
    	    break;
    	case ACTION_GO_DOCUMENT_END:
    	    moveinpanel (canvas, paint, MIP_END);
    	    break;
    	case ACTION_GO_PAGE_BACKWARD:
    	    moveinpanel (canvas, paint, MIP_PAGEUP);
    	    break;
    	case ACTION_GO_PAGE_FORWARD:
    	    moveinpanel (canvas, paint, MIP_PAGEDOWN);
    	    break;
    	default:
    	    if (event_id (event) == '\n' || event_id (event) == '\r') {
    	        xv_post_proc (midnight_dlg, 
    	            (void (*)(void *))do_enter, (void *) panel);
    	    } else if (event_id (event) == '\t') {
    	        if (((Widget *)cmdline)->wdata != (widget_data) NULL)
    	            xv_set (((Widget *)cmdline)->wdata,
    	                WIN_SET_FOCUS,
    	                NULL);
    	    } else if (event_is_key_left (event) || event_is_key_right (event) ||
        	event_is_key_top (event) || event_is_key_bottom (event)) {
        	int i = 1;
        
        	if (event_is_key_left (event))
            	    i = event_id (event) - KEY_LEFTFIRST + 1;
        	else if (event_is_key_right (event))
            	    i = event_id (event) - KEY_RIGHTFIRST + 1;
        	else if (event_is_key_top (event))
            	    i = event_id (event) - KEY_TOPFIRST + 1;
        	else if (event_is_key_bottom (event))
            	    i = event_id (event) - KEY_BOTTOMFIRST + 1;
		{
		    int j;
		    WButtonBar *bb = 
		        find_buttonbar (midnight_dlg, (Widget *) panel);
		    
		    if (bb != (WButtonBar *) NULL) {
		        if (strcmp (bb->labels [i - 1].text, "PullDn")) {
		            if (bb->labels [i - 1].function)
		                xv_post_proc (midnight_dlg,
		                    *bb->labels [i - 1].function,
		                    bb->labels [i - 1].data);
		        }
		    }
		}            	
    	    }
    }
    
    return 0;
}

char *xv_create_canvas_gc (Canvas canvas)
{
    GC gc = XCreateGC (dpy, 
        (Drawable) xv_get (xv_get (canvas, CANVAS_NTH_PAINT_WINDOW, 0), XV_XID),
        0, 0);
    
    XSetFont (dpy, gc, xv_get (xv_get (canvas, XV_FONT), XV_XID));
    XSetLineAttributes (dpy, gc, 1, LineSolid, JoinRound, CapNotLast);
    XSetGraphicsExposures (dpy, gc, False);
    
    return (char *) gc;
}

void xv_panel_split_proc (Xv_Window origview, Xv_Window newview, int pos)
{
    Rect *rect = (Rect *) xv_get (
        xv_get (origview, XV_KEY_DATA, KEY_DATA_TILE), DROP_SITE_REGION);

    xv_set (newview, 
        XV_KEY_DATA, KEY_DATA_TILE, xv_create (newview, DROP_SITE_ITEM, 
            DROP_SITE_EVENT_MASK, DND_ENTERLEAVE,
            DROP_SITE_REGION, rect,
            NULL),
        NULL);
        
    free (rect);
}
        
int x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel)
{
    Canvas canvas = 
        (Canvas) x_get_parent_object ((Widget *) panel, parent);
    Xv_opaque dropsite_item;
    Xv_window win;
    Rect rect;

    rect.r_left = PANELBORDER;
    rect.r_width = 220;
    rect.r_top = PANELBORDER;
    rect.r_height = 220;
    xv_set (canvas, 
        XV_KEY_DATA, KEY_DATA_PANEL, panel,
        XV_KEY_DATA, KEY_DATA_PANEL_CONTAINER, xv_get (canvas, XV_OWNER),
        CANVAS_PAINTWINDOW_ATTRS,
            WIN_CONSUME_EVENTS,
                KBD_DONE, KBD_USE, WIN_ASCII_EVENTS, WIN_LEFT_KEYS, WIN_TOP_KEYS,
                WIN_RIGHT_KEYS, WIN_META_EVENTS, WIN_EDIT_KEYS, WIN_MOTION_KEYS,
                WIN_TEXT_KEYS, 
                WIN_UP_EVENTS, WIN_MOUSE_BUTTONS, LOC_DRAG,
                ACTION_HELP, ACTION_DRAG_PREVIEW, ACTION_DRAG_LOAD, ACTION_DRAG_COPY,
                ACTION_DRAG_MOVE,
                NULL,
            WIN_EVENT_PROC, xv_panel_event_proc,
            NULL,
        CANVAS_X_PAINT_WINDOW, FALSE,
        CANVAS_RETAINED, FALSE,
        CANVAS_REPAINT_PROC, xv_panel_repaint_proc,
        WIN_CMS, mccms,
        OPENWIN_SPLIT,
            OPENWIN_SPLIT_INIT_PROC, xv_panel_split_proc,
            NULL,
        NULL);
        
    CANVAS_EACH_PAINT_WINDOW (canvas, win)
        xv_set (win,
            XV_KEY_DATA, KEY_DATA_TILE, xv_create (win, DROP_SITE_ITEM, 
                DROP_SITE_EVENT_MASK, DND_ENTERLEAVE,
                DROP_SITE_REGION, &rect,
                NULL),
            NULL);
    CANVAS_END_EACH
    
    panel->widget.wdata = (widget_data) canvas;
    panel->gc = xv_create_canvas_gc (canvas);
    panel->font = (void *) xv_get (canvas, XV_FONT);
    panel->ascent = ((XFontStruct *) xv_get (
        xv_get (canvas, XV_FONT), FONT_INFO))->ascent;
    panel->descent = ((XFontStruct *) xv_get (
        xv_get (canvas, XV_FONT), FONT_INFO))->descent;
    panel->item_height = panel->ascent + panel->descent;
    if (panel->item_height < 16)
        panel->item_height = 16;
    panel->item_height += 2 * PANELITEMBORDER;
    x_fill_panel (panel);
    return 1;
}

void x_panel_set_size (int idx)
{
}

void set_attr (int h, int m)
{
}
