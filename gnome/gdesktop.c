/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include "global.h"
#include <gdk/gdkx.h>
#include <gtk/gtkinvisible.h>
#include <gnome.h>
#include "dialog.h"
#define DIR_H_INCLUDE_HANDLE_DIRENT /* bleah */
#include "dir.h"
#include "ext.h"
#include "file.h"
#include "fileopctx.h"
#include "gconf.h"
#include "gdesktop.h"
#include "gdesktop-icon.h"
#include "gdesktop-init.h"
#include "gicon.h"
#include "gmain.h"
#include "gmetadata.h"
#include "gcmd.h"
#include "gdnd.h"
#include "gpopup.h"
#include "gprint.h"
#include "gscreen.h"
#include "../vfs/vfs.h"
#include "main.h"
#include "gmount.h"


struct layout_slot {
	int num_icons;		      /* Number of icons in this slot */
	GList *icons;		      /* The list of icons in this slot */
};


/* Configuration options for the desktop */

int desktop_use_shaped_icons = TRUE;
int desktop_use_shaped_text = FALSE;
int desktop_auto_placement = FALSE;
int desktop_snap_icons = FALSE;
int desktop_arr_r2l = FALSE;
int desktop_arr_b2t = FALSE;
int desktop_arr_rows = FALSE;

/* The computed name of the user's desktop directory */
char *desktop_directory;

/* Layout information:  number of rows/columns for the layout slots, and the
 * array of slots.  Each slot is an integer that specifies the number of icons
 * that belong to that slot.
 */
static int layout_screen_width;
static int layout_screen_height;
static int layout_cols;
static int layout_rows;
static struct layout_slot *layout_slots;


int desktop_wm_is_gnome_compliant = -1;

#define l_slots(u, v) (layout_slots[(u) * layout_rows + (v)])

/* The last icon to be selected */
static DesktopIconInfo *last_selected_icon;

/* Drag and drop sources and targets */

static GtkTargetEntry dnd_icon_sources[] = {
	{ TARGET_MC_DESKTOP_ICON_TYPE, 0, TARGET_MC_DESKTOP_ICON },
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_TEXT_PLAIN_TYPE, 0, TARGET_TEXT_PLAIN },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkTargetEntry dnd_icon_targets[] = {
	{ TARGET_MC_DESKTOP_ICON_TYPE, 0, TARGET_MC_DESKTOP_ICON },
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkTargetEntry dnd_desktop_targets[] = {
	{ TARGET_MC_DESKTOP_ICON_TYPE, 0, TARGET_MC_DESKTOP_ICON },
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

static int dnd_icon_nsources = sizeof (dnd_icon_sources) / sizeof (dnd_icon_sources[0]);
static int dnd_icon_ntargets = sizeof (dnd_icon_targets) / sizeof (dnd_icon_targets[0]);
static int dnd_desktop_ntargets = sizeof (dnd_desktop_targets) / sizeof (dnd_desktop_targets[0]);

/* Proxy window for DnD and clicks on the desktop */
static GtkWidget *proxy_invisible;

/* Offsets for the DnD cursor hotspot */
static int dnd_press_x, dnd_press_y;

/* Whether a call to select_icon() is pending because the initial click on an
 * icon needs to be resolved at button release time.  Also, we store the
 * event->state.
 */
static int dnd_select_icon_pending;
static guint dnd_select_icon_pending_state;

/* Whether the button release signal on a desktop icon should be stopped due to
 * the icon being selected by clicking on the text item.
 */
static int icon_select_on_text;

/* Proxy window for clicks on the root window */
static GdkWindow *click_proxy_gdk_window;

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


static DesktopIconInfo *desktop_icon_info_new (char *filename, char *url, char *caption,
					       int xpos, int ypos);

static GHashTable *icon_hash;


/* Convenience function to figure out the slot corresponding to an (x, y) position */
static void
get_slot_from_pos (int x, int y, int *u, int *v)
{
	int uu, vv;

	uu = (x + DESKTOP_SNAP_X / 2) / DESKTOP_SNAP_X;
	vv = (y + DESKTOP_SNAP_Y / 2) / DESKTOP_SNAP_Y;

	*u = CLAMP (uu, 0, layout_cols - 1);
	*v = CLAMP (vv, 0, layout_rows - 1);
}

/* Swaps two integer variables */
static void
swap (int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

/* Looks for a free slot in the layout_slots array and returns the coordinates
 * that coorespond to it.  "Free" means it either has zero icons in it, or it
 * has the minimum number of icons of all the slots.  Returns the number of
 * icons in the sought spot (ideally 0).
 */
static int
auto_pos (int sx, int ex, int sy, int ey, int *slot)
{
	int min, min_slot;
	int x, y;
	int val;
	int xinc, yinc;
	int r, b;

	min = l_slots (sx, sy).num_icons;
	min_slot = sx * layout_rows + sy;
	xinc = yinc = 1;

	if (desktop_arr_rows) {
		swap (&ex, &ey);
		swap (&sx, &sy);
	}

#if 0
	if (desktop_arr_r2l)
		swap (&sx, &ex);

	if (desktop_arr_b2t)
		swap (&sy, &ey);
#endif
	if (desktop_arr_r2l)
		xinc = -1;

	if (desktop_arr_b2t)
		yinc = -1;

	if (desktop_arr_rows)
		swap (&xinc, &yinc);

	r = desktop_arr_r2l;
	b = desktop_arr_b2t;

	if (desktop_arr_rows)
		swap (&r, &b);

	for (x = sx; (r ? (x >= ex) : (x <= ex)); x += xinc) {
		for (y = sy; (b ? (y >= ey) : (y <= ey)); y += yinc) {
			if (desktop_arr_rows)
				val = l_slots (y, x).num_icons;
			else
				val = l_slots (x, y).num_icons;

			if (val < min || val == 0) {
				min = val;
				if (desktop_arr_rows)
					min_slot = (y * layout_rows + x);
				else
					min_slot = (x * layout_rows + y);
				if (val == 0)
					break;
			}
		}

		if (val == 0)
			break;
	}

	*slot = min_slot;
	return min;
}

/* Looks for free space in the icon grid, scanning it in column-wise order */
static void
get_icon_auto_pos (int *x, int *y)
{
	int val1;
	int slot1;
	int slot;
	int sx, sy, ex, ey;

#if 0
	get_slot_from_pos (*x, *y, &sx, &sy);
#endif
	/* FIXME funky stuff going on with the efficient positioning thingy */
	if (desktop_arr_r2l)
		sx = layout_cols - 1;
	else
		sx = 0;

	if (desktop_arr_b2t)
		sy = layout_rows - 1;
	else
		sy = 0;

	if (desktop_arr_r2l)
		ex = 0;
	else
		ex = layout_cols - 1;

	if (desktop_arr_b2t)
		ey = 0;
	else
		ey = layout_rows - 1;

	/* Look forwards until the end of the grid.  If we could not find an
	 * empty spot, find the second best.
	 */

	val1 = auto_pos (sx, ex, sy, ey, &slot1);

	slot = slot1;

#if 0
	/* to be used at a later date */
	if (val1 == 0)
		slot = slot1;
	else {
		if (desktop_arr_r2l)
			sx = layout_cols - 1;
		else
			sx = 0;

		if (desktop_arr_b2t)
			sy = layout_rows - 1;
		else
			sy = 0;

		val2 = auto_pos (sx, ex, sy, ey, &slot2);
		if (val2 < val1)
			slot = slot2;
	}
#endif

	*x = (slot / layout_rows) * DESKTOP_SNAP_X;
	*y = (slot % layout_rows) * DESKTOP_SNAP_Y;
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
remove_from_slot (DesktopIconInfo *dii)
{
	if (dii->slot == -1)
		return;

	g_assert (layout_slots[dii->slot].num_icons >= 1);
	g_assert (layout_slots[dii->slot].icons != NULL);

	layout_slots[dii->slot].num_icons--;
	layout_slots[dii->slot].icons = g_list_remove (layout_slots[dii->slot].icons, dii);

	dii->slot = -1;
	dii->x = 0;
	dii->y = 0;
}

/* Places a desktop icon on the specified position */
static void
desktop_icon_info_place (DesktopIconInfo *dii, int xpos, int ypos)
{
	int u, v;
	char *filename;

	remove_from_slot (dii);

	if (xpos < 0)
		xpos = 0;
	else if (xpos > layout_screen_width)
		xpos = layout_screen_width - DESKTOP_SNAP_X;

	if (ypos < 0)
		ypos = 0;
	else if (ypos > layout_screen_height)
		ypos = layout_screen_height - DESKTOP_SNAP_Y;

	/* Increase the number of icons in the corresponding slot */

	get_slot_from_pos (xpos, ypos, &u, &v);

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

/* Destroys the specified desktop icon */
static void
desktop_icon_info_destroy (DesktopIconInfo *dii)
{
	if (last_selected_icon == dii)
		last_selected_icon = NULL;

	gtk_widget_destroy (dii->dicon);
	remove_from_slot (dii);

	g_hash_table_remove (icon_hash, dii->filename);

	g_free (dii->url);
	g_free (dii->filename);
	g_free (dii);
}

/* Destroys all the current desktop icons */
static void
destroy_desktop_icons (void)
{
	int i;
	GList *l;
	DesktopIconInfo *dii;

	for (i = 0; i < (layout_cols * layout_rows); i++) {
		l = layout_slots[i].icons;

		while (l) {
			dii = l->data;
			l = l->next;

			desktop_icon_info_destroy (dii);
		}
	}
}

/* Returns a list with all of the icons on the desktop */
static GList *
get_all_icons (void)
{
	GList *l, *res;
	int i;

	res = NULL;

	for (i = 0; i < (layout_cols * layout_rows); i++)
		for (l = layout_slots [i].icons; l; l = l->next)
			res = g_list_prepend (res, l->data);

	return res;
}

/* Returns the node in the list of DesktopIconInfo structures that contains the
 * icon for the specified filename.  If no icon is found, then returns NULL.
 */
static GList *
icon_exists_in_list (GList *list, char *filename)
{
	GList *l;
	DesktopIconInfo *dii;

	for (l = list; l; l = l->next) {
		dii = l->data;

		if (strcmp (filename, dii->filename) == 0)
			return l;
	}

	return NULL;
}

/* Loads from the metadata updated versions of the caption and the url */
static void
update_url (DesktopIconInfo *dii)
{
	char *fullname = g_concat_dir_and_file (desktop_directory, dii->filename);
	char *caption = NULL;
	char *url = NULL;
	int size;

	gnome_metadata_get (fullname, "icon-caption", &size, &caption);
	if (caption){
		desktop_icon_set_text (DESKTOP_ICON (dii->dicon), caption);
		g_free (caption);
	}

	gnome_metadata_get (fullname, "desktop-url", &size, &url);
	if (url){
		if (dii->url)
			g_free (dii->url);
		dii->url = url;
	}

	g_free (fullname);
}

typedef struct {
	char *filename;
	char *url;
	char *caption;
} file_and_url_t;

/* Reloads the desktop icons efficiently.  If there are "new" files for which no
 * icons have been created, then icons for them will be created started at the
 * specified position if user_pos is TRUE.  If it is FALSE, the icons will be
 * auto-placed.
 */
void
desktop_reload_icons (int user_pos, int xpos, int ypos)
{
	struct dirent *dirent;
	DIR *dir;
	char *full_name;
	int have_pos, x, y, size;
	DesktopIconInfo *dii;
	GSList *need_position_list, *sl;
	GList *all_icons, *l;
	char *desktop_url, *caption;
	const char *mime;
	int orig_xpos, orig_ypos;

	dir = mc_opendir (desktop_directory);
	if (!dir) {
		message (FALSE,
			 _("Warning"),
			 _("Could not open %s; will not have desktop icons"),
			 desktop_directory);
		return;
	}

	gnome_metadata_lock ();

	/* Read the directory.  For each file for which we do have an existing
	 * icon, do nothing.  Otherwise, if the file has its metadata for icon
	 * position set, create an icon for it.  Otherwise, store it in a list
	 * of new icons for which positioning is pending.
	 */

	need_position_list = NULL;
	all_icons = get_all_icons ();

	while ((dirent = mc_readdir (dir)) != NULL) {
		/* Skip . and .. */

		if ((dirent->d_name[0] == '.' && dirent->d_name[1] == 0)
		    || (dirent->d_name[0] == '.' && dirent->d_name[1] == '.'
			&& dirent->d_name[2] == 0))
			continue;

		l = icon_exists_in_list (all_icons, dirent->d_name);
		if (l) {
			GdkImlibImage *im;
			file_entry *fe;

			/* Reload the icon for this file, as it may have changed */

			full_name = g_concat_dir_and_file (desktop_directory, dirent->d_name);
			fe = file_entry_from_file (full_name);
			if (!fe){
				g_free (full_name);
				continue;
			}
			im = gicon_get_icon_for_file (desktop_directory, fe, FALSE);
			file_entry_free (fe);
			g_free (full_name);

			dii = l->data;
			desktop_icon_set_icon (DESKTOP_ICON (dii->dicon), im);

			update_url (dii);

			/* Leave the icon in the desktop by removing it from the list */

			all_icons = g_list_remove_link (all_icons, l);
			continue;
		}

		full_name = g_concat_dir_and_file (desktop_directory, dirent->d_name);
		have_pos = gmeta_get_icon_pos (full_name, &x, &y);

		if (gnome_metadata_get (full_name, "desktop-url", &size, &desktop_url) != 0)
			desktop_url = NULL;

		caption = NULL;
		gnome_metadata_get (full_name, "icon-caption", &size, &caption);

		if (have_pos) {
			dii = desktop_icon_info_new (dirent->d_name, desktop_url, caption, x, y);
			gtk_widget_show (dii->dicon);
		} else {
			file_and_url_t *fau;

			fau = g_new0 (file_and_url_t, 1);
			fau->filename = g_strdup (dirent->d_name);

			if (desktop_url)
				fau->url = g_strdup (desktop_url);

			if (caption)
				fau->caption  = g_strdup (caption);

			need_position_list = g_slist_prepend (need_position_list, fau);
		}

		g_free (full_name);

		if (desktop_url)
			g_free (desktop_url);

		if (caption)
			g_free (caption);
	}

	mc_closedir (dir);

	/* all_icons now contains a list of all of the icons that were not found
	 * in the ~/desktop directory, remove them.  To be paranoically tidy,
	 * also delete the icon position information -- someone may have deleted
	 * a file under us.
	 */

	for (l = all_icons; l; l = l->next) {
		dii = l->data;
		full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
		gnome_metadata_delete (full_name);
		g_free (full_name);
		desktop_icon_info_destroy (dii);
	}

	g_list_free (all_icons);

	/* Now create the icons for all the files that did not have their
	 * position set.  This makes auto-placement work correctly without
	 * overlapping icons.
	 */

	need_position_list = g_slist_reverse (need_position_list);

	orig_xpos = orig_ypos = 0;

	for (sl = need_position_list; sl; sl = sl->next) {
		file_and_url_t *fau;

		fau = sl->data;

		if (user_pos && sl == need_position_list) {
			/* If we are on the first icon, place it "by hand".
			 * Else, use automatic placement based on the position
			 * of the first icon of the series.
			 */
			if (desktop_auto_placement) {
				xpos = ypos = 0;
				get_icon_auto_pos (&xpos, &ypos);
			} else if (desktop_snap_icons)
				get_icon_snap_pos (&xpos, &ypos);

			orig_xpos = xpos;
			orig_ypos = ypos;
		} else {
			xpos = orig_xpos;
			ypos = orig_ypos;
			get_icon_auto_pos (&xpos, &ypos);
		}

		/* If the file dropped was a .desktop file, pull the suggested
		 * title and icon from there
		 */
		mime = gnome_mime_type_or_default (fau->filename, NULL);
		if (mime && strcmp (mime, "application/x-gnome-app-info") == 0) {
			GnomeDesktopEntry *entry;
			char *fullname;

			fullname = g_concat_dir_and_file (desktop_directory, fau->filename);
			entry = gnome_desktop_entry_load (fullname);
			if (entry) {
				if (entry->name) {
					if (fau->caption)
						g_free (fau->caption);

					fau->caption = g_strdup (entry->name);
					gnome_metadata_set (fullname, "icon-caption",
							    strlen (fau->caption) + 1,
							    fau->caption);
				}

				if (entry->icon)
					gnome_metadata_set (fullname, "icon-filename",
							    strlen (entry->icon) + 1,
							    entry->icon);

				gnome_desktop_entry_free (entry);
			}
			g_free (fullname);
		}

		dii = desktop_icon_info_new (fau->filename, fau->url, fau->caption, xpos, ypos);
		gtk_widget_show (dii->dicon);

		if (fau->url)
			g_free (fau->url);

		if (fau->caption)
			g_free (fau->caption);

		g_free (fau->filename);
		g_free (fau);
	}

	g_slist_free (need_position_list);
	gnome_metadata_unlock ();
}

static WPanel *create_panel_from_desktop(); /* Fwd decl */
static void free_panel_from_desktop(WPanel *panel);

/* Perform automatic arrangement of the desktop icons */
static void
desktop_arrange_icons (SortType type)
{
	WPanel *panel;
	sortfn *sfn;
	DesktopIconInfo *dii;
	int i;
	dir_list dir;
	int xpos, ypos;

	panel = create_panel_from_desktop ();
	sfn = sort_get_func_from_type (type);
	g_assert (sfn != NULL);

	do_sort (&panel->dir, sfn, panel->count - 1, FALSE, FALSE);
	dir = panel->dir;
	g_assert (dir.list != NULL);

	for (i = 0; i < dir.size; i++)
		remove_from_slot (desktop_icon_info_get_by_filename (dir.list[i].fname));

	for (i = 0; i < dir.size; i++) {
		dii = desktop_icon_info_get_by_filename (dir.list[i].fname);
		xpos = ypos = 0;
		get_icon_auto_pos (&xpos, &ypos);
		desktop_icon_info_place (dii, xpos, ypos);
	}

	free_panel_from_desktop (panel);
}

/* Unselects all the desktop icons except the one in exclude */
static void
unselect_all (DesktopIconInfo *exclude)
{
	int i;
	GList *l;
	DesktopIconInfo *dii;

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
select_range (DesktopIconInfo *dii, int sel)
{
	int du, dv, lu, lv;
	int min_u, min_v;
	int max_u, max_v;
	int u, v;
	GList *l;
	DesktopIconInfo *ldii;

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
	} else {
		min_u = lu;
		max_u = du;
	}

	if (dv < lv) {
		min_v = dv;
		max_v = lv;
	} else {
		min_v = lv;
		max_v = dv;
	}

	/* Select all the icons in the rectangle */

	for (u = min_u; u <= max_u; u++)
		for (v = min_v; v <= max_v; v++)
			for (l = l_slots (u, v).icons; l; l = l->next) {
				ldii = l->data;

				desktop_icon_select (DESKTOP_ICON (ldii->dicon), sel);
				ldii->selected = sel;
			}
}

/*
 * Handles icon selection and unselection due to button presses.  The
 * event_state is the state field of the event.
 */
static void
select_icon (DesktopIconInfo *dii, int event_state)
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

/* Convenience function to fill a file entry */
static void
file_entry_fill (file_entry *fe, struct stat *s, char *filename)
{
	fe->fname = g_strdup (x_basename (filename));
	fe->fnamelen = strlen (fe->fname);
	fe->buf = *s;
	fe->f.marked = FALSE;
	fe->f.link_to_dir = FALSE;
	fe->f.stalled_link = FALSE;
	fe->f.dir_size_computed = FALSE;

	if (S_ISLNK (s->st_mode)) {
		struct stat s2;

		if (mc_stat (filename, &s2) == 0)
			fe->f.link_to_dir = S_ISDIR (s2.st_mode) != 0;
		else
			fe->f.stalled_link = TRUE;
	}
}

/* Creates a file entry structure and fills it with information appropriate to the specified file.  */
file_entry *
file_entry_from_file (char *filename)
{
	file_entry *fe;
	struct stat s;

	if (mc_lstat (filename, &s) == -1) {
		g_warning ("Could not stat %s, bad things will happen", filename);
		return NULL;
	}

	fe = g_new (file_entry, 1);
	file_entry_fill (fe, &s, filename);
	return fe;
}

/* Frees a file entry structure */
void
file_entry_free (file_entry *fe)
{
	if (fe->fname)
		g_free (fe->fname);

	g_free (fe);
}

/* Sets the wmclass and name of a desktop icon to an unique value */
static void
set_icon_wmclass (DesktopIconInfo *dii)
{
	XClassHint *h;

	g_assert (GTK_WIDGET_REALIZED (dii->dicon));

	h = XAllocClassHint ();
	if (!h) {
		g_warning ("XAllocClassHint() failed!");
		return; /* eek */
	}

	h->res_name = dii->filename;
	h->res_class = "gmc-desktop-icon";

	XSetClassHint (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (GTK_WIDGET (dii->dicon)->window), h);
	XFree (h);
}

/* Removes the Gtk and Gdk grabs that are present when editing a desktop icon */
static void
remove_editing_grab (DesktopIconInfo *dii)
{
	gtk_grab_remove (dii->dicon);
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
}

/* Callback used when an icon's text changes.  We must validate the
 * rename and return the appropriate value.  The desktop icon info
 * structure is passed in the user data.
 */
static int
text_changed (GnomeIconTextItem *iti, gpointer data)
{
	DesktopIconInfo *dii;
	char *new_name;
	char *source;
	char *dest;
	char *buf;
	int size;
	int retval;

	dii = data;
	remove_editing_grab (dii);

	source = g_concat_dir_and_file (desktop_directory, dii->filename);
	new_name = gnome_icon_text_item_get_text (iti);

	if (gnome_metadata_get (source, "icon-caption", &size, &buf) != 0) {
		if (strcmp (new_name, dii->filename) == 0) {
			g_free (source);
			return TRUE;
		}

		/* No icon caption metadata, so rename the file */

		dest = g_concat_dir_and_file (desktop_directory, new_name);

		if (rename_file_with_context (source, dest) == FILE_CONT) {
			GList *icons;
			GList *l;

			/* See if there was an icon for the new name.  If so,
			 * destroy it first; desktop_reload_icons() will not be
			 * happy if two icons have the same filename.
			 */

			icons = get_all_icons ();
			l = icon_exists_in_list (icons, new_name);
			if (l)
				desktop_icon_info_destroy (l->data);

			g_list_free (icons);

			/* Set the new name */

			g_hash_table_remove (icon_hash, dii->filename);
			g_free (dii->filename);

			dii->filename = g_strdup (new_name);
			g_hash_table_insert (icon_hash, dii->filename, dii);
			set_icon_wmclass (dii);

			desktop_reload_icons (FALSE, 0, 0);
			retval = TRUE;
		} else
			retval = FALSE; /* FIXME: maybe pop up a warning/query dialog? */

		g_free (dest);
	} else {
		/* The icon has the icon-caption metadata, so change that instead */

		g_free (buf);

		buf = gnome_icon_text_item_get_text (iti);
		if (*buf) {
			gnome_metadata_set (source, "icon-caption", strlen (buf) + 1, buf);
			desktop_reload_icons (FALSE, 0, 0);
			retval = TRUE;
		} else
			retval = FALSE;
	}

	g_free (source);
	return retval;
}

/* Sets up the mouse grab for when a desktop icon is being edited */
static void
setup_editing_grab (DesktopIconInfo *dii)
{
	GdkCursor *ibeam;

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
	gdk_cursor_destroy (ibeam);
}

/*
 * Callback used when the user begins editing the icon text item in a
 * desktop icon.  It installs the mouse and keyboard grabs that are
 * required while an icon is being edited.
 */
static void
editing_started (GnomeIconTextItem *iti, gpointer data)
{
	DesktopIconInfo *dii;

	dii = data;

	/* Disable drags from this icon until editing is finished */

	gtk_drag_source_unset (DESKTOP_ICON (dii->dicon)->canvas);

	/* Unselect all icons but this one */
	unselect_all (dii);

	gtk_grab_add (dii->dicon);
	setup_editing_grab (dii);
	gdk_keyboard_grab (GTK_LAYOUT (DESKTOP_ICON (dii->dicon)->canvas)->bin_window,
			   FALSE, GDK_CURRENT_TIME);
}

/* Sets up the specified icon as a drag source, but does not connect the signals */
static void
setup_icon_dnd_actions (DesktopIconInfo *dii)
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
	DesktopIconInfo *dii;

	dii = data;
	remove_editing_grab (dii);

	/* Re-enable drags from this icon */

	setup_icon_dnd_actions (dii);
}

/* Callback used when the user stops selecting text in a desktop icon.  This function
 * restores the mouse grab that we had set up initially (the selection process changes
 * the grab and then removes it, so we need to restore the initial grab).
 */
static void
selection_stopped (GnomeIconTextItem *iti, gpointer data)
{
	DesktopIconInfo *dii;

	dii = data;
	setup_editing_grab (dii);
}

static char *mount_known_locations [] = {
	"/sbin/mount", "/bin/mount", "/etc/mount",
	"/usr/sbin/mount", "/usr/etc/mount", "/usr/bin/mount",
	NULL
};

static char *umount_known_locations [] = {
	"/sbin/umount", "/bin/umount", "/etc/umount",
	"/usr/sbin/umount", "/usr/etc/umount", "/usr/bin/umount",
	NULL
};

/*
 * Returns the full path to the mount command
 */
static char *
find_command (char **known_locations)
{
	int i;

	for (i = 0; known_locations [i]; i++){
		if (g_file_exists (known_locations [i]))
			return known_locations [i];
	}
	return NULL;
}

gboolean
is_mountable (char *filename, file_entry *fe, int *is_mounted, char **point)
{
	char buffer [128], *p;
	int len;

	if (point)
		*point = NULL;

	if (!S_ISLNK (fe->buf.st_mode))
		return FALSE;

	len = readlink (filename, buffer, sizeof (buffer));
	if (len == -1)
		return FALSE;
	buffer [len] = 0;

	p = is_block_device_mountable (buffer);
	if (!p)
		return FALSE;

	if (point)
		*point = p;
	else
		g_free (point);

	*is_mounted = is_block_device_mounted (buffer);

	return TRUE;
}

gboolean
do_mount_umount (char *filename, gboolean is_mount)
{
	static char *mount_command;
	static char *umount_command;
	char *op;
	char *buffer;

	if (is_mount){
		if (!mount_command)
			mount_command = find_command (mount_known_locations);
		op = mount_command;
	} else {
		if (!umount_command)
			umount_command = find_command (umount_known_locations);
		op = umount_command;
	}

	buffer = g_readlink (filename);
	if (buffer == NULL)
		return FALSE;

	if (op){
		gboolean success = TRUE;
		char *command;
		FILE *f;

		command = g_strconcat (op, " ", buffer, NULL);
		open_error_pipe ();
		f = popen (command, "r");
		if (f == NULL){
			success = !close_error_pipe (1, _("While running the mount/umount command"));
		} else
			success = !close_error_pipe (0, 0);
		pclose (f);

		g_free (buffer);
		return success;
	}
	g_free (buffer);
	return FALSE;
}

static char *eject_known_locations [] = {
	"/usr/bin/eject",
	"/sbin/eject",
	"/bin/eject",
	NULL
};

/*
 * Returns whether the device is ejectable
 *
 * Right now the test only checks if this system has the eject
 * command
 */
gboolean
is_ejectable (char *filename)
{
	char *buf;
	int size, retval;

	if (gnome_metadata_get (filename, "device-is-ejectable", &size, &buf) == 0){
		if (*buf)
			retval = TRUE;
		else
			retval = FALSE;
		g_free (buf);

		if (retval)
			return TRUE;
	}

	if (find_command (eject_known_locations))
		return TRUE;
	else
		return FALSE;
}

/*
 * Ejects the device pointed by filename
 */
gboolean
do_eject (char *filename)
{
	char *eject_command = find_command (eject_known_locations);
	char *command, *device;
	FILE *f;

	if (!eject_command)
		return FALSE;

	device = mount_point_to_device (filename);
	if (!device)
		return FALSE;

	command = g_strconcat (eject_command, " ", device, NULL);
	open_error_pipe ();
	f = popen (command, "r");
	if (f == NULL)
		close_error_pipe (1, _("While running the eject command"));
	else
		close_error_pipe (0, 0);

	pclose (f);

	return TRUE;
}

static gboolean
try_to_mount (char *filename, file_entry *fe)
{
	int x;

	if (!is_mountable (filename, fe, &x, NULL))
		return FALSE;

	return do_mount_umount (filename, TRUE);
}

/* This is a HORRIBLE HACK.  It creates a temporary panel structure for gpopup's
 * perusal.  Once gmc is rewritten, all file lists including panels will be a
 * single data structure, and the world will be happy again.
 */
static WPanel *
create_panel_from_desktop (void)
{
	WPanel *panel;
	int nicons, count;
	int marked_count, dir_marked_count;
	long total;
	int selected_index;
	int i;
	file_entry *fe;
	GList *l;
	struct stat s;

	panel = g_new0 (WPanel, 1);

	/* Count the number of desktop icons */

	nicons = 0;
	for (i = 0; i < layout_cols * layout_rows; i++)
		nicons += layout_slots[i].num_icons;

	/* Create the file entry list */

	panel->dir.size = nicons;

	count = 0;
	marked_count = 0;
	dir_marked_count = 0;
	total = 0;
	selected_index = -1;

	if (nicons != 0) {
		panel->dir.list = g_new (file_entry, nicons);

		fe = panel->dir.list;

		for (i = 0; i < layout_cols * layout_rows; i++)
			for (l = layout_slots[i].icons; l; l = l->next) {
				DesktopIconInfo *dii;
				char *full_name;

				dii = l->data;
				full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
				if (mc_lstat (full_name, &s) == -1) {
					g_warning ("Could not stat %s, bad things will happen",
						   full_name);
					continue;
				}

				file_entry_fill (fe, &s, full_name);
				if (dii->selected) {
					marked_count++;
					fe->f.marked = TRUE;

					if (dii == last_selected_icon)
						selected_index = count;

					if (S_ISDIR (fe->buf.st_mode)) {
						dir_marked_count++;
						if (fe->f.dir_size_computed)
							total += fe->buf.st_size;
					} else
						total += fe->buf.st_size;
				}

				g_free (full_name);
				fe++;
				count++;
			}
	}

	/* Fill the rest of the panel structure */

	panel->list_type = list_icons;
	strncpy (panel->cwd, desktop_directory, sizeof (panel->cwd));
	panel->count = count; /* the actual number of lstat()ed files */
	panel->marked = marked_count;
	panel->dirs_marked = dir_marked_count;
	panel->total = total;
	panel->selected = selected_index;
	panel->is_a_desktop_panel = TRUE;
	panel->id = -1;

	return panel;
}

/* Pushes our hacked-up desktop panel into the containers list */
WPanel *
push_desktop_panel_hack (void)
{
	WPanel *panel;
	PanelContainer *container;

	panel = create_panel_from_desktop ();
	container = g_new (PanelContainer, 1);
	container->splitted = FALSE;
	container->panel = panel;

	containers = g_list_append (containers, container);

	if (!current_panel_ptr)
		current_panel_ptr = container;
	else if (!other_panel_ptr)
		other_panel_ptr = container;

	/* Set it as the current panel and invoke the menu */

	set_current_panel (panel);
	mc_chdir (desktop_directory);
	return panel;
}

/* Frees our hacked-up panel created in the function above */
static void
free_panel_from_desktop (WPanel *panel)
{
	int i;

	for (i = 0; i < panel->count; i++)
		g_free (panel->dir.list[i].fname);

	if (panel->dir.list)
		g_free (panel->dir.list);

	g_free (panel);
}

/**
 * desktop_icon_info_open:
 * @dii: The desktop icon to open.
 *
 * Opens the specified desktop icon when the user double-clicks on it.
 **/
void
desktop_icon_info_open (DesktopIconInfo *dii)
{
	char *filename;
	file_entry *fe;
	int is_mounted;
	char *point;
	int launch;

	filename = NULL;
	fe = NULL;
	launch = FALSE;

	/* Set the cursor */

	desktop_icon_set_busy (dii, TRUE);

	/* Open the icon */

	if (dii->url) {
		gnome_url_show (dii->url);
		goto out;
	}

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);
	fe = file_entry_from_file (filename);
	if (!fe){
		message (1, _("Error"), "I could not fetch the information from the file");
		goto out;
	}

	if (is_mountable (filename, fe, &is_mounted, &point)){
		if (!is_mounted){
			if (try_to_mount (filename, fe))
				launch = TRUE;
			else
				launch = FALSE;
		} else
			launch = TRUE;

		if (launch)
			new_panel_at (point);
		g_free (point);
	} else {
		WPanel *panel;
		panel = push_desktop_panel_hack ();
		do_enter_on_file_entry (fe);
		layout_panel_gone (panel);
		free_panel_from_desktop (panel);
	}

 out:

	if (fe)
		file_entry_free (fe);

	if (filename)
		g_free (filename);

	/* Reset the cursor */

	desktop_icon_set_busy (dii, FALSE);
}

void
desktop_icon_info_delete (DesktopIconInfo *dii)
{
	char *full_name;
	struct stat s;
	FileOpContext *ctx;
	long progress_count = 0;
	double progress_bytes = 0;

	/* 1. Delete the file */
	ctx = file_op_context_new ();
	file_op_context_create_ui (ctx, OP_DELETE, TRUE);
	x_flush_events ();

	full_name = g_concat_dir_and_file (desktop_directory, dii->filename);

	if (lstat (full_name, &s) != -1) {
		if (S_ISLNK (s.st_mode))
			erase_file (ctx, full_name, &progress_count, &progress_bytes, TRUE);
		else {
			if (S_ISDIR (s.st_mode))
				erase_dir (ctx, full_name, &progress_count, &progress_bytes);
			else
				erase_file (ctx, full_name, &progress_count, &progress_bytes, TRUE);
		}

		gnome_metadata_delete (full_name);
	}

	g_free (full_name);
	file_op_context_destroy (ctx);

	/* 2. Destroy the dicon */
	desktop_icon_info_destroy (dii);
}

/**
 * desktop_icon_set_busy:
 * @dii: A desktop icon
 * @busy: TRUE to set a watch cursor, FALSE to reset the normal arrow cursor
 *
 * Sets a wait/normal cursor for a desktop icon.
 **/
void
desktop_icon_set_busy (DesktopIconInfo *dii, int busy)
{
	GdkCursor *cursor;

	g_return_if_fail (dii != NULL);

	if (!GTK_WIDGET_REALIZED (dii->dicon))
		return;

	cursor = gdk_cursor_new (busy ? GDK_WATCH : GDK_TOP_LEFT_ARROW);
	gdk_window_set_cursor (dii->dicon->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();
}

/**
 * desktop_icon_info_get_by_filename:
 * @filename: A filename relative to the desktop directory
 *
 * Returns the desktop icon structure that corresponds to the specified filename,
 * which should be relative to the desktop directory.
 *
 * Return value: The sought desktop icon, or NULL if it is not found.
 **/
DesktopIconInfo *
desktop_icon_info_get_by_filename (char *filename)
{
	g_return_val_if_fail (filename != NULL, NULL);

	return g_hash_table_lookup (icon_hash, filename);
}

/* Used to execute the popup menu for desktop icons */
static void
do_popup_menu (DesktopIconInfo *dii, GdkEventButton *event)
{
	char *filename;
	WPanel *panel;
	DesktopIconInfo *dii_temp;

	/* Create the panel and the container structure */
	panel = push_desktop_panel_hack ();
	dii_temp = NULL;
	if (panel->marked == 1)
		dii_temp = dii;

	filename = g_concat_dir_and_file (desktop_directory, dii->filename);

	if (gpopup_do_popup2 (event, panel, dii_temp) != -1)
		desktop_reload_icons (FALSE, 0, 0);

	g_free (filename);
	layout_panel_gone (panel);
	free_panel_from_desktop (panel);
}

/* Idle handler that opens a desktop icon.  See below for information on why we
 * do things this way.
 */
static gint
idle_open_icon (gpointer data)
{
	desktop_icon_info_open (data);
	return FALSE;
}

/* Event handler for desktop icons.  Button events are ignored when the icon is
 * being edited; these will be handled either by the icon's text item or the
 * icon_event_after() fallback.
 */
static gint
icon_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	DesktopIconInfo *dii;
	GnomeIconTextItem *iti;
	int on_text;
	int retval;

	dii = data;
	iti = GNOME_ICON_TEXT_ITEM (DESKTOP_ICON (dii->dicon)->text);

	on_text = item == GNOME_CANVAS_ITEM (iti);
	retval = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1 || event->button.button == 2) {
			/* If se are editing, do not handle the event ourselves
			 * -- either let the text item handle it, or wait until
			 * we fall back to the icon_event_after() callback.
			 */
			if (iti->editing)
				break;

			/* Save the mouse position for DnD */

			dnd_press_x = event->button.x;
			dnd_press_y = event->button.y;

			/* Handle icon selection if we are not on the text item
			 * or if the icon is not selected in the first place.
			 * Otherwise, if there are modifier keys pressed, handle
			 * icon selection instead of starting editing.
			 */
			if (!on_text
			    || !dii->selected
			    || (event->button.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
			    || event->button.button == 2) {
				/* If click on text, and the icon was not
				 * selected in the first place or shift is down,
				 * save this flag.
				 */
				if (on_text
				    && (!dii->selected || (event->button.state & GDK_SHIFT_MASK)))
					icon_select_on_text = TRUE;

				if ((dii->selected
				     && !(event->button.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				    || ((event->button.state & GDK_CONTROL_MASK)
					&& !(event->button.state & GDK_SHIFT_MASK))) {
					dnd_select_icon_pending = TRUE;
					dnd_select_icon_pending_state = event->button.state;
				} else
					select_icon (dii, event->button.state);

				retval = TRUE;
			}
		} else if (event->button.button == 3) {
			if (!dii->selected)
				select_icon (dii, event->button.state);

			do_popup_menu (dii, (GdkEventButton *) event);
			retval = TRUE;
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (event->button.button != 1 || iti->editing)
			break;

		/* We have an interesting race condition here.  If we open the
		 * desktop icon here instead of doing it in the idle handler,
		 * the icon thinks it must begin editing itself, even when the
		 * icon_select_on_text flag tries to prevent it.  I have no idea
		 * why this happens :-( - Federico
		 */
		gtk_idle_add (idle_open_icon, dii);
/*  		desktop_icon_info_open (dii); */
		icon_select_on_text = TRUE;
		retval = TRUE;
		break;

	case GDK_BUTTON_RELEASE:
		if (!(event->button.button == 1 || event->button.button != 2))
			break;

		if (on_text && icon_select_on_text) {
			icon_select_on_text = FALSE;
			retval = TRUE;
		}

		if (dnd_select_icon_pending) {
			select_icon (dii, dnd_select_icon_pending_state);
			dnd_select_icon_pending = FALSE;
			dnd_select_icon_pending_state = 0;
			retval = TRUE;
		}
		break;

	default:
		break;
	}

	/* If the click was on the text and we actually did something, then we
	 * need to stop the text item's event handler from executing.
	 */
	if (on_text && retval)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (iti), "event");

	return retval;
}

/* Fallback handler for when the icon is being edited and the user clicks
 * outside the icon's text item.  This indicates that editing should be accepted
 * and terminated.
 */
static gint
icon_event_after (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	DesktopIconInfo *dii;
	GnomeIconTextItem *iti;

	dii = data;
	iti = GNOME_ICON_TEXT_ITEM (DESKTOP_ICON (dii->dicon)->text);

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	g_return_val_if_fail (iti->editing, FALSE); /* sanity check for dropped events */

	gnome_icon_text_item_stop_editing (iti, TRUE);
	return TRUE;
}

/* Callback used when a drag from the desktop icons is started.  We set the drag icon to the proper
 * pixmap.
 */
static void
drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	DesktopIconInfo *dii;
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
			select_icon (dii, dnd_select_icon_pending_state);

		dnd_select_icon_pending = FALSE;
		dnd_select_icon_pending_state = 0;
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
	DesktopIconInfo *dii;
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
	DesktopIconInfo *dii;
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
		if (dii->url)
			gtk_selection_data_set (selection_data,
						selection_data->target,
						8,
						dii->url,
						strlen (dii->url));
		else {
			files = gnome_uri_list_extract_uris (filelist);
			if (files)
				gtk_selection_data_set (selection_data,
							selection_data->target,
							8,
							files->data,
							strlen (files->data));

			gnome_uri_list_free_strings (files);
		}

		break;

	default:
		g_assert_not_reached ();
	}

	g_free (filelist);
}

/* Callback used when a drag from the desktop is finished.  We need to reload
 * the desktop.
 */
static void
drag_end (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	desktop_reload_icons (FALSE, 0, 0);
}

/* Set up a desktop icon as a DnD source */
static void
setup_icon_dnd_source (DesktopIconInfo *dii)
{
	setup_icon_dnd_actions (dii);

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_begin",
			    (GtkSignalFunc) drag_begin,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_data_get",
			    (GtkSignalFunc) drag_data_get,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_end",
			    (GtkSignalFunc) drag_end,
			    dii);
}

/* Callback used when we get a drag_motion event from a desktop icon.  We have
 * to decide which operation to perform based on the type of the data the user
 * is dragging.
 */
static gboolean
icon_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time,
		  gpointer data)
{
	DesktopIconInfo *dii;
	char *filename;
	file_entry *fe;
	GdkDragAction action;
	GtkWidget *source_widget;
	int is_desktop_icon;

	dii = data;
	filename = g_concat_dir_and_file (desktop_directory, dii->filename);
	fe = file_entry_from_file (filename);
	g_free (filename);
	if (!fe)
		return 0; /* eeek */

	gdnd_find_panel_by_drag_context (context, &source_widget);
	is_desktop_icon = gdnd_drag_context_has_target (context, TARGET_MC_DESKTOP_ICON);

	action = gdnd_validate_action (context,
				       TRUE,
				       source_widget != NULL,
				       source_widget && is_desktop_icon,
				       desktop_directory,
				       fe,
				       dii->selected);

	gdk_drag_status (context, action, time);
	file_entry_free (fe);
	return TRUE;
}

/*
 * Returns the desktop icon that started the drag from the specified
 * context
 */
static DesktopIconInfo *
find_icon_by_drag_context (GdkDragContext *context)
{
	GtkWidget *source;
	int i;
	GList *l;
	DesktopIconInfo *dii;

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
 * Performs a drop of desktop icons onto the desktop.  It basically
 * moves the icons from their original position to the new
 * coordinates.
 */
static void
drop_desktop_icons (GdkDragContext *context, GtkSelectionData *data, int x, int y)
{
	DesktopIconInfo *source_dii, *dii;
	int dx, dy;
	int i;
	GList *l;
	GSList *sel_icons, *sl;

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

	/* Move the icons.  FIXME: handle auto-placement by reinserting the
	 * icons in the proper place.
	 */

	if (!desktop_auto_placement)
		for (sl = sel_icons; sl; sl = sl->next) {
			dii = sl->data;
			desktop_icon_info_place (dii, dii->x + dx, dii->y + dy);
		}

	/* Clean up */

	g_slist_free (sel_icons);
}

/* Handler for drag_data_received for desktop icons */
static void
icon_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
			 GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	DesktopIconInfo *dii;

	dii = user_data;

	if (gdnd_drag_context_has_target (context, TARGET_MC_DESKTOP_ICON) && dii->selected)
		drop_desktop_icons (context, data, x + dii->x, y + dii->y);
	else {
		char *full_name;
		file_entry *fe;

		full_name = g_concat_dir_and_file (desktop_directory, dii->filename);
		fe = file_entry_from_file (full_name);
		if (!fe) {
			g_free (full_name);
			return;
		}

		if (gdnd_perform_drop (context, data, full_name, fe))
			desktop_reload_icons (FALSE, 0, 0);

		file_entry_free (fe);
		g_free (full_name);
	}
}

/* Set up a desktop icon as a DnD destination */
static void
setup_icon_dnd_dest (DesktopIconInfo *dii)
{
	gtk_drag_dest_set (DESKTOP_ICON (dii->dicon)->canvas,
			   GTK_DEST_DEFAULT_DROP,
			   dnd_icon_targets,
			   dnd_icon_ntargets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_motion",
			    (GtkSignalFunc) (icon_drag_motion),
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->canvas), "drag_data_received",
			    (GtkSignalFunc) icon_drag_data_received,
			    dii);
}

/* Creates a new desktop icon.  The filename is the pruned filename inside the
 * desktop directory.  It does not show the icon.
 */
static DesktopIconInfo *
desktop_icon_info_new (char *filename, char *url, char *caption, int xpos, int ypos)
{
	DesktopIconInfo *dii;
	file_entry *fe;
	char *full_name;
	GdkImlibImage *icon_im;

	/* Create the icon structure */

	full_name = g_concat_dir_and_file (desktop_directory, filename);
	fe = file_entry_from_file (full_name);
	if (!fe)
		return NULL;

	dii = g_new (DesktopIconInfo, 1);
	dii->x = 0;
	dii->y = 0;
	dii->slot = -1;

	if (url) {
		dii->url = g_strdup (url);
		if (!caption)
			caption = url;
	} else {
		dii->url = NULL;
		if (caption == NULL)
			caption = filename;
	}

	icon_im = gicon_get_icon_for_file (desktop_directory, fe, FALSE);
	dii->dicon = desktop_icon_new (icon_im, caption);
	dii->filename = g_strdup (filename);
	dii->selected = FALSE;

	file_entry_free (fe);
	g_free (full_name);

	/* Connect to the icon's signals.  We connect to the image/stipple and
	 * text items separately so that the callback can distinguish between
	 * them.  This is not a hack.
	 */

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->icon), "event",
			    (GtkSignalFunc) icon_event,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->stipple), "event",
			    (GtkSignalFunc) icon_event,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "event",
			    (GtkSignalFunc) icon_event,
			    dii);

	/* Connect_after to button presses on the icon's window.  This is a
	 * fallback for when the icon is being edited and a button press is not
	 * handled by the icon's text item -- this means the user has clicked
	 * outside the text item and wishes to accept and terminate editing.
	 */
	gtk_signal_connect_after (GTK_OBJECT (dii->dicon), "button_press_event",
				  (GtkSignalFunc) icon_event_after,
				  dii);

	/* Connect to the text item's signals */

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "text_changed",
			    (GtkSignalFunc) text_changed, dii);

	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "editing_started",
			    (GtkSignalFunc) editing_started,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "editing_stopped",
			    (GtkSignalFunc) editing_stopped,
			    dii);
	gtk_signal_connect (GTK_OBJECT (DESKTOP_ICON (dii->dicon)->text), "selection_stopped",
			    (GtkSignalFunc) selection_stopped,
			    dii);

	/* We must set the icon's wmclass and name.  It is already realized (it
	 * comes out realized from desktop_icon_new()).
	 */
	set_icon_wmclass (dii);

	/* Prepare the DnD functionality for this icon */

	setup_icon_dnd_source (dii);
	setup_icon_dnd_dest (dii);

	/* Place the icon and append it to the list */
	desktop_icon_info_place (dii, xpos, ypos);

	/* Put into icon hash table */
	g_hash_table_insert (icon_hash, dii->filename, dii);

	return dii;
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

/*
 * Check that the user's desktop directory exists, and if not, create the
 * default desktop setup.
 */
static void
create_desktop_dir (void)
{
	if (getenv ("GNOME_DESKTOP_DIR") != NULL)
		desktop_directory = g_strdup (getenv ("GNOME_DESKTOP_DIR"));
	else
		desktop_directory = g_concat_dir_and_file (gnome_user_home_dir, DESKTOP_DIR_NAME);

	if (!g_file_exists (desktop_directory)) {
		/* Create the directory */
		mkdir (desktop_directory, 0777);

		/* Create the default set of icons */

		gdesktop_links_init ();
		gmount_setup_devices ();
		gprint_setup_devices ();
	}
}

/* Property placed on target windows */
typedef struct {
  guint8 byte_order;
  guint8 protocol_version;
  guint8 protocol_style;
  guint8 pad;
  guint32 proxy_window;
  guint16 num_drop_sites;
  guint16 padding;
  guint32 total_size;
} MotifDragReceiverInfo;

/* Sets up a proxy window for DnD on the specified X window.  Courtesy of Owen Taylor */
static gboolean
setup_xdnd_proxy (guint32 xid, GdkWindow *proxy_window)
{
	GdkAtom xdnd_proxy_atom;
	Window proxy_xid;
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

/* Sets up a window as a Motif DnD proxy */
static void
setup_motif_dnd_proxy (guint32 xid, GdkWindow *proxy_window)
{
	Window proxy_xid;
	MotifDragReceiverInfo info;
	Atom receiver_info_atom;
	guint32 myint;

	myint = 0x01020304;
	proxy_xid = GDK_WINDOW_XWINDOW (proxy_window);
	receiver_info_atom = gdk_atom_intern ("_MOTIF_DRAG_RECEIVER_INFO", FALSE);

	info.byte_order = (*((gchar *) &myint) == 1) ? 'B' : 'l';
	info.protocol_version = 0;
	info.protocol_style = 5; /* XmDRAG_DYNAMIC */
	info.proxy_window = proxy_xid;
	info.num_drop_sites = 0;
	info.total_size = sizeof (info);

	XChangeProperty (gdk_display, xid,
			 receiver_info_atom,
			 receiver_info_atom,
			 8, PropModeReplace,
			 (guchar *)&info,
			 sizeof (info));
}

/* Callback used when we get a drag_motion event from the desktop.  We must
 * decide what kind of operation can be performed with what the user is
 * dragging.
 */
static gboolean
desktop_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time,
		     gpointer data)
{
	GdkDragAction action;
	GtkWidget *source_widget;
	int is_desktop_icon;

	gdnd_find_panel_by_drag_context (context, &source_widget);
	is_desktop_icon = gdnd_drag_context_has_target (context, TARGET_MC_DESKTOP_ICON);

	action = gdnd_validate_action (context,
				       TRUE,
				       source_widget != NULL,
				       source_widget && is_desktop_icon,
				       desktop_directory,
				       NULL,
				       FALSE);

	gdk_drag_status (context, action, time);
	return TRUE;
}

/* Callback used when the root window receives a drop */
static void
desktop_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
			    GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	gint dx, dy;

	/* Fix the proxy window offsets */

	gdk_window_get_position (widget->window, &dx, &dy);
	x += dx;
	y += dy;

	if (gdnd_drag_context_has_target (context, TARGET_MC_DESKTOP_ICON))
		drop_desktop_icons (context, data, x, y);
	else {
		file_entry *desktop_fe;

		desktop_fe = file_entry_from_file (desktop_directory);
		if (!desktop_fe)
			return; /* eeek */

		if (gdnd_perform_drop (context, data, desktop_directory, desktop_fe))
			desktop_reload_icons (TRUE, x, y);

		file_entry_free (desktop_fe);
	}
}

/* Sets up drag and drop to the desktop root window */
static void
setup_desktop_dnd (void)
{
	if (!setup_xdnd_proxy (GDK_ROOT_WINDOW (), proxy_invisible->window))
		g_warning ("There is already a process taking drops on the desktop!\n");

	setup_motif_dnd_proxy (GDK_ROOT_WINDOW (), proxy_invisible->window);

	gtk_drag_dest_set (proxy_invisible,
			   GTK_DEST_DEFAULT_DROP,
			   dnd_desktop_targets,
			   dnd_desktop_ntargets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "drag_motion",
			    (GtkSignalFunc) desktop_drag_motion,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "drag_data_received",
			    (GtkSignalFunc) desktop_drag_data_received,
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

/* Callback for arranging icons by name */
static void
handle_arrange_icons_name (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_NAME);
}

/* Callback for arranging icons by file type */
static void
handle_arrange_icons_type (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_EXTENSION);
}

/* Callback for arranging icons by file size */
static void
handle_arrange_icons_size (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_SIZE);
}

/* Callback for arranging icons by access time */
static void
handle_arrange_icons_access (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_ACCESS);
}

/* Callback for arranging icons by modification time */
static void
handle_arrange_icons_mod (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_MODIFY);
}

/* Callback for arranging icons by change time */
static void
handle_arrange_icons_change (GtkWidget *widget, gpointer data)
{
	desktop_arrange_icons (SORT_CHANGE);
}

/* Callback for creating a new panel window */
static void
handle_new_window (GtkWidget *widget, gpointer data)
{
	new_panel_at (gnome_user_home_dir);
}

/* Callback for rescanning the desktop directory */
static void
handle_rescan_desktop (GtkWidget *widget, gpointer data)
{
	desktop_reload_icons (FALSE, 0, 0);
}

/* Rescans the mountable devices in the desktop and re-creates their icons */
void
desktop_rescan_devices (void)
{
	desktop_cleanup_devices ();
	gmount_setup_devices ();
	desktop_reload_icons (FALSE, 0, 0);
}

/* Callback for rescanning the mountable devices */
static void
handle_rescan_devices (GtkWidget *widget, gpointer data)
{
	desktop_rescan_devices ();
}

void
desktop_recreate_default_icons (void)
{
	gdesktop_links_init ();
	gprint_setup_devices ();
}

/* Callback for re-creating the default icons */
static void
handle_recreate_default_icons (GtkWidget *widget, gpointer data)
{
	desktop_recreate_default_icons ();
}

static void
set_background_image (GtkWidget *widget, gpointer data)
{
	gchar *bg_capplet;
	gchar *argv[1];
	GtkWidget *msg_box;

	bg_capplet = gnome_is_program_in_path ("background-properties-capplet");

	if (bg_capplet) {
		argv[0] = bg_capplet;
		gnome_execute_async (bg_capplet, 1, argv);
		g_free (bg_capplet);
	} else {
		msg_box = gnome_message_box_new (
			_("Unable to locate the file:\nbackground-properties-capplet\n"
			  "in your path.\n\nWe are unable to set the background."),
			GNOME_MESSAGE_BOX_WARNING,
			GNOME_STOCK_BUTTON_OK,
			NULL);
		gnome_dialog_run (GNOME_DIALOG (msg_box));
	}
}

/* Callback from menus to create a terminal.  If the user creates a terminal
 * from the desktop, he usually wants the cwd to be his home directory, not the
 * desktop directory.
 */
static void
new_terminal (GtkWidget *widget, gpointer data)
{
	if (is_a_desktop_panel (cpanel))
		mc_chdir (home_dir);

	gnome_open_terminal ();
}

static GnomeUIInfo gnome_panel_new_menu [] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Terminal"), N_("Launch a new terminal in the current directory"), new_terminal),
	/* If this ever changes, make sure you update create_new_menu accordingly. */
	GNOMEUIINFO_ITEM_NONE (N_("_Directory..."), N_("Creates a new directory"), gnome_mkdir_cmd),
	GNOMEUIINFO_ITEM_NONE (N_("URL L_ink..."),  N_("Creates a new URL link"), gnome_new_link),
	GNOMEUIINFO_ITEM_NONE (N_("_Launcher..."), N_("Creates a new launcher"), gnome_new_launcher),
	GNOMEUIINFO_END
};

/* Menu items for arranging the desktop icons */
GnomeUIInfo desktop_arrange_icons_items[] = {
	GNOMEUIINFO_ITEM_NONE (N_("By _Name"), NULL, handle_arrange_icons_name),
	GNOMEUIINFO_ITEM_NONE (N_("By File _Type"), NULL, handle_arrange_icons_type),
	GNOMEUIINFO_ITEM_NONE (N_("By _Size"), NULL, handle_arrange_icons_size),
	GNOMEUIINFO_ITEM_NONE (N_("By Time Last _Accessed"), NULL, handle_arrange_icons_access),
	GNOMEUIINFO_ITEM_NONE (N_("By Time Last _Modified"), NULL, handle_arrange_icons_mod),
	GNOMEUIINFO_ITEM_NONE (N_("By Time Last _Changed"), NULL, handle_arrange_icons_change),
	GNOMEUIINFO_END
};

/* The popup menu for the desktop */
GnomeUIInfo desktop_popup_items[] = {
	GNOMEUIINFO_MENU_NEW_SUBTREE (gnome_panel_new_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Arrange Icons"), desktop_arrange_icons_items),
	GNOMEUIINFO_ITEM_NONE (N_("Create _New Window"), NULL, handle_new_window),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Rescan _Desktop Directory"), NULL, handle_rescan_desktop),
	GNOMEUIINFO_ITEM_NONE (N_("Rescan De_vices"), NULL, handle_rescan_devices),
	GNOMEUIINFO_ITEM_NONE (N_("Recreate Default _Icons"), NULL, handle_recreate_default_icons),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Configure _Background Image"), NULL, set_background_image),
	GNOMEUIINFO_END
};

static int
strip_tearoff_menu_item (GnomeUIInfo *infos)
{
	GtkWidget *shell;
	GList *child_list;
	int n;

	g_assert (infos != NULL);

	shell = infos[0].widget->parent;
	child_list = gtk_container_children (GTK_CONTAINER (shell));
	n = g_list_length (child_list);

	if (child_list && GTK_IS_TEAROFF_MENU_ITEM (child_list->data)) {
		n--;
		gtk_widget_destroy (GTK_WIDGET (child_list->data));
#if 0
		gtk_widget_hide (GTK_WIDGET (child_list->data));
#endif
	}

	return n;
}

/* Executes the popup menu for the desktop */
static void
desktop_popup (GdkEventButton *event)
{
	GtkWidget *shell;
	GtkWidget *popup;
	gchar *file, *file2;
	WPanel *panel;
	gint i;

	/* Create the menu and then strip the tearoff menu items, sigh... */

	popup = gnome_popup_menu_new (desktop_popup_items);

	strip_tearoff_menu_item (desktop_arrange_icons_items);
	i = strip_tearoff_menu_item (gnome_panel_new_menu);
	shell = gnome_panel_new_menu[0].widget->parent;
	file = gnome_unconditional_datadir_file ("mc/templates");
	i = create_new_menu_from (file, shell, i);
	file2 = gnome_datadir_file ("mc/templates");
	if (file2 != NULL){
		if (strcmp (file, file2) != 0)
			create_new_menu_from (file2, shell, i);
	}
	g_free (file);
	g_free (file2);

	panel = push_desktop_panel_hack ();
	gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, NULL);
	layout_panel_gone (panel);
	free_panel_from_desktop (panel);
	gtk_widget_destroy (popup);

	desktop_reload_icons (FALSE, 0, 0);
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
	DesktopIconInfo *dii;

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
icon_is_in_area (DesktopIconInfo *dii, int x1, int y1, int x2, int y2)
{
	DesktopIcon *dicon;

	dicon = DESKTOP_ICON (dii->dicon);

	x1 -= dii->x;
	y1 -= dii->y;
	x2 -= dii->x;
	y2 -= dii->y;

	if (x1 == x2 && y1 == y2)
		return FALSE;

	if (x1 < dicon->icon_x + dicon->icon_w
	    && x2 >= dicon->icon_x
	    && y1 < dicon->icon_y + dicon->icon_h
	    && y2 >= dicon->icon_y)
		return TRUE;

	if (x1 < dicon->text_x + dicon->text_w
	    && x2 >= dicon->text_x
	    && y1 < dicon->text_y + dicon->text_h
	    && y2 >= dicon->text_y)
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
	DesktopIconInfo *dii;
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
						desktop_icon_select (DESKTOP_ICON (dii->dicon),
								     !dii->selected);
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

	/* maybe the user wants to click on the icon text */
	if (event->button == 1 && desktop_use_shaped_text) {
		int x = event->x;
		int y = event->y;
		GList *l, *icons = get_all_icons ();
		DesktopIconInfo *clicked = NULL;
		for (l = icons; l; l = l->next) {
			DesktopIconInfo *dii = l->data;
			DesktopIcon *di = DESKTOP_ICON (dii->dicon);
			int x1 = dii->x + di->text_x;
			int y1 = dii->y + di->text_y;
			int x2 = x1 + di->text_w;
			int y2 = y1 + di->text_h;
			if (x>=x1 && y>=y1 && x<=x2 && y<=y2)
				clicked = dii;
		}
		g_list_free (icons);
		if (clicked) {
			select_icon (clicked, event->state);
			return FALSE;
		}
	}

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

/* Terminates rubberbanding when the button is released.  This is shared by the
 * button_release handler and the motion_notify handler.
 */
static void
perform_release (guint32 time)
{
	draw_rubberband (click_current_x, click_current_y);
	gdk_pointer_ungrab (time);
	click_dragging = FALSE;

	update_drag_selection (click_current_x, click_current_y);

	XUngrabServer (GDK_DISPLAY ());
	gdk_flush ();
}

/* Handles button releases on the root window via the click_proxy_gdk_window */
static gint
click_proxy_button_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (!click_dragging || event->button != 1)
		return FALSE;

	perform_release (event->time);
	return TRUE;
}

/* Handles motion events when dragging the icon-selection rubberband on the desktop */
static gint
click_proxy_motion (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	if (!click_dragging)
		return FALSE;

	/* There exists the following race condition.  If in the button_press
	 * handler we manage to grab the server before the window manager can
	 * proxy the button release to us, then we wil not get the release
	 * event.  So we have to check the event mask and fake a release by
	 * hand.
	 */
	if (!(event->state & GDK_BUTTON1_MASK)) {
		perform_release (event->time);
		return TRUE;
	}

	draw_rubberband (click_current_x, click_current_y);
	draw_rubberband (event->x, event->y);
	update_drag_selection (event->x, event->y);
	click_current_x = event->x;
	click_current_y = event->y;

	return TRUE;
}

/*
 * Filter that translates proxied events from virtual root windows into normal
 * Gdk events for the proxy_invisible widget.
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
		desktop_wm_is_gnome_compliant = 0;
		g_warning ("Root window clicks will not work as no GNOME-compliant window manager could be found!");
		return;
	}
	desktop_wm_is_gnome_compliant = 1;

	/* Make the proxy window send events to the invisible proxy widget */
	gdk_window_set_user_data (click_proxy_gdk_window, proxy_invisible);

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

	/* Make the root window send events to the invisible proxy widget */
	gdk_window_set_user_data (GDK_ROOT_PARENT (), proxy_invisible);

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

	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "button_press_event",
			    (GtkSignalFunc) click_proxy_button_press,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "button_release_event",
			    (GtkSignalFunc) click_proxy_button_release,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "motion_notify_event",
			    (GtkSignalFunc) click_proxy_motion,
			    NULL);

	gtk_signal_connect (GTK_OBJECT (proxy_invisible), "property_notify_event",
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
	icon_hash = g_hash_table_new (g_str_hash, g_str_equal);

	gdnd_init ();
	gicon_init ();
	create_layout_info ();
	create_desktop_dir ();
	desktop_reload_icons (FALSE, 0, 0);

	/* Create the proxy window and initialize all proxying stuff */

	proxy_invisible = gtk_invisible_new ();
	gtk_widget_show (proxy_invisible);

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

	/* Remove click-on-desktop crap */

	gdk_window_unref (click_proxy_gdk_window);

	/* Remove DnD crap */

	gtk_widget_destroy (proxy_invisible);
	XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (), gdk_atom_intern ("XdndProxy", FALSE));

	g_hash_table_destroy (icon_hash);
}

void
desktop_create_url (const char *filename, const char *title, const char *url, const char *icon)
{
	FILE *f;

	f = fopen (filename, "w");
	if (f) {

		fprintf (f, "URL: %s\n", url);
		fclose (f);

		gnome_metadata_set (filename, "desktop-url",
				    strlen (url) + 1, url);
		gnome_metadata_set (filename, "icon-caption",
				    strlen (title) + 1, title);

		gnome_metadata_set (filename, "icon-filename", strlen (icon) + 1, icon);
	}
}
