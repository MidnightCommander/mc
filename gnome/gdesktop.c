/*
 * Controls the desktop contents
 * (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include "fs.h"
#include <gnome.h>
#include "util.h"
#include "gdesktop.h"
#include "../vfs/vfs.h"
#include <string.h>
#include "main.h"
#include "file.h"
#include "global.h"
#include "panel.h"
#include "gscreen.h"
#include "ext.h"
#include "dialog.h"
#include "gpageprop.h"
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

/* operations on drops */
enum {
	OPER_COPY,
	OPER_MOVE,
	OPER_LINK
};

/* The X11 root window */
static GnomeRootWin *root_window;

/* The full name of the desktop directory ~/desktop */
char *desktop_directory;

static void desktop_reload (char *desktop_dir);
static void desktop_icon_context_popup (GdkEventButton *event, desktop_icon_t *di);

/* The list with the filenames we have actually loaded */
static GList *desktop_icons;

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

/*
 * If the dentry is zero, then no information from the on-disk .desktop file is used
 * In this case, we probably will have to store the geometry for a file somewhere
 * else.
 */
static void
desktop_icon_set_position (desktop_icon_t *di)
{
	static int x, y = 10;
	static int current_x, current_y;
	
	x = -1;
	if (di->dentry && di->dentry->geometry){
		char *comma = strchr (di->dentry->geometry, ',');

		if (comma){
			x = atoi (di->dentry->geometry);
			comma++;
			y = atoi (comma);
		}
	}

	/* This find-spot routine can obviously be improved, left as an excercise
	 * to the hacker
	 */
	if (x == -1){
		x = current_x;
		y = current_y;

		gtk_widget_size_request (di->widget, &di->widget->requisition);
		current_y += di->widget->requisition.height + 8;
		if (current_y > gdk_screen_height ()){
			current_x += 0;
			current_y = 0;
		}
	}
	di->x = x;
	di->y = y;
	gtk_widget_set_uposition (di->widget, 6 + x, y);
}

/*
 * Returns the icon associated with the given file name, or app.xpm
 * if no icon is defined for this application
 */
static char *
get_desktop_icon (char *pathname)
{
	char *fname, *full_fname;

	fname = regex_command (x_basename (pathname), "Icon", 0, 0);

	/* Try the GNOME icon */
	if (fname){
		full_fname = gnome_unconditional_pixmap_file (fname);
		if (exist_file (full_fname))
			return full_fname;
		g_free (full_fname);
	}

	/* Try a mc icon */
	if (fname){
		full_fname = concat_dir_and_file (ICONDIR, fname);
		if (exist_file (full_fname))
			return full_fname;

		free (full_fname);
	}
	
	return gnome_unconditional_pixmap_file ("launcher-program.xpm");
}

/*
 * Hackisigh routine taken from GDK
 */
static void
gdk_dnd_drag_begin (GdkWindow *initial_window)
{
	GdkEventDragBegin tev;
	tev.type = GDK_DRAG_BEGIN;
	tev.window = initial_window;
	tev.u.allflags = 0;
	tev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
	
	gdk_event_put ((GdkEvent *) &tev);
}

void
artificial_drag_start (GdkWindow *window, int x, int y)
{
	GdkWindowPrivate *wp = (GdkWindowPrivate *) window;

	if (!wp->dnd_drag_enabled)
		return;
	if (!gdk_dnd.drag_perhaps)
		return;
	if (gdk_dnd.dnd_grabbed)
		return;
	if (gdk_dnd.drag_really)
		return;
	
	gdk_dnd_drag_addwindow (window);
	gdk_dnd_drag_begin (window);
	XGrabPointer (gdk_display, wp->xwindow, False,
		      ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
		      GrabModeAsync, GrabModeAsync, gdk_root_window,
		      None, CurrentTime);
	gdk_dnd.dnd_grabbed = TRUE;
	gdk_dnd.drag_perhaps = 1;
	gdk_dnd.drag_really = 1;
	gdk_dnd_display_drag_cursor (x, y, FALSE, TRUE);
}

static int operation_value;

static void
set_option (GtkWidget *widget, int value)
{
	operation_value = value;
	gtk_main_quit ();
}

static void
option_menu_gone ()
{
	operation_value = -1;
	gtk_main_quit ();
}

static int
get_operation (guint32 timestamp, int x, int y)
{
	static GtkWidget *menu;
	
	if (!menu){
		GtkWidget *item;
		
		menu = gtk_menu_new ();

		item = gtk_menu_item_new_with_label (_("Copy"));
		gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC(set_option), (void *) OPER_COPY);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		
		item = gtk_menu_item_new_with_label (_("Move"));
		gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC(set_option), (void *) OPER_MOVE);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		/* Not yet implemented the Link bits, so better to not show what we dont have */
		item = gtk_menu_item_new_with_label (_("Link"));
		gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC(set_option), (void *) OPER_LINK);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		gtk_signal_connect (GTK_OBJECT (menu), "hide", GTK_SIGNAL_FUNC(option_menu_gone), 0);
	} 

	/* Here, we could set the mask parameter (the last NULL) to a valid variable
	 * and find out if the shift/control keys were set and do something smart
	 * about that
	 */
	
	gtk_widget_set_uposition (menu, x, y);

	/* FIXME: We should catch any events that escape this menu and cancel it */
	operation_value = -1;
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 0, NULL, 1, timestamp);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);
	gtk_widget_hide (menu);
	
	return operation_value;
}

/* Used by check_window_id_in_one_panel and find_panel_owning_window_id for finding
 * the panel that contains the specified window id (used to figure where the drag
 * started)
 */
static WPanel *temp_panel;

static void
check_window_id_in_one_panel (gpointer data, gpointer user_data)
{
	PanelContainer *pc    = (PanelContainer *) data;
	int id                = (int) user_data;
	WPanel *panel         = pc->panel;
	GtkCList *clist       = GTK_CLIST (panel->list);
	GdkWindowPrivate  *gdk_wp;

	gdk_wp = (GdkWindowPrivate *) clist->clist_window;
	if (gdk_wp->xwindow == id){
		temp_panel = panel;
		return;
	}

	gdk_wp = (GdkWindowPrivate *) GTK_WIDGET (clist)->window;
	
	if (gdk_wp->xwindow == id){
		temp_panel = panel;
		return;
	}
}

static WPanel *
find_panel_owning_window_id (int id)
{
	temp_panel = NULL;
	g_list_foreach (containers, check_window_id_in_one_panel, (gpointer) id);
	return temp_panel;
}

static void
perform_drop_on_directory (WPanel *source_panel, int operation, char *dest)
{
	switch (operation){
	case OPER_COPY:
		panel_operate (source_panel, OP_COPY, dest);
		break;
		
	case OPER_MOVE:
		panel_operate (source_panel, OP_MOVE, dest);
		break;
	}
}

static void
perform_drop_manually (int operation, GdkEventDropDataAvailable *event, char *dest)
{
	int count = event->data_numbytes;
	char *p   = event->data;
	int len;

	switch (operation){
	case OPER_COPY:
		create_op_win (OP_COPY, 0);
		break;
		
	case OPER_MOVE:
		create_op_win (OP_MOVE, 0);
		break;
	}
	file_mask_defaults ();
	
	do {
		char *tmpf;
		
		len = 1 + strlen (event->data);
		count -= len;
		
		switch (operation){
		case OPER_COPY:
			tmpf = concat_dir_and_file (dest, x_basename (p));
			copy_file_file (p, tmpf, 1);
			free (tmpf);
			break;
			
		case OPER_MOVE:
			create_op_win (OP_MOVE, 0);
			file_mask_defaults ();
			tmpf = concat_dir_and_file (dest, x_basename (p));
			move_file_file (p, tmpf);
			free (tmpf);
			break;
		}
		p += len;
	} while (count > 0);
	
	destroy_op_win ();
}

static void
do_symlinks (GdkEventDropDataAvailable *event, char *dest)
{
	int count = event->data_numbytes;
	char *p = event->data;
	int len;

	do {
		char *full_dest_name;
		len = 1 + strlen (event->data);
		count -= len;

		full_dest_name = concat_dir_and_file (dest, x_basename (p));
		mc_symlink (p, full_dest_name);
		free (full_dest_name);
		p += len;
	} while (count > 0);
}

void
drop_on_directory (GdkEventDropDataAvailable *event, char *dest, int force_manually)
{
	WPanel *source_panel;
	int operation;
	
	operation = get_operation (event->timestamp, event->coords.x, event->coords.y);

	if (operation == -1)
		return;
	
	/* Optimization: if we are dragging from the same process, we can
	 * display a nicer status bar.
	 */
	source_panel = find_panel_owning_window_id (event->requestor);

	/* Symlinks do not use any panel/file.c optimization */
	if (operation == OPER_LINK){
		do_symlinks (event, dest);
		return;
	}
	
	if (source_panel  && !force_manually){
		perform_drop_on_directory (source_panel, operation, dest);
		update_one_panel_widget (source_panel, 0, UP_KEEPSEL);
		panel_update_contents (source_panel);
	} else
		perform_drop_manually (operation, event, dest);
	return;
}

static void
url_dropped (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	char *p;
	int count;
	int len;
	int is_directory = 0;

	/* if DI is set to zero, then it is a drop on the root window */
	if (di)
		is_directory = strcasecmp (di->dentry->type, "directory") == 0;
	else {
		drop_on_directory (event, desktop_directory, 1);
		desktop_reload (desktop_directory);
		return;
	}
	
	if (is_directory){
		drop_on_directory (event, di->dentry->exec, 0);
		return;
	}

	printf ("Arguments to non-directory (FIXME: needs to be implemented):\n");
	count = event->data_numbytes;
	p = event->data;
	do {
		len = 1 + strlen (event->data);
		count -= len;
		printf ("[%s], ", p);
		p += len;
	} while (count);
	printf ("\nReceiving: %s %d\n", (char *) event->data, (int) event->data_numbytes);
}

static void
drop_cb (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	if (strcmp (event->data_type, "icon/root") == 0){
		printf ("ICON DROPPED ON ROOT!\n");
	} else
		url_dropped (widget, event, di);
}

static void
connect_drop_signals (GtkWidget *widget, desktop_icon_t *di)
{
	GtkObject *o = GTK_OBJECT (widget);
	
	gtk_signal_connect (o, "drop_enter_event", GTK_SIGNAL_FUNC (gtk_true), di);
	gtk_signal_connect (o, "drop_leave_event", GTK_SIGNAL_FUNC (gtk_true), di);
	gtk_signal_connect (o, "drop_data_available_event", GTK_SIGNAL_FUNC (drop_cb), di);
}

static void
dentry_execute (desktop_icon_t *di)
{
	GnomeDesktopEntry *dentry = di->dentry;

	/* Ultra lame-o execute.  This should be replaced by the fixed regexp_command
	 * invocation 
	 */

	if (strcmp (di->dentry->type, "Directory") == 0){
		new_panel_at (di->dentry->exec);
	} else 
		gnome_desktop_entry_launch (dentry);
}

static void
start_icon_drag (GtkWidget *wi, GdkEventMotion *event)
{
	printf ("MOTION NOTIF!\n");
	artificial_drag_start (wi->window, event->x, event->y);
}

GdkPoint root_icon_drag_hotspot = { 15, 15 };

static void
desktop_icon_drag_request (GtkWidget *widget, GdkEventDragRequest *event, desktop_icon_t *di)
{
	printf ("Drag type: %s\n", event->data_type);
	
	if (strcmp (event->data_type, "url:ALL") == 0){
		gdk_window_dnd_data_set (widget->window, (GdkEvent *)event, di->pathname, strlen (di->pathname) + 1);
	} else {
		int drop_x, drop_y;

		drop_x = event->drop_coords.x - root_icon_drag_hotspot.x;
		drop_y = event->drop_coords.y - root_icon_drag_hotspot.y;
		
		/* Icon dropped on root.  We take care of it */
		printf ("Dropped at %d %d\n", drop_x, drop_y);
		gtk_widget_set_uposition (di->widget, drop_x, drop_y);
		if (di->dentry){
			char buffer [40];

			sprintf (buffer, "%d,%d", drop_x, drop_y);
			if (di->dentry->geometry)
				g_free (di->dentry->geometry);
			di->dentry->geometry = g_strdup (buffer);
			gnome_desktop_entry_save (di->dentry);
		}
	}
}

static GtkWidget *root_drag_ok_window;
static GtkWidget *root_drag_not_ok_window;

static void
destroy_shaped_dnd_windows (void)
{
	if (root_drag_not_ok_window){
		gtk_widget_destroy (root_drag_not_ok_window);
		root_drag_not_ok_window = 0;
	}
	
	if (root_drag_ok_window){
		gtk_widget_destroy (root_drag_ok_window);
		root_drag_ok_window = 0;
	}
}

/* As Elliot can not be bothered to fix his DnD code in Gdk and it is an absolute mess */
static int in_desktop_dnd;

static void
desktop_icon_drag_start (GtkWidget *widget, GdkEvent *event, desktop_icon_t *di)
{
	char *fname;

	if (in_desktop_dnd)
		return;
	
	in_desktop_dnd = 1;
	/* This should not happen, as the drag end routine should destroy those widgets */
	destroy_shaped_dnd_windows ();

	if (di->dentry)
		fname = strdup (di->dentry->icon);
	else
		fname = get_desktop_icon (di->pathname);

	if (fname){
		/* FIXME: we are using the same icon for ok and not ok drags */
		root_drag_ok_window     = make_transparent_window (fname);
		root_drag_not_ok_window = make_transparent_window (fname);

		gdk_dnd_set_drag_shape (root_drag_ok_window->window, &root_icon_drag_hotspot,
					root_drag_not_ok_window->window, &root_icon_drag_hotspot);
		gtk_widget_show (root_drag_not_ok_window);
		gtk_widget_show (root_drag_ok_window);
		free (fname);
	}
}

static void
desktop_icon_drag_end (GtkWidget *widget, GdkEvent *event, desktop_icon_t *di)
{
	in_desktop_dnd = 0;
	printf ("drag end!\n");
	destroy_shaped_dnd_windows ();
}

/*
 * Bind the signals so that we can make this icon draggable
 */
static void
desktop_icon_make_draggable (desktop_icon_t *di)
{
	GtkObject *obj = GTK_OBJECT (di->widget);
	char *drag_types [] = { "icon/root", "url:ALL" };

	/* To artificially start up drag and drop */
/*	gtk_signal_connect (obj, "motion_notify_event", GTK_SIGNAL_FUNC (start_icon_drag), di); */
	gtk_widget_dnd_drag_set (di->widget, TRUE, drag_types, ELEMENTS (drag_types));

	gtk_signal_connect (obj, "drag_request_event", GTK_SIGNAL_FUNC (desktop_icon_drag_request), di);
	gtk_signal_connect (obj, "drag_begin_event", GTK_SIGNAL_FUNC (desktop_icon_drag_start), di);
	gtk_signal_connect (obj, "drag_end_event", GTK_SIGNAL_FUNC (desktop_icon_drag_end), di);
}

/*
 * destroys a desktop_icon_t structure and anything that was held there,
 * including the desktop widget. 
 */
static void
desktop_release_desktop_icon_t (desktop_icon_t *di)
{
	if (di->dentry){
		gnome_desktop_entry_free (di->dentry);
	} else {
		free (di->pathname);
		di->pathname = 0;
	}

	if (di->widget){
		gtk_widget_destroy (di->widget);
		di->widget = 0;
	}
	free (di);
}

/*
 * Removes an icon from the desktop and kills the ~/desktop file associated with it
 */
static void
desktop_icon_remove (desktop_icon_t *di)
{
	desktop_icons = g_list_remove (desktop_icons, di);

	mc_unlink (di->pathname);
	desktop_release_desktop_icon_t (di);
}

/* Called by the pop up menu: removes the icon from the desktop */
static void
icon_delete (GtkWidget *widget, desktop_icon_t *di)
{
	desktop_icon_remove (di);
}

GtkWidget *
my_create_transparent_text_window (char *file, char *text)
{
	GtkWidget *w;
	int events = GDK_BUTTON_PRESS_MASK | GDK_BUTTON1_MOTION_MASK;
	
	w = create_transparent_text_window (file, text, events);
	if (!w){
		static char *default_pix;

		if (!default_pix){
			default_pix = gnome_unconditional_pixmap_file ("launcher-program.xpm");
		}
		w = create_transparent_text_window (default_pix, text, events);
		if (!w)
			return NULL;
	}
	return w;
}

static GtkWidget *
get_transparent_window_for_dentry (GnomeDesktopEntry *dentry)
{
	GtkWidget *window;
	char *icon_label;

	icon_label = dentry->name ? dentry->name : x_basename (dentry->exec);

	if (dentry->icon)
		window = my_create_transparent_text_window (dentry->icon, icon_label);
	else {
		static char *default_icon_path;
		static char exists;
		
		if (!default_icon_path) {
			default_icon_path = gnome_unconditional_pixmap_file ("launcher-program.xpm");
			if (g_file_exists (default_icon_path))
				exists = 1;
		}

		if (exists)
			window = my_create_transparent_text_window (default_icon_path, icon_label);
		else {
			window = gtk_window_new (GTK_WINDOW_POPUP);
			gtk_widget_set_usize (window, 20, 20);
		}
	}

	return window;
}

static int
dentry_button_click (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (event->button == 1){
		if (event->type == GDK_2BUTTON_PRESS)
			dentry_execute (di);

		return TRUE;
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3){
		desktop_icon_context_popup (event, di);
		return TRUE;
	}
	return FALSE;
}
		
char *drop_types [] = {
	"text/plain",
	"url:ALL",
};

static void
post_setup_desktop_icon (desktop_icon_t *di)
{
	desktop_icon_make_draggable (di);

	/* Setup the widget to make it useful: */

	/* 1. Drag and drop functionality */
	connect_drop_signals (di->widget, di);
	gtk_widget_dnd_drop_set (di->widget, TRUE, drop_types, ELEMENTS (drop_types), FALSE);

	/* 2. Double clicking executes the command */
	gtk_signal_connect (GTK_OBJECT (di->widget), "button_press_event", GTK_SIGNAL_FUNC (dentry_button_click), di);

	gtk_widget_show (di->widget);
}

/* Pops up the icon properties pages */
static void
icon_properties (GtkWidget *widget, desktop_icon_t *di)
{
	int retval;

	retval = item_properties (di->widget, di->pathname, di);

	if (retval & (GPROP_TITLE | GPROP_ICON)) {
		gtk_widget_destroy (di->widget);

		di->widget = get_transparent_window_for_dentry (di->dentry);

		post_setup_desktop_icon (di);
		gnome_desktop_entry_save (di->dentry);
	}
}

/*
 * Activates the context sensitive menu for this icon
 */
static void
desktop_icon_context_popup (GdkEventButton *event, desktop_icon_t *di)
{
	static GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();

	/* We connect_object_after to the items so that we can destroy
         * the menu at the proper time.
	 */

	item = gtk_menu_item_new_with_label (_("Properties"));
	gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC (icon_properties), di);
	gtk_signal_connect_object_after (GTK_OBJECT (item), "activate",
					 GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (menu));
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Delete"));
	gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC (icon_delete), di);
	gtk_signal_connect_object_after (GTK_OBJECT (item), "activate",
					 GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (menu));
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_widget_show (item);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 0, NULL, 3, event->time);
}

char *root_drop_types [] = {
	"icon/root",
	"url:ALL"
};

static void
desktop_load_from_dentry (GnomeDesktopEntry *dentry)
{
	GtkWidget *window;
	desktop_icon_t *di;

	window = get_transparent_window_for_dentry (dentry);

	if (!window)
		return;

	di = xmalloc (sizeof (desktop_icon_t), "desktop_load_entry");
	di->dentry   = dentry;
	di->widget   = window;
	di->pathname = dentry->location;

	desktop_icons = g_list_prepend (desktop_icons, di);

	post_setup_desktop_icon (di);
	desktop_icon_set_position (di);
}

/*
 * Loads a .desktop file from FILENAME for the desktop.
 */
static void
desktop_load_dentry (char *filename)
{
	GnomeDesktopEntry  *dentry;
	
	dentry = gnome_desktop_entry_load (filename);

	if (!dentry)
		return;

	desktop_load_from_dentry (dentry);
}

/*
 * Creates a new DIRECTORY/.directory file which is just a .dekstop
 * on directories.   And then loads it into the desktop
 */
static void
desktop_create_directory_entry (char *dentry_path, char *pathname, char *short_name)
{
	GnomeDesktopEntry *dentry;

	dentry = xmalloc (sizeof (GnomeDesktopEntry), "dcde");
	dentry->name     = g_strdup (short_name);
	dentry->comment  = NULL;
	dentry->tryexec  = NULL;
	dentry->exec     = g_strdup (pathname);
	dentry->icon     = gnome_unconditional_pixmap_file ("gnome-folder.png");
	dentry->docpath  = NULL;
	dentry->type     = g_strdup ("Directory");
	dentry->location = g_strdup (dentry_path);
	dentry->geometry = NULL;
	
	gnome_desktop_entry_save (dentry);
	desktop_load_from_dentry (dentry);
}

static int
file_is_executable (char *path)
{
	struct stat s;

	if (mc_stat (path, &s) == -1)
		return 0;
	
	if (is_exe (s.st_mode))
		return 1;
	return 0;

}

static int
desktop_file_exec (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1){
		if (di->dentry){
			printf ("FIXME: No support for dentry loaded stuff yet\n");
		} else {
			if (file_is_executable (di->pathname)){
				char *tmp = name_quote (di->pathname, 0);

				if (!confirm_execute || (query_dialog (_(" The Midnight Commander "),
								       _(" Do you really want to execute? "),
								       0, 2, _("&Yes"), _("&No")) == 0))
					execute (tmp);
				free (tmp);
			} else {
				char *result, *command;

				result = regex_command (di->pathname, "Open", NULL, 0);
				if (result && (strcmp (result, "Success") == 0))
					return TRUE;
				command = input_expand_dialog (_("Open with..."),
							       _("Enter extra arguments:"),
							       di->pathname);
				if (command){
					execute (command);
					free (command);
				}
			}
		}
		return TRUE;
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3){
		desktop_icon_context_popup (event, di);
		return TRUE;
	}

	return FALSE;
}

static char **
drops_from_event (GdkEventDropDataAvailable *event)
{
	int count, i, len;
	int arguments;
	char *p, **argv;

	/* Count the number of file names received */
	count = event->data_numbytes;
	p = event->data;
	arguments = 0;
	while (count){
		arguments++;
		len = strlen (p) + 1;
		count -= len;
		p += len;
	}

	/* Create the exec vector with all of the filenames */
	argv = (char **) xmalloc (sizeof (char *) * arguments + 1, "arguments");
	count = event->data_numbytes;
	p = event->data;
	i = 0;
	do {
		len = 1 + strlen (p);
		count -= len;
		argv [i++] = p;
		p += len;
	} while (count);
	argv [i] = 0;

	return argv;
}

static void
drop_on_launch_entry (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	struct stat s;
	char *r;
	char **drops;
	
	/* try to stat it, if it fails, remove it from desktop */
	if (!mc_stat (di->pathname, &s) == 0){
		desktop_icon_remove (di);
		return;
	}

	drops = drops_from_event (event);
	
	r = regex_command (di->pathname, "Drop", drops, 0);
	if (strcmp (r, "Success") == 0){
		free (drops);
		return;
	}
	
	if (is_exe (s.st_mode))
		exec_direct (di->pathname, drops);
	
	free (drops);
}

static void
desktop_create_launch_entry (char *pathname, char *short_name)
{
	GtkWidget *window;
	desktop_icon_t *di;
	char *icon;

	icon = get_desktop_icon (pathname);
	window = my_create_transparent_text_window (icon, x_basename (pathname));
	g_free (icon);
	if (!window)
		return;
	
	di = xmalloc (sizeof (desktop_icon_t), "dcle");
	di->dentry = NULL;
	di->widget = window;
	di->pathname = strdup (pathname);

	desktop_icon_set_position (di);
	desktop_icon_make_draggable (di);
	
	desktop_icons = g_list_prepend (desktop_icons, (gpointer) di);

	/* Double clicking executes the command, single clicking brings up context menu */
	gtk_signal_connect (GTK_OBJECT (window), "button_press_event", GTK_SIGNAL_FUNC (desktop_file_exec), di);
	gtk_widget_realize (window);
	
	gtk_signal_connect (GTK_OBJECT (window), "drop_data_available_event",
			    GTK_SIGNAL_FUNC (drop_on_launch_entry), di);

	gtk_widget_dnd_drop_set (window, TRUE, drop_types, ELEMENTS (drop_types), FALSE);

	gtk_widget_show (window);
}

static int 
desktop_pathname_loaded (char *pathname)
{
	GList *p = desktop_icons;

	for (; p; p = p->next){
		desktop_icon_t *di = p->data;

		if (strcmp (di->pathname, pathname) == 0)
			return 1;
	}
	return 0;
}

static void
desktop_setup_icon (char *filename, char *full_pathname)
{
	struct stat s;

	if (mc_stat (full_pathname, &s) == -1)
		return;

	if (S_ISDIR (s.st_mode)){
		char *dir_full = concat_dir_and_file (full_pathname, ".directory");

		if (!desktop_pathname_loaded (dir_full)){
			if (exist_file (dir_full))
				desktop_load_dentry (dir_full);
			else
				desktop_create_directory_entry (dir_full, full_pathname, filename);
		}

	} else {
		if (strstr (filename, ".desktop")){
			if (!desktop_pathname_loaded (full_pathname))
				desktop_load_dentry (full_pathname);
		} else {
			char *desktop_version;
			
			desktop_version = copy_strings (full_pathname, ".desktop", NULL);
			if (!exist_file (desktop_version) && !desktop_pathname_loaded (full_pathname))
				desktop_create_launch_entry (full_pathname, filename);
			free (desktop_version);
		}
	}
}


/*
 * Load all of the entries available on the ~/desktop directory
 * So far, we support: .desktop files;  directories (they get a .directory file);
 * sylinks to directories;  other programs. 
 */
static void
desktop_reload (char *desktop_dir)
{
	struct dirent *dent;
	DIR *dir;
	
	dir = mc_opendir (desktop_dir);
	if (dir == NULL){
		message (1, _(" Warning "), _(" Could not open %s directory"), desktop_dir, NULL);
		return;
	}

	while ((dent = mc_readdir (dir)) != NULL){
		char *full;

		/* ignore '.' */
		if (dent->d_name [0] == '.' && dent->d_name [1] == 0)
			continue;

		/* ignore `..' */
		if (dent->d_name [0] == '.' && dent->d_name [1] == '.' && dent->d_name [2] == 0)
			continue;

		full = concat_dir_and_file (desktop_dir, dent->d_name);
		desktop_setup_icon (dent->d_name, full);
		free (full);
	}
}

/*
 * Copy the system defaults to the user ~/desktop directory and setup a
 * Home directory link
 */
static void
desktop_setup_default (char *desktop_dir)
{
	char *mc_desktop_dir;
	char *desktop_dir_home_link;
	
	desktop_dir_home_link = concat_dir_and_file (desktop_dir, "Home directory.desktop");

	mc_desktop_dir = concat_dir_and_file (mc_home, MC_LIB_DESKTOP);

	if (exist_file (mc_desktop_dir)){
		create_op_win (OP_COPY, 0);
		file_mask_defaults ();
		copy_dir_dir (mc_desktop_dir, desktop_dir);
		destroy_op_win ();
	} else 
		mkdir (desktop_dir, 0777);
	
	desktop_create_directory_entry (desktop_dir_home_link, "~", "Home directory");
	free (desktop_dir_home_link);
	free (mc_desktop_dir);
}

/*
 * configures the root window dropability
 */
void
desktop_root (void)
{
	GtkWidget *rw;

	rw = gnome_rootwin_new ();
	connect_drop_signals (rw, NULL);
	gtk_widget_realize (rw);
	gtk_widget_dnd_drop_set (rw, TRUE, root_drop_types, ELEMENTS (root_drop_types), FALSE);
	gtk_widget_show (rw);
	root_window = GNOME_ROOTWIN (rw);
}

/*
 * entry point to start up the gnome desktop
 */
void
start_desktop (void)
{
	desktop_directory = concat_dir_and_file (home_dir, "desktop");

	if (!exist_file (desktop_directory))
		desktop_setup_default (desktop_directory);

	desktop_root ();
	desktop_reload (desktop_directory);
}

/*
 * shutdown the desktop
 */
void
stop_desktop (void)
{
	GList *p;

	for (p = desktop_icons; p; p = p->next){
		desktop_icon_t *di = p->data;

		desktop_release_desktop_icon_t (di);
	}
	image_cache_destroy ();
}
