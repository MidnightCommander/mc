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
} desktop_icon_t;

/* The list of icons on the desktop */
static GList *desktop_icons;

/*
 * Creates a shaped window, toplevel, non-wm managed window from a given file
 * uses the all-mighty Imlib to do all the difficult work for us
 */
GtkWidget *
shaped_icon_new_from_file (char *file, int extra_events)
{
	GtkWidget *window;
	GdkImlibImage *im;
	GdkWindowPrivate *private;

	if (!g_file_exists (file))
		return;
	
	im = gdk_imlib_load_image (file);
	if (!im)
		return NULL;

	gtk_widget_push_visual (gdk_imlib_get_visual ());
	gtk_widget_push_colormap (gdk_imlib_get_colormap ());

	window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_widget_set_events (window, gtk_widget_get_events (window) | extra_events);
	
	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_widget_set_usize (window, im->rgb_width, im->rgb_height);
	gtk_widget_realize (window);
	gdk_window_resize (window->window, im->rgb_width, im->rgb_height);
	private = (GdkWindowPrivate *) window->window;
	private->width = im->rgb_width;
	private->height = im->rgb_height;
	gdk_imlib_apply_image (im, window->window);

	gdk_imlib_destroy_image (im);

	return window;
}

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

		current_y += 64;
		if (current_y > gdk_screen_height ()){
			current_x += 64;
			current_y = 0;
		}
	}
	di->x = x;
	di->y = y;
	gtk_widget_set_uposition (widget, x, y);
}

static void
drop_cb (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	char *p;
	int count;
	int len;
	
	if (strcmp (event->data_type, "url:ALL") == 0){
		count = event->data_numbytes;
		p = event->data;
		do {
			len = 1 + strlen (event->data);
			count -= len;
			printf ("Receiving: %s\n", p);
			p += len;
		} while (count);
		printf ("Receiving: %s %d\n", event->data, event->data_numbytes);
		return;
	}

	if (strcmp (event->data_type, "urls:ALL") == 0){
	}
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
		
static void
desktop_load_dentry (char *filename)
{
	GnomeDesktopEntry  *dentry;
	desktop_icon_t *di;
	GtkWidget *window;

	char *drop_types [] = {
		"text/plain"
		"url:ALL",
	};
	
	dentry = gnome_desktop_entry_load (filename);

	if (!dentry)
		return;

	if (dentry->icon)
		window = shaped_icon_new_from_file (dentry->icon, GDK_BUTTON_PRESS_MASK);
	else {
		static char *default_icon_path;
		static char exists;
		
		if (!default_icon_path){
			default_icon_path = gnome_unconditional_pixmap_file ("launcher-program.xpm");
			if (g_file_exists (default_icon_path))
				exists = 1;
		}

		if (exists)
			window = shaped_icon_new_from_file (default_icon_path, GDK_BUTTON_PRESS_MASK);
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
	gtk_widget_dnd_drop_set (window, TRUE, drop_types, 1, FALSE);
	
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
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
		printf ("YOu just discovered that regex_command ('open') is not being invoked yet\n");
}

static void
desktop_create_launch_entry (char *pathname, char *short_name)
{
	GtkWidget *window;
	desktop_icon_t *di;
	char *icon;

	icon = get_desktop_icon (pathname);
	window = shaped_icon_new_from_file (icon, GDK_BUTTON_PRESS_MASK);
	g_free (icon);
	di = xmalloc (sizeof (desktop_icon_t), "dcle");
	di->dentry = NULL;
	di->widget = window;
	desktop_icon_set_position (di, window);

	desktop_icons = g_list_prepend (desktop_icons, (gpointer) di);

	/* Double clicking executes the command */
	gtk_signal_connect (GTK_OBJECT (window), "button_press_event", GTK_SIGNAL_FUNC (desktop_file_exec), di);
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
			else
				desktop_create_launch_entry (full, dent->d_name);
		}
		
		free (full);
	}
}

static void
desktop_setup_default (char *desktop_dir)
{
	char *mc_desktop_dir;

	mc_desktop_dir = concat_dir_and_file (mc_home, MC_LIB_DESKTOP);
	
	create_op_win (OP_COPY, 0);
	file_mask_defaults ();
	copy_dir_dir (mc_desktop_dir, desktop_dir);
	destroy_op_win ();
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
 
