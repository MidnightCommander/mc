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

/* The list of icons on the desktop */
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
get_operation (int x, int y)
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

#if 0
		/* Not yet implemented the Link bits, so better to not show what we dont have */
		item = gtk_menu_item_new_with_label (_("Link"));
		gtk_signal_connect (GTK_OBJECT (item), "activate", GTK_SIGNAL_FUNC(set_option), (void *) OPER_LINK);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
#endif
		gtk_signal_connect (GTK_OBJECT (menu), "hide", GTK_SIGNAL_FUNC(option_menu_gone), 0);
	} 

	/* Here, we could set the mask parameter (the last NULL) to a valid variable
	 * and find out if the shift/control keys were set and do something smart
	 * about that
	 */
	
	gtk_widget_set_uposition (menu, x, y);

	/* FIXME: We should catch any events that escape this menu and cancel it */
	operation_value = -1;
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 0, NULL, 1, GDK_CURRENT_TIME);
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

static void make_symlinks (WPanel *source_panel, char *target_dir);

static void
perform_drop_on_panel (WPanel *source_panel, int operation, char *dest)
{
	switch (operation){
	case OPER_COPY:
		panel_operate (source_panel, OP_COPY, dest);
		break;
		
	case OPER_MOVE:
		panel_operate (source_panel, OP_MOVE, dest);
		break;
		
	case OPER_LINK:
		make_symlinks (source_panel, dest);
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

void
drop_on_panel (GdkEventDropDataAvailable *event, char *dest)
{
	WPanel *source_panel;
	int x, y;
	int operation;
	
	operation = get_operation (event->coords.x, event->coords.y);
	
	source_panel = find_panel_owning_window_id (event->requestor);

	if (source_panel)
		perform_drop_on_panel (source_panel, operation, dest);
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
	int is_directory = strcasecmp (di->dentry->type, "directory") == 0;

	if (is_directory){
		drop_on_panel (event, di->dentry->exec);
		return;
	}
		
	count = event->data_numbytes;
	p = event->data;
	do {
		len = 1 + strlen (event->data);
		count -= len;
		printf ("Receiving: %s\n", p);
		p += len;
	} while (count);
	printf ("Receiving: %s %d\n", event->data, event->data_numbytes);
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

static void
dentry_button_click (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
		dentry_execute (di);

	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
		dentry_properties (di);
}
		
char *drop_types [] = {
	"text/plain",
	"url:ALL",
};

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static void
desktop_load_dentry (char *filename)
{
	GnomeDesktopEntry  *dentry;
	desktop_icon_t *di;
	GtkWidget *window;
	char      *icon_label;
	
	dentry = gnome_desktop_entry_load (filename);

	if (!dentry)
		return;

	icon_label = dentry->name ? dentry->name : x_basename (dentry->exec);
	if (dentry->icon)
		window = create_transparent_text_window (dentry->icon, icon_label, GDK_BUTTON_PRESS_MASK);
	else {
		static char *default_icon_path;
		static char exists;
		
		if (!default_icon_path){
			default_icon_path = gnome_unconditional_pixmap_file ("launcher-program.xpm");
			if (g_file_exists (default_icon_path))
				exists = 1;
		}

		if (exists)
			window = create_transparent_text_window (default_icon_path, icon_label, GDK_BUTTON_PRESS_MASK);
		else {
			window = gtk_window_new (GTK_WINDOW_POPUP);
			gtk_widget_set_usize (window, 20, 20);
		}
	}
	if (!window)
		return;
	
	di = xmalloc (sizeof (desktop_icon_t), "desktop_load_entry");
	di->dentry = dentry;
	di->widget = window;
	
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

	gnome_desktop_entry_load (dentry_path);
}

/*
 * Returns the icon associated with the given file name, or app.xpm
 * if no icon is defined for this application
 */
static char *
get_desktop_icon (char *pathname)
{
	char *fname, *full_fname;

	fname = regex_command (pathname, "Icon", 0, 0);

	/* Try the system icon */
	full_fname = gnome_unconditional_pixmap_file (fname);
	if (full_fname)
		return full_fname;
	
	return gnome_unconditional_pixmap_file ("launcher-program.xpm");
}

static void
desktop_file_exec (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (!(event->type == GDK_2BUTTON_PRESS && event->button == 1))
		return;

	if (di->dentry){
		printf ("FIXME: No support for dentry loaded stuff yet\n");
	} else 
		regex_command (di->pathname, "Open", 0, 0);
}

static void
drop_on_launch_entry (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	if (strcmp (event->data_type, "url:ALL") == 0){
	}
	
}

static void
desktop_create_launch_entry (char *pathname, char *short_name)
{
	GtkWidget *window;
	desktop_icon_t *di;
	char *icon;

	icon = get_desktop_icon (pathname);
	window = create_transparent_text_window (icon, x_basename (pathname), GDK_BUTTON_PRESS_MASK);
	g_free (icon);
	di = xmalloc (sizeof (desktop_icon_t), "dcle");
	di->dentry = NULL;
	di->widget = window;
	di->pathname = strdup (pathname);
	desktop_icon_set_position (di, window);

	desktop_icons = g_list_prepend (desktop_icons, (gpointer) di);

	/* Double clicking executes the command */
	gtk_signal_connect (GTK_OBJECT (window), "button_press_event", GTK_SIGNAL_FUNC (desktop_file_exec), di);
	gtk_widget_realize (window);
	
	gtk_signal_connect (GTK_OBJECT (window), "drop_data_available_event",
			    GTK_SIGNAL_FUNC (drop_on_launch_entry), di);

	gtk_widget_dnd_drop_set (window, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
	
	gtk_widget_show (window);
}

/*
 * Desktop initialization code
 */
static void
desktop_load (char *desktop_dir)
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
		struct stat s;
		char *full;

		/* ignore '.' */
		if (dent->d_name [0] == '.' && dent->d_name [1] == 0)
			continue;

		/* ignore `..' */
		if (dent->d_name [0] == '.' && dent->d_name [1] == '.' && dent->d_name [2] == 0)
			continue;

		full = concat_dir_and_file (desktop_dir, dent->d_name);
		mc_stat (full, &s);

		if (S_ISDIR (s.st_mode)){
			char *dir_full = concat_dir_and_file (full, ".directory");

			if (exist_file (dir_full))
				desktop_load_dentry (dir_full);
			else
				desktop_create_directory_entry (dir_full, full, dent->d_name);
			
			free (dir_full);
		} else {
			if (strstr (dent->d_name, ".desktop"))
				desktop_load_dentry (full);
			else {
				char *desktop_version;
				
				desktop_version = copy_strings (full, ".desktop", NULL);
				if (!exist_file (desktop_version))
					desktop_create_launch_entry (full, dent->d_name);
				free (desktop_version);
			}
		}
		
		free (full);
	}
}

static void
make_symlinks (WPanel *source_panel, char *target_dir)
{
	printf ("weee! you are right, creating symbolic links by dnd is still not working\n");
}

static void
desktop_setup_default (char *desktop_dir)
{
	char *mc_desktop_dir;

	mc_desktop_dir = concat_dir_and_file (mc_home, MC_LIB_DESKTOP);

	if (exist_file (mc_desktop_dir)){
		create_op_win (OP_COPY, 0);
		file_mask_defaults ();
		copy_dir_dir (mc_desktop_dir, desktop_dir);
		destroy_op_win ();
	}
	free (mc_desktop_dir);
}

void
start_desktop (void)
{
	char *f = concat_dir_and_file (home_dir, "desktop");

	if (!exist_file (f))
		desktop_setup_default (f);
	
	desktop_load (f);
	free (f);
}
 
