/* Midnight Commander Tk initialization and main loop

   Copyright (C) 1995, 1997 Miguel de Icaza
   Copyright (C) 1995 Jakub Jelinek
   
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

/*
 * The Tk version of the code tries to use as much code as possible
 * from the curses version of the code, so we Tk as barely as possible
 * and manage most of the events ourselves.  This may cause some
 * confusion at the beginning.
 */
 
#include <config.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "tkmain.h"
#include "key.h"
#include "global.h"
#include "tty.h"		/* for KEY_BACKSPACE */

/* Tcl interpreter */
Tcl_Interp *interp;

/* if true, it's like mc, otherwise is like mxc */
int use_one_window = 1;

static char *display = NULL;

/* Used to quote the next character, emulates mi_getch () */
static int mi_getch_waiting = 0;
static int mi_getch_value;

static Tk_ArgvInfo arg_table[] = {
{"-display", TK_ARGV_STRING, (char *) NULL, (char *) &display,
             "Display to use"},
{(char *) NULL, TK_ARGV_END, (char *) NULL, (char *) NULL,
             (char *) NULL}
};

static int tkmc_callback (ClientData cd, Tcl_Interp *i, int ac, char *av[]);

char *ALPHA =
"\n\n"
"\n\t\t\t*** IMPORTANT ***\n\n"
"The Tk edition of the Midnight Commander is an ALPHA release of the code.\n\n"
"Many features are missing and in general, not all of the features that\n"
"are available in the text mode version of the program are available on\n"
"this edition.\n\n"
"The internal viewer is incomplete, the built-in editor does not work\n"
"the Tree widget does not work, the Info Widget is incomplete, the setup\n"
"code is incomplete, the keybindings are incomplete, and little testing\n"
"has been done in this code.  ** This is not a finished product **\n\n"
"Feel free to send fixes to mc-devel@nuclecu.unam.mx\n\n\n";


int
xtoolkit_init (int *argc, char *argv[])
{
    interp = Tcl_CreateInterp ();
    if (Tk_ParseArgv (interp, (Tk_Window) NULL, argc, argv, arg_table, 0)
	!= TCL_OK) {
	fprintf(stderr, "%s\n", interp->result);
	exit(1);
    }

    printf (ALPHA);

    COLS = 80;
    LINES = 25;
    /* Pass DISPLAY variable to child procedures */
    if (display != NULL) {
	Tcl_SetVar2 (interp, "env", "DISPLAY", display, TCL_GLOBAL_ONLY);
    }

    Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
    
    Tcl_SetVar (interp, "LIBDIR", mc_home, TCL_GLOBAL_ONLY);
    Tcl_SetVar (interp, "mc_running", "1", TCL_GLOBAL_ONLY);
    
    Tcl_SetVar(interp, "one_window",
	       use_one_window ? "1" : "0", TCL_GLOBAL_ONLY);

    /*
     * Initialize the Tk application.
     */
#ifdef OLD_VERSION
    tkwin = Tk_CreateMainWindow (interp, display, "Midnight Commander","tkmc");
#endif
/* #define HAVE_BLT */
    if (Tcl_Init (interp) != TCL_OK){
	fprintf (stderr, "Tcl_Init: %s\n", interp->result);
	exit (1);
    }
    if (Tk_Init (interp) != TCL_OK){
	fprintf (stderr, "Tk_Init: %s\n", interp->result);
	exit (1);
    }
#ifdef HAVE_BLT
give an error here
    if (Blt_DragDropInit (interp) != TCL_OK){
        fprintf (stderr, "Blt_DragDropInit: %s\n", interp->result);
	exit (1);
    }
    Tcl_SetVar (interp, "have_blt", "1", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar (interp, "have_blt", "0", TCL_GLOBAL_ONLY);
#endif
    
    Tcl_CreateCommand (interp, "tkmc", tkmc_callback, 0, 0);
    {
        char *mc_tcl = concat_dir_and_file (mc_home, "mc.tcl");
	
	if (Tcl_EvalFile (interp, mc_tcl) == TCL_OK){
	    free (mc_tcl);
	    return 0;
	} else {
	    printf ("%s\n", interp->result);
	    exit (1);
	}
    }
}


int
xtoolkit_end (void)
{
    /* Cleanup */
    Tcl_Eval (interp, "exit");
    return 1;
}

void
tk_evalf (char *format, ...)
{
    va_list ap;
    char buffer [1024];

    va_start (ap, format);
    vsprintf (buffer, format, ap);
    if (Tcl_Eval (interp, buffer) != TCL_OK){
	fprintf (stderr, "[%s]: %s\n", buffer, interp->result);
    }
    va_end (ap);
}

int
tk_evalf_val (char *format, ...)
{
    va_list ap;
    char buffer [1024];
    int r;

    va_start (ap, format);
    vsprintf (buffer, format, ap);
    r = Tcl_Eval (interp, buffer);
    va_end (ap);
    return r;
}

widget_data
xtoolkit_create_dialog (Dlg_head *h, int flags)
{
    char *dialog_name = copy_strings (".", h->name, 0);

    tk_evalf ("toplevel %s", dialog_name);
    h->grided = flags & DLG_GRID;
    if (h->grided)
	tk_evalf ("create_gui_canvas %s", dialog_name);

    return (widget_data) dialog_name;
}

void
x_set_dialog_title (Dlg_head *h, char *title)
{
    tk_evalf ("wm title %s {%s}", (char *)(h->wdata), title);
}

widget_data
xtoolkit_get_main_dialog (Dlg_head *h)
{
    return (widget_data) strdup (".");
}

/* Creates the containers */
widget_data
x_create_panel_container (int which)
{
    char *s, *cmd;

    cmd  = use_one_window ? "frame" : "toplevel";
    s   = which ? ".right" : ".left";
    tk_evalf ("%s %s", cmd, s);

    s   = which ? ".right.canvas" : ".left.canvas";
    tk_evalf ("create_container %s", s);
    return (widget_data) s;
}

void
x_panel_container_show (widget_data wdata)
{
}

static int
do_esc_key (int d)
{
    if (isdigit(d))
	return KEY_F(d-'0');
    if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z' )
	|| d == '\n' || d == '\t' || d == XCTRL ('h') || d == '!'
	|| d == KEY_BACKSPACE || d == 127 || d == '\r')
	return ALT(d);
    else {
	return ESC_CHAR;
    }
}

/* This commands has this sintax:
 * [r|a|c] ascii
 *
 * r, a, c, k: regular, alt, control, keysym
 */
 
static int
tkmc_callback (ClientData cd, Tcl_Interp *i, int ac, char *av[])
{
    Dlg_head *h = current_dlg;
    int key;
    static int got_esc;

    key = av [2][0];

    /* Control or alt keys? */
    if (av [1][0] == 'c' || av [1][0] == 'a'){
	if (!key)
	    return TCL_OK;
	
	/* Regular? */
	
	if (!isascii (key)){
	    return TCL_OK;
	} else { 
	    key = tolower (key);
	}
    }

    switch (av [1][0]){
    case 'a':
	key = ALT(key);
	break;

    case 'c':
	key = XCTRL(key);
	break;

    case 'r':
	if (!key)
	    return TCL_OK;
	break;
	
    case 'k':
	if (!(key = lookup_keysym (av [2])))
	    return TCL_OK;
	break;

    case 'e':
	key = ESC_CHAR;
	dlg_key_event (h, key);
	update_cursor (h);
	return TCL_OK;
    }
    
    if (key == '\r')
	key = '\n';

    if (key == ESC_CHAR){
	if (!got_esc){
	    got_esc = 1;
	    return TCL_OK;
	}
	got_esc = 0;
    }

    if (got_esc){
	key = do_esc_key (key);
	got_esc = 0;
    }

    if (mi_getch_waiting && key){
	mi_getch_waiting = 0;
	mi_getch_value = key;
	return TCL_OK;
    }
    dlg_key_event (h, key);
    update_cursor (h);
    return TCL_OK;
}

void
x_focus_widget (Widget_Item *p)
{
    char *wname;

    if (!p->widget)
	return;
    
    wname = (char *) p->widget->wdata;

    if (!wname)
	return;

    tk_evalf ("focus %s", wname+1);
}

/* Setup done before using tkrundlg_event */
void
x_init_dlg (Dlg_head *h)
{
    Widget_Item *h_track;
    char *top_level_name;
    int  grided;
    int i;

    /* Set wlist to hold all the widget names that will be ran */
    h_track = h->current;
    grided = h->grided;
    top_level_name = (char *) h->wdata;

    /* Set up the layout of the dialog box.
     * We support two ways for laying out twidgets on Tk/mc:
     *
     * 1. At widget creation time a unique name is assigned to
     *    each widget.  We create a global list called wlist that
     *    holds the names and invoke a routine that does the layout
     *    using Tk laying out primitives (usually pack).
     *
     * 2. If the widgets were created with supplied names, then
     *    the widgets are intended to be laid out with the grid
     *    manager with a property table contained in the Tcl source
     *    file.
     *
     *    If there are no properties defined for a dialog box, then
     *    a small Tcl-based GUI designed is started up so that the
     *    programmer can define the grided layout for it
     */
    
    if (grided){
	tk_evalf ("set components {}");
	for (i = 0; i < h->count; i++){
	    tk_evalf ("set components \"%s $components\"", h_track->widget->tkname);
	    h_track = h_track->prev;
	}
	tk_evalf ("layout_with_grid %s %d", h->name, h->count);
	if (atoi (interp->result) == 0){
	    tk_evalf ("run_gui_design .%s", h->name);

	    /* Run Tk code */
	    while (1)
		Tcl_DoOneEvent (TCL_ALL_EVENTS);
	}
    } else {
	tk_evalf ("set wlist {}");

	for (i = 0; i < h->count; i++){
	    char *wname = (char *)h_track->widget->wdata;
	    
	    if (wname)
		tk_evalf ("set wlist \"%s $wlist\"", wname+1);
	    h_track = h_track->prev;
	}

	tk_evalf ("layout_%s", h->name);
    }

    /* setup window bindings */
    tk_evalf ("bind_setup %s", top_level_name);

    /* If this is not the main window, center it */
    if (top_level_name [0] && top_level_name [1]){
	tk_evalf ("center_win %s", top_level_name);
    }
}

int
tkrundlg_event (Dlg_head *h)
{
    /* Run the dialog */
    while (h->running){
	if (h->send_idle_msg){
	    if (Tcl_DoOneEvent (TCL_DONT_WAIT))
		if (idle_hook)
		    execute_hooks (idle_hook);
		
	    while (Tcl_DoOneEvent (TCL_DONT_WAIT) &&
		   h->running && h->send_idle_msg){
		(*h->callback) (h, 0, DLG_IDLE);
	    }
	} else 
	    Tcl_DoOneEvent (TCL_ALL_EVENTS); 
    }

    return 1;
}

int
Tcl_AppInit (Tcl_Interp *interp)
{
    return TCL_OK;
}

void
x_interactive_display ()
{
}

int
tk_getch ()
{
    mi_getch_waiting = 1;
    
    while (mi_getch_waiting)
	Tk_DoOneEvent (TK_ALL_EVENTS);
    
    return mi_getch_value;
}

void
tk_dispatch_all (void)
{
    while (Tk_DoOneEvent (TK_DONT_WAIT))
	;
}

void
x_destroy_dlg (Dlg_head *h)
{
    tk_evalf ("destroy %s", (char *) h->wdata);
    tk_dispatch_all ();
    free ((char *) h->wdata);
    tk_end_frame ();		/* Cleanup, in case I forget */
}

void
edition_post_exec (void)
{
    if (iconify_on_exec)
	tk_evalf ("wm deiconify .");
}

void
edition_pre_exec (void)
{
    if (iconify_on_exec)
	tk_evalf ("wm iconify .");
}
