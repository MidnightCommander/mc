/* Tk Viewer stuff.
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   -----

   This module is here so that I can learn to shut up my mouth.
   I know, Jakub is enjoying this moment, anyways, this code is still
   smaller than a complete tkview.c program, and it's just interface
   code so that the code knows about the window resize.
*/

#include <stdio.h>

#define WANT_WIDGETS
#include "dlg.h"
#include "view.h"
#include "tkmain.h"

x_init_view (WView *view)
{
    view->status_shown = 0;
    view->current_line = 1;
    view->cache_len = 80;
    view->last_col = 0;
    view->cache = xmalloc (81, "view->cache");
    view->color_cache = xmalloc (81, "view->cache");
    view->direction = 1;
    bzero (view->cache, 81);
    view->dest = 0;
}

void
x_destroy_view (WView *view)
{
    free (view->cache);
    free (view->color_cache);
}


/* Accepts the dim command with the width and the height in characters */
static int
tk_viewer_callback (ClientData cd, Tcl_Interp *interp, int ac, char *av[])
{
    WView *view = (WView *) cd;

    if (av [1][0] != 'd')
	return TCL_OK;
    view->widget.cols  = atoi (av [2]);
    view->widget.lines = atoi (av [3]);
    return TCL_OK;
}

void
x_create_viewer (WView *view)
{
    char *cmd;
    widget_data parent;
    
    /* First, check if our parent is ".", if this is the case, then
     * create a stand alone viewer, otherwise, we create a paneled
     * version of the viewer
     */
    
    if (view->have_frame){
	parent = view->widget.wcontainer;
    } else {
	parent = view->widget.parent->wdata;
    }
    cmd = tk_new_command (parent, view, tk_viewer_callback, 'v');
    
    tk_evalf ("newview %d %s %s %s", view->have_frame,
	      view->have_frame ? (char *) view->widget.wcontainer : "{}",
	      cmd+1, cmd);
}

void
x_focus_view (WView *view)
{
	tk_evalf ("focus %s.v.view", wtk_win (view->widget));
}

void
view_status (WView *view)
{
    char *window = wtk_win (view->widget);
    
    if (!view->status_shown){
	view->status_shown = 0;

	tk_evalf ("view_update_info %s {%s} {%d} {%s} {%s}",
		  window, name_trunc (view->filename ? view->filename:
				      view->command ? view->command:"", 20),
		  -view->start_col, size_trunc (view->s.st_size),
		  view->growing_buffer ? "[grow]":"[]");
    }
}

void
view_percent (WView *view, int p, int w)
{
    fprintf (stderr, "Missing tk view_percent\n");
}

/* The major part of the Tk code deals with caching a line (the
 * current one) of text to avoid expensive calls to the Tk text widget
 * callback.
 *
 * We cache all this information on view->cache and the colors on
 * view->color_cache.
 *
 * FIXME: the cache does not know about the contents of the physical
 * text widget (depends on your concept of physical), so if we happen
 * to hit the case where row is decremented in the void display () routine,
 * we will end up with a clean line.
 */

static int current_color;

void
view_set_color (WView *view, int font)
{
    current_color = font;
}

void
view_display_clean(WView *view, int h, int w)
{
    char *win = wtk_win (view->widget);

    tk_evalf ("cleanview %s.v.view", win);
}

void
view_add_character (WView *view, int c)
{
    view->cache [view->dest] = c;
    view->color_cache [view->dest] = current_color;
}

void
view_add_string (WView *view, char *s)
{
    while (*s)
	view_add_character (view, *s++);
}

static char *
get_tk_tag_name (int color)
{
    /* Those names are the names of the tags in the Tk source */
    static char *color_tag_names [] = {
	"normal", "bold", "underline", "mark"
    };

    return color_tag_names [color];
}

/*
 * Tk: Flushes the contents of view->cache to the Tk text widget
 *
 * We get the command information and call the command directly
 * to avoid escaping the view->cache contents.
 */
static void
flush_line (WView *view)
{
    char *win = wtk_win (view->widget);
    int  row = view->current_line;
    char *text_name;
    Tcl_CmdInfo info;
    int  i, prev_color;
    char str_row [30];
    char *av [5];
    int  len = strlen (view->cache);

    /* Fetch the address and clientData for the view */
    text_name = copy_strings (win, ".v.view", 0);
    Tcl_GetCommandInfo (interp, text_name, &info);

    /* Setup arguments to the command:
     * $win.v.view insert @$row,0 $view->cache
     */
    sprintf (str_row, "%d.0", row);
    i = 0;
    av [i++] = text_name;
    av [i++] = "insert";
    av [i++] = str_row;
    av [i++] = view->cache;

    /* Call the callback :-) */
    (*info.proc) (info.clientData, interp, i, av);
    bzero (view->cache, view->cache_len);

    /* Colorize the line */
    for (prev_color = 0, i = 0; i < len; i++){
	int new_color_start;
	char *color_name;
	    
	if (view->color_cache [i] == prev_color)
	    continue;

	new_color_start = i;
	
	prev_color = view->color_cache [i];
	
	for (;i < len && view->color_cache [i] == prev_color; i++)
	    ;
	
	color_name = get_tk_tag_name (prev_color);
	tk_evalf ("%s tag add %s %d.%d %d.%d",
		  text_name, color_name,
		  row, new_color_start-1,
		  row, i);
    }
    
    bzero (view->color_cache, view->cache_len);
    view->last_col = 0;
    free (text_name);
}

/* Tk: Mantains the line cache */
void
view_gotoyx (WView *view, int row, int col)
{
    if (row != view->current_line){
	flush_line (view);
    }
    view->current_line = row;

    /* In case the user has resized the viewer */
    if (col > view->cache_len){
	char *new;

	new = xmalloc (col + 1, "cache");
	strcpy (new, view->cache);
	free (view->cache);
	view->cache = new;

	new = xmalloc (col + 1, "cache");
	strcpy (new, view->color_cache);
	free (view->color_cache);
	view->color_cache = new;
	
	view->cache_len = col;
    }

    view->dest = col;
    for (; view->last_col+1 < col; view->last_col++){
	view->cache [view->last_col] = ' ';
	view->color_cache [view->last_col] = current_color;
    }
    view->last_col = col;
}

