/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

/*
 * TO-DO list for the desktop;
 *
 * - Put an InputOnly window over icons to be able to select them even if the user clicks on
 *   the transparent area.
 *
 * - DnD from file windows to icons.
 *
 * - DnD from icons to desktop (move icon).
 *
 * - DnD from icons to windows.
 *
 * - DnD from icons to icons (file->directory or file->executable).
 *
 * - Popup menus for icons.
 *
 * - Select icons with rubberband on the root window.
 *
 */

#include <config.h>
#include "fs.h"
#include <gdk/gdkx.h>
#include <gtk/gtkinvisible.h>
#include <gnome.h>
#include "dialog.h"
#define DIR_H_INCLUDE_HANDLE_DIRENT /* bleah */
#include "dir.h"
#include "file.h"
#include "gdesktop.h"
#include "gdesktop-icon.h"
#include "gicon.h"
#include "gmain.h"
#include "gmetadata.h"
#include "gdnd.h"
#include "gpopup.h"
#include "../vfs/vfs.h"


/* Name of the user's desktop directory (i.e. ~/desktop) */
#define DESKTOP_DIR_NAME "desktop"


struct layout_slot {
	int num_icons;		      /* Number of icons in this slot */
	GList *icons;		      /* The list of icons in this slot */
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
static int layout_screen_width;
static int layout_screen_height;
static int layout_cols;
static int layout_rows;
static struct layout_slot *layout_slots;

#define l_slots(u, v) (layout_slots[(u) * layout_rows + (v)])

/* The last icon to be selected */
static struct desktop_icon_info *last_selected_icon;

/* Drag and drop sources and targets */

static GtkTargetEntry dnd_icon_sources[] = {
	{ "application/x-mc-desktop-icon", 0, TARGET_MC_DESKTOP_ICON },
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/plain", 0, TARGET_TEXT_PLAIN },
	{ "_NETSCAPE_URL", 0, TARGET_URL }
};

static GtkTargetEntry dnd_icon_targets[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};

static GtkTargetEntry dnd_desktop_targets[] = {
	{ "application/x-mc-desktop-icon", 0, TARGET_MC_DESKTOP_ICON },
	{ "text/uri-list", 0, TARGET_URI_LIST }
};

static int dnd_icon_nsources = sizeof (dnd_icon_sources) / sizeof (dnd_icon_sources[0]);
static int dnd_icon_ntargets = sizeof (dnd_icon_targets) / sizeof (dnd_icon_targets[0]);
static int dnd_desktop_ntargets = sizeof (dnd_desktop_targets) / sizeof (dnd_desktop_targets[0]);

/* Proxy window for DnD on the root window */
static GtkWidget *dnd_proxy_window;

/* Offsets for the DnD cursor hotspot */
static int dnd_press_x, dnd_press_y;

/* Whether a call to select_icon() is pending because the initial click on an
 * icon had the GDK_CONTROL_MASK in it.  */
static int dnd_select_icon_pending;

/* Proxy window for clicks on the root window */
static GdkWindow *click_proxy_gdk_window;
static GtkWidget *click_proxy_invisible;

/* GC for drawing the rubberband rectangle */
static GdkGC *click_gc;

/* Starting click position and event state for rubberbanding on the desktop */
static int click_start_x;
static int click_start_y;
static int click_start_state;

/* Current mouse position for rubberbanding on the desktop */
static int click_current_x;
static int click_current_y;

static int click_dragging;


static struct desktop_icon_info *desktop_icon_info_new (char *filename, int auto_pos, int xpos, int ypos);


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
	int u, v;
	int val, dist;
	int dx, dy;

	min = l_slots (0, 0).num_icons;
	min_x = min_y = 0;
	min_dist = INT_MAX;

	for (u = 0; u < layout_cols; u++)
		for (v = 0; v < layout_rows; v++) {
			val = l_slots (u, v).num_icons;

			dx = *x - u * DESKTOP_SNAP_X;
			dy = *y - v * DESKTOP_SNAP_Y;
			dist = dx * dx + dy * dy;

			if ((val == min && dist < min_dist) || (val < min)) {
				min = val;
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

/*
 * Places a desktop icon.  If auto_pos is true, then the function will
 * look for a place to position the icon automatically, else it will
 * use the specified coordinates, snapped to the grid if the global
 * desktop_snap_icons flag is set.
 */
static void
desktop_icon_info_place (struct desktop_icon_info *dii, int auto_pos, int xpos, int ypos)
{
	int u, v;
	char *filename;

	if (auto_pos) {
		if (desktop_auto_placement)
			get_icon_auto_pos (&xpos, &ypos);
		else if (desktop_snap_icons)
			get_icon_snap_pos (&xpos, &ypos);
	}

	if (xpos < 0)
		xpos = 0;
	else if (xpos > layout_screen_width)
		xpos = layout_screen_width - DESKTOP_SNAP_X;

	if (ypos < 0)
		ypos = 0;
	else if (ypos > layout_screen_height)
		ypos = layout_screen_height - DESKTOP_SNAP_Y;

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

	/* Save the information */

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);
	gmeta_set_icon_pos (filename, dii->x, dii->y);
	g_free (filename);
}

/*
 * Returns TRUE if there is already an icon in the desktop for the
 * specified filename, FALSE otherwise.
 */
static int
icon_exists (char *filename)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;
			if (strcmp (filename, dii->filename) == 0)
				return TRUE;
		}

	return FALSE;
}

static GList *
icon_exists_in_list (GList *list, char *filename)
{
	GList *l;

	for (l = list; l; l = l->next){
		struct desktop_icon_info *dii = l->data;

		if (strcmp (filename, dii->filename) == 0)
			return l;
	}
	return NULL;
}

/*
 * Returns a GList with all of the icons on the desktop
 */
GList *
desktop_get_all_icons (void)
{
	GList *l, *res;
	int i;

	res = NULL;
	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots [i].icons; l; l = l->next){
			res = g_list_prepend (res, l->data);
		}

	return res;
}

/*
 * Reads the ~/desktop directory and creates the desktop icons.  If
 * incremental is TRUE, then an icon will not be created for a file if
 * there is already an icon for it, and icons will be created starting
 * at the specified position.
 */
static void
load_desktop_icons (int incremental, int xpos, int ypos)
{
	struct dirent *dirent;
	DIR *dir;
	char *full_name;
	int have_pos, x, y;
	struct desktop_icon_info *dii;
	GSList *need_position_list, *l;
	GList *all_icons;

	dir = mc_opendir (desktop_directory);
	if (!dir) {
		message (FALSE,
			 _("Warning"),
			 _("Could not open %s; will not have initial desktop icons"),
			 desktop_directory);
		return;
	}

	/*
	 * First create the icons for all the files that do have their
	 * icon position set.  Build a list of the icons that do not
	 * have their position set.
	 */

	need_position_list = NULL;

	all_icons = desktop_get_all_icons ();
	
	while ((dirent = mc_readdir (dir)) != NULL) {
		if (((dirent->d_name[0] == '.') && (dirent->d_name[1] == 0))
		    || ((dirent->d_name[0] == '.') && (dirent->d_name[1] == '.') && (dirent->d_name[2] == 0)))
			continue;

		if (incremental){
			GList *element;

			element = icon_exists_in_list (all_icons, dirent->d_name);
			
			if (element){
				g_list_remove_link (all_icons, element);
				continue;
			}

		}
		
		full_name = g_concat_dir_and_file (desktop_directory, dirent->d_name);

		have_pos = gmeta_get_icon_pos (full_name, &x, &y);

		if (have_pos) {
			dii = desktop_icon_info_new (dirent->d_name, FALSE, x, y);
			gtk_widget_show (dii->dicon);

			g_free (full_name);
		} else
			need_position_list = g_slist_prepend (need_position_list, g_strdup (dirent->d_name));
	}

	mc_closedir (dir);

	/*
	 * all_icons now contains a list of all of the icons that were not found
	 * in the ~/desktop directory, remove them.
	 */
	if (incremental){
		GList *l;
		
		for (l = all_icons; l; l = l->next){
			struct desktop_icon_info *dii = l->data;

			desktop_icon_destroy (dii);
		}
	}
	g_list_free (all_icons);

	/*
	 * Now create the icons for all the files that did not have their position set.  This makes
	 * auto-placement work correctly without overlapping icons.
	 */

	need_position_list = g_slist_reverse (need_position_list);

	for (l = need_position_list; l; l = l->next) {
		dii = desktop_icon_info_new (l->data, TRUE, xpos, ypos);
		gtk_widget_show (dii->dicon);
		g_free (l->data);
	}

	g_slist_free (need_position_list);
}

/* Destroys all the current desktop icons */
static void
destroy_desktop_icons (void)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++) {
		l = layout_slots[i].icons;

		while (l) {
			dii = l->data;
			l = l->next;

			desktop_icon_destroy (dii);
		}
	}
}

/*
 * Reloads the desktop icons.  If incremental is TRUE, then the
 * existing icons will not be destroyed first, and the new icons will
 * be put at the specified position.
 */
static void
reload_desktop_icons (int incremental, int x, int y)
{
	if (!incremental)
		destroy_desktop_icons ();

	load_desktop_icons (incremental, x, y);
	x_flush_events ();
}

/* Unselects all the desktop icons except the one in exclude */
static void
unselect_all (struct desktop_icon_info *exclude)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			if (dii->selected && dii != exclude) {
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

/*
 * Handles icon selection and unselection due to button presses.  The
 * event_state is the state field of the event.
 */
static void
select_icon (struct desktop_icon_info *dii, int event_state)
{
	int range;
	int additive;

	range = ((event_state & GDK_SHIFT_MASK) != 0);
	additive = ((event_state & GDK_CONTROL_MASK) != 0);

	if (!additive)
		unselect_all (NULL);

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

/* Creates a file entry structure and fills it with information appropriate to the specified file.  */
static file_entry *
file_entry_from_file (char *filename)
{
	file_entry *fe;
	struct stat s;

	if (mc_lstat (filename, &s) == -1) {
		g_warning ("Could not stat %s, bad things will happen", filename);
		return NULL;
	}

	fe = g_new (file_entry, 1);
	fe->fname = g_strdup (x_basename (filename));
	fe->fnamelen = strlen (fe->fname);
	fe->buf = s;
	fe->f.marked = FALSE;
	fe->f.link_to_dir = FALSE;
	fe->f.stalled_link = FALSE;

	if (S_ISLNK (s.st_mode)) {
		struct stat s2;

		if (mc_stat (filename, &s2) == 0)
			fe->f.link_to_dir = S_ISDIR (s2.st_mode) != 0;
		else
			fe->f.stalled_link = TRUE;
	}

	return fe;
}

/* Frees a file entry structure */
static void
file_entry_free (file_entry *fe)
{
	if (fe->fname)
		g_free (fe->fname);

	g_free (fe);
}

/*
 * Callback used when an icon's text changes.  We must validate the
 * rename and return the appropriate value.  The desktop icon info
 * structure is passed in the user data.
 */
static int
text_changed (GnomeIconTextItem *iti, gpointer data)
{
	struct desktop_icon_info *dii;
	char *new_name;
	char *source;
	char *dest;
	int retval;

	dii = data;

	source = g_concat_dir_and_file (desktop_directory, dii->filename);
	new_name = gnome_icon_text_item_get_text (iti);
	dest = g_concat_dir_and_file (desktop_directory, new_name);

	if (mc_rename (source, dest) == 0) {
		g_free (dii->filename);
		dii->filename = g_strdup (new_name);
		retval = TRUE;
	} else
		retval = FALSE; /* FIXME: maybe pop up a warning/query dialog? */

	g_free (source);
	g_free (dest);

	return retval;
}

/*
 * Callback used when the user begins editing the icon text item in a
 * desktop icon.  It installs the mouse and keyboard grabs that are
 * required while an icon is being edited.
 */
static void
editing_started (GnomeIconTextItem *iti, gpointer data)
{
	struct desktop_icon_info *dii;
	GdkCursor *ibeam;

	dii = data;

	/* Disable drags from this icon until editing is finished */

	gtk_drag_source_unset (DESKTOP_ICON (dii->dicon)->canvas);

	/* Unselect all icons but this one */
	unselect_all (dii);

	ibeam = gdk_cursor_new (GDK_XTERM);
	gdk_pointer_grab (dii->dicon->window,
			  TRUE,
			  (GDK_BUTTON_PRESS_MASK
			   | GDK_BUTTON_RELEASE_MASK
			   | GDK_POINTER_MOTION_MASK
			   | GDK_ENTER_NOTIFY_MASK
			   | GDK_LEAVE_NOTIFY_MASK),
			  NULL,
			  ibeam,
			  GDK_CURRENT_TIME);
	gtk_grab_add (dii->dicon);
	gdk_cursor_destroy (ibeam);

	gdk_keyboard_grab (GTK_LAYOUT (DESKTOP_ICON (dii->dicon)->canvas)->bin_window, FALSE, GDK_CURRENT_TIME);
}

/* Sets up the specified icon as a drag source, but does not connect the signals */
static void
setup_icon_dnd_actions (struct desktop_icon_info *dii)
{
	gtk_drag_source_set (DESKTOP_ICON (dii->dicon)->canvas,
			     GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			     dnd_icon_sources,
			     dnd_icon_nsources,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
}

/*
 * Callback used when the user finishes editing the icon text item in
 * a desktop icon.  It removes the mouse and keyboard grabs.
 */
static void
editing_stopped (GnomeIconTextItem *iti, gpointer data)
{
	struct desktop_icon_info *dii;

	dii = data;

	gtk_grab_remove (dii->dicon);
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);

	/* Re-enable drags from this icon */

	setup_icon_dnd_actions (dii);
}

/* Used to open a desktop icon when the user double-clicks on it */
void
desktop_icon_open (struct desktop_icon_info *dii)
{
	char *filename;
	file_entry *fe;

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);

	fe = file_entry_from_file (filename);

	if (S_ISDIR (fe->buf.st_mode) || link_isdir (fe))
		new_panel_at (filename);
	else
		do_enter_on_file_entry (fe);

	file_entry_free (fe);
}

void
desktop_icon_delete (struct desktop_icon_info *dii)
{
	char *full_name;
	struct stat s;
	long progress_count = 0;
	double progress_bytes = 0;

	/* 1. Delete the file */
	create_op_win (OP_DELETE, 1);
	x_flush_events ();
	
	full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
	stat (full_name, &s);
	if (S_ISDIR (s.st_mode))
		erase_dir (full_name, &progress_count, &progress_bytes);
	else
		erase_file (full_name, &progress_count, &progress_bytes, TRUE);
	g_free (full_name);
	destroy_op_win ();

	/* 2. Destroy the dicon */
	desktop_icon_destroy (dii);
}

/* Used to execute the popup menu for desktop icons */
static void
do_popup_menu (struct desktop_icon_info *dii, GdkEventButton *event)
{
	char *filename;

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);

	if (gpopup_do_popup (event, NULL, dii, 0, filename) != -1)
		reload_desktop_icons (TRUE, 0, 0); /* bleah */

	g_free (filename);
}

/* Callback activated when a button is redirected from the desktop to
 * the icon during a grab.
 */
static gint
window_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct desktop_icon_info *dii;
	GtkWidget *parent;

	dii = data;

	/* We should only get this while editing. But check anyways -
	 * we ignore events in a child of our a event widget
	 */
	parent = gtk_get_event_widget ((GdkEvent *)event);
	if (parent)
		parent = parent->parent;

	while (parent) {
		if (widget == parent)
			return FALSE;
		parent = parent->parent;
	}
	
	if (GNOME_ICON_TEXT_ITEM (DESKTOP_ICON (dii->dicon)->text)->editing) {
		gnome_icon_text_item_stop_editing (
			GNOME_ICON_TEXT_ITEM (
				DESKTOP_ICON (dii->dicon)->text), TRUE);
		return TRUE;
	}

	return FALSE;
}

/* Callback used when a button is pressed on a desktop icon */
static gint
icon_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct desktop_icon_info *dii;
	int retval;

	dii = data;

	/* If the text is being edited, do not handle clicks by ourselves */

	if (GNOME_ICON_TEXT_ITEM (DESKTOP_ICON (dii->dicon)->text)->editing)
		return FALSE;

	/* Save the mouse position for DnD */

	dnd_press_x = event->x;
	dnd_press_y = event->y;

	/* Process the event */

	retval = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1) {
			/* If (only) the Control key is down, then we have to delay the icon selection */

			dnd_select_icon_pending = ((event->state & GDK_CONTROL_MASK)
						   && !((event->state & GDK_CONTROL_MASK)
							&& (event->state & GDK_SHIFT_MASK)));

			if (!dnd_select_icon_pending) {
				select_icon (dii, event->state);
				retval = TRUE;
			}
		} else if (event->button == 3) {
			do_popup_menu (dii, event);
			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button != 1)
			break;

		desktop_icon_open (dii);
		retval = TRUE;
		break;

	default:
		break;
	}

	/* Keep the canvas items from getting the signal */
#if 0
	if (retval)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "button_press_event");
#endif
	return retval;
}

/* Handler for button releases on desktop icons.  If there was a pending
 * selection on the icon, then the function performs the selection.
 */
static gint
icon_button_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct desktop_icon_info *dii;

	dii = data;

	if (dnd_select_icon_pending) {
		select_icon (dii, GDK_CONTROL_MASK);
		dnd_select_icon_pending = FALSE;

		return TRUE;
	} else if (GNOME_ICON_TEXT_ITEM (DESKTOP_ICON (dii->dicon)->text)->selecting) {
		dii->finishing_selection = TRUE;
	}

	return FALSE;
}

/* Handler for button releases on desktop icons.  If there was a pending
 * selection on the icon, then the function performs the selection.
 */
static gint
icon_button_release_after (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct desktop_icon_info *dii;

	dii = data;

	if (dii->finishing_selection) {
		/* Restore the pointer grab here because the icon item just
		 * called gdk_pointer_ungrab()
		 */
		GdkCursor *ibeam = gdk_cursor_new (GDK_XTERM);
		gdk_pointer_grab (dii->dicon->window,
				  TRUE,
				  (GDK_BUTTON_PRESS_MASK
				   | GDK_BUTTON_RELEASE_MASK
				   | GDK_POINTER_MOTION_MASK
				   | GDK_ENTER_NOTIFY_MASK
				   | GDK_LEAVE_NOTIFY_MASK),
				  NULL,
				  ibeam,
				  GDK_CURRENT_TIME);
		gdk_cursor_destroy (ibeam);

		dii->finishing_selection = FALSE;
	}

	return FALSE;
}

/* Callback used when a drag from the desktop icons is started.  We set the drag icon to the proper
 * pixmap.
 */
static void
drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	struct desktop_icon_info *dii;
	DesktopIcon *dicon;
	GtkArg args[3];
	GdkImlibImage *im;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	int x, y;

	dii = data;
	dicon = DESKTOP_ICON (dii->dicon);

	/* See if the icon was pending to be selected */

	if (dnd_select_icon_pending) {
		if (!dii->selected)
			select_icon (dii, GDK_CONTROL_MASK);

		dnd_select_icon_pending = FALSE;
	}

	/* FIXME: see if it is more than one icon and if so, use a multiple-files icon. */

	args[0].name = "image";
	args[1].name = "x";
	args[2].name = "y";
	gtk_object_getv (GTK_OBJECT (dicon->icon), 3, args);
	im = GTK_VALUE_BOXED (args[0]);
	x = GTK_VALUE_DOUBLE (args[1]);
	y = GTK_VALUE_DOUBLE (args[2]);

	gdk_imlib_render (im, im->rgb_width, im->rgb_height);
	pixmap = gdk_imlib_copy_image (im);
	mask = gdk_imlib_copy_mask (im);

	gtk_drag_set_icon_pixmap (context,
				  gtk_widget_get_colormap (dicon->canvas),
				  pixmap,
				  mask,
				  dnd_press_x - x,
				  dnd_press_y - y);

	gdk_pixmap_unref (pixmap);
	gdk_bitmap_unref (mask);
}

/* Builds a string with the URI-list of the selected desktop icons */
static char *
build_selected_icons_uri_list (int *len)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;
	char *filelist, *p;
	int desktop_dir_len;

	/* First, count the number of selected icons and add up their filename lengths */

	*len = 0;
	desktop_dir_len = strlen (desktop_directory);

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			/* "file:" + desktop_directory + "/" + dii->filename + "\r\n" */

			if (dii->selected)
				*len += 5 + desktop_dir_len + 1 + strlen (dii->filename) + 2;
		}

	/* Second, create the file list string */

	filelist = g_new (char, *len + 1); /* plus 1 for null terminator */
	p = filelist;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			if (dii->selected) {
				strcpy (p, "file:");
				p += 5;

				strcpy (p, desktop_directory);
				p += desktop_dir_len;

				*p++ = '/';

				strcpy (p, dii->filename);
				p += strlen (dii->filename);

				strcpy (p, "\r\n");
				p += 2;
			}
		}

	*p = 0;

	return filelist;
}

/* Callback used to get the drag data from the desktop icons */
static void
drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
	       guint info, guint32 time, gpointer data)
{
	struct desktop_icon_info *dii;
	char *filelist;
	int len;
	GList *files;

	dii = data;
	filelist = build_selected_icons_uri_list (&len);

	switch (info) {
	case TARGET_MC_DESKTOP_ICON:
	case TARGET_URI_LIST:
	case TARGET_TEXT_PLAIN:
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					filelist,
					len);
		break;

        case TARGET_URL:
		files = gnome_uri_list_extract_uris (filelist);
		if (files) {
			gtk_selection_data_set (selection_data,
						selection_data->target,
						8,
						files->data,
						strlen (files->data));
		}
		gnome_uri_list_free_strings (files);
		break;

	default:
		g_assert_not_reached ();
	}

	g_free (filelist);
}

/* Set up a desktop icon as a DnD source */
static void
setup_icon_dnd_source (struct desktop_icon_info *dii)
{
	setup_icon_dnd_actions (dii);

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_begin",
			    (GtkSignalFunc) drag_begin,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get),
			    dii);
}

/**
 * drop_on_file_entry
 */
static void
desktop_icon_drop_uri_list (struct desktop_icon_info *dii,
			    GdkDragContext           *context,
			    GtkSelectionData         *data)
{
	char *filename;
	file_entry *fe;
	int size;
	char *buf, *mime_type;
	
	filename = g_concat_dir_and_file (desktop_directory, dii->filename);

	fe = file_entry_from_file (filename);
	if (!fe)
		return; /* eek */

	/*
	 * 1. If it is directory, drop the files there
	 */
	if (fe->f.link_to_dir){
		gdnd_drop_on_directory (context, data, filename);
		goto out;
	}

	/*
	 * 2. Try to use a metadata-based drop action 
	 */
	if (gnome_metadata_get (filename, "drop-action", &size, &buf) == 0){
		/*action_drop (filename, buf, context, data);*/ /* Fixme: i'm undefined */
		free (buf);
		goto out;
	}

	/* 
	 * 3. Try a drop action from the mime-type
	 */
	mime_type = gnome_mime_type_or_default (filename, NULL);
	if (mime_type){
		char *action;
		
		action = gnome_mime_get_value (mime_type, "drop-action");
		
		if (action){
			/*action_drop (filename, action, context, data);*/ /* Fixme: i'm undefined */
			goto out;
		}
	}

	/*
	 * 4. Executable.  Try metadata keys for "open" and mime type for "open"
	 */
	if (is_exe (fe->buf.st_mode) && if_link_is_exe (fe)){
		
	}

out:
	file_entry_free (fe);
}

static void
icon_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
			 GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	struct desktop_icon_info *dii;

	dii = user_data;

	switch (info) {
	case TARGET_URI_LIST:
		desktop_icon_drop_uri_list (dii, context, data);
		break;

	default:
		break;
	}
}

/* Set up a desktop icon as a DnD destination */
static void
setup_icon_dnd_dest (struct desktop_icon_info *dii)
{
	char *filename;
	file_entry *fe;
	int actions;

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);
	fe = file_entry_from_file (filename);
	g_free (filename);

	if (!fe)
		return; /* eek */

	/* See what actions are appropriate for this icon */

	if (fe->f.link_to_dir)
		actions = GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK;
	else if (is_exe (fe->buf.st_mode) && if_link_is_exe (fe))
		actions = GDK_ACTION_COPY;
	else
		actions = 0;

	file_entry_free (fe);

	if (!actions)
		return;

	/* Connect the drop signals */

	gtk_drag_dest_set (DESKTOP_ICON (dii->dicon)->canvas,
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			   dnd_icon_targets,
			   dnd_icon_ntargets,
			   actions);

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_data_received",
			    GTK_SIGNAL_FUNC (icon_drag_data_received),
			    dii);
}

/*
 * Creates a new desktop icon.  The filename is the pruned filename
 * inside the desktop directory.  If auto_pos is false, it will use
 * the specified coordinates for the icon.  Else, it will use auto-
 * positioning trying to start at the specified coordinates.  It does
 * not show the icon.
 */
static struct desktop_icon_info *
desktop_icon_info_new (char *filename, int auto_pos, int xpos, int ypos)
{
	struct desktop_icon_info *dii;
	file_entry *fe;
	char *full_name;
	GdkImlibImage *icon_im;

	/* Create the icon structure */

	full_name = g_concat_dir_and_file (desktop_directory, filename);
	fe = file_entry_from_file (full_name);
	icon_im = gicon_get_icon_for_file_speed (fe, FALSE);

	dii = g_new (struct desktop_icon_info, 1);
	dii->dicon = desktop_icon_new (icon_im, filename);
	dii->x = 0;
	dii->y = 0;
	dii->slot = -1;
	dii->filename = g_strdup (filename);
	dii->selected = FALSE;

	file_entry_free (fe);
	g_free (full_name);

	/* Connect to the icon's signals */

	gtk_signal_connect_after (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "button_press_event",
			    (GtkSignalFunc) icon_button_press,
			    dii);
	gtk_signal_connect (GTK_OBJECT (dii->dicon), "button_press_event",
			    (GtkSignalFunc) window_button_press,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "button_release_event",
				  (GtkSignalFunc) icon_button_release,
				  dii);
	gtk_signal_connect_after (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "button_release_event",
				  (GtkSignalFunc) icon_button_release_after,
				  dii);

	/* Connect to the text item's signals */

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "text_changed",
			    (GtkSignalFunc) text_changed,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "editing_started",
			    (GtkSignalFunc) editing_started,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "editing_stopped",
			    (GtkSignalFunc) editing_stopped,
			    dii);

	/* Prepare the DnD functionality for this icon */

	setup_icon_dnd_source (dii);
	setup_icon_dnd_dest (dii);

	/* Place the icon and append it to the list */

	desktop_icon_info_place (dii, auto_pos, xpos, ypos);
	return dii;
}

/*
 * Frees a desktop icon information structure, and destroy the icon
 * widget.  Does not remove the structure from the desktop_icons list!
 */
void
desktop_icon_destroy (struct desktop_icon_info *dii)
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
	layout_screen_width = gdk_screen_width ();
	layout_screen_height = gdk_screen_height ();
	layout_cols = (layout_screen_width + DESKTOP_SNAP_X - 1) / DESKTOP_SNAP_X;
	layout_rows = (layout_screen_height + DESKTOP_SNAP_Y - 1) / DESKTOP_SNAP_Y;
	layout_slots = g_new0 (struct layout_slot, layout_cols * layout_rows);
}

static void
setup_trashcan (char *desktop_dir)
{
	char *trashcan_dir;
	char *trash_pix;
	
	trashcan_dir = g_concat_dir_and_file (desktop_directory, _("Trashcan"));
	trash_pix = g_concat_dir_and_file (ICONDIR, "trash.xpm");
		
	if (!g_file_exists (trashcan_dir)){
		mkdir (trashcan_dir, 0777);
		gnome_metadata_set (
			trashcan_dir, "icon-filename", strlen (trash_pix)+1, trash_pix);
	}
	
	g_free (trashcan_dir);
	g_free (trash_pix);
}

/*
 * Check that the user's desktop directory exists, and if not, create
 * the default desktop setup.
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
		}
		g_free (home_link_name);
	}

/*	setup_trashcan (desktop_directory); */
}

/* Sets up a proxy window for DnD on the specified X window.  Courtesy of Owen Taylor */
static gboolean 
setup_xdnd_proxy (guint32 xid, GdkWindow *proxy_window)
{
	GdkAtom xdnd_proxy_atom;
	guint32 proxy_xid;
	Atom type;
	int format;
	unsigned long nitems, after;
	Window *proxy_data;
	Window proxy;
	guint32 old_warnings;

	XGrabServer (GDK_DISPLAY ());

	xdnd_proxy_atom = gdk_atom_intern ("XdndProxy", FALSE);
	proxy_xid = GDK_WINDOW_XWINDOW (proxy_window);
	type = None;
	proxy = None;

	old_warnings = gdk_error_warnings;

	gdk_error_code = 0;
	gdk_error_warnings = 0;

	/* Check if somebody else already owns drops on the root window */

	XGetWindowProperty (GDK_DISPLAY (), xid,
			    xdnd_proxy_atom, 0,
			    1, False, AnyPropertyType,
			    &type, &format, &nitems, &after,
			    (guchar **) &proxy_data);

	if (type != None) {
		if (format == 32 && nitems == 1)
			proxy = *proxy_data;

		XFree (proxy_data);
	}

	/* The property was set, now check if the window it points to exists and
	 * has a XdndProxy property pointing to itself.
	 */
	if (proxy) {
		XGetWindowProperty (GDK_DISPLAY (), proxy, 
				    xdnd_proxy_atom, 0, 
				    1, False, AnyPropertyType,
				    &type, &format, &nitems, &after, 
				    (guchar **) &proxy_data);

		if (!gdk_error_code && type != None) {
			if (format == 32 && nitems == 1)
				if (*proxy_data != proxy)
					proxy = GDK_NONE;

			XFree (proxy_data);
		} else
			proxy = GDK_NONE;
	}

	if (!proxy) {
		/* OK, we can set the property to point to us */

		XChangeProperty (GDK_DISPLAY (), xid,
				 xdnd_proxy_atom, gdk_atom_intern ("WINDOW", FALSE),
				 32, PropModeReplace,
				 (guchar *) &proxy_xid, 1);
	}

	gdk_error_code = 0;
	gdk_error_warnings = old_warnings;
  
	XUngrabServer (GDK_DISPLAY ());
	gdk_flush ();

	if (!proxy) {
		/* Mark our window as a valid proxy window with a XdndProxy
		 * property pointing recursively;
		 */
		XChangeProperty (GDK_DISPLAY (), proxy_xid,
				 xdnd_proxy_atom, gdk_atom_intern ("WINDOW", FALSE),
				 32, PropModeReplace,
				 (guchar *) &proxy_xid, 1);
      
		return TRUE;
	} else
		return FALSE;
}

/* Returns the desktop icon that started the drag from the specified context */
struct desktop_icon_info *
find_icon_by_drag_context (GdkDragContext *context)
{
	GtkWidget *source;
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	source = gtk_drag_get_source_widget (context);
	if (!source)
		return NULL;

	source = gtk_widget_get_toplevel (source);

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			if (dii->dicon == source)
				return dii;
		}

	return NULL;
}

/*
 * Performs a drop of desktop icons onto the desktop.  It basically moves the icons from their
 * original position to the new coordinates.
 */
static void
drop_desktop_icons (GdkDragContext *context, GtkSelectionData *data, int x, int y)
{
	struct desktop_icon_info *source_dii, *dii;
	int dx, dy;
	int i;
	GList *l;
	GSList *sel_icons, *sl;

	/* FIXME: this needs to do the right thing (what Windows does) when desktop_auto_placement
	 * is enabled.
	 */

	/* Find the icon that the user is dragging */

	source_dii = find_icon_by_drag_context (context);
	if (!source_dii) {
		g_warning ("Eeeeek, could not find the icon that started the drag!");
		return;
	}

	/* Compute the distance to move icons */

	if (desktop_snap_icons)
		get_icon_snap_pos (&x, &y);

	dx = x - source_dii->x - dnd_press_x;
	dy = y - source_dii->y - dnd_press_y;

	/* Build a list of selected icons */

	sel_icons = NULL;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;
			if (dii->selected)
				sel_icons = g_slist_prepend (sel_icons, l->data);
		}

	/* Move the icons */

	for (sl = sel_icons; sl; sl = sl->next) {
		dii = sl->data;
		desktop_icon_info_place (dii, FALSE, dii->x + dx, dii->y + dy);
	}

	/* Clean up */

	g_slist_free (sel_icons);
}

/* Callback used when the root window receives a drop */
static void
desktop_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
			    GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	int retval;
	gint dx, dy;

	/* Fix the proxy window offsets */

	gdk_window_get_position (widget->window, &dx, &dy);
	x += dx;
	y += dy;

	switch (info) {
	case TARGET_MC_DESKTOP_ICON:
		drop_desktop_icons (context, data, x, y);
		break;

	case TARGET_URI_LIST:
		retval = gdnd_drop_on_directory (context, data, desktop_directory);
		if (retval)
			reload_desktop_icons (TRUE, x, y);

		break;

	default:
		break;
	}
}

/* Sets up drag and drop to the desktop root window */
static void
setup_desktop_dnd (void)
{
	dnd_proxy_window = gtk_invisible_new ();
	gtk_widget_show (dnd_proxy_window);

	if (!setup_xdnd_proxy (GDK_ROOT_WINDOW (), dnd_proxy_window->window))
		g_warning ("There is already a process taking drop windows on the desktop\n");

	gtk_drag_dest_set (dnd_proxy_window,
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			   dnd_desktop_targets,
			   dnd_desktop_ntargets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (dnd_proxy_window), "drag_data_received",
			    GTK_SIGNAL_FUNC (desktop_drag_data_received),
			    NULL);
}

/* Looks for the proxy window to get root window clicks from the window manager */
static GdkWindow *
find_click_proxy_window (void)
{
	GdkAtom click_proxy_atom;
	Atom type;
	int format;
	unsigned long nitems, after;
	Window *proxy_data;
	Window proxy;
	guint32 old_warnings;
	GdkWindow *proxy_gdk_window;

	XGrabServer (GDK_DISPLAY ());

	click_proxy_atom = gdk_atom_intern ("_WIN_DESKTOP_BUTTON_PROXY", FALSE);
	type = None;
	proxy = None;

	old_warnings = gdk_error_warnings;

	gdk_error_code = 0;
	gdk_error_warnings = 0;

	/* Check if the proxy window exists */

	XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
			    click_proxy_atom, 0,
			    1, False, AnyPropertyType,
			    &type, &format, &nitems, &after,
			    (guchar **) &proxy_data);

	if (type != None) {
		if (format == 32 && nitems == 1)
			proxy = *proxy_data;

		XFree (proxy_data);
	}

	/* The property was set, now check if the window it points to exists and
	 * has a _WIN_DESKTOP_BUTTON_PROXY property pointing to itself.
	 */

	if (proxy) {
		XGetWindowProperty (GDK_DISPLAY (), proxy,
				    click_proxy_atom, 0,
				    1, False, AnyPropertyType,
				    &type, &format, &nitems, &after,
				    (guchar **) &proxy_data);

		if (!gdk_error_code && type != None) {
			if (format == 32 && nitems == 1)
				if (*proxy_data != proxy)
					proxy = GDK_NONE;

			XFree (proxy_data);
		} else
			proxy = GDK_NONE;
	}

	gdk_error_code = 0;
	gdk_error_warnings = old_warnings;

	XUngrabServer (GDK_DISPLAY ());
	gdk_flush ();

	if (proxy)
		proxy_gdk_window = gdk_window_foreign_new (proxy);
	else
		proxy_gdk_window = NULL;

	return proxy_gdk_window;
}

/* Executes the popup menu for the desktop */
static void
desktop_popup (GdkEventButton *event)
{
	printf ("FIXME: display desktop popup menu\n");
}

/* Draws the rubberband rectangle for selecting icons on the desktop */
static void
draw_rubberband (int x, int y)
{
	int x1, y1, x2, y2;

	if (click_start_x < x) {
		x1 = click_start_x;
		x2 = x;
	} else {
		x1 = x;
		x2 = click_start_x;
	}

	if (click_start_y < y) {
		y1 = click_start_y;
		y2 = y;
	} else {
		y1 = y;
		y2 = click_start_y;
	}
	
	gdk_draw_rectangle (GDK_ROOT_PARENT (), click_gc, FALSE, x1, y1, x2 - x1, y2 - y1);
}

/*
 * Stores dii->selected into dii->tmp_selected to keep the original selection
 * around while the user is rubberbanding.
 */
static void
store_temp_selection (void)
{
	int i;
	GList *l;
	struct desktop_icon_info *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			dii->tmp_selected = dii->selected;
		}
}

/**
 * icon_is_in_area:
 * @dii: the desktop icon information
 * 
 * Returns TRUE if the specified icon is at least partially inside the specified
 * area, or FALSE otherwise.
 */
static int
icon_is_in_area (struct desktop_icon_info *dii, int x1, int y1, int x2, int y2)
{
	DesktopIcon *dicon;

	dicon = DESKTOP_ICON (dii->dicon);

	/* FIXME: this only intersects the rectangle with the icon image's
	 * bounds.  Doing the "hard" intersection with the actual shape of the
	 * image is left as an exercise to the reader.
	 */

	x1 -= dii->x;
	y1 -= dii->y;
	x2 -= dii->x;
	y2 -= dii->y;

	if (x1 < dicon->icon_x + dicon->icon_w - 1
	    && x2 > dicon->icon_x
	    && y1 < dicon->icon_y + dicon->icon_h - 1
	    && y2 > dicon->icon_y)
		return TRUE;

	if (x1 < dicon->text_x + dicon->text_w - 1
	    && x2 > dicon->text_x
	    && y1 < dicon->text_y + dicon->text_h - 1
	    && y2 > dicon->text_y)
		return TRUE;

	return FALSE;
}

/* Update the selection being rubberbanded.  It selects or unselects the icons
 * as appropriate.
 */
static void
update_drag_selection (int x, int y)
{
	int x1, y1, x2, y2;
	int i;
	GList *l;
	struct desktop_icon_info *dii;
	int additive, invert, in_area;

	if (click_start_x < x) {
		x1 = click_start_x;
		x2 = x;
	} else {
		x1 = x;
		x2 = click_start_x;
	}

	if (click_start_y < y) {
		y1 = click_start_y;
		y2 = y;
	} else {
		y1 = y;
		y2 = click_start_y;
	}

	/* Select or unselect icons as appropriate */

	additive = click_start_state & GDK_SHIFT_MASK;
	invert = click_start_state & GDK_CONTROL_MASK;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots[i].icons; l; l = l->next) {
			dii = l->data;

			in_area = icon_is_in_area (dii, x1, y1, x2, y2);

			if (in_area) {
				if (invert) {
					if (dii->selected == dii->tmp_selected) {
						desktop_icon_select (DESKTOP_ICON (dii->dicon), !dii->selected);
						dii->selected = !dii->selected;
					}
				} else if (additive) {
					if (!dii->selected) {
						desktop_icon_select (DESKTOP_ICON (dii->dicon), TRUE);
						dii->selected = TRUE;
					}
				} else {
					if (!dii->selected) {
						desktop_icon_select (DESKTOP_ICON (dii->dicon), TRUE);
						dii->selected = TRUE;
					}
				}
			} else if (dii->selected != dii->tmp_selected) {
				desktop_icon_select (DESKTOP_ICON (dii->dicon), dii->tmp_selected);
				dii->selected = dii->tmp_selected;
			}

		}
}

/* Handles button presses on the root window via the click_proxy_gdk_window */
static gint
click_proxy_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GdkCursor *cursor;

	if (event->button == 1) {
		click_start_x = event->x;
		click_start_y = event->y;
		click_start_state = event->state;

		XGrabServer (GDK_DISPLAY ());

		cursor = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
		gdk_pointer_grab (GDK_ROOT_PARENT (),
				  FALSE,
				  GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
				  NULL,
				  cursor,
				  event->time);
		gdk_cursor_destroy (cursor);

		/* If no modifiers are pressed, we unselect all the icons */

		if ((click_start_state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0)
			unselect_all (NULL);

		store_temp_selection (); /* Save the original selection */

		draw_rubberband (event->x, event->y);
		click_current_x = event->x;
		click_current_y = event->y;
		click_dragging = TRUE;

		return TRUE;
	} else if (event->button == 3) {
		desktop_popup (event);
		return TRUE;
	}

	return FALSE;
}

/* Handles button releases on the root window via the click_proxy_gdk_window */
static gint
click_proxy_button_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (!click_dragging || event->button != 1)
		return FALSE;

	draw_rubberband (click_current_x, click_current_y);
	gdk_pointer_ungrab (event->time);
	click_dragging = FALSE;

	update_drag_selection (event->x, event->y);

	XUngrabServer (GDK_DISPLAY ());

	return TRUE;
}

/* Handles motion events when dragging the icon-selection rubberband on the desktop */
static gint
click_proxy_motion (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	if (!click_dragging)
		return FALSE;

	draw_rubberband (click_current_x, click_current_y);
	draw_rubberband (event->x, event->y);
	update_drag_selection (event->x, event->y);
	click_current_x = event->x;
	click_current_y = event->y;

	return TRUE;
}

/*
 * Filter that translates proxied events from virtual root windows into normal
 * Gdk events for the click_proxy_invisible widget.
 */
static GdkFilterReturn
click_proxy_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent *xev;

	xev = xevent;

	switch (xev->type) {
	case ButtonPress:
	case ButtonRelease:
		/* Translate button events into events that come from the proxy
		 * window, so that we can catch them as a signal from the
		 * invisible widget.
		 */
		if (xev->type == ButtonPress)
			event->button.type = GDK_BUTTON_PRESS;
		else
			event->button.type = GDK_BUTTON_RELEASE;

		gdk_window_ref (click_proxy_gdk_window);

		event->button.window = click_proxy_gdk_window;
		event->button.send_event = xev->xbutton.send_event;
		event->button.time = xev->xbutton.time;
		event->button.x = xev->xbutton.x;
		event->button.y = xev->xbutton.y;
		event->button.state = xev->xbutton.state;
		event->button.button = xev->xbutton.button;

		return GDK_FILTER_TRANSLATE;

	case DestroyNotify:
		/* The proxy window was destroyed (i.e. the window manager
		 * died), so we have to cope with it
		 */
		if (((GdkEventAny *) event)->window == click_proxy_gdk_window) {
			gdk_window_destroy_notify (click_proxy_gdk_window);
			click_proxy_gdk_window = NULL;
		}

		return GDK_FILTER_REMOVE;

	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

/*
 * Creates a proxy window to receive clicks from the root window and
 * sets up the necessary event filters.
 */
static void
setup_desktop_click_proxy_window (void)
{
	click_proxy_gdk_window = find_click_proxy_window ();
	if (!click_proxy_gdk_window) {
		g_warning ("Root window clicks will not work as no GNOME-compliant window manager could be found!");
		return;
	}

	/* Make the proxy window send events to the invisible proxy widget */
	gdk_window_set_user_data (click_proxy_gdk_window, click_proxy_invisible);

	/* Add our filter to get events */
	gdk_window_add_filter (click_proxy_gdk_window, click_proxy_filter, NULL);

	/*
	 * The proxy window for clicks sends us button press events with
	 * SubstructureNotifyMask.  We need StructureNotifyMask to receive
	 * DestroyNotify events, too.
	 */

	XSelectInput (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (click_proxy_gdk_window),
		      SubstructureNotifyMask | StructureNotifyMask);
}

/*
 * Handler for PropertyNotify events from the root window; it must change the
 * proxy window to a new one.
 */
static gint
click_proxy_property_notify (GtkWidget *widget, GdkEventProperty *event, gpointer data)
{
	if (event->window != GDK_ROOT_PARENT ())
		return FALSE;

	if (event->atom != gdk_atom_intern ("_WIN_DESKTOP_BUTTON_PROXY", FALSE))
		return FALSE;

	/* If there already is a proxy window, destroy it */

	click_proxy_gdk_window = NULL;

	/* Get the new proxy window */

	setup_desktop_click_proxy_window ();

	return TRUE;
}

#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01, };

/* Sets up the window manager proxy window to receive clicks on the desktop root window */
static void
setup_desktop_clicks (void)
{
	GdkColormap *cmap;
	GdkColor color;
	GdkBitmap *stipple;

	click_proxy_invisible = gtk_invisible_new ();
	gtk_widget_show (click_proxy_invisible);

	/* Make the root window send events to the invisible proxy widget */
	gdk_window_set_user_data (GDK_ROOT_PARENT (), click_proxy_invisible);

	/* Add our filter to get button press/release events (they are sent by
	 * the WM * with the window set to the root).  Our filter will translate
	 * them to a GdkEvent with the proxy window as its window field.
	 */
	gdk_window_add_filter (GDK_ROOT_PARENT (), click_proxy_filter, NULL);

	/* Select for PropertyNotify events from the root window */

	XSelectInput (GDK_DISPLAY (), GDK_ROOT_WINDOW (), PropertyChangeMask);

	/* Create the proxy window for clicks on the root window */
	setup_desktop_click_proxy_window ();

	/* Connect the signals */

	gtk_signal_connect (GTK_OBJECT (click_proxy_invisible), "button_press_event",
			    (GtkSignalFunc) click_proxy_button_press,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (click_proxy_invisible), "button_release_event",
			    (GtkSignalFunc) click_proxy_button_release,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (click_proxy_invisible), "motion_notify_event",
			    (GtkSignalFunc) click_proxy_motion,
			    NULL);

	gtk_signal_connect (GTK_OBJECT (click_proxy_invisible), "property_notify_event",
			    (GtkSignalFunc) click_proxy_property_notify,
			    NULL);

	/* Create the GC to paint the rubberband rectangle */

	click_gc = gdk_gc_new (GDK_ROOT_PARENT ());

	cmap = gdk_window_get_colormap (GDK_ROOT_PARENT ());

	gdk_color_white (cmap, &color);
	if (color.pixel == 0)
		gdk_color_black (cmap, &color);

	gdk_gc_set_foreground (click_gc, &color);
	gdk_gc_set_function (click_gc, GDK_XOR);

	gdk_gc_set_fill (click_gc, GDK_STIPPLED);

	stipple = gdk_bitmap_create_from_data (NULL, gray50_bits, gray50_width, gray50_height);
	gdk_gc_set_stipple (click_gc, stipple);
	gdk_bitmap_unref (stipple);
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
	load_desktop_icons (FALSE, 0, 0);
	setup_desktop_dnd ();
	setup_desktop_clicks ();
}

/**
 * desktop_destroy
 *
 * Shuts the desktop down by destroying the desktop icons.
 */
void
desktop_destroy (void)
{
	/* Destroy the desktop icons */

	destroy_desktop_icons ();

	/* Cleanup */

	g_free (layout_slots);
	layout_slots = NULL;
	layout_cols = 0;
	layout_rows = 0;

	g_free (desktop_directory);
	desktop_directory = NULL;

	/* Remove DnD crap */

	gtk_widget_destroy (dnd_proxy_window);
	XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (), gdk_atom_intern ("XdndProxy", FALSE));

	/* Remove click-on-desktop crap */

	gdk_window_unref (click_proxy_gdk_window);
	gtk_widget_destroy (click_proxy_invisible);
}
