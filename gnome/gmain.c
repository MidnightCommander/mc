/*
 * Midnight Commander -- GNOME frontend
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 *
 */

#include <config.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#define WANT_WIDGETS
#include "x.h"
#include "main.h"
#include "key.h"
#include "global.h"
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "tty.h"		/* for KEY_BACKSPACE */
#include "command.h"

GdkColorContext *mc_cc;

#define MAX_COLOR_PAIRS 32
struct gmc_color_pairs_s gmc_color_pairs [MAX_COLOR_PAIRS];

char *default_edition_colors =
"normal=black:"
"directory=blue:"
"marked=white,seagreen:"
"execute=green:"
"link=yellow:"
"device=magenta:"
"core=red:"
"special=black";

void
init_pair (int index, GdkColor *fore, GdkColor *back)
{
	if (index < 0 || index > MAX_COLOR_PAIRS){
		printf ("init_pair called with invalid index\n");
		exit (1);
	}
	gmc_color_pairs [index].fore = fore;
	gmc_color_pairs [index].back = back;
}

void
get_color (char *cpp, GdkColor **colp)
{
	GdkColor *new_color;
	gint     status;

	new_color = g_new (GdkColor, 1);
	gdk_color_parse (cpp, new_color);
	new_color->pixel = 0;
	status = 0;
	gdk_color_context_get_pixels (mc_cc, &new_color->red, &new_color->green, &new_color->blue, 1,
				      &new_color->pixel, &status);
	*colp = new_color;
}

static void
gmc_color_init (void)
{
	mc_cc = gdk_color_context_new (gtk_widget_get_default_visual (),
				       gtk_widget_get_default_colormap ());
}

int
xtoolkit_init (int *argc, char *argv [])
{
	LINES = 40;
	COLS = 80;
	gnome_init ("gmc", NULL, *argc, argv, 0, NULL);
	gmc_color_init ();
	/* FIXME: Maybe this should return something from gnome_init() */
	return 0;
}

int
xtoolkit_end (void)
{
	return 1;
}

int
dialog_key_pressed (GtkWidget *win, GdkEventKey *event, Dlg_head *h)
{
	static int on_escape;
	int key;

	key = translate_gdk_keysym_to_curses (event);
	if (key == -1)
		return FALSE;

	if (!on_escape){
		if (key == 27){
			on_escape = 1;
			gtk_signal_emit_stop_by_name (GTK_OBJECT (win), "key_press_event");
			return TRUE;
		}
	} else {
		if (key  != 27){
			if (key >= '0' && key <= '9')
				key = KEY_F(key - '0');
			else
				key = ALT (key);

			if (key == ALT('<'))
				key = KEY_HOME;
			if (key == ALT('>'))
				key = KEY_END;
		}
		on_escape = 0;
	}
	
	gtk_signal_emit_stop_by_name (GTK_OBJECT (win), "key_press_event");
	dlg_key_event (h, key);
	return TRUE;
}

void
bind_gtk_keys (GtkWidget *w, Dlg_head *h)
{
	gtk_signal_connect (GTK_OBJECT (w),
			    "key_press_event",
			    GTK_SIGNAL_FUNC (dialog_key_pressed),
			    h);
}

widget_data
xtoolkit_create_dialog (Dlg_head *h, int flags)
{
	GtkWidget *win, *ted;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	h->grided = flags;
	h->idle_fn_tag = -1;
	if (!(flags & DLG_NO_TED)){
		ted = gtk_ted_new_layout (h->name, LIBDIR "/layout");
		gtk_container_add (GTK_CONTAINER (win), ted);
		gtk_widget_show (ted);
		
		bind_gtk_keys (GTK_WIDGET (ted), h);
	}
	bind_gtk_keys (GTK_WIDGET (win), h);
	return (widget_data) win;
}

void
x_set_dialog_title (Dlg_head *h, char *title)
{
	gtk_window_set_title (GTK_WINDOW (h->wdata), title);
}

widget_data
xtoolkit_get_main_dialog (Dlg_head *h)
{
	GtkWidget *win;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	return (widget_data) win; 
}

/* Creates the containers */
widget_data
x_create_panel_container (int which)
{
	GtkWidget *win;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	return (widget_data) 0;
}

void
x_panel_container_show (widget_data wdata)
{
	printf ("x_panel_container_show\n");
	gtk_widget_show (GTK_WIDGET (wdata));
}

void
x_add_widget (Dlg_head *h, Widget_Item *w)
{
	if (!(h->grided & DLG_NO_TED)){
		GtkTed *ted = GTK_TED (GTK_BIN (h->wdata)->child);

		gtk_ted_add (ted, GTK_WIDGET (w->widget->wdata), w->widget->tkname);
		bind_gtk_keys (GTK_WIDGET (w->widget->wdata), h);
	}
}

void
x_init_dlg (Dlg_head *h)
{
	if (!(h->grided & DLG_NO_TED)){
		GtkTed *ted = GTK_TED (GTK_BIN (h->wdata)->child);
		Widget_Item *p, *first;
	
		first = p = h->current;
		do {
			gtk_ted_add (ted, GTK_WIDGET (p->widget->wdata), p->widget->tkname);
			bind_gtk_keys (GTK_WIDGET (p->widget->wdata), h);
			p = p->next;
		} while (p != first);
		gtk_ted_prepare (ted);
		gtk_grab_add (GTK_WIDGET (ted)); 

		gtk_widget_show (GTK_WIDGET (h->wdata));
	}
	x_focus_widget (h->current);
}


void
x_destroy_dlg (Dlg_head *h)
{
	if (!(h->grided & DLG_NO_TED))
		gtk_grab_remove (GTK_WIDGET (GTK_BIN (h->wdata)->child));
	gtk_widget_destroy (GTK_WIDGET(h->wdata));
}

void
gtkrundlg_event (Dlg_head *h)
{
	gtk_main ();
}

void
edition_pre_exec ()
{
}

void
edition_post_exec ()
{
}

void
done_screen ()
{
}

void
setup_sigwinch ()
{
}

int
dialog_panel_callback (struct Dlg_head *h, int id, int msg)
{
	return default_dlg_callback (h, id, msg);
}

void
x_flush_events (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static int
gnome_idle_handler (gpointer data)
{
	Dlg_head *h = data;

	(*h->callback)(h, 0, DLG_IDLE);
	return TRUE;
}

/* Turn on and off the idle message sending */
void
x_set_idle (Dlg_head *h, int enable_idle)
{
	if (enable_idle){
		if (h->idle_fn_tag != -1)
			return;
		h->idle_fn_tag = gtk_idle_add (gnome_idle_handler, h);
	} else {
		if (h->idle_fn_tag == -1)
			return;
		gtk_idle_remove (h->idle_fn_tag);
		h->idle_fn_tag = -1;
	}
}

void
create_panels (void)
{
	Dlg_head *h;
	WPanel *panel;
	
	start_desktop ();
	cmdline = command_new (0, 0, 0);
	the_hint = label_new (0, 0, 0, NULL);

	gnome_init_panels ();
	
	h = create_dlg (0, 0, 24, 80, 0, dialog_panel_callback, "[panel]", "midnight", DLG_NO_TED);
	
	panel = create_container (h, "My Panel");
	add_widget (h, panel);
	set_current_panel (0);
	run_dlg (h);
}

