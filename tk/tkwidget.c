/* Widgets for the Midnight Commander, Tk interface code

   Copyright (C) 1994, 1995, 1996 Miguel de Icaza
   Copyright (C) 1995 Jakub Jelinek
   Copyright (C) 1994 Radek Doulik

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

   TODO: Destroy all the command names that are created for each
   widget during runtime.

 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tkmain.h"
#include "tkwidget.h"
#include "dlg.h"

static char
widget_number (Widget *w)
{
    Dlg_head *h = w->parent;
    const int count = h->count;
    Widget_Item *item;
    int i;

    item = h->first;
    for (i = 0; i < count; i++){
	if (item->widget == w)
	    return i;
	item = item->next;
    }
    /* Actually, if we get this far, this message should not appear */
    fprintf (stderr, "Inconsistent widget\n");
    exit (1);
}

/* Returns a new command prefixed with an x:
   x.query.$the_frame.b0

   for the command of the query dialog box button 0

   This allow us to control the widget with the command:
   .query.$the_frame.b0
   
   and to access the C code callback with the:
   x.query.$the_frame.b0
   command.
*/
char *
tk_new_command (widget_data parent, void *widget,
		       tcl_callback callback, char id)
{
    char buffer [60];
    Widget *w = (Widget *) widget;
    Dlg_head *h = w->parent;
    char *cmd;

    /* wdata holds the frame information, it will be replaced with the
     * the widget command name
     */
    if (h->grided && w->tkname)
	sprintf (buffer, ".widgets.%s", w->tkname);
    else {
	char *format;
	
	if (((char *) parent) [1] == 0)
	    format = "%s%c%d";
	else
	    format = ".%s%c%d";
	
	sprintf (buffer, format, w->frame, id, widget_number (w));
    }
    cmd = copy_strings ("x", (char *)parent, buffer, 0);
    w->wdata = (widget_data) cmd;
    if (callback)
	Tcl_CreateCommand (interp, cmd, callback, w, NULL);
    return cmd;
}


/* Button routines */

static int
is_default_button (WButton *b)
{
    return b->action == B_ENTER;
}

/* Called from the Tcl/Tk program when a button has been pressed */
static int
tk_button_callback (ClientData cd, Tcl_Interp *interp, int argc, char *argv [])
{
    WButton *b  = (WButton *) cd;
    Dlg_head *h = (Dlg_head *) b->widget.parent;
    int stop = 0;
    char *cmd = wtcl_cmd (b->widget);
    
    if (b->callback)
	stop = (*b->callback)(b->action, b->callback_data);
    if (!b->callback || stop){
	h->running = 0;
	h->ret_value = b->action;
    }
    return TCL_OK;
}

void
x_button_set (WButton *b, char *text)
{
    char *cmd = wtcl_cmd (b->widget);
    char *postfix = is_default_button (b) ? ".button" : "";
    
    tk_evalf ("%s%s configure -text {%s}", cmd+1, postfix, text);
}

/* Creates the button $parent.$newname and executes $newname command */
int
x_create_button (Dlg_head *h, widget_data parent, WButton *b)
{
    char *cmd;
    
    cmd = tk_new_command (parent, b, tk_button_callback, 'b');
    tk_evalf ("newbutton %s %s {%s} %d", cmd+1, cmd, b->text, is_default_button (b));
    return 1;
}



/* Radio button routines */
static int
tk_radio_callback (ClientData cd, Tcl_Interp *interp, int argc, char *argv [])
{
    WRadio   *r = (WRadio *) cd;

    if (STREQ (argv [1], "select")){
	r->pos = r->sel = atoi (argv [2]);
    }
    (r->widget.callback)(r->widget.parent, r, WIDGET_KEY, ' ');
    return TCL_OK;
}

int
x_create_radio (Dlg_head *h, widget_data parent, WRadio *r)
{
    int i;
    char *cmd;

    cmd = tk_new_command (parent, r, tk_radio_callback, 'r');
    tk_evalf ("newradio %s", cmd+1);
    
    for (i = 0; i < r->count; i++){
	tk_evalf ("radio_item %d {%s} %s %d", i, r->texts [i], cmd,
	    r->sel == i);
    }
    return 1;
}

/* Checkbuttons routines */

static int
tk_check_callback (ClientData cd, Tcl_Interp *interp, int argc, char *argv [])
{
    WCheck   *c = (WCheck *) cd;
    Dlg_head *h = c->widget.parent;

    (*c->widget.callback)(h, c, WIDGET_KEY, ' ');
    return TCL_OK;
}

int
x_create_check (Dlg_head *h, widget_data parent, WCheck *c)
{
    char *cmd;

    cmd = tk_new_command (parent, c, tk_check_callback, 'c');
    tk_evalf ("newcheck %s %s {%s} %d", cmd+1, cmd, c->text, c->state);

    return 1;
}


/* Input line */

int input_event (Gpm_Event *event, WInput *b);

/* This callback command accepts the following commands:
** 
** $win mouse x-position
** $win setmark x-position
*/
static int
tk_input_callback (ClientData cd, Tcl_Interp *interp, int argc, char *av [])
{
    Gpm_Event e;
    WInput *in = (WInput *) cd;
    
    if (strcmp (av [1], "mouse") == 0){
	e.x = atoi (av [2]) + 1;
	e.y = 0;
	e.type = GPM_DOWN;
	input_event (&e, (WInput *) cd);
	return TCL_OK;
    } else if (strcmp (av [1], "setmark") == 0){
	in->mark = in->point;
	return TCL_OK;
    }
    
    return TCL_OK;
}

void
x_update_input (WInput *in)
{
    char *name = wtcl_cmd (in->widget);
    int  p;
    
    if (!name)
	return;

    name++;
    if (in->is_password){
	tk_evalf ("%s delete 0 end", name);
	tk_evalf ("%s insert end {*secret*}", name);
	p = 9;
    } else {
	tk_evalf ("entry_save_sel %s", name);
	if (in->inserted_one && isascii (in->inserted_one) && in->inserted_one > ' '){
	    tk_evalf ("%s insert insert {%c}", name, in->inserted_one);
	    in->inserted_one = 0;
	} else {
	    tk_evalf ("%s delete 0 end", name);
	    tk_evalf ("%s insert end {%s}", name, in->buffer);
	}
	p = in->point;
    }
    tk_evalf ("%s icursor %d; entry_restore_sel %s", name, p, name);
}

int
x_create_input (Dlg_head *h, widget_data parent, WInput *in)
{
    char *cmd;

    cmd = tk_new_command (parent, in, tk_input_callback, 'i');
    tk_evalf ("newinput %s {%s}", cmd+1, in->buffer ? in->buffer: "");
    return 1;
}

void
x_listbox_select_nth (WListbox *l, int nth)
{
    if (!wtcl_cmd (l->widget))
	return;
    
    tk_evalf ("listbox_sel %s %d", wtk_win (l->widget), nth);
}

void
x_listbox_delete_nth (WListbox *l, int nth)
{
    tk_evalf ("%s delete %d", wtk_win (l->widget), nth);
}

int
x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l)
{
    char *cmd;
    int  i;
    WLEntry *e;
    
    cmd = tk_new_command (parent, l, 0, 'x');
    tk_evalf ("listbox %s -width %d -selectmode single", cmd+1, l->widget.cols);

    /* As Jakub said on his very fine xvwidget.c: the user could add some
     * entries to the listbox before the tk-listbox was created, so we have
     * to add it manually :-)
     */
    i = 0;
    if ((e = l->list) == NULL)
	return 1;

    do {
	tk_evalf ("%s insert %d {%s}", cmd+1, i, e->text);
	if (e == l->current)
	    x_listbox_select_nth (l, i);
	e = e->next;
	i++;
    } while (e != l->list);
    return 1;
}

/*
 * 30.10.96 bor: x_list_insert added in reverse order as x_create_listbox
 */
void
x_list_insert (WListbox *l, WLEntry *p, WLEntry *e)
{
    int i;
    char *t = e->text;

    if (wtcl_cmd (l->widget) == NULL)
	return;

    for (i = 0; p != NULL && p != e; p = p->next, i++)
	;
    tk_evalf ("%s insert %d {%s}", wtk_win (l->widget), i, t);
}

int
x_create_label (Dlg_head *g, widget_data parent, WLabel *l)
{
    char *cmd;

    cmd = tk_new_command (parent, l, 0, 'l');
    tk_evalf ("label %s -text {%s}", cmd+1, l->text);
    return 1;
}


void
x_label_set_text (WLabel *label, char *text)
{
    char *lname;

    if (!wtcl_cmd (label->widget))
	return;
    
    tk_evalf ("%s configure -text {%s}", wtk_win (label->widget),
	      text ? text : "");
}


static int
tk_buttonbar_callback (ClientData cd, Tcl_Interp *in, int ac, char *av [])
{
    WButtonBar *bb = (WButtonBar *) cd;
    Dlg_head *h = (Dlg_head *) bb->widget.parent;
    int which = atoi (av [1]);

    (*bb->labels [which].function)(bb->labels [which].data);
    return TCL_OK;
}

int
x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb)
{
    char *cmd;
    int  i;
    
    cmd = tk_new_command (parent, bb, tk_buttonbar_callback, 'n');
    tk_evalf ("frame %s", cmd+1);
    for (i = 0; i < 10; i++){
	tk_evalf ("newbutton %s.%d {%s %d} {%d %s} 0", cmd+1, i, cmd, i, i+1,
		  bb->labels [i].text ? bb->labels [i].text : "");
	tk_evalf ("%s.%d configure -padx 1", cmd+1, i);
	tk_evalf ("pack %s.%d -side left -fill x -expand 1 -ipady 0",
	    cmd+1, i);
    }
    return 1;
}

void
x_redefine_label (WButtonBar *bb, int idx)
{
    if (!bb->widget.wdata)
	return;

    if (!*(char *) bb->widget.wdata)
	return;
    
    if (!bb->labels [idx-1].text)
	return;
    tk_evalf ("%s.%d configure -text {%d %s}",
        wtk_win (bb->widget), idx-1, idx, bb->labels [idx-1].text);
}

/* destructor for the Tk widgets */
void
x_destroy_cmd (void *w)
{
    Widget *widget = (Widget *)w;
    
    if (widget->wdata){
	Tcl_DeleteCommand (interp, wtcl_cmd (*widget));
	tk_evalf ("destroy %s", wtk_win (*widget));
	free (wtcl_cmd (*widget));
    }
}

int
x_find_buttonbar_check (WButtonBar *bb, Widget *paneletc)
{
    return (strcmp ((char *) bb->widget.wcontainer,
		    (char *)paneletc->wcontainer));
}

/* The only parameter accepted is the size of the widget width */
static int
tk_gauge_callback (ClientData cd, Tcl_Interp *in, int ac, char *av [])
{
    WGauge *g = (WGauge *) cd;

    g->pixels = atoi (av [1]);
    return TCL_OK;
}

int
x_create_gauge (Dlg_head *h, widget_data parent, WGauge *g)
{
    char *cmd;

    cmd = tk_new_command (parent, g, tk_gauge_callback, 'g');
    tk_evalf ("newgauge %s", cmd+1);
    return 1;
}

static void
x_gauge_display (WGauge *g)
{
    tk_evalf ("%s coords gauge 0 0 %d 90", wtk_win (g->widget),
	      (int) (g->current*g->pixels)/g->max);
}

void
x_gauge_show (WGauge *g)
{
    tk_evalf ("gauge_%s %s", g->shown ? "shown":"hidden", wtk_win (g->widget));
    if (g->shown)
	x_gauge_display (g);
}

void
x_gauge_set_value (WGauge *g, int max, int current)
{
    if (g->shown)
	x_gauge_display (g);
}
