/* GNU Midnight Commander -- GNOME edition
 *
 * Directory display routines
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <string.h>
#include "x.h"
#include "util.h"
#include "global.h"
#include "dir.h"
#include "command.h"
#include "panel.h"
#define WANT_WIDGETS		/* bleah */
#include "main.h"
#include "color.h"
#include "mouse.h"
#include "layout.h"		/* get_panel_widget */
#include "ext.h"		/* regex_command */
#include "cmd.h"		/* copy_cmd, ren_cmd, delete_cmd, ... */
#include "gscreen.h"
#include "dir.h"
#include "dialog.h"
#include "setup.h"
#include "file.h"
#include "fileopctx.h"
#include "gdesktop.h"
#include "gdnd.h"
#include "gtkdtree.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "gcmd.h"
#include "gcliplabel.h"
#include "gicon.h"
#include "gtkflist.h"
#include "../vfs/vfs.h"
#include <gdk/gdkprivate.h>

#ifndef MAX
#   define MAX(a,b) ((a) > (b) ? a : b)
#endif

/* Offsets within the default_column_width array for the different listing types */
static const int column_width_pos[LIST_TYPES] = {
	GMC_COLUMNS_BRIEF,
	0,
	-1,
	GMC_COLUMNS_BRIEF + GMC_COLUMNS_DETAILED,
	-1
};

/* Default column widths for file listings */
int default_column_width[GMC_COLUMNS];

/* default format for custom view */
char* default_user_format = NULL;

/* Whether to display the tree view on the left */
int tree_panel_visible = -1;

/* The pixmaps */
#include "dir-close.xpm"
#include "link.xpm"
#include "dev.xpm"

/* Timeout for auto-scroll on drag */
#define SCROLL_TIMEOUT 100

/* Timeout for opening a tree branch on drag */
#define TREE_OPEN_TIMEOUT 1000

/* This is used to initialize our pixmaps */
static int pixmaps_ready;
GdkPixmap *icon_directory_pixmap;
GdkBitmap *icon_directory_mask;
GdkPixmap *icon_link_pixmap;
GdkBitmap *icon_link_mask;
GdkPixmap *icon_dev_pixmap;
GdkBitmap *icon_dev_mask;

static GtkTargetEntry drag_types [] = {
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_TEXT_PLAIN_TYPE, 0, TARGET_TEXT_PLAIN },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkTargetEntry drop_types [] = {
	{ TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
	{ TARGET_URL_TYPE, 0, TARGET_URL }
};

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

/* GtkWidgets with the shaped windows for dragging */
GtkWidget *drag_directory    = NULL;
GtkWidget *drag_directory_ok = NULL;
GtkWidget *drag_multiple     = NULL;
GtkWidget *drag_multiple_ok  = NULL;

static void file_list_popup (GdkEventButton *event, WPanel *panel);


void
repaint_file (WPanel *panel, int file_index, int move, int attr, int isstatus)
{
}

/*
 * Invoked by the generic code:  show current working directory
 */
void
show_dir (WPanel *panel)
{
	assign_text (panel->current_dir, panel->cwd);
	update_input (panel->current_dir, 1);
	gtk_window_set_title (GTK_WINDOW (panel->xwindow), panel->cwd);
}

/*
 * Utility routine:  Try to load a bitmap for a file_entry
 */
static void
panel_file_list_set_type_bitmap (GtkCList *cl, int row, int column, int color, file_entry *fe)
{
	/* Here, add more icons */
	switch (color){
	case DIRECTORY_COLOR:
		gtk_clist_set_pixmap (cl, row, column, icon_directory_pixmap, icon_directory_mask);
		break;
	case LINK_COLOR:
		gtk_clist_set_pixmap (cl, row, column, icon_link_pixmap, icon_link_mask);
		break;
	case DEVICE_COLOR:
		gtk_clist_set_pixmap (cl, row, column, icon_dev_pixmap, icon_dev_mask);
		break;
	}
}

static void
panel_cancel_drag_scroll (WPanel *panel)
{
	g_return_if_fail (panel != NULL);

	if (panel->timer_id != -1){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}
}

/*
 * Sets the color attributes for a given row.
 */
static void
panel_file_list_set_row_colors (GtkCList *cl, int row, int color_pair)
{
	gtk_clist_set_foreground (cl, row, gmc_color_pairs [color_pair].fore);
	gtk_clist_set_background (cl, row, gmc_color_pairs [color_pair].back);
}

/*
 * Update the status of the back and forward history buttons.
 * Called from the generic code
 */
void
x_panel_update_marks (WPanel *panel)
{
	int ff = panel->dir_history->next ? 1 : 0;
	int bf = panel->dir_history->prev ? 1 : 0;

	if (!panel->fwd_b)
		return;

	gtk_widget_set_sensitive (panel->fwd_b, ff);
	gtk_widget_set_sensitive (panel->back_b, bf);
}

static GtkAdjustment *
scrolled_window_get_vadjustment (GtkScrolledWindow *sw)
{
	GtkRange *vsb = GTK_RANGE (sw->vscrollbar);
	GtkAdjustment *va = vsb->adjustment;

	return va;
}

/*
 * Listing view: Load the contents
 */
static void
panel_fill_panel_list (WPanel *panel)
{
	const int top       = panel->count;
	const int items     = panel->format->items;
	const int selected  = panel->selected;
	GtkCList *cl        = CLIST_FROM_SW (panel->list);
	int i, col, type_col, color;
	int width, p;
	char  **texts;

	texts = g_new (char *, items + 1);

	gtk_clist_freeze (GTK_CLIST (cl));
	gtk_clist_clear (GTK_CLIST (cl));

	/* which column holds the type information */
	type_col = -1;

	g_assert (items == cl->columns);

	texts [items] = NULL;
	for (i = 0; i < top; i++){
		file_entry *fe = &panel->dir.list [i];
		format_e *format = panel->format;
		int n;

		for (col = 0; format; format = format->next){
			if (!format->use_in_gui)
			    continue;

			if (type_col == -1)
				if (strcmp (format->id, "type") == 0)
					type_col = col;

			if (!format->string_fn)
				texts[col] = "";
			else
				texts[col] = (* format->string_fn) (fe, 10);
			col++;
		}

		n = gtk_clist_append (cl, texts);

		/* Do not let the user select .. */
		if (strcmp (fe->fname, "..") == 0)
			gtk_clist_set_selectable (cl, n, FALSE);

		color = file_compute_color (NORMAL, fe);
		panel_file_list_set_row_colors (cl, i, color);

		if (type_col != -1)
			panel_file_list_set_type_bitmap (cl, i, type_col, color, fe);

		if (fe->f.marked)
			gtk_clist_select_row (cl, i, 0);
		else
			gtk_clist_unselect_row (cl, i, 0);

	}

	g_free (texts);

	/* This is needed as the gtk_clist_append changes selected under us :-( */
	panel->selected = selected;

	p = column_width_pos[panel->list_type]; /* offset in column_width */
	g_assert (p >= 0);
	for (i = 0; i < items; i++) {
		width = panel->column_width[p + i];
		if (width == 0)
			width = gtk_clist_optimal_column_width (cl, i);

		gtk_clist_set_column_width (cl, i, width);
	}

	gtk_clist_thaw (GTK_CLIST (cl));
}

/*
 * Icon view: load the panel contents
 */
static void
panel_fill_panel_icons (WPanel *panel)
{
	const int top       = panel->count;
	const int selected  = panel->selected;
	GnomeIconList *icons = ILIST_FROM_SW (panel->icons);
	int i;
	GdkImlibImage *image;

	gnome_icon_list_freeze (icons);
	gnome_icon_list_clear (icons);

	for (i = 0; i < top; i++) {
		file_entry *fe = &panel->dir.list [i];
		int p;

		image = gicon_get_icon_for_file (panel->cwd, fe, TRUE);
		p = gnome_icon_list_append_imlib (icons, image, fe->fname);

		if (fe->f.marked)
			gnome_icon_list_select_icon (icons, p);
		else
			gnome_icon_list_unselect_icon (icons, p);
	}

	/* This is needed as the gtk_gnome_icon_list_append_imlib changes selected under us :-( */
	panel->selected = selected;

	gnome_icon_list_thaw (icons);
}

/*
 * Invoked from the generic code to fill the display
 */
void
x_fill_panel (WPanel *panel)
{
	if (panel->list_type == list_icons)
		panel_fill_panel_icons (panel);
	else
		panel_fill_panel_list (panel);

	gtk_signal_handler_block_by_data (GTK_OBJECT (panel->tree), panel);

	if (vfs_current_is_local ()){
		char buffer [MC_MAXPATHLEN];

		get_current_wd (buffer, sizeof (buffer)-1);
		gtk_dtree_select_dir (GTK_DTREE (panel->tree), buffer);
	} else
		gtk_dtree_select_dir (GTK_DTREE (panel->tree), panel->cwd);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (panel->tree), panel);
}

static void
gmc_panel_set_size (int index, int boot)
{
	Widget *w;
	WPanel *p;

	w = (Widget *) get_panel_widget (index);
	p = (WPanel *) w;
	w->cols = 40;
	w->lines = 25;
	set_panel_formats (p);
	paint_panel (p);

	if (!boot)
		paint_frame (p);

	x_fill_panel (p);
}

void
x_panel_set_size (int index)
{
	printf ("WARNING: set size called\n");
	gmc_panel_set_size (index, 1);
}

/*
 * Invoked when the f.mark field of a file item changes
 */
void
x_panel_select_item (WPanel *panel, int index, int value)
{
	int color;

	color = file_compute_color (NORMAL, &panel->dir.list[index]);
	panel_file_list_set_row_colors (CLIST_FROM_SW (panel->list), index, color);
}

void
x_select_item (WPanel *panel)
{
	if (is_a_desktop_panel (panel))
		return;

	do_file_mark (panel, panel->selected, 1);
	display_mini_info (panel);

	if (panel->list_type == list_icons) {
		GnomeIconList *list = ILIST_FROM_SW (panel->icons);

		gnome_icon_list_select_icon (list, panel->selected);
		if (GTK_WIDGET (list)->allocation.x != -1)
			if (gnome_icon_list_icon_is_visible (list, panel->selected) != GTK_VISIBILITY_FULL)
				gnome_icon_list_moveto (list, panel->selected, 0.5);
	} else {
		GtkCList *clist = CLIST_FROM_SW (panel->list);

		gtk_clist_select_row (clist, panel->selected, 0);

		/* Make it visible */
		if (gtk_clist_row_is_visible (clist, panel->selected) != GTK_VISIBILITY_FULL)
			gtk_clist_moveto (clist, panel->selected, 0, 0.5, 0.0);
	}
}

void
x_unselect_item (WPanel *panel)
{
	int selected = panel->selected;

	if (panel->list_type == list_icons)
		gnome_icon_list_unselect_all (ILIST_FROM_SW (panel->icons), NULL, NULL);
	else
		gtk_clist_unselect_all (CLIST_FROM_SW (panel->list));

	panel->selected = selected;
}

void
x_filter_changed (WPanel *panel)
{
}

void
x_adjust_top_file (WPanel *panel)
{
/*	gtk_clist_moveto (GTK_CLIST (panel->list), panel->top_file, 0, 0.0, 0.0); */
}

static void
panel_file_list_select_row (GtkWidget *file_list, gint row, gint column,
			    GdkEvent *event, gpointer data)
{
	WPanel *panel;

	panel = data;

	panel->selected = row;
	do_file_mark (panel, row, 1);
	display_mini_info (panel);
	execute_hooks (select_file_hook);
}

static void
panel_file_list_unselect_row (GtkWidget *widget, int row, int columns, GdkEvent *event, gpointer data)
{
	WPanel *panel;

	panel = data;
	do_file_mark (panel, row, 0);
	display_mini_info (panel);

	if (panel->marked == 0)
		panel->selected = 0;
}

static void
panel_file_list_resize_callback (GtkCList *clist, gint column, gint width, WPanel *panel)
{
	int p;

	p = column_width_pos[panel->list_type]; /* offset in column_width */
	g_assert (p >= 0);

	panel->column_width[p + column] = width;

	/* make this default */
	memcpy (default_column_width, panel->column_width, sizeof (default_column_width));
}

static void
panel_file_list_column_callback (GtkWidget *widget, int col, WPanel *panel)
{
	format_e *format;
	int i;

	for (i = 0, format = panel->format; format; format = format->next){
		if (!format->use_in_gui)
			continue;
		if (i == col){
			sortfn *sorting_routine;

			sorting_routine = get_sort_fn (format->id);
			if (!sorting_routine)
				return;

			if (sorting_routine == panel->sort_type)
				panel->reverse = !panel->reverse;
			panel->sort_type = sorting_routine;

			do_re_sort (panel);
			return;
		}
		i++;
	}
}

/* Convenience function to load a pixmap and mask from xpm data */
static void
create_pixmap (char **data, GdkPixmap **pixmap, GdkBitmap **mask)
{
	GdkImlibImage *im;

	im = gdk_imlib_create_image_from_xpm_data (data);
	gdk_imlib_render (im, im->rgb_width, im->rgb_height);
	*pixmap = gdk_imlib_copy_image (im);
	*mask = gdk_imlib_copy_mask (im);
	gdk_imlib_destroy_image (im);
}

static void
panel_create_pixmaps (void)
{
	pixmaps_ready = TRUE;

	create_pixmap (DIRECTORY_CLOSE_XPM, &icon_directory_pixmap, &icon_directory_mask);
	create_pixmap (link_xpm, &icon_link_pixmap, &icon_link_mask);
	create_pixmap (dev_xpm, &icon_dev_pixmap, &icon_dev_mask);
}

typedef gboolean (*desirable_fn)(WPanel *p, int x, int y);
typedef gboolean (*scroll_fn)(gpointer data);

/* Sets up a timer to scroll a panel component when something is being dragged
 * over it.  The `desirable' function should return whether it is desirable to
 * scroll at the specified coordinates; and the `scroll' function should
 * actually scroll the view.
 */
static void
panel_setup_drag_scroll (WPanel *panel, int x, int y, desirable_fn desirable, scroll_fn scroll)
{
	panel_cancel_drag_scroll (panel);

	panel->drag_motion_x = x;
	panel->drag_motion_y = y;

	if ((* desirable) (panel, x, y))
		panel->timer_id = gtk_timeout_add (SCROLL_TIMEOUT, scroll, panel);
}

static void
panel_file_list_configure (WPanel *panel, GtkWidget *sw, GtkWidget *file_list)
{
	format_e *format = panel->format;
	int i;

	/* Set sorting callback */
	gtk_signal_connect (GTK_OBJECT (file_list), "click_column",
			    GTK_SIGNAL_FUNC (panel_file_list_column_callback), panel);

	/* Set column resize callback */
	gtk_signal_connect (GTK_OBJECT (file_list), "resize_column",
			    GTK_SIGNAL_FUNC (panel_file_list_resize_callback), panel);

	/* Avoid clist's broken focusing behavior */
	GTK_WIDGET_UNSET_FLAGS (file_list, GTK_CAN_FOCUS);

	/* Semi-sane selection mode */
	gtk_clist_set_selection_mode (GTK_CLIST (file_list), GTK_SELECTION_EXTENDED);

	for (i = 0, format = panel->format; format; format = format->next) {
		GtkJustification just = GTK_JUSTIFY_LEFT;

		if (!format->use_in_gui)
			continue;

		/* Set desired justification */
		switch (HIDE_FIT (format->just_mode)) {
		case J_LEFT:
			just = GTK_JUSTIFY_LEFT;
			break;

		case J_RIGHT:
			just = GTK_JUSTIFY_RIGHT;
			break;

		case J_CENTER:
			just = GTK_JUSTIFY_CENTER;
			break;
		}

		gtk_clist_set_column_justification (GTK_CLIST (file_list), i, just);
		i++;
	}
}

/* Creates an URI list to be transferred during a drop operation */
static char *
panel_build_selected_file_list (WPanel *panel, int *file_list_len)
{
	if (panel->marked){
		char *sep = "\r\n";
		char *data, *copy;
		int i, total_len;
		int cwdlen = strlen (panel->cwd) + 1;
		int filelen = strlen ("file:");
		int seplen  = strlen ("\r\n");

		/* first pass, compute the length */
		total_len = 0;
		for (i = 0; i < panel->count; i++)
			if (panel->dir.list [i].f.marked)
				total_len += (filelen + cwdlen
					      + panel->dir.list [i].fnamelen
					      + seplen);

		total_len++;
		data = copy = g_malloc (total_len+1);
		for (i = 0; i < panel->count; i++)
			if (panel->dir.list [i].f.marked){
				strcpy (copy, "file:");
				strcpy (&copy [filelen], panel->cwd);
				copy [filelen+cwdlen-1] = '/';
				strcpy (&copy [filelen + cwdlen], panel->dir.list [i].fname);
				strcpy (&copy [filelen + cwdlen + panel->dir.list [i].fnamelen], sep);
				copy += filelen + cwdlen + panel->dir.list [i].fnamelen + seplen;
			}
		data [total_len] = 0;
		*file_list_len = total_len;
		return data;
	} else {
		char *fullname, *uri;

		fullname = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);

		uri = g_strconcat ("file:", fullname, NULL);
		g_free (fullname);

		*file_list_len = strlen (uri) + 1;
		return uri;
	}
}

/**
 * panel_drag_data_get:
 *
 * Invoked when a drag operation has been performed, this routine
 * provides the data to be transfered
 */
static void
panel_drag_data_get (GtkWidget        *widget,
		     GdkDragContext   *context,
		     GtkSelectionData *selection_data,
		     guint            info,
		     guint32          time,
		     WPanel           *panel)
{
	int len;
	char *data;
	GList *files;

	panel_cancel_drag_scroll (panel);
	data = panel_build_selected_file_list (panel, &len);

	switch (info){
	case TARGET_URI_LIST:
	case TARGET_TEXT_PLAIN:
		gtk_selection_data_set (selection_data, selection_data->target, 8, data, len);
		break;

	case TARGET_URL:
		files = gnome_uri_list_extract_uris (data);
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

	g_free (data);
}

/**
 * panel_drag_data_delete:
 *
 * Invoked when the destination requests the information to be deleted
 * possibly because the operation was MOVE.
 */
static void
panel_drag_data_delete (GtkWidget *widget, GdkDragContext *context, WPanel *panel)
{
	/* Things is: The File manager already handles file moving */
}

/* Performs a drop on a panel.  If idx is -1, then drops on the panel's cwd
 * itself.
 */
static void
drop_on_panel (WPanel *panel, int idx, GdkDragContext *context, GtkSelectionData *selection_data)
{
	char *file;
	file_entry *fe;
	int drop_on_dir;
	int reload;

	g_assert (panel != NULL);
	g_assert (idx == -1 || idx < panel->count);
	g_assert (context != NULL);
	g_assert (selection_data != NULL);

	drop_on_dir = FALSE;

	if (idx == -1)
		drop_on_dir = TRUE;
	else {
		fe = &panel->dir.list[idx];
		file = g_concat_dir_and_file (panel->cwd, fe->fname);

		if (!((S_ISDIR (fe->buf.st_mode) || fe->f.link_to_dir)
		      || gdnd_can_drop_on_file (file, fe))) {
			g_free (file);
			drop_on_dir = TRUE;
		}
	}

	if (drop_on_dir) {
		file = panel->cwd;
		fe = file_entry_from_file (file);
		if (!fe)
			return; /* eeeek */
	}

	reload = gdnd_perform_drop (context, selection_data, file, fe);

	if (file != panel->cwd)
		g_free (file);

	if (drop_on_dir)
		file_entry_free (fe);

	if (reload) {
		update_panels (UP_OPTIMIZE, UP_KEEPSEL);
		repaint_screen ();
	}
}

/**
 * panel_icon_list_drag_data_received:
 *
 * Invoked on the target side of a Drag and Drop operation when data has been
 * dropped.
 */
static void
panel_icon_list_drag_data_received (GtkWidget          *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint32             time,
				    WPanel              *panel)
{
	int idx;

	idx = gnome_icon_list_get_icon_at (GNOME_ICON_LIST (widget), x, y);
	drop_on_panel (panel, idx, context, selection_data);
}

/**
 * panel_clist_drag_data_received:
 *
 * Invoked on the target side of a Drag and Drop operation when data has been
 * dropped.
 */
static void
panel_clist_drag_data_received (GtkWidget          *widget,
				GdkDragContext     *context,
				gint                x,
				gint                y,
				GtkSelectionData   *selection_data,
				guint               info,
				guint32             time,
				WPanel              *panel)
{
	GtkCList *clist;
	int row;

	clist = GTK_CLIST (widget);

	/* Normalize the y coordinate to the clist_window */

	y -= (GTK_CONTAINER (clist)->border_width
	      + clist->column_title_area.y
	      + clist->column_title_area.height);

	if (gtk_clist_get_selection_info (GTK_CLIST (widget), x, y, &row, NULL) == 0)
		row = -1;

	drop_on_panel (panel, row, context, selection_data);
}

/**
 * panel_tree_drag_data_received:
 *
 * Invoked on the target side when a drop has been received in the Tree
 */
static void
panel_tree_drag_data_received (GtkWidget          *widget,
			       GdkDragContext     *context,
			       gint                x,
			       gint                y,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint32             time,
			       WPanel              *panel)
{
	GtkDTree *dtree = GTK_DTREE (widget);
	GtkCTreeNode *node;
	int row, col;
	file_entry *fe;
	char *path;
	int reload;

	if (!gtk_clist_get_selection_info (GTK_CLIST (dtree), x, y, &row, &col))
		return;

	node = gtk_ctree_node_nth (GTK_CTREE (dtree), row);
	if (!node)
		return;

	path = gtk_dtree_get_row_path (dtree, node);
	fe = file_entry_from_file (path);
	if (!fe) {
		g_free (path);
		return; /* eeeek */
	}

	reload = gdnd_perform_drop (context, selection_data, path, fe);

	file_entry_free (fe);
	g_free (path);

	if (reload) {
		update_panels (UP_OPTIMIZE, UP_KEEPSEL);
		repaint_screen ();
	}
}

static void
load_dnd_icons (void)
{
	if (!drag_directory)
		drag_directory = gnome_stock_transparent_window (GNOME_STOCK_PIXMAP_NOT, NULL);

	if (!drag_directory_ok)
		drag_directory_ok = gnome_stock_transparent_window (GNOME_STOCK_PIXMAP_NEW, NULL);

	if (!drag_multiple)
		drag_multiple = gnome_stock_transparent_window (GNOME_STOCK_PIXMAP_NOT, NULL);

	if (!drag_multiple_ok)
		drag_multiple_ok = gnome_stock_transparent_window (GNOME_STOCK_PIXMAP_MULTIPLE, NULL);
}

/* Convenience function to start a drag operation for the icon and file lists */
static void
start_drag (GtkWidget *widget, int button, GdkEvent *event)
{
	GtkTargetList *list;
	GdkDragContext *context;

	list = gtk_target_list_new (drag_types, ELEMENTS (drag_types));

	context = gtk_drag_begin (widget, list,
				  (GDK_ACTION_COPY | GDK_ACTION_MOVE
				   | GDK_ACTION_LINK | GDK_ACTION_ASK),
				  button, event);
	gtk_drag_set_icon_default (context);
}

static int
panel_widget_motion (GtkWidget *widget, GdkEventMotion *event, WPanel *panel)
{
	if (!panel->maybe_start_drag)
		return FALSE;

	if (!((panel->maybe_start_drag == 1 && (event->state & GDK_BUTTON1_MASK))
	      || (panel->maybe_start_drag == 2 && (event->state & GDK_BUTTON2_MASK))))
		return FALSE;

	/* This is the same threshold value that is used in gtkdnd.c */

	if (MAX (abs (panel->click_x - event->x),
		 abs (panel->click_y - event->y)) <= 3)
		return FALSE;

	start_drag (widget, panel->maybe_start_drag, (GdkEvent *) event);
	return FALSE;
}

/**
 * panel_drag_begin:
 *
 * Invoked when a drag is starting in the List view or the Icon view
 */
static void
panel_drag_begin (GtkWidget *widget, GdkDragContext *context, WPanel *panel)
{
	panel->dragging = 1;
}

/**
 * panel_drag_end:
 *
 * Invoked when a drag has finished in the List view or the Icon view
 */
static void
panel_drag_end (GtkWidget *widget, GdkDragContext *context, WPanel *panel)
{
	panel_cancel_drag_scroll (panel);
	panel->dragging = 0;
}

/**
 * panel_clist_scrolling_is_desirable:
 *
 * If the cursor is in a position close to either edge (top or bottom)
 * and there is possible to scroll the window, this routine returns
 * true.
 */
static gboolean
panel_clist_scrolling_is_desirable (WPanel *panel, int x, int y)
{
	GtkAdjustment *va;

	va = scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (panel->list));

	if (y < 10) {
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (CLIST_FROM_SW (panel->list)->clist_window_height - 10)) {
			if (va->value < va->upper - va->page_size)
				return TRUE;
		}
	}

	return FALSE;
}


/**
 * panel_clist_scroll:
 *
 * Timer callback invoked to scroll the clist window
 */
static gboolean
panel_clist_scroll (gpointer data)
{
	WPanel *panel = data;
	GtkAdjustment *va;
	double v;

	va = scrolled_window_get_vadjustment (panel->list);

	if (panel->drag_motion_y < 10) {
		v = va->value - va->step_increment;
		if (v < va->lower)
			v = va->lower;

		gtk_adjustment_set_value (va, v);
	} else {
		v = va->value + va->step_increment;
		if (v > va->upper - va->page_size)
			v = va->upper - va->page_size;

		gtk_adjustment_set_value (va, v);
	}

	return TRUE;
}

/* Callback used for drag motion events over the clist.  We set up
 * auto-scrolling and validate the drop to present the user with the correct
 * feedback.
 */
static gboolean
panel_clist_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time,
			 gpointer data)
{
	WPanel *panel;
	GtkCList *clist;
	GdkDragAction action;
	GtkWidget *source_widget;
	gint idx;
	file_entry *fe;
	char *full_name;

	panel = data;

	/* Normalize the y coordinate to the clist_window */

	clist = CLIST_FROM_SW (panel->list);
	y -= (GTK_CONTAINER (clist)->border_width
	      + clist->column_title_area.y
	      + clist->column_title_area.height);

	if (y < 0) {
		gdk_drag_status (context, 0, time);
		goto out;
	}

	/* Set up auto-scrolling */

	panel_setup_drag_scroll (panel, x, y,
				 panel_clist_scrolling_is_desirable,
				 panel_clist_scroll);

	/* Validate the drop */

	gdnd_find_panel_by_drag_context (context, &source_widget);

	if (!gtk_clist_get_selection_info (GTK_CLIST (widget), x, y, &idx, NULL))
		fe = NULL;
	else
		fe = &panel->dir.list[idx];

	full_name = fe ? g_concat_dir_and_file (panel->cwd, fe->fname) : panel->cwd;

	action = gdnd_validate_action (context,
				       FALSE,
				       source_widget != NULL,
				       source_widget == widget,
				       full_name,
				       fe,
				       fe ? fe->f.marked : FALSE);

	if (full_name != panel->cwd)
		g_free (full_name);

	gdk_drag_status (context, action, time);

 out:
	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "drag_motion");
	return TRUE;
}

/**
 * panel_clist_drag_leave
 *
 * Invoked when the dragged object has left our region
 */
static void
panel_clist_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	WPanel *panel;

	panel = data;
	panel_cancel_drag_scroll (panel);
}

/**
 * panel_icon_list_scrolling_is_desirable:
 *
 * If the cursor is in a position close to either edge (top or bottom)
 * and there is possible to scroll the window, this routine returns
 * true.
 */
static gboolean
panel_icon_list_scrolling_is_desirable (WPanel *panel, int x, int y)
{
	GtkAdjustment *va;

	va = scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (panel->icons));

	if (y < 10) {
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (GTK_WIDGET (ILIST_FROM_SW (panel->icons))->allocation.height - 10)) {
			if (va->value < va->upper - va->page_size)
				return TRUE;
		}
	}

	return FALSE;
}


/**
 * panel_icon_list_scroll:
 *
 * Timer callback invoked to scroll the clist window
 */
static gboolean
panel_icon_list_scroll (gpointer data)
{
	WPanel *panel = data;
	GtkAdjustment *va;
	double v;

	va = scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (panel->icons));

	if (panel->drag_motion_y < 10) {
		v = va->value - va->step_increment;
		if (v < va->lower)
			v = va->lower;

		gtk_adjustment_set_value (va, v);
	} else {
		v = va->value + va->step_increment;
		if (v > va->upper - va->page_size)
			v = va->upper - va->page_size;

		gtk_adjustment_set_value (va, v);
	}

	return TRUE;
}

/* Callback used for drag motion events in the icon list.  We need to set up
 * auto-scrolling and validate the drop to present the user with the correct
 * feedback.
 */
static gboolean
panel_icon_list_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time,
			     gpointer data)
{
	WPanel *panel;
	GdkDragAction action;
	GtkWidget *source_widget;
	int idx;
	file_entry *fe;
	char *full_name;

	panel = data;

	/* Set up auto-scrolling */

	panel_setup_drag_scroll (panel, x, y,
				 panel_icon_list_scrolling_is_desirable,
				 panel_icon_list_scroll);

	/* Validate the drop */

	gdnd_find_panel_by_drag_context (context, &source_widget);

	idx = gnome_icon_list_get_icon_at (GNOME_ICON_LIST (widget), x, y);
	fe = (idx == -1) ? NULL : &panel->dir.list[idx];

	full_name = fe ? g_concat_dir_and_file (panel->cwd, fe->fname) : panel->cwd;

	action = gdnd_validate_action (context,
				       FALSE,
				       source_widget != NULL,
				       source_widget == widget,
				       full_name,
				       fe,
				       fe ? fe->f.marked : FALSE);

	if (full_name != panel->cwd)
		g_free (full_name);

	gdk_drag_status (context, action, time);
	return TRUE;
}

/**
 * panel_icon_list_drag_leave:
 *
 * Invoked when the dragged object has left our region
 */
static void
panel_icon_list_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	WPanel *panel = data;

	panel_cancel_drag_scroll (panel);
}

/* Handler for the row_popup_menu signal of the file list. */
static void
panel_file_list_row_popup_menu (GtkFList *flist, GdkEventButton *event, gpointer data)
{
	WPanel *panel;

	panel = data;
	gpopup_do_popup2 (event, panel, NULL);
}

/* Handler for the empty_popup_menu signal of the file list. */
static void
panel_file_list_empty_popup_menu (GtkFList *flist, GdkEventButton *event, gpointer data)
{
	WPanel *panel;

	panel = data;
	file_list_popup (event, panel);
}

/* Handler for the open_row signal of the file list */
static void
panel_file_list_open_row (GtkFList *flist, gpointer data)
{
	WPanel *panel;

	panel = data;
	do_enter (panel);
}

/* Handler for the start_drag signal of the file list */
static void
panel_file_list_start_drag (GtkFList *flist, gint button, GdkEvent *event, gpointer data)
{
	start_drag (GTK_WIDGET (flist), button, event);
}

/*
 * Create, setup the file listing display.
 */
static GtkWidget *
panel_create_file_list (WPanel *panel)
{
	const int items = panel->format->items;
	format_e  *format = panel->format;
	GtkWidget *file_list;
	GtkWidget *sw;
	gchar     **titles;
	int       i;

	titles = g_new (char *, items);
	for (i = 0; i < items; format = format->next)
		if (format->use_in_gui)
			titles [i++] = format->title;

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	file_list = gtk_flist_new_with_titles (panel, items, titles);
	gtk_container_add (GTK_CONTAINER (sw), file_list);
	gtk_widget_show (file_list);

	panel_file_list_configure (panel, sw, file_list);
	g_free (titles);

	gtk_signal_connect (GTK_OBJECT (file_list), "select_row",
			    GTK_SIGNAL_FUNC (panel_file_list_select_row),
			    panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "unselect_row",
			    GTK_SIGNAL_FUNC (panel_file_list_unselect_row),
			    panel);

	/* Connect to the flist signals */

	gtk_signal_connect (GTK_OBJECT (file_list), "row_popup_menu",
			    GTK_SIGNAL_FUNC (panel_file_list_row_popup_menu),
			    panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "empty_popup_menu",
			    GTK_SIGNAL_FUNC (panel_file_list_empty_popup_menu),
			    panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "open_row",
			    GTK_SIGNAL_FUNC (panel_file_list_open_row),
			    panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "start_drag",
			    GTK_SIGNAL_FUNC (panel_file_list_start_drag),
			    panel);

	/* Set up drag and drop */

	load_dnd_icons ();

	gtk_drag_dest_set (GTK_WIDGET (file_list),
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (file_list), "drag_data_get",
			    GTK_SIGNAL_FUNC (panel_drag_data_get), panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_data_delete",
			    GTK_SIGNAL_FUNC (panel_drag_data_delete), panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_data_received",
			    GTK_SIGNAL_FUNC (panel_clist_drag_data_received), panel);

	/*
	 * This signal is provided for scrolling the main window
	 * if data is being dragged
	 */
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_motion",
			    GTK_SIGNAL_FUNC (panel_clist_drag_motion), panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_leave",
			    GTK_SIGNAL_FUNC (panel_clist_drag_leave), panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_begin",
			    GTK_SIGNAL_FUNC (panel_drag_begin), panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "drag_end",
			    GTK_SIGNAL_FUNC (panel_drag_end), panel);

	return sw;
}

/*
 * Callback: icon selected
 */
static void
panel_icon_list_select_icon (GtkWidget *widget, int index, GdkEvent *event, WPanel *panel)
{
	panel->selected = index;
	do_file_mark (panel, index, 1);
	display_mini_info (panel);
	execute_hooks (select_file_hook);

	/* Do not let the user select .. */
	if (strcmp (panel->dir.list[index].fname, "..") == 0)
		gnome_icon_list_unselect_icon (GNOME_ICON_LIST (widget), index);

	if (!event)
		return;

	switch (event->type){
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3)
			gpopup_do_popup2 ((GdkEventButton *) event, panel, NULL);

		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 2){
			char *fullname;

			if (S_ISDIR (panel->dir.list [index].buf.st_mode) ||
			    panel->dir.list [index].f.link_to_dir){
				fullname = concat_dir_and_file (panel->cwd, panel->dir.list [index].fname);
				new_panel_at (fullname);
				g_free (fullname);
			}
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1) {
			do_enter (panel);
		}
		break;

	default:
		break;
	}
}

static void
panel_icon_list_unselect_icon (GtkWidget *widget, int index, GdkEvent *event, WPanel *panel)
{
	do_file_mark (panel, index, 0);
	display_mini_info (panel);
	if (panel->marked == 0)
		panel->selected = 0;
}

static int
queue_reread_cmd (gpointer data)
{
	reread_cmd ();
	return FALSE;
}

/* Renames a file using a file operation context.  Returns FILE_CONT on success. */
int
rename_file_with_context (char *source, char *dest)
{
	FileOpContext *ctx;
	struct stat s;
	long count;
	double bytes;
	int retval;

	if (mc_lstat (source, &s) != 0)
		return FILE_ABORT;

	ctx = file_op_context_new ();
	file_op_context_create_ui (ctx, OP_MOVE, FALSE);

	count = 1;
	bytes = s.st_size;

	retval = move_file_file (ctx, source, dest, &count, &bytes);
	file_op_context_destroy (ctx);

	return retval;
}

static int
panel_icon_renamed (GtkWidget *widget, int index, char *dest, WPanel *panel)
{
	char *source;
	char *fullname;
	int retval;

	if (strcmp (dest, panel->dir.list[index].fname) == 0)
		return TRUE; /* do nothing if the name did not change */

	source = g_concat_dir_and_file (cpanel->cwd, panel->dir.list[index].fname);
	fullname = g_concat_dir_and_file (cpanel->cwd, dest);

	if (rename_file_with_context (source, dest) == FILE_CONT) {
		g_free (panel->dir.list [index].fname);
		panel->dir.list [index].fname = g_strdup (dest);

		gtk_idle_add (queue_reread_cmd, NULL);
		retval = TRUE;
	} else
		retval = FALSE;

	g_free (source);
	g_free (fullname);

	return retval;
}

/* Callback for rescanning the cwd */
static void
handle_rescan_directory (GtkWidget *widget, gpointer data)
{
	reread_cmd ();
}

/* The popup menu for file panels */
static GnomeUIInfo file_list_popup_items[] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Rescan Directory"), N_("Reloads the current directory"),
			       handle_rescan_directory),
	GNOMEUIINFO_ITEM_NONE (N_("New _Directory..."), N_("Creates a new directory here"),
			       gnome_mkdir_cmd),
	GNOMEUIINFO_ITEM_NONE (N_("New _File..."), N_("Creates a new file here"),
			       gnome_newfile_cmd),
	GNOMEUIINFO_END
};

/* Creates the popup menu when the user clicks button 3 on the blank area of the
 * file panels.
 */
static void
file_list_popup (GdkEventButton *event, WPanel *panel)
{
	GtkWidget *popup;

	popup = gnome_popup_menu_new (file_list_popup_items);
	gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, panel);
	gtk_widget_destroy (popup);
}


/* Returns whether an icon in the icon list is being edited.  FIXME:  This
 * function uses a fantastically ugly hack to figure this out.  It would be
 * saner to have a function provided by the icon list widget to do it, but we
 * can't break forwards compatibility at this point.  It would be even saner to
 * have a good DnD API for the icon list.
 */
static int
editing_icon_list (GnomeIconList *gil)
{
	GnomeCanvasItem *item;

	item = GNOME_CANVAS (gil)->focused_item;
	return (item && GNOME_IS_ICON_TEXT_ITEM (item) && GNOME_ICON_TEXT_ITEM (item)->editing);
}

/*
 * Strategy for activaing the drags from the icon-list:
 *
 * The icon-list uses the button-press/motion-notify events for
 * the banding selection.  We catch the events and only if the
 * click happens in an icon and the user moves the mouse enough (a
 * threshold to give it a better feel) activa the drag and drop.
 *
 */
static int
panel_icon_list_button_press (GtkWidget *widget, GdkEventButton *event, WPanel *panel)
{
	GnomeIconList *gil = GNOME_ICON_LIST (widget);
	int icon;

	if (editing_icon_list (gil))
		return FALSE;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	icon = gnome_icon_list_get_icon_at (gil, event->x, event->y);

	if (icon == -1) {
		if (event->button == 3) {
			file_list_popup (event, panel);
			return TRUE;
		}
	} else if (event->button != 3)
		panel->maybe_start_drag = event->button;

	panel->click_x = event->x;
	panel->click_y = event->y;
	return FALSE;
}

static int
panel_icon_list_button_release (GtkWidget *widget, GdkEventButton *event, WPanel *panel)
{
	panel->maybe_start_drag = 0;
	return FALSE;
}

/* Create and setup the icon field display */
static GtkWidget *
panel_create_icon_display (WPanel *panel)
{
	GtkWidget *sw;
	GnomeIconList *ilist;

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ilist = GNOME_ICON_LIST (
		gnome_icon_list_new_flags (90, NULL,
					   (GNOME_ICON_LIST_IS_EDITABLE
					    | GNOME_ICON_LIST_STATIC_TEXT)));
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (ilist));
	gtk_widget_show (GTK_WIDGET (ilist));

	gnome_icon_list_set_separators (ilist, " /-_.");
	gnome_icon_list_set_row_spacing (ilist, 2);
	gnome_icon_list_set_col_spacing (ilist, 2);
	gnome_icon_list_set_icon_border (ilist, 2);
	gnome_icon_list_set_text_spacing (ilist, 2);

	gnome_icon_list_set_selection_mode (ilist, GTK_SELECTION_MULTIPLE);
	GTK_WIDGET_SET_FLAGS (ilist, GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (ilist), "select_icon",
			    GTK_SIGNAL_FUNC (panel_icon_list_select_icon),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "unselect_icon",
			    GTK_SIGNAL_FUNC (panel_icon_list_unselect_icon),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "text_changed",
			    GTK_SIGNAL_FUNC (panel_icon_renamed),
			    panel);

	/* Setup the icons and DnD */

	load_dnd_icons ();

	gtk_drag_dest_set (GTK_WIDGET (ilist),
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (ilist), "drag_data_get",
			    GTK_SIGNAL_FUNC (panel_drag_data_get),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_data_delete",
			    GTK_SIGNAL_FUNC (panel_drag_data_delete),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_data_received",
			    GTK_SIGNAL_FUNC (panel_icon_list_drag_data_received),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_begin",
			    GTK_SIGNAL_FUNC (panel_drag_begin), panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_end",
			    GTK_SIGNAL_FUNC (panel_drag_end), panel);

	/* These implement our drag-start activation code, as we have a pretty oveloaded widget */

	gtk_signal_connect (GTK_OBJECT (ilist), "button_press_event",
			    GTK_SIGNAL_FUNC (panel_icon_list_button_press),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "button_release_event",
			    GTK_SIGNAL_FUNC (panel_icon_list_button_release),
			    panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "motion_notify_event",
			    GTK_SIGNAL_FUNC (panel_widget_motion),
			    panel);

	/* This signal is provide for scrolling the main window if data is being
	 * dragged.
	 */
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_motion",
			    GTK_SIGNAL_FUNC (panel_icon_list_drag_motion), panel);
	gtk_signal_connect (GTK_OBJECT (ilist), "drag_leave",
			    GTK_SIGNAL_FUNC (panel_icon_list_drag_leave), panel);

	return sw;
}

static void
panel_switch_new_display_mode (WPanel *panel)
{
	GtkWidget *old_list = panel->list;

	if (!old_list)
		return;

	panel->list = panel_create_file_list (panel);
	gtk_widget_destroy (old_list);
	gtk_container_add (GTK_CONTAINER (panel->panel_listbox), panel->list);
	gtk_widget_show_all (panel->list);
}

static GtkWidget *
panel_create_cwd (Dlg_head *h, WPanel *panel, void **entry)
{
	WInput *in;

	in = input_new (0, 0, 0, 10, "", "cwd");
	add_widget (h, in);

	/* Force the creation of the gtk widget */
	send_message_to (h, (Widget *) in, WIDGET_INIT, 0);

	*entry = in;
	/* FIXME: for now, we set the usize.  Ultimately, toolbar
	 * will let you expand it, we hope.
	 */
	gtk_widget_set_usize (GTK_WIDGET (in->widget.wdata), 296, -1);
	return GTK_WIDGET (in->widget.wdata);
}

void
display_mini_info (WPanel *panel)
{
	GnomeAppBar *bar = GNOME_APPBAR (panel->ministatus);

	if (panel->searching) {
		char *buf;

		buf = g_strdup_printf (_("Search: %s"), panel->search_buffer);
		gnome_appbar_pop (bar);
		gnome_appbar_push (bar, buf);
		g_free (buf);
		return;
	}

	if (panel->marked){
		char *buf;

		buf = g_strdup_printf ((panel->marked == 1) ? _("%s bytes in %d file") : _("%s bytes in %d files"),
				       size_trunc_sep (panel->total),
				       panel->marked);
		gnome_appbar_pop (bar);
		gnome_appbar_push (bar, buf);
		g_free (buf);
		return;
	}

	if (S_ISLNK (panel->dir.list [panel->selected].buf.st_mode)){
		char *link, link_target [MC_MAXPATHLEN];
		int  len;

		link = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
		len = mc_readlink (link, link_target, MC_MAXPATHLEN);
		g_free (link);

		if (len > 0){
			link_target [len] = 0;
			/* FIXME: Links should be handled differently */
			/* str = g_strconcat ("-> ", link_target, NULL); */
			gnome_appbar_pop (bar);
			gnome_appbar_push (bar, " ");
			/* g_free (str); */
		} else {
			gnome_appbar_pop (bar);
			gnome_appbar_push (bar, _("<readlink failed>"));
		}
		return;
	}

	if (panel->estimated_total > 8){
		int  len = panel->estimated_total;
		char *buffer;

		buffer = g_malloc (len + 2);
		format_file (buffer, panel, panel->selected, panel->estimated_total-2, 0, 1);
		buffer [len] = 0;
		gnome_appbar_pop (bar);
		gnome_appbar_push (bar, buffer);
		g_free (buffer);
	}
	if (panel->list_type == list_icons){
		if (panel->marked == 0){
			gnome_appbar_pop (bar);
			gnome_appbar_push (bar, " ");
		}
	}
}

/* Signal handler for DTree's "directory_changed" signal */
static void
panel_chdir (GtkDTree *dtree, char *path, WPanel *panel)
{
	if (panel->dragging)
		return;

	if (do_panel_cd (panel, path, cd_exact))
		return; /* success */

	if (panel->list_type == list_icons)
		gnome_icon_list_clear (ILIST_FROM_SW (panel->icons));
	else
		gtk_clist_clear (CLIST_FROM_SW (panel->list));

	strncpy (panel->cwd, path, sizeof (panel->cwd));
	show_dir (panel);
}

static void
set_cursor (WPanel *panel, GdkCursorType type)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (type);
	gdk_window_set_cursor (GTK_WIDGET (panel->xwindow)->window, cursor);
	gdk_cursor_destroy (cursor);
	gdk_flush ();
}

static void
panel_tree_scan_begin (GtkWidget *widget, gpointer data)
{
	set_cursor (data, GDK_WATCH);
}

static void
panel_tree_scan_end (GtkWidget *widget, gpointer data)
{
	set_cursor (data, GDK_TOP_LEFT_ARROW);
}

/* Handler for the possibly_ungrab signal of the dtree widget */
static void
panel_tree_possibly_ungrab (GtkWidget *widget, gpointer data)
{
	WPanel *panel;

	panel = data;

	/* The stupid clist button press handler grabs the mouse.  We will get
	 * called when the user presses the mouse on the tree, so we ungrab it.
	 * Also, we have to make sure we don't knock away a DnD grab.
	 */
	if (!panel->drag_tree_dragging_over)
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
}

/* Callback for the drag_begin signal of the tree */
static void
panel_tree_drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	GtkDTree *dtree;
	WPanel *panel;

	dtree = GTK_DTREE (widget);
	panel = data;

	panel->dragging = TRUE;
	dtree->drag_dir = g_strdup (dtree->current_path);
}

/* Callback for the drag_end signal of the tree */
static void
panel_tree_drag_end (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	GtkDTree *dtree;
	WPanel *panel;

	dtree = GTK_DTREE (widget);
	panel = data;

	panel->dragging = FALSE;
	g_free (dtree->drag_dir);
	dtree->drag_dir = NULL;
}

/* Callback for the drag_data_get signal of the tree */
static void
panel_tree_drag_data_get (GtkWidget *widget, GdkDragContext *context,
			  GtkSelectionData *selection_data, guint info, guint time, gpointer data)
{
	GtkDTree *dtree;
	char *str;

	dtree = GTK_DTREE (widget);

	switch (info){
	case TARGET_URI_LIST:
	case TARGET_TEXT_PLAIN:
		str = g_strconcat ("file:", dtree->drag_dir, NULL);
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					str, strlen (str) + 1);
		break;

	default:
		/* FIXME: handle TARGET_URL */
		break;
	}
}

/**
 * tree_drag_open_directory:
 *
 * This routine is invoked in a delayed fashion if the user
 * keeps the drag cursor still over the widget.
 */
static gint
tree_drag_open_directory (gpointer data)
{
	WPanel *panel;
	GtkDTree *dtree;
	GtkCTreeNode *node;

	panel = data;
	dtree = GTK_DTREE (panel->tree);

	node = gtk_ctree_node_nth (GTK_CTREE (panel->tree), panel->drag_tree_row);
	g_assert (node != NULL);

	if (!GTK_CTREE_ROW (node)->expanded) {
#if 0
		/* FIXME: Disabled until fully debugged.  Should also be configurable. */
		dtree->auto_expanded_nodes = g_list_append (dtree->auto_expanded_nodes, node);
#endif
		gtk_ctree_expand (GTK_CTREE (panel->tree), node);
	}

	return FALSE;
}

/* Handles automatic collapsing of the tree nodes when doing drag and drop */
static void
panel_tree_check_auto_expand (WPanel *panel, GtkCTreeNode *current)
{
	GtkDTree *dtree;
	GtkCTree *ctree;
	GtkCList *clist;
	GList *free_list;
	GList *tmp_list;
	gint row, old_y, new_y;

	dtree = GTK_DTREE (panel->tree);
	ctree = GTK_CTREE (panel->tree);
	clist = GTK_CLIST (panel->tree);
	tmp_list = dtree->auto_expanded_nodes;

	if (current)
		for (; tmp_list; tmp_list = tmp_list->next)
			if (!gtk_dtree_is_ancestor (dtree, tmp_list->data, current))
				break;

	/* Collapse the rows as necessary. If possible, try to do so that the
	 * "current" stays the same place on the screen.
	 */
	if (tmp_list) {
		if (current) {
			row = g_list_position (clist->row_list, (GList *) current);
			old_y = row * clist->row_height - clist->vadjustment->value;
		}

		free_list = tmp_list;

		for (; tmp_list; tmp_list = tmp_list->next)
			gtk_ctree_collapse (GTK_CTREE (panel->tree), tmp_list->data);

		/* We have to calculate the row position again because rows may
		 * have shifted during the collapse.
		 */
		if (current) {
			row = g_list_position (clist->row_list, (GList *) current);
			new_y = row * clist->row_height - clist->vadjustment->value;

			if (new_y != old_y)
				gtk_adjustment_set_value (clist->vadjustment,
							  clist->vadjustment->value + new_y - old_y);
		}

		if (free_list->prev)
			free_list->prev->next = NULL;
		else
			dtree->auto_expanded_nodes = NULL;

		free_list->prev = NULL;
		g_list_free (free_list);
	}
}

/**
 * panel_tree_scrolling_is_desirable:
 *
 * If the cursor is in a position close to either edge (top or bottom)
 * and there is possible to scroll the window, this routine returns
 * true.
 */
static gboolean
panel_tree_scrolling_is_desirable (WPanel *panel, int x, int y)
{
	GtkDTree *dtree = GTK_DTREE (panel->tree);
	GtkAdjustment *va;

	va = scrolled_window_get_vadjustment (panel->tree_scrolled_window);

	if (y < 10) {
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (GTK_WIDGET (dtree)->allocation.height - 10)){
			if (va->value < va->upper - va->page_size)
				return TRUE;
		}
	}

	return FALSE;
}

/* Timer callback to scroll the tree */
static gboolean
panel_tree_scroll (gpointer data)
{
	WPanel *panel = data;
	GtkAdjustment *va;
	double v;

	va = scrolled_window_get_vadjustment (panel->tree_scrolled_window);

	if (panel->drag_motion_y < 10) {
		v = va->value - va->step_increment;
		if (v < va->lower)
			v = va->lower;

		gtk_adjustment_set_value (va, v);
	} else {
		v = va->value + va->step_increment;
		if (v > va->upper - va->page_size)
			v = va->upper - va->page_size;

		gtk_adjustment_set_value (va, v);
	}

	return TRUE;
}

/* Callback for the drag_motion signal of the tree.  We set up a timer to
 * automatically open nodes if the user hovers over them.
 */
static gboolean
panel_tree_drag_motion (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time,
			gpointer data)
{
	GtkDTree *dtree;
	WPanel *panel;
	int on_row, row, col;
	GtkCTreeNode *node;
	GdkDragAction action;
	GtkWidget *source_widget;
	char *row_path;
	int on_drag_row;

	dtree = GTK_DTREE (widget);
	panel = data;

	panel->drag_tree_dragging_over = TRUE;
	panel_setup_drag_scroll (panel, x, y,
				 panel_tree_scrolling_is_desirable,
				 panel_tree_scroll);

	on_row = gtk_clist_get_selection_info (GTK_CLIST (widget), x, y, &row, &col);

	/* Remove the auto-expansion timeout if we are on the blank area of the
	 * tree or on a row different from the previous one.
	 */
	if ((!on_row || row != panel->drag_tree_row) && panel->drag_tree_timeout_id != 0) {
		gtk_timeout_remove (panel->drag_tree_timeout_id);
		panel->drag_tree_timeout_id = 0;

		if (panel->drag_tree_fe) {
			file_entry_free (panel->drag_tree_fe);
			panel->drag_tree_fe = NULL;
		}
	}

	action = 0;

	if (on_row) {
		node = gtk_ctree_node_nth (GTK_CTREE (dtree), row);
		row_path = gtk_dtree_get_row_path (dtree, node);

		if (row != panel->drag_tree_row) {
			/* Highlight the row by selecting it */

			panel_tree_check_auto_expand (panel, node);

			dtree->internal = TRUE;
			gtk_clist_select_row (GTK_CLIST (widget), row, 0);
			dtree->internal = FALSE;

			/* Create the file entry to validate drops */

			panel->drag_tree_fe = file_entry_from_file (row_path);

			/* Install the timeout handler for auto-expansion */

			panel->drag_tree_timeout_id = gtk_timeout_add (TREE_OPEN_TIMEOUT,
								       tree_drag_open_directory,
								       panel);
			panel->drag_tree_row = row;
		}

		/* Validate the action */

		gdnd_find_panel_by_drag_context (context, &source_widget);

		/* If this tree is the drag source, see if the user is trying to
		 * drop on the row being dragged.  Otherwise, consider all rows
		 * as not selected.
		 */
		if (source_widget == widget) {
			g_assert (dtree->drag_dir != NULL);
			on_drag_row = strcmp (row_path, dtree->drag_dir) == 0;
		} else
			on_drag_row = FALSE;

		action = gdnd_validate_action (context,
					       FALSE,
					       source_widget != NULL,
					       source_widget == widget,
					       row_path,
					       panel->drag_tree_fe,
					       on_drag_row);

		g_free (row_path);
	} else {
		panel->drag_tree_row = -1;
		panel_tree_check_auto_expand (panel, NULL);
	}

	gdk_drag_status (context, action, time);
	return TRUE;
}

/* Callback for the drag_leave signal of the tree.  We deactivate the timeout for
 * automatic branch expansion.
 */
static void
panel_tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	WPanel *panel;

	panel = data;

	panel->drag_tree_dragging_over = FALSE;
	panel_cancel_drag_scroll (panel);

	if (panel->drag_tree_timeout_id != 0) {
		gtk_timeout_remove (panel->drag_tree_timeout_id);
		panel->drag_tree_timeout_id = 0;
	}

	if (panel->drag_tree_fe) {
		file_entry_free (panel->drag_tree_fe);
		panel->drag_tree_fe = NULL;
	}

	if (panel->drag_tree_row != -1)
		panel_tree_check_auto_expand (panel, NULL);

	panel->drag_tree_row = -1;
}

#if CONTEXT_MENU_ON_TREE
static void
tree_do_op (GtkWidget *tree, WPanel *panel, int operation)
{
}

static void
tree_copy_cmd (GtkWidget *tree, WPanel *panel)
{
	tree_do_op (tree, panel, OP_COPY);
}

static void
tree_del_cmd (GtkWidget *tree, WPanel *panel)
{
	tree_do_op (tree, panel, OP_DELETE);
}

static void
tree_ren_cmd (GtkWidget *tree, WPanel *panel)
{
	tree_do_op (tree, panel, OP_MOVE);
}

static GnomeUIInfo tree_popup_items[] = {
	GNOMEUIINFO_ITEM_STOCK(N_("_Copy..."), N_("Copy directory"), tree_copy_cmd, GNOME_STOCK_PIXMAP_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("_Delete..."), N_("Delete directory"), tree_del_cmd, GNOME_STOCK_PIXMAP_TRASH),
        GNOMEUIINFO_ITEM_NONE(N_("_Move..."), N_("Rename or move directory"), tree_ren_cmd),

	GNOMEUIINFO_END
};

static void
panel_tree_button_press (GtkWidget *widget, GdkEventButton *event, WPanel *panel)
{
	GtkWidget *popup;

	if (event->type != GDK_BUTTON_PRESS)
		return;

	if (event->button != 3)
		return;

	popup = gnome_popup_menu_new (tree_popup_items);
	gnome_popup_menu_do_popup_modal (popup, NULL, NULL, event, panel);
	gtk_widget_destroy (popup);
}
#endif

/**
 * panel_create_tree_view:
 *
 * Create and initializes the GtkDTree widget for being used in the
 * Panel
 */
static GtkWidget *
panel_create_tree_view (WPanel *panel)
{
	GtkWidget *tree;

	tree = gtk_dtree_new ();
	gtk_ctree_set_line_style (GTK_CTREE (tree), GTK_CTREE_LINES_DOTTED);
        gtk_ctree_set_expander_style (GTK_CTREE (tree), GTK_CTREE_EXPANDER_SQUARE);
        gtk_ctree_set_indent (GTK_CTREE (tree), 10);

	/* DTree signals */

	gtk_signal_connect (GTK_OBJECT (tree), "directory_changed",
			    GTK_SIGNAL_FUNC (panel_chdir), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "scan_begin",
			    GTK_SIGNAL_FUNC (panel_tree_scan_begin), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "scan_end",
			    GTK_SIGNAL_FUNC (panel_tree_scan_end), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "possibly_ungrab",
			    GTK_SIGNAL_FUNC (panel_tree_possibly_ungrab), panel);

	/* Set up drag source */

	gtk_drag_source_set (GTK_WIDGET (tree), GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			     drag_types, ELEMENTS (drag_types),
			     (GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY
			      | GDK_ACTION_ASK | GDK_ACTION_DEFAULT));

	gtk_signal_connect (GTK_OBJECT (tree), "drag_begin",
			    GTK_SIGNAL_FUNC (panel_tree_drag_begin), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_end",
			    GTK_SIGNAL_FUNC (panel_tree_drag_end), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_data_get",
			    GTK_SIGNAL_FUNC (panel_tree_drag_data_get), panel);

	/* Set up drag destination */

	gtk_drag_dest_set (GTK_WIDGET (tree),
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	gtk_signal_connect (GTK_OBJECT (tree), "drag_motion",
			    GTK_SIGNAL_FUNC (panel_tree_drag_motion), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_leave",
			    GTK_SIGNAL_FUNC (panel_tree_drag_leave), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_data_received",
			    GTK_SIGNAL_FUNC (panel_tree_drag_data_received), panel);

#ifdef CONTEXT_MENU_ON_TREE
	/* Context sensitive menu */
	gtk_signal_connect_after (GTK_OBJECT (tree), "button_press_event",
				  GTK_SIGNAL_FUNC (panel_tree_button_press), panel);
	gtk_clist_set_button_actions (GTK_CLIST (tree), 2, GTK_BUTTON_SELECTS);
#endif
	return tree;
}

/*
 * create_and_setup_pane:
 *
 * Creates the horizontal GtkPaned widget that holds the tree
 * and the listing/iconing displays
 */
static GtkWidget *
create_and_setup_pane (WPanel *panel)
{
	GtkWidget *pane;
	GtkWidget *tree = panel->tree;
	GdkFont *tree_font = tree->style->font;
	int size;

	pane = gtk_hpaned_new ();

	if (tree_panel_visible == -1)
		 size = 20 * gdk_string_width (tree_font, "W");
	else {
		if (tree_panel_visible)
			size = tree_panel_visible;
		else
			size = 0;
	}
#if 0
	gtk_paned_set_position (GTK_PANED (pane), size);
#else
	/*
	 * Hack: set the default startup size for the pane without
	 * using _set_usize which would set the minimal size
	 */
	GTK_PANED (pane)->child1_size = size;
	GTK_PANED (pane)->position_set = TRUE;
#endif
	gtk_widget_show (pane);

	return pane;
}

static void
panel_back (GtkWidget *button, WPanel *panel)
{
	directory_history_prev (panel);
}

static void
panel_fwd (GtkWidget *button, WPanel *panel)
{
	directory_history_next (panel);
}

static void
panel_up (GtkWidget *button, WPanel *panel)
{
	do_panel_cd (panel, "..", cd_exact);
}

static void
rescan_panel (GtkWidget *widget, gpointer data)
{
	reread_cmd ();
}

static void
go_home (GtkWidget *widget, WPanel *panel)
{
	do_panel_cd (panel, "~", cd_exact);
}

/* The toolbar */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Back"), N_("Go to the previously visited directory"),
				panel_back, GNOME_STOCK_PIXMAP_BACK),
	GNOMEUIINFO_ITEM_STOCK (N_("Up"), N_("Go up a level in the directory heirarchy"),
				panel_up, GNOME_STOCK_PIXMAP_UP),
	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Go to the next directory"),
				panel_fwd, GNOME_STOCK_PIXMAP_FORWARD),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Rescan"), N_("Rescan the current directory"),
				rescan_panel, GNOME_STOCK_PIXMAP_REFRESH),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Home"), N_("Go to your home directory"),
				go_home, GNOME_STOCK_PIXMAP_HOME),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_RADIOLIST(panel_view_toolbar_uiinfo),
	GNOMEUIINFO_END
};

static void
do_ui_signal_connect (GnomeUIInfo *uiinfo, gchar *signal_name,
		GnomeUIBuilderData *uibdata)
{
	if (uiinfo->moreinfo)
		gtk_signal_connect (GTK_OBJECT (uiinfo->widget),
				    signal_name, uiinfo->moreinfo, uibdata->data ?
				    uibdata->data : uiinfo->user_data);
}

static void
tree_size_allocate (GtkWidget *widget, GtkAllocation *allocation, WPanel *panel)
{
	if (allocation->width <= 0){
		tree_panel_visible = 0;
	} else {
		tree_panel_visible = allocation->width;
	}
	save_setup ();
}

void
x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel)
{
	GtkWidget *status_line, *vbox, *ministatus_box;
	GtkWidget *cwd;
	GtkWidget *dock;
	GnomeUIBuilderData uibdata;

	panel->xwindow = gtk_widget_get_toplevel (GTK_WIDGET (panel->widget.wdata));

	panel->table = gtk_table_new (2, 1, 0);

	/*
	 * Tree View
	 */
	panel->tree_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (panel->tree_scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	panel->tree = panel_create_tree_view (panel);
	gtk_container_add (GTK_CONTAINER (panel->tree_scrolled_window), panel->tree);
	gtk_widget_show_all (panel->tree_scrolled_window);

	/*
	 * Icon and Listing display
	 */
	panel->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (panel->notebook), FALSE);
	gtk_widget_show (panel->notebook);

	panel->icons = panel_create_icon_display (panel);
	gtk_widget_show (panel->icons);

	memcpy (panel->column_width, default_column_width, sizeof (default_column_width));
	panel->list  = panel_create_file_list (panel);
	gtk_widget_ref (panel->icons);
	gtk_widget_ref (panel->list);

	panel->panel_listbox = gtk_event_box_new ();
	gtk_widget_show (panel->panel_listbox);
	gtk_container_add (GTK_CONTAINER (panel->panel_listbox), panel->list);

	gtk_notebook_append_page (GTK_NOTEBOOK (panel->notebook), panel->icons, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (panel->notebook), panel->panel_listbox, NULL);
	gtk_notebook_set_page (GTK_NOTEBOOK (panel->notebook),
			       panel->list_type == list_icons ? 0 : 1);

	gtk_widget_show (panel->icons);
	gtk_widget_show (panel->list);
	gtk_widget_show (panel->notebook);

	/*
	 * Pane
	 */
	panel->pane = create_and_setup_pane (panel);
	gtk_paned_add1 (GTK_PANED (panel->pane), panel->tree_scrolled_window);
	gtk_signal_connect (GTK_OBJECT (panel->tree_scrolled_window), "size_allocate",
			    GTK_SIGNAL_FUNC (tree_size_allocate), panel);

	/*
	 * Current Working directory
	 */
	
	cwd = panel_create_cwd (h, panel, &panel->current_dir);

	/*
	 * We go through a lot of pain, wrestling with gnome_app* and gmc's @#$&*#$ internal structure and
	 * make the #@$*&@#$ toolbars here...
	 */

	status_line = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	uibdata.connect_func = do_ui_signal_connect;
	uibdata.data = panel;
	uibdata.is_interp = FALSE;
	uibdata.relay_func = NULL;
	uibdata.destroy_func = NULL;

	gnome_app_fill_toolbar_custom (GTK_TOOLBAR (status_line), toolbar,  &uibdata, NULL);
	gnome_app_add_toolbar (GNOME_APP (panel->xwindow),
			       GTK_TOOLBAR (status_line),
			       "gmc-toolbar0",
			       GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
			       GNOME_DOCK_TOP,
			       2, 0, 0);
	panel->view_toolbar_items = copy_uiinfo_widgets (panel_view_toolbar_uiinfo);

	panel->back_b   = toolbar[0].widget;
	panel->up_b     = toolbar[1].widget;
	panel->fwd_b    = toolbar[2].widget;
	panel_update_marks (panel);

	/* Set the list type by poking a toolbar item.  Yes, this is hackish.
	 * We fall back to icon view if a certain listing type is not supported.
	 * Be sure to keep this in sync with the uiinfo arrays in glayout.c.
	 */

	if (panel->list_type == list_brief)
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (panel_view_toolbar_uiinfo[1].widget), TRUE);
	else if (panel->list_type == list_full)
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (panel_view_toolbar_uiinfo[2].widget), TRUE);
	else if (panel->list_type == list_user)
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (panel_view_toolbar_uiinfo[3].widget), TRUE);
	else
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (panel_view_toolbar_uiinfo[0].widget), TRUE);

	status_line = gtk_hbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (status_line), 3);
	gtk_box_pack_start (GTK_BOX (status_line),
                            gtk_label_new (_("Location:")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (status_line), cwd, TRUE, TRUE, 0);

	dock =  gnome_dock_item_new ("gmc-toolbar1",
                                     (GNOME_DOCK_ITEM_BEH_EXCLUSIVE
                                      | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL));
	gtk_container_add (GTK_CONTAINER (dock), status_line);
	gnome_dock_add_item (GNOME_DOCK (GNOME_APP (panel->xwindow)->dock),
			     GNOME_DOCK_ITEM (dock), GNOME_DOCK_TOP, 1, 0, 0, FALSE);

	gtk_widget_show_all (dock);

	panel->view_table = gtk_table_new (1, 1, 0);
	gtk_widget_show (panel->view_table);

	/*
	 * The status bar.
	 */
	ministatus_box = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (ministatus_box), GTK_SHADOW_IN);

	panel->status = gtk_label_new (_("Show all files"));
	gtk_misc_set_alignment (GTK_MISC (panel->status), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (panel->status), 2, 0);

	gtk_box_pack_start (GTK_BOX (panel->ministatus), ministatus_box, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (ministatus_box), panel->status);

	gtk_widget_show (ministatus_box);
	gtk_widget_show (panel->status);

	/*
	 * Put the icon list and the file listing in a nice frame
	 */

	/* Add both the icon view and the listing view */
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->notebook, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);

	gtk_table_attach (GTK_TABLE (panel->table), panel->pane, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);

	gtk_paned_add2 (GTK_PANED (panel->pane), panel->view_table);

	/*
	 * ministatus_box is a container created just to put the
	 * panel->ministatus inside.
	 *
	 * Then the resize mode for ministatus_box is changed to stop
	 * any size-changed messages to be propagated above.
	 *
	 * This is required as the panel->ministatus Label is changed
	 * very often (to display status information).  If this hack
	 * is not made, then the resize is queued and the whole window
	 * flickers each time this changes
	 */
#if 0
	ministatus_box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (ministatus_box), panel->ministatus);
	gtk_widget_show (ministatus_box);
	gtk_container_set_resize_mode (GTK_CONTAINER (ministatus_box), GTK_RESIZE_QUEUE);
	gtk_table_attach (GTK_TABLE (panel->table), ministatus_box, 0, 1, 2, 3,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0, 0);

	gtk_table_attach (GTK_TABLE (panel->table), frame, 0, 1, 3, 4,
			  GTK_EXPAND | GTK_FILL,
			  0, 0, 0);

#endif
	/* Ultra nasty hack: pull the vbox from wdata */
	vbox =  GTK_WIDGET (panel->widget.wdata);

	panel->widget.wdata = (widget_data) panel->table;

	/* Now, insert our table in our parent */
	gtk_container_add (GTK_CONTAINER (vbox), panel->table);
	gtk_widget_show (vbox);
	gtk_widget_show (panel->table);

	if (!(panel->widget.options & W_PANEL_HIDDEN))
		gtk_widget_show (gtk_widget_get_toplevel (panel->table));

	if (!pixmaps_ready)
		panel_create_pixmaps ();

	/* In GNOME the panel wants to have the cursor, to avoid "auto" focusing the
	 * filter input line
	 */
	panel->widget.options |= W_WANT_CURSOR;
	panel->estimated_total = 0;

	panel->timer_id = -1;

	/* re-set the user_format explicitly */
	if (default_user_format != NULL) {
		g_free (panel->user_format);
		panel->user_format = g_strdup (default_user_format);
	}
}

void
panel_update_cols (Widget *panel, int frame_size)
{
	panel->cols  = 60;
	panel->lines = 20;
}

char *
get_nth_panel_name (int num)
{
    static char buffer [BUF_TINY];

    if (!num)
        return "New Left Panel";
    else if (num == 1)
        return "New Right Panel";
    else {
        g_snprintf (buffer, sizeof (buffer), "%ith Panel", num);
        return buffer;
    }
}

void
load_hint (void)
{
	char *hint;

	if ((hint = get_random_hint ())){
		if (*hint)
			set_hintbar (hint);
		g_free (hint);
	} else
		set_hintbar ("The GNOME File Manager " VERSION);

}

void
paint_frame (WPanel *panel)
{
}

void
x_reset_sort_labels (WPanel *panel)
{
	int page;

	if (!panel->notebook)
		return;

	if (panel->list_type == list_icons){
		page = 0;
	} else {
		page = 1;
		panel_switch_new_display_mode (panel);
	}
	gtk_notebook_set_page (GTK_NOTEBOOK (panel->notebook), page);
}

/* Releases all of the X resources allocated */
void
x_panel_destroy (WPanel *panel)
{
	gtk_widget_destroy (GTK_WIDGET (panel->xwindow));
}
