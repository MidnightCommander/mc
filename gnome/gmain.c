/*
 * Midnight Commander -- GNOME frontend
 *
 * Copyright (C) 1997, 1998 The Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 *
 */

#include <config.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#define WANT_WIDGETS		/* bleah */
#include "tty.h"		/* for KEY_BACKSPACE */
#include "x.h"
#include "main.h"
#include "key.h"
#include "global.h"
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "gcmd.h"
#include "gcorba.h"
#include "gdesktop.h"
#include "gsession.h"
#include "command.h"
#include "cmd.h"

GdkColorContext *mc_cc;

#define MAX_COLOR_PAIRS 40
struct gmc_color_pairs_s gmc_color_pairs [MAX_COLOR_PAIRS];

char *default_edition_colors =
"normal=black:"
"selected=white,darkblue:"
"viewunderline=brightred,blue:"
"directory=blue:"
"markselect=yellow,darkblue:"
"marked=yellow,seagreen:"
"execute=slateblue:"
"link=green:"
"stalledlink=brightred:"
"device=magenta:"
"core=red:"
"menuhotsel=cyan,black:"
"errors=white,red:"
"reverse=black,lightcyan:"
"special=black";

int dialog_panel_callback (struct Dlg_head *h, int id, int msg);

/* The Dlg_head for the whole desktop */
Dlg_head *desktop_dlg;

/* Session client */
GnomeClient *session_client;

/* Used during argument processing */
extern int finish_program;

/* Whether the gmc server was present or not */
int corba_have_server = FALSE;

/* This is only used by the editor, so we provide a dummy implementation */
void
try_alloc_color_pair (char *str, char *str2)
{
}

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
	gmc_color_init ();

	return 0;
}

void
interactive_display (char *filename, char *node)
{
	/* FIXME: Implement gnome version */
}

void
beep (void)
{
	gdk_beep ();
}

void
xtoolkit_end (void)
{
	/* Do nothing */
}

/*
 * Keystroke event handler for all of the Gtk widgets created by
 * the GNOME Midnight Commander.  A special case is handled at
 * the top
 */
int
dialog_key_pressed (GtkWidget *win, GdkEventKey *event, Dlg_head *h)
{
	GtkWidget *w;
	static int on_escape;
	int key;

	/*
	 * Find out if the focused widget is an IconList and
	 * if so, check if it has a currently focused item is
	 * on editing mode as we do not want to handle key
	 * events while the icon name is being edited.
	 */
	w = win;
	while (w && (GTK_IS_CONTAINER (w) && !GNOME_IS_ICON_LIST (w)))
		w = GTK_CONTAINER (w)->focus_child;
	
	if (w && GNOME_IS_ICON_LIST (w)){
		GnomeCanvas *c = GNOME_CANVAS (w);

		if (c->focused_item && GNOME_IS_ICON_TEXT_ITEM (c->focused_item)){
			GnomeIconTextItem *i = GNOME_ICON_TEXT_ITEM (c->focused_item);

			if (i->editing)
				return FALSE;
			
		}
	} 

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
	
	if (dlg_key_event (h, key)){
		gtk_signal_emit_stop_by_name (GTK_OBJECT (win), "key_press_event");
		return TRUE;
	} else
		return FALSE;
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

	if (!(flags & DLG_NO_TOPLEVEL)){
		if (flags & DLG_GNOME_APP)
			win = gnome_app_new ("mc", h->name);
		else {
			win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
			gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);
		}
	} else
		win = 0;
	
	h->grided = flags;
	h->idle_fn_tag = -1;
	if (!(flags & DLG_NO_TED)){
		ted = gtk_ted_new_layout (h->name, LIBDIR "/layout");
		gtk_container_add (GTK_CONTAINER (win), ted);
		gtk_widget_show (ted);
		
		bind_gtk_keys (GTK_WIDGET (ted), h);
	}
	if (win){
		bind_gtk_keys (GTK_WIDGET (win), h);
	}
	return (widget_data) win;
}

/* Used to bind a window for an already created Dlg_head.  This is
 * used together with the DLG_NO_TOPLEVEL: the dialog is created
 * with the DLG_NO_TOPLEVEL and later, when the window is created
 * it is assigned with this routine
 */
void
x_dlg_set_window (Dlg_head *h, GtkWidget *win)
{
	h->wdata = (widget_data) win;
	bind_gtk_keys (GTK_WIDGET (win), h);	
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

static int
gnome_dlg_send_destroy (GtkWidget *widget, GdkEvent *event, Dlg_head *h)
{
	gtk_widget_hide (GTK_WIDGET (h->wdata));
	h->ret_value = B_CANCEL;
	dlg_stop (h);
	return TRUE;
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
		
		if (!ted->need_gui){
			gtk_grab_add (GTK_WIDGET (ted));
			gtk_window_set_policy (GTK_WINDOW (h->wdata), 0, 0, 0);
		}
		gtk_widget_show (GTK_WIDGET (h->wdata));
	}
	gtk_signal_connect (GTK_OBJECT (h->wdata), "delete_event",
			    GTK_SIGNAL_FUNC (gnome_dlg_send_destroy), h);
	x_focus_widget (h->current);
}

/*
 * This function is invoked when the dialog is started to be
 * destroyed, before any widgets have been destroyed.
 *
 * We only hide the toplevel Gtk widget to avoid the flickering
 * of the destruction process
 */
void
x_destroy_dlg_start (Dlg_head *h)
{
	gtk_widget_hide (GTK_WIDGET (h->wdata));
}

/*
 * Called when the Dlg_head has been destroyed.  This only cleans
 * up/releases the frontend resources
 */
void
x_destroy_dlg (Dlg_head *h)
{
	if (!(h->grided & DLG_NO_TED))
		gtk_grab_remove (GTK_WIDGET (GTK_BIN (h->wdata)->child));
	if (h->wdata){
		gtk_widget_destroy (GTK_WIDGET(h->wdata));
		h->wdata = 0;
	}
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

	if (h->send_idle_msg){
		(*h->callback)(h, 0, DLG_IDLE);
		return TRUE;
	} else
		return FALSE;
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
		gnome_idle_handler (h);
	}
}

int
dialog_panel_callback (struct Dlg_head *h, int id, int msg)
{
	WInput *in;
	Widget_Item *dh;
	void *current_widget;	/* The current widget */
	
	if (msg == DLG_KEY && id == '\n'){
		current_widget = (void *) h->current->widget;

		if (is_a_panel (current_widget))
			return 0;

		dh = h->current;
		do {
			if (is_a_panel (dh->widget)){
				WPanel *p = (WPanel *) dh->widget;

				if (current_widget == p->filter_w){
					in = (WInput *) current_widget;
					set_panel_filter_to (p, g_strdup (in->buffer));
					return MSG_HANDLED;
				}
			
				if (current_widget == p->current_dir){
					WInput *in = p->current_dir;
			
					do_panel_cd (p, in->buffer, cd_parse_command);
					assign_text (in, p->cwd);
					update_input (in, 1);
			
					return MSG_HANDLED;
				}
			}
			dh = dh->next;
		} while (dh != h->current);
	}

	if (msg == DLG_UNHANDLED_KEY || msg == DLG_HOTKEY_HANDLED)
		return midnight_callback (h, id, msg);

	return 0;
}

/* Ugly function to support the hack described in non_corba_create_panels() */
static gint
idle_destroy_panel (gpointer data)
{
	WPanel *panel;

	panel = data;
	gnome_close_panel (GTK_WIDGET (panel->widget.wdata), panel);
	return FALSE;
}

/* Initializes the desktop and creates the initial panels.  This is to be used
 * when we do not have a CORBA server.
 */
static void
non_corba_create_panels (void)
{
	WPanel *panel;

	desktop_init ();
	gnome_init_panels ();

	cmdline = command_new (0, 0, 0);
	the_hint = label_new (0, 0, 0, NULL);
	desktop_dlg = create_dlg (0, 0, 24, 80, 0,
				  dialog_panel_callback, "[panel]", "midnight", DLG_NO_TED);

	session_load ();

	/* The following is a hack.  We have to have at least one panel for
	 * run_dlg() to be happy.  So we destroy it in the idle loop if we
	 * didn't need it in the first place.  This idle function is to be run
	 * at a higher priority than the one used in session_load().
	 */

	panel = new_panel_at (".");
	gtk_idle_add_priority (GTK_PRIORITY_DEFAULT, idle_destroy_panel, panel);
	panel->widget.options |= W_PANEL_HIDDEN;

	run_dlg (desktop_dlg);

	desktop_destroy ();
}

/*
 * Only at startup we have a strange condition: if more than one
 * panel is created, then the code hangs inside X, it keeps waiting
 * for a reply for something in Imlib that never returns.
 *
 * Creating the panels on the idle loop takes care of this
 */
void
create_panels (void)
{
	if (!corba_have_server)
		non_corba_create_panels ();
	else {
		if (!nowindows)
			corba_create_window ();

		session_set_restart (FALSE);
	}
}

void
gmc_do_quit (void)
{
	/* FIXME: This wont get us out from a dialog box */
	gtk_main_quit ();
	quit = 1;
	dlg_stop (desktop_dlg);
}
#if 0
static void
session_die (void)
{
	gmc_do_quit ();
}

/*
 * Save the session callback
 */
static int
session_save_state (GnomeClient *client, gint phase, GnomeRestartStyle save_style, gint shutdown,
		    GnomeInteractStyle  interact_style, gint fast, gpointer client_data)
{
	char *sess_id;
	char **argv = g_malloc (sizeof (char *) * ((g_list_length (containers) * 3) + 2));
	GList *l, *free_list = 0;
	int   i;

	sess_id = gnome_client_get_id (client);
	
	argv [0] = client_data;
	for (i = 1, l = containers; l; l = l->next){
		PanelContainer *pc = l->data;
		char *geom;

		geom = gnome_geometry_string (GTK_WIDGET (pc->panel->widget.wdata)->window);
		
		argv [i++] = pc->panel->cwd;
		argv [i++] = "--geometry";
		argv [i++] = geom;
		free_list = g_list_append (free_list, geom);
	}

	/* If no windows were open */
	if (i == 1){
		argv [i++] = "--nowindows";
	}

	argv [i] = NULL;
	gnome_client_set_clone_command (client, i, argv);
	gnome_client_set_restart_command (client, i, argv);
	gnome_config_sync ();

	for (l = free_list; l; l = l->next)
		g_free (l->data);
	g_list_free (free_list);

	g_free (argv);

	if (shutdown){
		quit = 1;
		dlg_stop (midnight_dlg);
	}
	return 1;
}

void
session_management_setup (char *name)
{
	session_client = gnome_master_client ();

	if (session_client){
		gnome_client_set_restart_style (session_client,
						finish_program ? GNOME_RESTART_NEVER : GNOME_RESTART_IMMEDIATELY);
		gtk_signal_connect (GTK_OBJECT (session_client), "save_yourself",
				    GTK_SIGNAL_FUNC (session_save_state), name);
		gtk_signal_connect (GTK_OBJECT (session_client), "die",
				    GTK_SIGNAL_FUNC (session_die), NULL);
	}
}
#endif
/*
 * Configures the GtkWindow/GnomeDialog from a WPanel.
 *
 * This makes the window centered on the screen and binds it to
 * its parent container for better window manager experience
 */
void
gmc_window_setup_from_panel (GnomeDialog *dialog, WPanel *panel)
{
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gnome_dialog_set_parent (dialog, GTK_WINDOW (panel->xwindow));
}

/**
 * gnome_check_super_user:
 * @void: 
 * 
 * Puts out a warning if the user is running gmc as root.  If the user selects
 * Cancel, then gmc will terminate.
 **/
void
gnome_check_super_user (void)
{
	GtkWidget *warning_dlg;
	GnomeClient *client;

	if (geteuid () != 0)
		return;

	warning_dlg = gnome_message_box_new (
		_("You are running the GNOME Midnight Commander as root. \n\n"
		  "You will not be protected from severly damaging your system."),
		GNOME_MESSAGE_BOX_WARNING,
		GNOME_STOCK_BUTTON_OK, NULL);

	gnome_dialog_run (GNOME_DIALOG (warning_dlg));
}
