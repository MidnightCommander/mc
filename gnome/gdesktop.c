/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */
#if 1
#include <config.h>
#include "fs.h"
#include <gnome.h>
#include "dialog.h"
#include "gdesktop.h"
#include "gdesktop-icon.h"
#include "gmetadata.h"
#include "../vfs/vfs.h"

/* use grid? */
int icons_snap_to_grid = 1;

/* Name of the user's desktop directory (i.e. ~/Desktop) */
#define DESKTOP_DIR_NAME "desktop"


/* Types of desktop icons */
enum icon_type {
	ICON_FILE,		/* Denotes a file (or symlink to a file) */
	ICON_DIRECTORY		/* Denotes a directory (or symlink to one) */
};


/* This structure defines the information carried by a desktop icon */
struct desktop_icon_info {
	GtkWidget *dicon;	/* The desktop icon widget */
	int x, y;		/* Position in the desktop */
	int slot;		/* Index of the slot the icon is in, or -1 for none */
	char *filename;		/* The file this icon refers to (relative to the desktop_directory) */
	enum icon_type type;	/* Type of icon, used to determine menu and DnD behavior */
	int selected : 1;	/* Is the icon selected? */
};

struct layout_slot {
	int num_icons;		/* Number of icons in this slot */
	GList *icons;		/* The list of icons in this slot */
};


/* Configuration options for the desktop */

int desktop_use_shaped_icons = TRUE;
int desktop_auto_placement = FALSE;
int desktop_snap_icons = FALSE;

/* The computed name of the user's desktop directory */
static char *desktop_directory;

/* Layout information:  number of rows/columns for the layout slots, and the array of slots.  Each
 * slot is an integer that specifies the number of icons that belong to that slot.
 */
static int layout_cols;
static int layout_rows;
static struct layout_slot *layout_slots;

#define l_slots(u, v) (layout_slots[(u) * layout_rows + (v)])

/* The last icon to be selected */
static struct desktop_icon_info *last_selected_icon;


/* Looks for a free slot in the layout_slots array and returns the coordinates that coorespond to
 * it.  "Free" means it either has zero icons in it, or it has the minimum number of icons of all
 * the slots.
 */
static void
get_icon_auto_pos (int *x, int *y)
{
	int min, min_x, min_y;
	int u, v;
	int val;

	min = l_slots (0, 0).num_icons;
	min_x = min_y = 0;

	for (u = 0; u < layout_cols; u++)
		for (v = 0; v < layout_rows; v++) {
			val = l_slots (u, v).num_icons;

			if (val == 0) {
				/* Optimization: if it is zero, return immediately */

				*x = u * DESKTOP_SNAP_X;
				*y = v * DESKTOP_SNAP_Y;
				return;
			} else if (val < min) {
				min = val;
				min_x = u;
				min_y = v;
			}
		}

	*x = min_x * DESKTOP_SNAP_X;
	*y = min_y * DESKTOP_SNAP_Y;
}

/* Snaps the specified position to the icon grid.  It looks for the closest free spot on the grid,
 * or the closest one that has the least number of icons in it.
 */
static void
get_icon_snap_pos (int *x, int *y)
{
	int min, min_x, min_y;
	int min_dist;
	int sx, sy;
	int u, v;
	int val, dist;
	int dx, dy;

	min = l_slots (0, 0).num_icons;
	min_x = min_y = 0;
	min_dist = INT_MAX;

	sx = DESKTOP_SNAP_X * ((*x + DESKTOP_SNAP_X / 2) / DESKTOP_SNAP_X);
	sy = DESKTOP_SNAP_Y * ((*y + DESKTOP_SNAP_Y / 2) / DESKTOP_SNAP_Y);

	for (u = 0; u < layout_cols; u++)
		for (v = 0; v < layout_rows; v++) {
			val = l_slots (u, v).num_icons;

			dx = sx - u;
			dy = sy - v;
			dist = dx * dx + dy * dy;

			if ((val == min && dist < min_dist) || (val < min)) {
				min_dist = dist;
				min_x = u;
				min_y = v;
			}
		}

	*x = min_x * DESKTOP_SNAP_X;
	*y = min_y * DESKTOP_SNAP_Y;
}

/* Removes an icon from the slot it is in, if any */
static void
remove_from_slot (struct desktop_icon_info *dii)
{
	if (dii->slot == -1)
		return;

	g_assert (layout_slots[dii->slot].num_icons >= 1);
	g_assert (layout_slots[dii->slot].icons != NULL);

	layout_slots[dii->slot].num_icons--;
	layout_slots[dii->slot].icons = g_list_remove (layout_slots[dii->slot].icons, dii);
}

/* Places a desktop icon.  If auto_pos is true, then the function will look for a place to position
 * the icon automatically, else it will use the specified coordinates, snapped to the grid if the
 * global desktop_snap_icons flag is set.
 */
static void
desktop_icon_info_place (struct desktop_icon_info *dii, int auto_pos, int xpos, int ypos)
{
	int u, v;

	if (auto_pos)
		get_icon_auto_pos (&xpos, &ypos);
	else if (desktop_snap_icons)
		get_icon_snap_pos (&xpos, &ypos);

	/* Increase the number of icons in the corresponding slot */

	remove_from_slot (dii);

	u = xpos / DESKTOP_SNAP_X;
	v = ypos / DESKTOP_SNAP_Y;

	dii->slot = u * layout_rows + v;
	layout_slots[dii->slot].num_icons++;
	layout_slots[dii->slot].icons = g_list_append (layout_slots[dii->slot].icons, dii);

	/* Move the icon */
	
	dii->x = xpos;
	dii->y = ypos;
	gtk_widget_set_uposition (dii->dicon, xpos, ypos);
}

/* Unselects all the desktop icons */
static void
unselect_all (void)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			if (dii->selected) {
				desktop_icon_select (DESKTOP_ICON (dii->dicon), FALSE);
				dii->selected = FALSE;
			}
		}
}

/* Sets the selection state of a range to the specified value.  The range starts at the
 * last_selected_icon and ends at the specified icon.
 */
static void
select_range (struct desktop_icon_info *dii, int sel)
{
	int du, dv, lu, lv;
	int min_u, min_v;
	int max_u, max_v;
	int u, v;
	GList *l;
	struct desktop_icon_info *ldii;
	struct desktop_icon_info *min_udii, *min_vdii;
	struct desktop_icon_info *max_udii, *max_vdii;

	/* Find out the selection range */

	if (!last_selected_icon)
		last_selected_icon = dii;

	du = dii->slot / layout_rows;
	dv = dii->slot % layout_rows;
	lu = last_selected_icon->slot / layout_rows;
	lv = last_selected_icon->slot % layout_rows;

	if (du < lu) {
		min_u = du;
		max_u = lu;
		min_udii = dii;
		max_udii = last_selected_icon;
	} else {
		min_u = lu;
		max_u = du;
		min_udii = last_selected_icon;
		max_udii = dii;
	}

	if (dv < lv) {
		min_v = dv;
		max_v = lv;
		min_vdii = dii;
		max_vdii = last_selected_icon;
	} else {
		min_v = lv;
		max_v = dv;
		min_vdii = last_selected_icon;
		max_vdii = dii;
	}

	/* Select all the icons in the rectangle */

	for (u = min_u; u <= max_u; u++)
		for (v = min_v; v <= max_v; v++)
			for (l = l_slots (u, v).icons; l; l = l->next) {
				ldii = l->data;

				if ((u == min_u && ldii->x < min_udii->x)
				    || (v == min_v && ldii->y < min_vdii->y)
				    || (u == max_u && ldii->x > max_udii->x)
				    || (v == max_v && ldii->y > max_vdii->y))
					continue;

				desktop_icon_select (DESKTOP_ICON (ldii->dicon), sel);
				ldii->selected = sel;
			}
}

/* Handles icon selection and unselection due to button presses */
static void
select_icon (struct desktop_icon_info *dii, GdkEventButton *event)
{
	int range;
	int additive;

	range = ((event->state & GDK_SHIFT_MASK) != 0);
	additive = ((event->state & GDK_CONTROL_MASK) != 0);

	if (!additive)
		unselect_all ();

	if (!range) {
		if (additive) {
			desktop_icon_select (DESKTOP_ICON (dii->dicon), !dii->selected);
			dii->selected = !dii->selected;
		} else if (!dii->selected) {
			desktop_icon_select (DESKTOP_ICON (dii->dicon), TRUE);
			dii->selected = TRUE;
		}

		last_selected_icon = dii;

		if (dii->selected)
			gdk_window_raise (dii->dicon->window);
	} else
		select_range (dii, TRUE);
}

/* Handler for events on desktop icons.  The on_text flag specifies whether the event ocurred on the
 * text item in the icon or not.
 */
static gint
desktop_icon_info_event (struct desktop_icon_info *dii, GdkEvent *event, int on_text)
{
	int retval;

	retval = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if ((event->button.button == 1) && !(on_text && dii->selected)) {
			select_icon (dii, (GdkEventButton *) event);
			retval = TRUE;
		} else if (event->button.button == 3)
			retval = TRUE; /* FIXME: display menu */

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button.button != 1)
			break;

		/* FIXME: activate icon */

		retval = TRUE;
		break;

	default:
		break;
	}

	/* If we handled the event, do not pass it on to the icon text item */

	if (on_text && retval)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text),
					      "event");

	return retval;
}

/* Handler for button presses on the images on desktop icons.  The desktop icon info structure is
 * passed in the user data.
 */
static gint
icon_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	return desktop_icon_info_event (data, event, FALSE);
}

/* Handler for button presses on the text on desktop icons.  The desktop icon info structure is
 * passed in the user data.
 */
static gint
text_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	return desktop_icon_info_event (data, event, TRUE);
}

/* Creates a new desktop icon.  The filename is the pruned filename inside the desktop directory.
 * If auto_pos is true, then the function will look for a place to position the icon automatically,
 * else it will use the specified coordinates.  It does not show the icon.
 */
static struct desktop_icon_info *
desktop_icon_info_new (char *filename, int auto_pos, int xpos, int ypos)
{
	struct desktop_icon_info *dii;
	char *full_name;
	char *icon_name;
	struct stat s;

	full_name = g_concat_dir_and_file (desktop_directory, filename);

	if (mc_stat (full_name, &s) != 0) {
		g_warning ("Could not stat %s; will not use a desktop icon", full_name);
		g_free (full_name);
		return NULL;
	}

	/* Create the icon structure */

	icon_name = meta_get_icon_for_file (full_name);

	dii = g_new (struct desktop_icon_info, 1);
	dii->dicon = desktop_icon_new (icon_name, filename);
	dii->x = 0;
	dii->y = 0;
	dii->slot = -1;
	dii->filename = g_strdup (filename);
	dii->type = S_ISDIR (s.st_mode) ? ICON_DIRECTORY : ICON_FILE;
	dii->selected = FALSE;

	g_free (full_name);
	g_free (icon_name);

	/* Connect to the icon's signals */

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->icon), "event",
			    (GtkSignalFunc) icon_event,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "event",
			    (GtkSignalFunc) text_event,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->stipple), "event",
			    (GtkSignalFunc) icon_event,
			    dii);

	/* Place the icon and append it to the list */

	desktop_icon_info_place (dii, auto_pos, xpos, ypos);
	return dii;
}

/* Frees a desktop icon information structure, and destroy the icon widget.  Does not remove the
 * structure from the desktop_icons list!
 */
static void
desktop_icon_info_free (struct desktop_icon_info *dii)
{
	gtk_widget_destroy (dii->dicon);
	remove_from_slot (dii);

	g_free (dii->filename);
	g_free (dii);
}

/* Creates the layout information array */
static void
create_layout_info (void)
{
	layout_cols = (gdk_screen_width () + DESKTOP_SNAP_X - 1) / DESKTOP_SNAP_X;
	layout_rows = (gdk_screen_height () + DESKTOP_SNAP_Y - 1) / DESKTOP_SNAP_Y;
	layout_slots = g_new0 (struct layout_slot, layout_cols * layout_rows);
}

/* Check that the user's desktop directory exists, and if not, create it with a symlink to the
 * user's home directory so that an icon will be displayed.
 */
static void
create_desktop_dir (void)
{
	char *home_link_name;

	desktop_directory = g_concat_dir_and_file (gnome_user_home_dir, DESKTOP_DIR_NAME);

	if (!g_file_exists (desktop_directory)) {
		/* Create the directory */

		mkdir (desktop_directory, 0777);

		/* Create the link to the user's home directory so that he will have an icon */

		home_link_name = g_concat_dir_and_file (desktop_directory, _("Home directory"));

		if (mc_symlink (gnome_user_home_dir, home_link_name) != 0) {
			message (FALSE,
				 _("Warning"),
				 _("Could not symlink %s to %s; will not have initial desktop icons."),
				 gnome_user_home_dir, home_link_name);
			g_free (home_link_name);
			return;
		}

		g_free (home_link_name);
	}
}

/* Reads the ~/Desktop directory and creates the initial desktop icons */
static void
load_initial_desktop_icons (void)
{
	struct dirent *dirent;
	DIR *dir;
	char *full_name;
	int have_pos, x, y;
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	dir = mc_opendir (desktop_directory);
	if (!dir) {
		message (FALSE,
			 _("Warning"),
			 _("Could not open %s; will not have initial desktop icons"),
			 desktop_directory);
		return;
	}

	while ((dirent = mc_readdir (dir)) != NULL) {
		if (((dirent->d_name[0] == '.') && (dirent->d_name[1] == 0))
		    || ((dirent->d_name[0] == '.') && (dirent->d_name[1] == '.') && (dirent->d_name[2] == 0)))
			continue;

		full_name = g_concat_dir_and_file (desktop_directory, dirent->d_name);

		have_pos = meta_get_icon_pos (full_name, &x, &y);
		desktop_icon_info_new (dirent->d_name, !have_pos, x, y);

		g_free (full_name);
	}

	mc_closedir (dir);

	/* Show all the icons */

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;
			gtk_widget_show (dii->dicon);
		}
}

/**
 * desktop_init
 *
 * Initializes the desktop by setting up the default icons (if necessary), setting up drag and drop,
 * and other miscellaneous tasks.
 */
void
desktop_init (void)
{
	create_layout_info ();
	create_desktop_dir ();
	load_initial_desktop_icons ();
}

/**
 * desktop_destroy
 *
 * Shuts the desktop down by destroying the desktop icons.
 */
void
desktop_destroy (void)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	/* Destroy the desktop icons */

	for (i = 0; i < (layout_cols * layout_rows); i++) {
		l = layout_slots[i].icons;

		while (l) {
			dii = l->data;
			l = l->next;

			desktop_icon_info_free (dii);
		}
	}

	/* Cleanup */

	g_free (layout_slots);
	layout_slots = NULL;
	layout_cols = 0;
	layout_rows = 0;

	g_free (desktop_directory);
	desktop_directory = NULL;
}








#else

#include <config.h>
#include "fs.h"
#include <gnome.h>
#include "gdesktop-icon.h"
#include "gdesktop.h"
#include "../vfs/vfs.h"
#include <string.h>
#include "mad.h"
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
#include "gcache.h"
#include "gmain.h"

/* places used in the grid */
static char *spot_array;

/* number of icons that fit along the x and y axis */
static int x_spots, y_spots;

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

static void desktop_reload (char *desktop_dir, GdkPoint *drop_position);
static void desktop_icon_context_popup (GdkEventButton *event, desktop_icon_t *di);

/* The list with the filenames we have actually loaded */
static GList *desktop_icons;

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static void
init_spot_list (void)
{
	int size;
	
	x_spots = gdk_screen_width () / SNAP_X;
	y_spots = gdk_screen_height () / SNAP_Y;
	size = (x_spots * y_spots) / 8;
	spot_array = xmalloc (size+1, "spot_array");
	memset (spot_array, 0, size);
}

static int
is_spot_set (int x, int y)
{
	int o = (x * x_spots + y);
	int idx = o / 8;
	int bit = o % 8;
	
	return spot_array [idx] & (1 << bit);
}

static void
set_spot_val (int x, int y, int set)
{
	int o = (x * x_spots + y);
	int idx = o / 8;
	int bit = o % 8;

	if (set)
		spot_array [idx] |= (1 << bit);
	else
		spot_array [idx] &= ~(1 << bit);
}

static void
allocate_free_spot (int *rx, int *ry)
{
	int x, y;
	
	for (x = 0; x < x_spots; x++)
		for (y = 0; y < y_spots; y++)
			if (!is_spot_set (x, y)){
				*rx = x;
				*ry = y;
				set_spot_val (x, y, 1);
				return;
			}
}

static void
snap_to (desktop_icon_t *di, int absolute, int x, int y)
{
	int nx = x/SNAP_X;
	int ny = y/SNAP_Y;

	if (!absolute && is_spot_set (nx, ny))
		allocate_free_spot (&di->grid_x, &di->grid_y);
	else {
		set_spot_val (nx, ny, 1);
		di->grid_x = nx;
		di->grid_y = ny;
	}

}

/* Get snapped position for an icon */
static void
get_icon_screen_x_y (desktop_icon_t *di, int *x, int *y)
{
	int w, h;

	w = DESKTOP_ICON (di->widget)->width;
	h = DESKTOP_ICON (di->widget)->height;

	if (di->grid_x != -1){
		*x = di->grid_x * SNAP_X;
		*y = di->grid_y * SNAP_Y;
		
		*x = *x + (SNAP_X - w) / 2;
		if (*x < 0)
			*x = 0;
		
		if (h > SNAP_Y)
			*y = *y + (SNAP_Y - h) / 2;
		else
			*y = *y + (SNAP_Y - h);
	} else {
		*x = di->x;
		*y = di->y;
	}
}

/*
 * If the dentry is zero, then no information from the on-disk .desktop file is used
 * In this case, we probably will have to store the geometry for a file somewhere
 * else.
 */
static int current_x, current_y;

static void
desktop_icon_set_position (desktop_icon_t *di)
{
	static int x, y = 10;

	x = -1;
	if (di->dentry && di->dentry->geometry){
		char *comma = strchr (di->dentry->geometry, ',');

		if (comma){
			x = atoi (di->dentry->geometry);
			comma++;
			y = atoi (comma);
		}
	}

	if (icons_snap_to_grid){
		if (x == -1){
			x = current_x;
			y = current_y;

			current_y += SNAP_Y;
			if (current_y > gdk_screen_height ())
				current_x += SNAP_X;

			snap_to (di, 1, x, y);
		} else
			snap_to (di, 0, x, y);

		get_icon_screen_x_y (di, &x, &y);
	} else {
		/* This find-spot routine can obviously be improved, left as an excercise
		 * to the hacker
		 */
		if (x == -1){
			x = current_x;
			y = current_y;
			
			current_y += DESKTOP_ICON (di)->height + 8;
			if (current_y > gdk_screen_height ()){
				current_x += SNAP_X;
				current_y = 0;
			}
		}
		x += 6;
		di->grid_x = di->grid_y = -1;
	}

	di->x = x;
	di->y = y;
	
	gtk_widget_set_uposition (di->widget, x, y);
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
		free (fname);
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
	
	return gnome_unconditional_pixmap_file ("launcher-program.png");
}

/*
 * Hackisigh routine taken from GDK
 */
#ifdef OLD_DND
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
#if 1
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
	gdk_dnd.drag_really = 1;
	gdk_dnd_display_drag_cursor (x, y, FALSE, TRUE);
#else
	gdk_dnd.real_sw = wp;
	gdk_dnd.dnd_drag_start.x = x;
	gdk_dnd.dnd_drag_start.y = y;
	gdk_dnd.drag_perhaps = 1;
          if(gdk_dnd.drag_startwindows)
            {
              g_free(gdk_dnd.drag_startwindows);
              gdk_dnd.drag_startwindows = NULL;
            }
          gdk_dnd.drag_numwindows = gdk_dnd.drag_really = 0;
          gdk_dnd.dnd_grabbed = FALSE;
	{
           /* Set motion mask for first DnD'd window, since it
               will be the one that is actually dragged */
            XWindowAttributes dnd_winattr;
            XSetWindowAttributes dnd_setwinattr;

            /* We need to get motion events while the button is down, so
               we can know whether to really start dragging or not... */
            XGetWindowAttributes(gdk_display, (Window)wp->xwindow,
                                 &dnd_winattr);
            
            wp->dnd_drag_savedeventmask = dnd_winattr.your_event_mask;
            dnd_setwinattr.event_mask = 
              wp->dnd_drag_eventmask = ButtonMotionMask | ButtonPressMask | ButtonReleaseMask |
                        EnterWindowMask | LeaveWindowMask;
            XChangeWindowAttributes(gdk_display, wp->xwindow,
                                    CWEventMask, &dnd_setwinattr);
        }
#endif
}
#endif /* OLD_DND */

static GdkDragAction operation_value;

static void
set_option (GtkWidget *widget, GdkDragAction value)
{
	operation_value = value;
	gtk_main_quit ();
}

static void
option_menu_gone ()
{
	operation_value = GDK_ACTION_ASK;
	gtk_main_quit ();
}

static GdkDragAction
get_operation (guint32 timestamp, int x, int y)
{
	static GtkWidget *menu;

	if (!menu){
		GtkWidget *item;
		
		menu = gtk_menu_new ();

		item = gtk_menu_item_new_with_label (_("Copy"));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC(set_option), (void *) GDK_ACTION_COPY);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		
		item = gtk_menu_item_new_with_label (_("Move"));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC(set_option), (void *) GDK_ACTION_MOVE);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		/* Not yet implemented the Link bits, so better to not show what we dont have */
		item = gtk_menu_item_new_with_label (_("Link"));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC(set_option), (void *) GDK_ACTION_LINK);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);

		gtk_signal_connect (GTK_OBJECT (menu), "hide", GTK_SIGNAL_FUNC(option_menu_gone), 0);
	} 

	gtk_widget_set_uposition (menu, x, y);

	/* FIXME: We should catch any events that escape this menu and cancel it */
	operation_value = GDK_ACTION_ASK;
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 0, NULL, 1, timestamp);
	gtk_grab_add (menu);
	gtk_main ();
	gtk_grab_remove (menu);
	gtk_widget_hide (menu);
	
	return operation_value;
}

/*
 * Used by check_window_id_in_one_panel and find_panel_owning_window_id for finding
 * the panel that contains the specified window id (used to figure where the drag
 * started)
 */
static WPanel *temp_panel;

static void
check_window_in_one_panel (gpointer data, gpointer user_data)
{
	PanelContainer *pc    = (PanelContainer *) data;
	GdkWindowPrivate *w   = (GdkWindowPrivate *) user_data;
	WPanel *panel         = pc->panel;

	if (panel->list_type == list_icons){
		GnomeIconList *icon_list = GNOME_ICON_LIST (panel->icons);
		GdkWindowPrivate *wp = (GdkWindowPrivate *) GTK_WIDGET (icon_list)->window;
		
		if (w->xwindow == wp->xwindow){
			temp_panel = panel;
			return;
		}
	} else {
		GtkCList *clist       = GTK_CLIST (panel->list);
		GdkWindowPrivate *wp = (GdkWindowPrivate *) clist->clist_window;
		
		if (w->xwindow == wp->xwindow){
			temp_panel = panel;
			return;
		}
	}
}

static WPanel *
find_panel_owning_window (GdkWindow *window)
{
	temp_panel = NULL;

	printf ("Looking for window %x\n", window);
	
	g_list_foreach (containers, check_window_in_one_panel, window);

	return temp_panel;
}

static void
perform_drop_on_directory (WPanel *source_panel, GdkDragAction action, char *dest)
{
	switch (action){
	case GDK_ACTION_COPY:
		panel_operate (source_panel, OP_COPY, dest);
		break;
		
	case GDK_ACTION_MOVE:
		panel_operate (source_panel, OP_MOVE, dest);
		break;
	}
}

static void
perform_drop_manually (GList *names, GdkDragAction action, char *dest)
{
	struct stat buf;

	switch (action){
	case GDK_ACTION_COPY:
		create_op_win (OP_COPY, 0);
		break;
		
	case GDK_ACTION_MOVE:
		create_op_win (OP_MOVE, 0);
		break;

	default:
		g_assert_not_reached ();
	}

	file_mask_defaults ();

	for (; names; names = names->next){
		char *p = names->data;
		char *tmpf;
		int res, v;
		
		if (strncmp (p, "file:", 5) == 0)
			p += 5;
				
		switch (action){
		case GDK_ACTION_COPY:
			tmpf = concat_dir_and_file (dest, x_basename (p));
			do {
				res = mc_stat (p, &buf);
				if (res != 0){
					v = file_error (" Could not stat %s \n %s ", tmpf);
					if (v != FILE_RETRY)
						res = 0;
				} else {
					if (S_ISDIR (buf.st_mode))
						copy_dir_dir (p, tmpf, 1, 0, 0, 0);
					else
						copy_file_file (p, tmpf, 1);
				}
			} while (res != 0);
			free (tmpf);
			break;
			
		case GDK_ACTION_MOVE:
			tmpf = concat_dir_and_file (dest, x_basename (p));
			do {
				res = mc_stat (p, &buf);
				if (res != 0){
					v = file_error (" Could not stat %s \n %s ", tmpf);
					if (v != FILE_RETRY)
						res = 0;
				} else {
					if (S_ISDIR (buf.st_mode))
						move_dir_dir (p, tmpf);
					else
						move_file_file (p, tmpf);
				}
			} while (res != 0);
			free (tmpf);
			break;

		default:
			g_assert_not_reached ();
		}
	}

	destroy_op_win ();
}

static void
do_symlinks (GList *names, char *dest)
{
	for (; names; names = names->next){
		char *full_dest_name;
		char *name = names->data;

		full_dest_name = concat_dir_and_file (dest, x_basename (name));
		if (strncmp (name, "file:", 5) == 0)
			mc_symlink (name+5, full_dest_name);
		else
			mc_symlink (name, full_dest_name);

		free (full_dest_name);
	} 
}

#if OLD_DND
static char **
drops_from_event (GdkEventDropDataAvailable *event, int *argc)
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
	*argc = i;
	
	return argv;
}
#endif /* OLD_DND */

void
drop_on_directory (GtkSelectionData *sel_data, GdkDragContext *context,
		   GdkDragAction action, char *dest, int force_manually)
{
	WPanel *source_panel;
	GList *names;

	gdk_flush ();
	g_warning ("Figure out the data type\n");
	if (sel_data->data == NULL)
		return;

	printf ("action=%d\n", action);
	if (action == GDK_ACTION_ASK){
		g_warning ("I need the event here\n");
#if 0
		action = get_operation (event->timestamp, event->coords.x, event->coords.y);
#endif
	}

	printf ("action=%d\n", action);
	if (action == GDK_ACTION_ASK)
		return;
	
	/*
	 * Optimization: if we are dragging from the same process, we can
	 * display a nicer status bar.
	 */
	source_panel = find_panel_owning_window (context->source_window);

	printf ("SOurce_Panel=%p\n", source_panel);

	names = gnome_uri_list_extract_uris ((char *)sel_data->data);

	/* Symlinks do not use any panel/file.c optimization */
	if (action == GDK_ACTION_LINK){
		do_symlinks (names, dest);
		gnome_uri_list_free_strings (names);
		return;
	}
	
	if (source_panel  && !force_manually){
		perform_drop_on_directory (source_panel, action, dest);
		update_one_panel_widget (source_panel, 0, UP_KEEPSEL);
		panel_update_contents (source_panel);
	} else
		perform_drop_manually (names, action, dest);

	gnome_uri_list_free_strings (names);
	return;
}

/*
 * destroys a desktop_icon_t structure and anything that was held there,
 * including the desktop widget. 
 */
static void
desktop_release_desktop_icon_t (desktop_icon_t *di, int destroy_dentry)
{
	if (di->dentry){
		if (destroy_dentry)
			gnome_desktop_entry_destroy (di->dentry);
		else
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

static int
remove_directory (char *path)
{
	int i;

	if (confirm_delete){
		char *buffer;
		
		if (know_not_what_am_i_doing)
			query_set_sel (1);
		buffer = copy_strings (_("Do you want to delete "), path, "?", NULL);
		i = query_dialog (_("Delete"), buffer,
				  D_ERROR, 2, _("&Yes"), _("&No"));
		free (buffer);
		if (i != 0)
			return 0;
	}
	create_op_win (OP_DELETE, 0);
	erase_dir (path);
	destroy_op_win ();
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	return 1;
}

/*
 * Removes an icon from the desktop and kills the ~/desktop file associated with it
 */
static void
desktop_icon_remove (desktop_icon_t *di)
{
	desktop_icons = g_list_remove (desktop_icons, di);

	if (di->dentry == NULL){
		/* launch entry */
		mc_unlink (di->pathname);
	} else {
		/* a .destop file or a directory */
		/* Remove the .desktop */
		mc_unlink (di->dentry->location);

		if (strcmp (di->dentry->type, "Directory") == 0){
			struct stat s;

			if (mc_lstat (di->dentry->exec[0], &s) == 0){
				if (S_ISLNK (s.st_mode))
					mc_unlink (di->dentry->exec[0]);
				else 
					if (!remove_directory (di->dentry->exec[0]))
						return;
			}
		} else {
			if (strncmp (di->dentry->exec [0], desktop_directory, strlen (desktop_directory)) == 0)
				mc_unlink (di->dentry->exec [0]);
		}
	}
	desktop_release_desktop_icon_t (di, 1);
}

#ifdef OLD_DN
static void
drop_on_launch_entry (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	struct stat s;
	char *r;
	char **drops;
	int drop_count;
	
	/* try to stat it, if it fails, remove it from desktop */
	if (!mc_stat (di->dentry->exec [0], &s) == 0){
		desktop_icon_remove (di);
		return;
	}

	drops = drops_from_event (event, &drop_count);
	
	r = regex_command (di->pathname, "Drop", drops, 0);
	if (r && strcmp (r, "Success") == 0){
		free (drops);
		return;
	}
	
	if (is_exe (s.st_mode))
		gnome_desktop_entry_launch_with_args (di->dentry, drop_count, drops);
	
	free (drops);
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
		char *drop_location;

		drop_on_directory (event, desktop_directory, 1);
		desktop_reload (desktop_directory, &event->coords);
		return;
	}
	
	if (is_directory){
		drop_on_directory (event, di->dentry->exec[0], 0);
		return;
	}

	/* Last case: regular desktop stuff */
	drop_on_launch_entry (widget, event, di);
}

static int
drop_cb (GtkWidget *widget, GdkEventDropDataAvailable *event, desktop_icon_t *di)
{
	if (strcmp (event->data_type, "icon/root") == 0){
		printf ("ICON DROPPED ON ROOT!\n");
	} if (strcmp (event->data_type, "url:ALL") == 0 ||
	      (strcmp (event->data_type, "file:ALL") == 0) ||
	      (strcmp (event->data_type, "text/plain") == 0)){
		url_dropped (widget, event, di);
	} else
		return FALSE;
	return TRUE;
}

static void
drop_enter_leave ()
{
/*	printf ("Enter/Leave\n");  */
}

static void
connect_drop_signals (GtkWidget *widget, desktop_icon_t *di)
{
	GtkObject *o = GTK_OBJECT (widget);
	
	gtk_signal_connect (o, "drop_enter_event", GTK_SIGNAL_FUNC (drop_enter_leave), di);
	gtk_signal_connect (o, "drop_leave_event", GTK_SIGNAL_FUNC (drop_enter_leave), di);
	gtk_signal_connect (o, "drop_data_available_event", GTK_SIGNAL_FUNC (drop_cb), di);
}
#endif /* OLD DND */

void
desktop_icon_execute (GtkWidget *ignored, desktop_icon_t *di)
{
	/* Ultra lame-o execute.  This should be replaced by the fixed regexp_command
	 * invocation 
	 */

	if (strcmp (di->dentry->type, "Directory") == 0)
		new_panel_at (di->dentry->exec[0]);
	else 
		gnome_desktop_entry_launch (di->dentry);
}

static void
start_icon_drag (GtkWidget *wi, GdkEventMotion *event)
{
	printf ("MOTION NOTIF!\n");
#ifdef OLD_DND
	artificial_drag_start (wi->window, event->x, event->y);
#endif
}

GdkPoint root_icon_drag_hotspot = { 15, 15 };

static void
desktop_icon_configure_position (desktop_icon_t *di, int x, int y)
{
	gtk_widget_set_uposition (di->widget, x, y);

	if (di->dentry){
		char buffer [40];

		sprintf (buffer, "%d,%d", x, y);
		if (di->dentry->geometry)
			g_free (di->dentry->geometry);
		di->dentry->geometry = g_strdup (buffer);
		gnome_desktop_entry_save (di->dentry);
	}
}

#ifdef OLD_DND 
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
		
		if (di->grid_x != -1)
			set_spot_val (di->grid_x, di->grid_y, 0);

		if (icons_snap_to_grid){
			snap_to (di, 0, drop_x, drop_y);
			get_icon_screen_x_y (di, &drop_x, &drop_y);
		}

		desktop_icon_configure_position (di, drop_x, drop_y);
	}
}
#endif

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

void
gnome_arrange_icons (void)
{
	GList *l;
	
	current_x = current_y = 0;
	memset (spot_array, 0, (x_spots * y_spots)/8);
	
	for (l = desktop_icons; l; l = l->next){
		desktop_icon_t *di = l->data;
		int x, y;
		
		snap_to (di, 1, current_x, current_y);
		get_icon_screen_x_y (di, &x, &y);
		desktop_icon_configure_position (di, x, y);

		current_y += SNAP_Y;
		if (current_y == gdk_screen_height ()){
			current_y = 0;
			current_x += SNAP_X;
		}
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
		if (root_drag_not_ok_window && root_drag_ok_window){
#ifdef OLD_DND
			gdk_dnd_set_drag_shape (root_drag_ok_window->window, &root_icon_drag_hotspot,
						root_drag_not_ok_window->window, &root_icon_drag_hotspot);
#endif
			gtk_widget_show (root_drag_not_ok_window);
			gtk_widget_show (root_drag_ok_window);
		}
		free (fname);
	}
}

static void
desktop_icon_drag_end (GtkWidget *widget, GdkEvent *event, desktop_icon_t *di)
{
	in_desktop_dnd = 0;
	destroy_shaped_dnd_windows ();
}

/*
 * Bind the signals so that we can make this icon draggable
 */
static void
desktop_icon_make_draggable (desktop_icon_t *di)
{
	GtkObject *obj;
        GList *child;
	char *drag_types [] = { "icon/root", "url:ALL" };

        child = gtk_container_children(GTK_CONTAINER(di->widget));
        obj = GTK_OBJECT (child->data);
  /* To artificially start up drag and drop */

#ifdef OLD_DND
/*	gtk_signal_connect (obj, "motion_notify_event", GTK_SIGNAL_FUNC (start_icon_drag), di); */
	gtk_widget_dnd_drag_set (GTK_WIDGET(child->data), TRUE, drag_types, ELEMENTS (drag_types));

	gtk_signal_connect (obj, "drag_request_event", GTK_SIGNAL_FUNC (desktop_icon_drag_request), di);
	gtk_signal_connect (obj, "drag_begin_event", GTK_SIGNAL_FUNC (desktop_icon_drag_start), di);
	gtk_signal_connect (obj, "drag_end_event", GTK_SIGNAL_FUNC (desktop_icon_drag_end), di);
#endif
}

/* Called by the pop up menu: removes the icon from the desktop */
void
desktop_icon_delete (GtkWidget *widget, desktop_icon_t *di)
{
	desktop_icon_remove (di);
}

GtkWidget *
create_desktop_icon (char *file, char *text)
{
	GtkWidget *w;

	if (g_file_exists (file))
		w = desktop_icon_new (file, text);
	else {
		static char *default_image;

		if (!default_image)
			default_image = gnome_unconditional_pixmap_file ("launcher-program.png");

		if (g_file_exists (default_image))
			w = desktop_icon_new (default_image, text);
		else
			w = NULL;
	}

	return w;
}

static GtkWidget *
get_desktop_icon_for_dentry (GnomeDesktopEntry *dentry)
{
	GtkWidget *dicon;
	char *icon_label;

	icon_label = dentry->name ? dentry->name : x_basename (dentry->exec[0]);

	if (dentry->icon)
		dicon = create_desktop_icon (dentry->icon, icon_label);
	else {
		static char *default_icon_path;
		static char exists;
		
		if (!default_icon_path) {
			default_icon_path = gnome_unconditional_pixmap_file ("launcher-program.png");
			if (g_file_exists (default_icon_path))
				exists = 1;
		}

		if (exists)
			dicon = create_desktop_icon (default_icon_path, icon_label);
		else {
			dicon = gtk_window_new (GTK_WINDOW_POPUP);
			gtk_widget_set_usize (dicon, 20, 20);
		}
	}

	return dicon;
}

static GtkWidget *
get_desktop_icon_for_di (desktop_icon_t *di)
{
	GtkWidget *window;
	char *icon_label, *icon;

	icon_label = x_basename (di->pathname);

	icon = get_desktop_icon (di->pathname);
	window = create_desktop_icon (icon, icon_label);
	g_free (icon);
	return window;
}

static int
dentry_button_click (GtkWidget *widget, GdkEventButton *event, desktop_icon_t *di)
{
	if (event->button == 1){
		if (event->type == GDK_2BUTTON_PRESS)
			desktop_icon_execute (widget, di);

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
post_setup_desktop_icon (desktop_icon_t *di, int show)
{
        GList *child;
	desktop_icon_make_draggable (di);

	/* Setup the widget to make it useful: */

#ifdef OLD_DND
	/* 1. Drag and drop functionality */
  
        child = gtk_container_children(GTK_CONTAINER(di->widget));
        connect_drop_signals (GTK_WIDGET(child->data), di);
	gtk_widget_dnd_drop_set (GTK_WIDGET(child->data), TRUE, drop_types, ELEMENTS (drop_types), FALSE);
	
	/* 2. Double clicking executes the command */
	gtk_signal_connect (GTK_OBJECT (child->data), "button_press_event", GTK_SIGNAL_FUNC (dentry_button_click), di);

#endif
	if (show)
		gtk_widget_show (di->widget);
}

/* Pops up the icon properties pages */
void
desktop_icon_properties (GtkWidget *widget, desktop_icon_t *di)
{
	int retval;

	retval = item_properties (di->widget, di->pathname, di);

	if (retval & (GPROP_TITLE | GPROP_ICON | GPROP_FILENAME)) {
		gtk_widget_destroy (di->widget);

		if (di->dentry)
			di->widget = get_desktop_icon_for_dentry (di->dentry);
		else
			di->widget = get_desktop_icon_for_di (di);

		if (icons_snap_to_grid && di->grid_x != -1)
			get_icon_screen_x_y (di, &di->x, &di->y);

		gtk_widget_set_uposition (di->widget, di->x, di->y);

		post_setup_desktop_icon (di, 1);

		if (di->dentry)
			gnome_desktop_entry_save (di->dentry);
	}
}

/*
 * Activates the context sensitive menu for this icon
 */
static void
desktop_icon_context_popup (GdkEventButton *event, desktop_icon_t *di)
{
	file_popup (event, NULL, di, 0, di->dentry->exec [0]);
}

char *root_drop_types [] = {
	"icon/root",
	"url:ALL"
};

static void
desktop_load_from_dentry (GnomeDesktopEntry *dentry)
{
	GtkWidget *dicon;
	desktop_icon_t *di;

	dicon = get_desktop_icon_for_dentry (dentry);

	if (!dicon)
		return;

	di = xmalloc (sizeof (desktop_icon_t), "desktop_load_entry");
	di->dentry   = dentry;
	di->widget   = dicon;
	di->pathname = dentry->location;

	desktop_icons = g_list_prepend (desktop_icons, di);

	post_setup_desktop_icon (di, 0);
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

/* Set the drop position to NULL, we only drop the
 * first icon on the spot it was dropped, th rest
 * get auto-layouted.  Perhaps this should be an option.
 */
static void
desktop_setup_geometry_from_point (GnomeDesktopEntry *dentry, GdkPoint **point)
{
	char buffer [40];

	sprintf (buffer, "%d,%d", (*point)->x, (*point)->y);
	dentry->geometry = g_strdup (buffer);
	*point = NULL;
}

/*
 * Creates a new DIRECTORY/.directory file which is just a .dekstop
 * on directories.   And then loads it into the desktop
 */
static void
desktop_create_directory_entry (char *dentry_path, char *pathname, char *short_name, GdkPoint **pos)
{
	GnomeDesktopEntry *dentry;

	dentry = xmalloc (sizeof (GnomeDesktopEntry), "dcde");
	memset (dentry, 0, sizeof (GnomeDesktopEntry));
	dentry->name     = g_strdup (short_name);
	dentry->exec     = (char **) malloc (2 * sizeof (char *));
	dentry->exec[0]  = g_strdup (pathname);
	dentry->exec[1]  = NULL;
	dentry->exec_length = 1;
	dentry->icon     = gnome_unconditional_pixmap_file ("gnome-folder.png");
	dentry->type     = g_strdup ("Directory");
	dentry->location = g_strdup (dentry_path);
	if (pos && *pos)
		desktop_setup_geometry_from_point (dentry, pos);
	
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

static void
desktop_create_launch_entry (char *desktop_file, char *pathname, char *short_name, GdkPoint **pos)
{
	GnomeDesktopEntry *dentry;
	GtkWidget *dicon;
	desktop_icon_t *di;
	char *icon;
	struct stat s;

	stat (pathname, &s);
	dentry = xmalloc (sizeof (GnomeDesktopEntry), "launch_entry");
	memset (dentry, 0, sizeof (GnomeDesktopEntry));
	
	dentry->name     = g_strdup (short_name);
	dentry->exec     = (char **) malloc (2 * sizeof (char *));
	dentry->exec[0]  = g_strdup (pathname);
	dentry->exec[1]  = NULL;
	dentry->exec_length = 1;
	dentry->icon     = get_desktop_icon (short_name);
	dentry->type     = g_strdup ("File");
	dentry->location = g_strdup (desktop_file);
	dentry->terminal = 1;
	if (pos && *pos)
		desktop_setup_geometry_from_point (dentry, pos);
		
	gnome_desktop_entry_save (dentry);
	desktop_load_from_dentry (dentry);
#if 0
	dicon = create_desktop_icon (icon, x_basename (pathname));
	g_free (icon);
	if (!dicon)
		return;
	
	di = xmalloc (sizeof (desktop_icon_t), "dcle");
	di->dentry = NULL;
	di->widget = dicon;
	di->pathname = strdup (pathname);

	desktop_icon_set_position (di);
	desktop_icon_make_draggable (di);
	
	desktop_icons = g_list_prepend (desktop_icons, (gpointer) di);

	/* Double clicking executes the command, single clicking brings up context menu */
	gtk_signal_connect (GTK_OBJECT (dicon), "button_press_event", GTK_SIGNAL_FUNC (desktop_file_exec), di);
	gtk_widget_realize (dicon);
	
	gtk_signal_connect (GTK_OBJECT (dicon), "drop_data_available_event",
			    GTK_SIGNAL_FUNC (drop_on_launch_entry), di);

	gtk_widget_dnd_drop_set (dicon, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
#endif
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
desktop_setup_icon (char *filename, char *full_pathname, GdkPoint **desired_position)
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
				desktop_create_directory_entry (dir_full, full_pathname, filename, desired_position);
		}
		free (dir_full);
	} else {
		if (strstr (filename, ".desktop")){
			if (!desktop_pathname_loaded (full_pathname))
				desktop_load_dentry (full_pathname);
		} else {
			char *desktop_version;
			
			desktop_version = copy_strings (full_pathname, ".desktop", NULL);
			if (!exist_file (desktop_version) && !desktop_pathname_loaded (full_pathname))
				desktop_create_launch_entry (desktop_version, full_pathname, filename, desired_position);
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
desktop_reload (char *desktop_dir, GdkPoint *drop_position)
{
	struct dirent *dent;
	GList *l;
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
		desktop_setup_icon (dent->d_name, full, &drop_position);

		free (full);
	}
	mc_closedir (dir);
	
	/* Show all of the widgets */
	for (l = desktop_icons; l; l = l->next){
		desktop_icon_t *di = l->data;
		
		gtk_widget_show (di->widget);
	}
}

static void
desktop_load (char *desktop_dir)
{
	desktop_reload (desktop_dir, NULL);
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
		copy_dir_dir (mc_desktop_dir, desktop_dir, 1, 0, 0, 0);
		destroy_op_win ();
	} else 
		mkdir (desktop_dir, 0777);
	
	desktop_create_directory_entry (desktop_dir_home_link, "~", "Home directory", NULL);
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
#ifdef OLD_DND
	connect_drop_signals (rw, NULL);
	gtk_widget_dnd_drop_set (rw, TRUE, root_drop_types, ELEMENTS (root_drop_types), FALSE);
	gtk_widget_realize (rw);
	gtk_widget_show (rw);
	root_window = GNOME_ROOTWIN (rw);
#endif
}

/*
 * entry point to start up the gnome desktop
 */
void
start_desktop (void)
{
	init_spot_list ();
	desktop_directory = concat_dir_and_file (home_dir, "desktop");

	if (!exist_file (desktop_directory))
		desktop_setup_default (desktop_directory);

	desktop_root ();
	desktop_load (desktop_directory);
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

		desktop_release_desktop_icon_t (di, 0);
	}
	image_cache_destroy ();
}

#endif
