/* Tk panel stuff.
   Copyright (C) 1995, 1997 Miguel de Icaza
   
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
#include <stdlib.h>		/* atoi */
#include "fs.h"
#include "dir.h"
#include "tkmain.h"
#include "panel.h"
#include "command.h"
#include "panel.h"		/* current_panel */
#include "command.h"		/* cmdline */
#include "main.h"
#include "mouse.h"
#include "layout.h"		/* get_panel_widget */
#include "ext.h"		/* regex_command */
#include "cmd.h"		/* copy_cmd, ren_cmd, delete_cmd, ... */
#include "tkscreen.h"

void do_enter (WPanel *panel);

/* These two variables keep track of the dimensions of a character in the
 * panel listing.
 */
static int panel_font_width;
static int panel_font_height;

static char *attrib;

void
repaint_file (WPanel *panel, int file_index, int move, int attr, int isstatus)
{
}

void
show_dir (WPanel *panel)
{
    char *tkw = wtk_win (panel->widget);
    char *filter = panel->filter ? panel->filter : "*";

    if (!panel->widget.wdata)
	    return;
    
    tk_evalf ("%s.cwd configure -text {%s [%s]}", tkw, panel->cwd, filter);
}

void
x_fill_panel (WPanel *panel)
{
    const int top = panel->count;
    const int panel_width = panel->widget.cols - 2;
    char  *panel_name, *tag;
    char  buffer [255];
    int   i;
    int   selected;
    if (!panel->widget.wcontainer)
	return;

    selected = panel->selected;
    
    panel_name = copy_strings (wtk_win (panel->widget), ".m.p.panel", 0);

    tk_evalf ("%s delete 0.0 end", panel_name);
    for (i = 0; i < top; i++){
	file_entry *fe = &panel->dir.list [i];
	int marked = fe->f.marked;
	
	format_file (buffer, panel, i, panel_width, NORMAL, 0);

	if (i)
	    tk_evalf ("%s insert end \\n", panel_name);

	
	/* Set the mark tag name -- this probably should go into generic code */
	if (marked)
	    tag = "marked";
	else {
	    tag = "regular";
	    if (S_ISLNK (fe->buf.st_mode)){
		if (fe->f.link_to_dir)
		    tag = "directory";
	    } else if (S_ISDIR (fe->buf.st_mode))
		tag = "directory";
	    else if (is_exe (fe->buf.st_mode))
		tag = "executable";
	}
	tk_evalf ("%s insert end {%s} %s", panel_name, buffer, tag);
    }
    if (panel->active)
	x_select_item (panel);
#if 0
    /* The first time, the panel does not yet have a command */
    if (panel->widget.wdata)
	x_select_item (panel);
    free (panel_name);
#endif
}

/* Adjusts a panel size */
static void
tk_panel_set_size (int index, int boot)
{
    Widget *w;
    WPanel *p;
    char *pn;
    int cols, lines;
    
    w = (Widget *) get_panel_widget (index);
    
    tk_evalf ("panel_info cols");
    cols = atoi (interp->result);
    
    tk_evalf ("panel_info lines");
    lines = atoi (interp->result);

    if ((lines < 10) || (cols < 3))
	return;
    
    w = get_panel_widget (index);
    p = (WPanel *) w;
    
    w->cols = cols;
    w->lines = lines;

    set_panel_formats (p);
    paint_panel (p);
    /* FIXME: paint_panel() includes calls to paint_frame() & x_fill_panel(). */
    /* Why does we call them 2 times ? Timur */
    if (!boot)
	    paint_frame (p);
    x_fill_panel (p);
}

/* Called by panel bootstrap routine */
void
x_panel_set_size (int index)
{
    tk_panel_set_size (index, 1);
}

/* Called at runtime when the size of the window changes */
static void
x_change_size (char *panel, int boot)
{
    if (strncmp (panel, ".left", 5) == 0)
	tk_panel_set_size (0, 0);
    else
	tk_panel_set_size (1, 0);
}

#if TCL_MAJOR_VERSION > 7
/* Tcl 8.xx support */
static void
compute_font_size (char *font, char *win, char *dest)
{
    Tk_Window window_id;
    Tk_Font tkfont;
    Tk_FontMetrics fmPtr;
    int width, height;
    
    window_id = Tk_NameToWindow (interp, win, Tk_MainWindow (interp));
    if (window_id == NULL) {
        fprintf (stderr, "Error: %s\n\r", interp->result);
        exit (1);
    }
    tkfont = Tk_GetFont (interp, window_id, font);
    if (tkfont == NULL) {
        fprintf (stderr, 
            "This should not happend: %s\n", interp->result);
        exit (1);
    }
    Tk_GetFontMetrics (tkfont, &fmPtr);

    width = Tk_TextWidth (tkfont, "O", 1);
    height = fmPtr.linespace;
    Tk_FreeFont (tkfont);

    sprintf (dest, "%d %d", height, width);
}

#else

#ifdef HAVE_TKFONT
/* PreTcl 8.xx support */
#define GetFontMetrics(tkfont)    \
                ((CONST TkFontMetrics *) &((TkFont *) (tkfont))->fm)

static void
compute_font_size (char *font, char *win, char *dest)
{
    Tk_Window window_id;
    Tk_Font tkfont;
    const TkFontMetrics *fmPtr;
    int width, height;
    
    window_id = Tk_NameToWindow (interp, win, Tk_MainWindow (interp));
    if (window_id == NULL) {
        fprintf (stderr, "Error: %s\n\r", interp->result);
        exit (1);
    }
    tkfont = Tk_GetFont (interp, window_id, font);
    if (tkfont == NULL) {
        fprintf (stderr, 
            "This should not happend: %s\n", interp->result);
        exit (1);
    }
    fmPtr = GetFontMetrics (tkfont);

    width = fmPtr->maxWidth;
    height = fmPtr->ascent + fmPtr->descent;
    Tk_FreeFont (tkfont);

    sprintf (dest, "%d %d", height, width);
}

#else
static char
compute_font_size (char *font, char *win, char *dest)
{
    Tk_Uid    font_uid;
    Tk_Window window_id;
    XFontStruct *f;
    int width, height;
    
    font_uid = Tk_GetUid (font);
    window_id = Tk_NameToWindow (interp, win, Tk_MainWindow (interp));
    if (window_id == NULL){
	fprintf (stderr, "Error: %s\n\r", interp->result);
	exit (1);
    }
    f = Tk_GetFontStruct (interp, window_id, font_uid);

    if (f == NULL){
	fprintf (stderr, "This should not happend: %s\n", interp->result);
	exit (1);
    }

    width = f->max_bounds.width;
    height = f->ascent + f->descent;

    /* Ok, we will use this dummy until monday, the correct thing
     * to do is extract the Tk font information from Tk's hash
     * table (tkFont.c).
     *
     * TkFont->widths ['0'] holds the width
     */
    sprintf (dest, "%d %d", height, width);
    Tk_FreeFontStruct (f);
}
#endif
#endif

/*
 * This routine is called when the user has chosen an action from the popup 
 * menu. Handles the Copy, Move and Delete commands internally, anything 
 * else ends in the regex_command routine.
 */
static void
tk_invoke (WPanel *panel, int idx, char *operation)
{
    char *filename = panel->dir.list [idx].fname;
    int  movedir;
    
    if (STREQ (operation, "Copy"))
	copy_cmd ();
    else if (STREQ (operation, "Move"))
	ren_cmd ();
    else if (STREQ (operation, "Delete"))
	delete_cmd ();
    else {
	regex_command (filename, operation, NULL, &movedir);
    }
}

static void
tk_add_popup_entry (char *option, char *cmd, int idx)
{
    tk_evalf (".m add command -label {%s} -command {%s invoke %d {%s}}", option, cmd, idx, option);
}

/* This routine assumes the Tcl code has already created the .m menu */
static void
tk_load_popup (WPanel *panel, char *cmd, int idx)
{
    char *filename = panel->dir.list [idx].fname;
    char *p, *q;
    int c;

    tk_evalf ("popup_add_action {%s} %s %d", filename, cmd, idx);
    
    p = regex_command (filename, NULL, NULL, NULL);
    if (!p)
	return;
    tk_evalf (".m add separator");
    for (;;){
	while (*p == ' ' || *p == '\t')
	    p++;
	if (!*p)
	    break;
	q = p;
	while (*q && *q != '=' && *q != '\t')
	    q++;
	c = *q;
	*q = 0;
	tk_add_popup_entry (p, cmd, idx);
	if (!c)
	    break;
	p = q + 1;
    }
}

void
tk_set_sort (WPanel *panel, char *name)
{
    sortfn *f;

    if (!name || (f = get_sort_fn (name)) == NULL)
	return;
    
    if (!name)
	return;
    set_sort_to (panel, f);
}

void
tk_return_drag_text (WPanel *panel)
{
    if (panel->marked){
	sprintf (interp->result, "%d file%s", panel->marked,
		 panel->marked== 1 ? "" : "s");
    } else
	strcpy (interp->result, "1 file");
}

int
tk_panel_callback (ClientData cd, Tcl_Interp *interp, int ac, char *av[])
{
    Gpm_Event e;
    WPanel *panel = (WPanel *) cd;
    char b [20];
    char *p;
    int  mouse_etype = 0;
    
    p = av [1];
    
    if (STREQ (p, "mdown"))
	mouse_etype = GPM_DOWN;
    else if (STREQ (p, "mup"))
	mouse_etype = GPM_UP;
    else if (STREQ (p, "double"))
	mouse_etype = GPM_DOUBLE|GPM_UP;
    else if (STREQ (p,  "motion"))
	mouse_etype = GPM_MOVE|GPM_DRAG;
    else if (STREQ (p, "resize")){
	x_change_size (av [2], 0);
	return TCL_OK;
    } else if (STREQ (p, "fontdim")){
	compute_font_size (av [2], av [3], interp->result);
	return TCL_OK;
    } else if (STREQ (p, "load")){
	tk_load_popup (panel, av [0], atoi (av [2])-1);
	tk_evalf ("tk_popup .m %s %s", av [3], av [4]);
	return TCL_OK;
    } else if (STREQ (p, "invoke")){
	tk_invoke (panel, atoi (av [2]), av [3]);
	return TCL_OK;
    } else if (STREQ (p, "refresh")){
	reread_cmd ();
	return TCL_OK;
    } else if (STREQ (p, "setmask")){
	filter_cmd ();
	return TCL_OK;
    } else if (STREQ (p, "nomask")){
	if (panel->filter){
	    free (panel->filter);
	    panel->filter = 0;
	}
	reread_cmd ();
	return TCL_OK;
    } else if (STREQ (p, "sort")) {
	tk_set_sort (panel, av [2]);
	return TCL_OK;
    } else if (STREQ (p, "reverse")){
	panel->reverse = !panel->reverse;
	do_re_sort (panel);
	return TCL_OK;
    } else if (STREQ (p, "dragtext")){
	tk_return_drag_text (panel);
	return TCL_OK;
    } 

    if (STREQ (p, "top")){
	int old_sel = panel->selected;
	int new = old_sel;
	
	panel->top_file = atoi (av [2]) - 1;
	if (old_sel < panel->top_file)
	    new = panel->top_file;
	if (old_sel > (panel->top_file + ITEMS (panel)))
	    new = panel->top_file + ITEMS (panel) - 1;

	if (new != old_sel){
	    unselect_item (panel);
	    panel->selected = new;
	    select_item (panel);
	}
	return TCL_OK;
    }
    
    if (mouse_etype){
	for (p = av [3]; *p && *p != '.'; p++)
	    ;
	*p++ = 0;
	e.buttons = atoi (av [2]);
    	e.y = atoi (av [3]) + 2 - panel->top_file;
	e.x = atoi (p);
	e.type = mouse_etype;
	panel_event (&e, panel);
    } else {
	/* We are currently dealing with those */
	fprintf (stderr, "Unknown command: %s\n", p);
	return TCL_ERROR;
    }
    return TCL_OK;
}

void
x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel)
{
    char *cmd;
    widget_data container = panel->widget.wcontainer;
    
    cmd = tk_new_command  (container, panel, tk_panel_callback, 'p');
    tk_evalf ("panel_setup %s %s", wtk_win (panel->widget), cmd);
    tk_evalf ("panel_bind %s %s", wtk_win (panel->widget), cmd);
    paint_frame (panel);
    x_fill_panel (panel);
}

/* Called when f.marked has changed on a file */
void
x_panel_select_item (WPanel *panel, int index, int value)
{
    char *s;

    s = value ? "add" : "remove";
    tk_evalf ("panel_mark_entry %s %s %d", wtk_win(panel->widget), s, index+1);
}

void
x_unselect_item (WPanel *panel)
{
    const int selected = panel->selected + 1;

    if (panel->active)
	tk_evalf ("panel_unmark_entry %s %d\n", wtk_win (panel->widget), selected);
}

void
x_select_item (WPanel *panel)
{
    const int selected = panel->selected + 1;
    char *cmd = wtcl_cmd (panel->widget);

    if (!(cmd && *cmd))
	return;

    if (panel->active)
	tk_evalf ("panel_select %s %d %s", wtk_win (panel->widget),
		  selected, cmd);
}

void
x_adjust_top_file (WPanel *panel)
{
    tk_evalf ("%s.m.p.panel yview %d.0", wtk_win (panel->widget),
         panel->top_file+1);
}

void
x_filter_changed (WPanel *panel)
{
    show_dir (panel);
}

/* The following two routines may be called at the very beginning of the
 * program, so we have to check if the widget has already been constructed
 */
static sort_label_last_pos;

void
x_add_sort_label (WPanel *panel, int index, char *text, char *tag, void *sr)
{
    int pos;
    
    if (!panel->format_modified)
	return;

    if (!panel->widget.wdata)
	return;

    /* We may still not have this information */
    if (!panel_font_width){
	tk_evalf ("panel_info heightc");
	panel_font_height = atoi (interp->result);
	
	tk_evalf ("panel_info widthc");
	panel_font_width = atoi (interp->result);
    }
    
    pos = sort_label_last_pos;
    sort_label_last_pos += (strlen (text) * panel_font_width) + (4);

    /* The addition down there is to account for the canvas border width */
    tk_evalf ("panel_add_sort %s %d {%s} %d %d {%s}", wtk_win(panel->widget),
	      strlen (text), text, pos , sort_label_last_pos, *tag ? tag : "Type");
}

void
x_sort_label_start (WPanel *panel)
{
    if (!panel->widget.wdata)
	return;
    sort_label_last_pos = 0;
    tk_evalf ("panel_sort_label_start %s", wtk_win (panel->widget));
}

void
x_reset_sort_labels (WPanel *panel)
{
    if (!panel->widget.wdata)
	return;
    sort_label_last_pos = 0;
    tk_evalf ("panel_reset_sort_labels %s", wtk_win (panel->widget));
}

void
panel_update_cols (Widget *widget, int frame_size)
{
	/* nothing */
}
