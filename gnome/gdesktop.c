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
#include <gdk/gdkx.h>

/* Types of desktop icons:
 *
 * o Application: Double click: start up application;
 *                Dropping:     start up program with arguments.
 *
 * o Directory:   Double click: opens the directory in a panel.
 *                Double click: copies/moves files.
 *
 * o File:        Opens the application according to regex_command
 */ 
		  
typedef enum {
	application,
	directory,
	file
} icon_t;

/* A structure that describes each icon on the desktop */
typedef struct {
	GnomeDesktopEntry *dentry;
	GtkWidget         *widget;
	icon_t            type;
	int               x, y;
	char              *title;
	char              *pathname;
} desktop_icon_t;

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

/* The list with the filenames we have actually loaded */
static GList *desktop_icons;

/*
 * If the dentry is zero, then no information from the on-disk .desktop file is used
 * In this case, we probably will have to store the geometry for a file somewhere
 * else.
 */
static void
desktop_icon_set_position (desktop_icon_t *di, GtkWidget *widget)
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

		gtk_widget_size_request (widget, &widget->requisition);
		current_y += widget->requisition.height + 8;
		if (current_y > gdk_screen_height ()){
			current_x += 0;
			current_y = 0;
		}
	}
	di->x = x;
	di->y = y;
	gtk_widget_set_uposition (widget, 6 + x, y);
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
	int x, y;
	int operation;
	
	operation = get_operation (event->timestamp, event->coords.x, event->coords.y);

	/* Optimization: if we are dragging from the same process, we can
	 * display a nicer status bar.
	 */
	source_panel = find_panel_owning_window_id (event->requestor);

	/* Symlinks do not use any panel/file.c optimization */
	if (operation == OPER_LINK){
		do_symlinks (event, dest);
		return;
	}
	
	if (source_panel  && !force_manually)
		perform_drop_on_directory (source_panel, operation, dest);
	else
		perform_drop_manually (operation, event, dest);
	return;
}

static void
drop_cb (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
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
	printf ("\nReceiving: %s %d\n", event->data, event->data_numbytes);
	
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
dentry_properties (desktop_icon_t *di)
{
	printf ("Edit this widget properties\n");
}

static int
dentry_button_click (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1){
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

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

GtkWidget *
my_create_transparent_text_window (char *file, char *text, int extra_events)
{
	GtkWidget *w;

	w = create_transparent_text_window (file, text, extra_events);
	if (!w){
		static char *default_pix;

		if (!default_pix){
			default_pix = gnome_unconditional_pixmap_file ("launcher-program.xpm");
		}
		w = create_transparent_text_window (default_pix, text, extra_events);
		if (!w)
			return NULL;
	}
	return w;
}

static void
desktop_load_from_dentry (GnomeDesktopEntry *dentry)
{
	desktop_icon_t *di;
	GtkWidget *window;
	char      *icon_label;

	icon_label = dentry->name ? dentry->name : x_basename (dentry->exec);
	if (dentry->icon)
		window = my_create_transparent_text_window (dentry->icon, icon_label, GDK_BUTTON_PRESS_MASK);
	else {
		static char *default_icon_path;
		static char exists;
		
		if (!default_icon_path){
			default_icon_path = gnome_unconditional_pixmap_file ("launcher-program.xpm");
			if (g_file_exists (default_icon_path))
				exists = 1;
		}

		if (exists)
			window = my_create_transparent_text_window (default_icon_path, icon_label, GDK_BUTTON_PRESS_MASK);
		else {
			window = gtk_window_new (GTK_WINDOW_POPUP);
			gtk_widget_set_usize (window, 20, 20);
		}
	}
	if (!window)
		return;
	
	di = xmalloc (sizeof (desktop_icon_t), "desktop_load_entry");
	di->dentry   = dentry;
	di->widget   = window;
	di->pathname = dentry->location;
		
	desktop_icon_set_position (di, window);
	
	desktop_icons = g_list_prepend (desktop_icons, (gpointer) di);
	
	/* Setup the widget to make it useful: */

	/* 1. Drag and drop functionality */
	connect_drop_signals (window, di);
	gtk_widget_dnd_drop_set (window, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
	
	/* 2. Double clicking executes the command */
	gtk_signal_connect (GTK_OBJECT (window), "button_press_event", GTK_SIGNAL_FUNC (dentry_button_click), di);

	gtk_widget_show (window);
}

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

static void
desktop_load_dentry (char *filename)
{
	GnomeDesktopEntry  *dentry;
	
	dentry = gnome_desktop_entry_load (filename);

	if (!dentry)
		return;

	desktop_load_from_dentry (dentry);
}

static void
desktop_create_directory_entry (char *dentry_path, char *pathname, char *short_name)
{
	GnomeDesktopEntry *dentry;

	dentry = xmalloc (sizeof (GnomeDesktopEntry), "dcde");
	dentry->name    = g_strdup (short_name);
	dentry->comment = NULL;
	dentry->tryexec = NULL;
	dentry->exec    = g_strdup (pathname);
	dentry->icon    = gnome_unconditional_pixmap_file ("gnome-folder.png");
	dentry->docpath = NULL;
	dentry->type    = g_strdup ("Directory");
	dentry->location = g_strdup (dentry_path);

	gnome_desktop_entry_save (dentry);
	desktop_load_from_dentry (dentry);
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
	full_fname = gnome_unconditional_pixmap_file (fname);
	if (exist_file (full_fname))
		return full_fname;
	g_free (full_fname);

	/* Try a mc icon */
	full_fname = concat_dir_and_file (ICONDIR, fname);
	if (exist_file (full_fname))
		return full_fname;

	free (full_fname);
	
	return gnome_unconditional_pixmap_file ("launcher-program.xpm");
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

/* Pops up the icon properties pages */
static void
icon_properties (GtkWidget *widget, desktop_icon_t *di)
{
	printf ("Sorry, no property pages yet\n");
	gtk_main_quit ();
}

static void
desktop_icon_remove (desktop_icon_t *di)
{
	desktop_icons = g_list_remove (desktop_icons, di);

	mc_unlink (di->pathname);
	gtk_widget_destroy (di->widget);
	desktop_release_desktop_icon_t (di);
}

/* Removes the icon from the desktop */
static void
icon_delete (GtkWidget *widget, desktop_icon_t *di)
{
	desktop_icon_remove (di);
	gtk_main_quit ();
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

	item = gtk_menu_item_new_with_label (_("Properties"));
	gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC (icon_properties), di);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Delete"));
	gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC (icon_delete), di);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_widget_show (item);
	
	gtk_widget_set_uposition (menu, event->x, event->y);

	gtk_grab_add (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 0, NULL, 3, event->time);
	gtk_main ();
	gtk_grab_remove (menu);
	gtk_widget_destroy (menu);
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

				if (!confirm_execute || (query_dialog (" The Midnight Commander ",
								       " Do you really want to execute? ",
								       0, 2, "&Yes", "&No") == 0))
					execute (tmp);
				free (tmp);
			} else {
				regex_command (di->pathname, "Open", NULL, 0);
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

static void
drop_on_executable (desktop_icon_t *di, GdkEventDropDataAvailable *event)
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

	/* invoke */
	exec_direct (di->pathname, argv);
}

static void
drop_on_launch_entry (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	struct stat s;

	/* try to stat it, if it fails, remove it from desktop */
	if (!mc_stat (di->pathname, &s) == 0){
		desktop_icon_remove (di);
		return;
	}
	
	if (is_exe (s.st_mode)){
		drop_on_executable (di, event);
		return;
	}
}

static void
desktop_create_launch_entry (char *pathname, char *short_name)
{
	GtkWidget *window;
	desktop_icon_t *di;
	char *icon;

	icon = get_desktop_icon (pathname);
	window = my_create_transparent_text_window (icon, x_basename (pathname), GDK_BUTTON_PRESS_MASK);
	g_free (icon);
	if (!window)
		return;
	
	di = xmalloc (sizeof (desktop_icon_t), "dcle");
	di->dentry = NULL;
	di->widget = window;
	di->pathname = strdup (pathname);

	desktop_icon_set_position (di, window);

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
	GnomeDesktopEntry *entry;
	
	dir = mc_opendir (desktop_dir);
	if (dir == NULL){
		message (1, " Warning ", " Could not open %s directory", desktop_dir, NULL);
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
	gtk_widget_dnd_drop_set (rw, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
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
 
