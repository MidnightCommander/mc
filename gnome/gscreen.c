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
#include <stdlib.h>		/* atoi */
#include "fs.h"
#include "mad.h"
#include "x.h"
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
#include "gdesktop.h"
#include "gdnd.h"
#include "gtkdtree.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "gcliplabel.h"
#include "gblist.h"
#include "gicon.h"
#include "../vfs/vfs.h"
#include <gdk/gdkprivate.h>

/* The pixmaps */
#include "directory.xpm"
#include "link.xpm"
#include "dev.xpm"
#include "listing-list.xpm"
#include "listing-iconic.xpm"

/* This is used to initialize our pixmaps */
static int pixmaps_ready;
GdkPixmap *icon_directory_pixmap;
GdkBitmap *icon_directory_mask;
GdkPixmap *icon_link_pixmap;
GdkBitmap *icon_link_mask;
GdkPixmap *icon_dev_pixmap;
GdkBitmap *icon_dev_mask;
static GtkTargetEntry drag_types [] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/plain",    0, TARGET_TEXT_PLAIN },
	{ "_NETSCAPE_URL", 0, TARGET_URL }
};

static GtkTargetEntry drop_types [] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

/* GtkWidgets with the shaped windows for dragging */
GtkWidget *drag_directory    = NULL;
GtkWidget *drag_directory_ok = NULL;
GtkWidget *drag_multiple     = NULL;
GtkWidget *drag_multiple_ok  = NULL;


static void panel_file_list_configure_contents (GtkWidget *sw, WPanel *panel, int main_width, int height);

#define CLIST_FROM_SW(panel_list) GTK_CLIST (GTK_BIN (panel_list)->child)


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
	char  **texts;

	texts = malloc (sizeof (char *) * items);

	gtk_clist_freeze (GTK_CLIST (cl));
	gtk_clist_clear (GTK_CLIST (cl));

	/* which column holds the type information */
	type_col = -1;

	for (i = 0; i < top; i++){
		file_entry *fe = &panel->dir.list [i];
		format_e *format = panel->format;

		for (col = 0; format; format = format->next){
			if (!format->use_in_gui)
			    continue;

			if (type_col == -1)
				if (strcmp (format->id, "type") == 0)
					type_col = col;

			if (!format->string_fn)
				texts [col] = "";
			else
				texts [col] = (*format->string_fn)(fe, 10);
			col++;
		}
		gtk_clist_append (cl, texts);

		color = file_compute_color (fe->f.marked ? MARKED : NORMAL, fe);
		panel_file_list_set_row_colors (cl, i, color);
		if (type_col != -1)
			panel_file_list_set_type_bitmap (cl, i, type_col, color, fe);
	}
	/* This is needed as the gtk_clist_append changes selected under us :-( */
	panel->selected = selected;
	select_item (panel);
	gtk_clist_thaw (GTK_CLIST (cl));
	free (texts);
}

/*
 * Icon view: load the panel contents
 */
static void
panel_fill_panel_icons (WPanel *panel)
{
	GnomeIconList *icons = GNOME_ICON_LIST (panel->icons);
	const int top       = panel->count;
	const int selected  = panel->selected;
	int i;
	GdkImlibImage *image;

	gnome_icon_list_freeze (icons);
	gnome_icon_list_clear (icons);

	for (i = 0; i < top; i++){
		file_entry *fe = &panel->dir.list [i];

		image = gicon_get_icon_for_file (fe);
		gnome_icon_list_append_imlib (icons, image, fe->fname);
	}
	/* This is needed as the gtk_clist_append changes selected under us :-( */
	panel->selected = selected;
	gnome_icon_list_thaw (icons);
	select_item (panel);
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

	color = file_compute_color (value ? MARKED : NORMAL, &panel->dir.list[index]);
	panel_file_list_set_row_colors (CLIST_FROM_SW (panel->list), index, color);
}

void
x_select_item (WPanel *panel)
{
	if (panel->list_type == list_icons){
		GnomeIconList *list = GNOME_ICON_LIST (panel->icons);

		do_file_mark (panel, panel->selected, 1);
		display_mini_info (panel);
		gnome_icon_list_select_icon (list, panel->selected);

		if (list->icon_list){
			if (GTK_WIDGET (list)->allocation.x != -1)
				if (gnome_icon_list_icon_is_visible (list, panel->selected) != GTK_VISIBILITY_FULL)
					gnome_icon_list_moveto (list, panel->selected, 0.5);
		}
		gnome_canvas_update_now (GNOME_CANVAS (list));
	} else {
		GtkCList *clist = CLIST_FROM_SW (panel->list);
		int color, marked;

		if (panel->dir.list [panel->selected].f.marked)
			marked = 1;
		else
			marked = 0;

		color = file_compute_color (marked ? MARKED_SELECTED : SELECTED, &panel->dir.list [panel->selected]);
		panel_file_list_set_row_colors (CLIST_FROM_SW (panel->list), panel->selected, color);

		/* Make it visible */
		if (gtk_clist_row_is_visible (clist, panel->selected) != GTK_VISIBILITY_FULL)
			gtk_clist_moveto (clist, panel->selected, 0, 0.5, 0.0);
	}
}

void
x_unselect_item (WPanel *panel)
{
	if (panel->list_type == list_icons){
	        int selected = panel->selected;

		/* This changes the panel->selected */
		gnome_icon_list_unselect_all (GNOME_ICON_LIST (panel->icons), NULL, NULL);
		panel->selected = selected;
	} else {
		int color;
		int val;

		val = panel->dir.list [panel->selected].f.marked ? MARKED : NORMAL;
		color = file_compute_color (val, &panel->dir.list [panel->selected]);
		panel_file_list_set_row_colors (CLIST_FROM_SW (panel->list), panel->selected, color);
	}
}

void
x_filter_changed (WPanel *panel)
{
	assign_text (panel->filter_w, panel->filter ? panel->filter : "");
	update_input (panel->filter_w, 1);
}

void
x_adjust_top_file (WPanel *panel)
{
/*	gtk_clist_moveto (GTK_CLIST (panel->list), panel->top_file, 0, 0.0, 0.0); */
}

/*
 * These two constants taken from Gtk sources, hack to figure out how much
 * of the clist is visible
 */
#define COLUMN_INSET 3
#define CELL_SPACING 1

/*
 * Configures the columns title sizes for the panel->list CList widget
 */
static void
panel_file_list_configure_contents (GtkWidget *sw, WPanel *panel, int main_width, int height)
{
	GtkCList *clist;
	format_e *format = panel->format;
	int i, used_columns, expandables, items;
	int char_width, usable_pixels, extra_pixels, width;
	int total_columns, extra_columns;
	int expand_space, extra_space, shrink_space;
	int lost_pixels, display_the_mini_info;

	/* Pass 1: Count minimum columns,
	 * set field_len to default to the requested_field_len
	 * and compute how much space we lost to the column decorations
	 */
	lost_pixels = used_columns = expandables = items = 0;
	for (format = panel->format; format; format = format->next) {
		format->field_len = format->requested_field_len;
		if (!format->use_in_gui)
			continue;

		used_columns += format->field_len;
		items++;
		if (format->expand)
			expandables++;
		lost_pixels += CELL_SPACING + (2 * COLUMN_INSET);
	}

	/* The left scrollbar might take some space from us, use this information */
	if (GTK_WIDGET_VISIBLE (GTK_SCROLLED_WINDOW (sw)->vscrollbar)) {
		int scrollbar_width = GTK_WIDGET (GTK_SCROLLED_WINDOW (sw)->vscrollbar)->requisition.width;
		int scrollbar_space = GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT (sw)->klass)->scrollbar_spacing;

		lost_pixels += scrollbar_space + scrollbar_width;
	}

	char_width = gdk_string_width (sw->style->font, "xW") / 2;
	width = main_width - lost_pixels;

	extra_pixels  = width % char_width;
	usable_pixels = width - extra_pixels;
	total_columns = usable_pixels / char_width;
	extra_columns = total_columns - used_columns;
	if (extra_columns > 0) {
		expand_space  = extra_columns / expandables;
		extra_space   = extra_columns % expandables;
	} else
		extra_space = expand_space = 0;

	/*
	 * Hack: the default mini-info display only gets displayed
	 * if panel->estimated_total is not zero, ie, if this has been
	 * initialized for the first time.
	 */

	display_the_mini_info = (panel->estimated_total == 0);
	panel->estimated_total = total_columns;

	if (display_the_mini_info)
		display_mini_info (panel);

	/* If we dont have enough space, shorten the fields */
	if (used_columns > total_columns) {
		expand_space = 0;
		shrink_space = (used_columns - total_columns) / items;
	} else
		shrink_space = 0;

	clist = CLIST_FROM_SW (sw);

	gtk_clist_freeze (clist);

	for (i = 0, format = panel->format; format; format = format->next) {
		if (!format->use_in_gui)
			continue;

		format->field_len += (format->expand ? expand_space : 0) - shrink_space;
		gtk_clist_set_column_width (clist, i, format->field_len * char_width);
		i++;
	}

	gtk_clist_thaw (clist);
}

static void
internal_select_item (GtkWidget *file_list, WPanel *panel, int row)
{
	unselect_item (panel);
	panel->selected = row;

	select_item (panel);
}

static void
panel_file_list_select_row (GtkWidget *file_list, int row, int column, GdkEvent *event, WPanel *panel)
{
	int current_selection = panel->selected;

	if (!event) {
		internal_select_item (file_list, panel, row);
		return;
	}

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		gtk_clist_unselect_row (CLIST_FROM_SW (panel->list), row, 0);
		internal_select_item (file_list, panel, row);

		switch (event->button.button) {
		case 1:
			if (!(event->button.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				break;

			/* fallback if shift-click is pressed */
			do_file_mark_range (panel, row, current_selection);
			break;

		case 2:
			do_file_mark (panel, row, !panel->dir.list[row].f.marked);
			break;

		case 3:
			gpopup_do_popup ((GdkEventButton *) event, panel, NULL, row, panel->dir.list[row].fname);
			break;
		}

		break;

	case GDK_2BUTTON_PRESS:
		gtk_clist_unselect_row (CLIST_FROM_SW (panel->list), row, 0);
		if (event->button.button == 1)
			do_enter (panel);
		break;

	default:
		break;
	}
}

/* Figure out the number of visible lines in the panel */
static void
panel_file_list_compute_lines (GtkScrolledWindow *sw, WPanel *panel, int height)
{
	int lost_pixels = 0;
	if (GTK_WIDGET_VISIBLE (sw->hscrollbar)) {
		int scrollbar_width = GTK_WIDGET (sw->hscrollbar)->requisition.width;
		int scrollbar_space = GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT (sw)->klass)->scrollbar_spacing;

		lost_pixels = scrollbar_space + scrollbar_width;
	}
	panel->widget.lines = (height-lost_pixels) / (CLIST_FROM_SW (sw)->row_height + CELL_SPACING);
}

static void
panel_file_list_size_allocate_hook (GtkWidget *sw, GtkAllocation *allocation, WPanel *panel)
{
	gtk_signal_handler_block_by_data (GTK_OBJECT (sw), panel);
	panel_file_list_configure_contents (sw, panel, allocation->width, allocation->height);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (sw), panel);

	panel_file_list_compute_lines (GTK_SCROLLED_WINDOW (sw), panel, allocation->height);
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

	create_pixmap (directory_xpm, &icon_directory_pixmap, &icon_directory_mask);
	create_pixmap (link_xpm, &icon_link_pixmap, &icon_link_mask);
	create_pixmap (dev_xpm, &icon_dev_pixmap, &icon_dev_mask);
}

typedef gboolean (*desirable_fn)(WPanel *p, int x, int y);
typedef gboolean (*scroll_fn)(gpointer data);

static gboolean
panel_setup_drag_motion (WPanel *panel, int x, int y, desirable_fn desirable, scroll_fn scroll)
{
	if (panel->timer_id != -1){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}

	panel->drag_motion_x = x;
	panel->drag_motion_y = y;

	if ((*desirable)(panel, x, y)){
		panel->timer_id = gtk_timeout_add (60, scroll, panel);
		return TRUE;
	}

	return FALSE;
}

static void
panel_file_list_scrolled (GtkAdjustment *adj, WPanel *panel)
{
	if (!GTK_IS_ADJUSTMENT (adj)) {
		fprintf (stderr, "file_list_is_scrolled is called and there are not enough boats!\n");
		exit (1);
	}
}

static void
panel_configure_file_list (WPanel *panel, GtkWidget *sw, GtkWidget *file_list)
{
	format_e *format = panel->format;
	GtkObject *adjustment;
	int i;

	/* Set sorting callback */
	gtk_signal_connect (GTK_OBJECT (file_list), "click_column",
			    GTK_SIGNAL_FUNC (panel_file_list_column_callback), panel);

	/* Configure the CList */

	gtk_clist_set_selection_mode (GTK_CLIST (file_list), GTK_SELECTION_SINGLE);

	for (i = 0, format = panel->format; format; format = format->next) {
		GtkJustification just;

		if (!format->use_in_gui)
			continue;

		/* Set desired justification */
		if (format->just_mode == J_LEFT)
			just = GTK_JUSTIFY_LEFT;
		else
			just = GTK_JUSTIFY_RIGHT;

		gtk_clist_set_column_justification (GTK_CLIST (file_list), i, just);
		i++;
	}

	/* Configure the scrolbars */
	adjustment = GTK_OBJECT (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw)));
	gtk_signal_connect_after (adjustment, "value_changed",
				  GTK_SIGNAL_FUNC (panel_file_list_scrolled), panel);
}

/*
 * Creates an uri list to be transfered during a drop operation.
 */
static void *
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
				total_len += (filelen + cwdlen + panel->dir.list [i].fnamelen + seplen);

		total_len++;
		data = copy = xmalloc (total_len+1, "build_selected_file_list");
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

		uri = copy_strings ("file:", fullname, NULL);
		free (fullname);

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
	GnomeIconList *gil = GNOME_ICON_LIST (widget);
	char *dir;
	int idx;

	idx = gnome_icon_list_get_icon_at (gil, x, y);
	if (idx == -1)
		dir = g_strdup (panel->cwd);
	else {
		if (panel->dir.list [idx].f.link_to_dir ||
		    S_ISDIR (panel->dir.list [idx].buf.st_mode))
			dir = concat_dir_and_file (panel->cwd, panel->dir.list [idx].fname);
		else
			dir = g_strdup (panel->cwd);
	}

	gdnd_drop_on_directory (context, selection_data, dir);
	free (dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
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
	GtkCList *clist = GTK_CLIST (widget);
	char *dir;
	int row;

	if (gtk_clist_get_selection_info (clist, x, y, &row, NULL) == 0)
		dir = g_strdup (panel->cwd);
	else {
		g_assert (row < panel->count);

		if (S_ISDIR (panel->dir.list [row].buf.st_mode) ||
		    panel->dir.list [row].f.link_to_dir)
			dir = concat_dir_and_file (panel->cwd, panel->dir.list [row].fname);
		else
			dir = g_strdup (panel->cwd);
	}

	gdnd_drop_on_directory (context, selection_data, dir);
	free (dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
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
	char *path;

	if (!gtk_clist_get_selection_info (GTK_CLIST (dtree), x, y, &row, &col))
		return;

	node = gtk_ctree_node_nth (GTK_CTREE (dtree), row);
	if (!node)
		return;
	gtk_ctree_expand_recursive (GTK_CTREE (dtree), node);
	path = gtk_dtree_get_row_path (dtree, node, 0);

	gdnd_drop_on_directory (context, selection_data, path);

	g_free (path);
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

static int
panel_clist_button_press (GtkWidget *widget, GdkEventButton *event, WPanel *panel)
{
	panel->maybe_start_drag = event->button;

	panel->click_x = event->x;
	panel->click_y = event->y;

	return FALSE;
}

static int
panel_clist_button_release (GtkWidget *widget, GdkEventButton *event, WPanel *panel)
{
	panel->maybe_start_drag = 0;
	return FALSE;
}

static int
panel_widget_motion (GtkWidget *widget, GdkEventMotion *event, WPanel *panel)
{
	GtkTargetList *list;
	GdkDragContext *context;
	int action;
		
	if (!panel->maybe_start_drag)
		return FALSE;

	if ((abs (event->x - panel->click_x) < 4) ||
	    (abs (event->y - panel->click_y) < 4))
		return FALSE;

	list = gtk_target_list_new (drag_types, ELEMENTS (drag_types));

	if (panel->maybe_start_drag == 2)
		action = GDK_ACTION_ASK;
	else
		action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
	
	context = gtk_drag_begin (widget, list, action,
				  panel->maybe_start_drag, (GdkEvent *) event);
	gtk_drag_set_icon_default (context);

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

	va = scrolled_window_get_vadjustment (panel->list);

	if (y < 10){
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (GTK_WIDGET (panel->list)->allocation.height-20)){
			if (va->value < va->upper)
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

	va = scrolled_window_get_vadjustment (panel->list);

	if (panel->drag_motion_y < 10)
		gtk_adjustment_set_value (va, va->value - va->step_increment);
	else{
		gtk_adjustment_set_value (va, va->value + va->step_increment);
	}
	return TRUE;
}

/**
 * panel_clist_drag_motion:
 *
 * Invoked when an application dragging over us has the the cursor moved.
 * If we are close to the top or bottom, we scroll the window
 */
static gboolean
panel_clist_drag_motion (GtkWidget *widget, GdkDragContext *ctx, int x, int y, guint time, void *data)
{
	WPanel *panel = data;
	
	panel_setup_drag_motion (panel, x, y, panel_clist_scrolling_is_desirable, panel_clist_scroll);
	return TRUE;
}

/**
 * panel_clist_drag_leave
 *
 * Invoked when the dragged object has left our region
 */
static void
panel_clist_drag_leave (GtkWidget *widget, GdkDragContext *ctx, guint time, void *data)
{
	WPanel *panel = data;

	if (panel->timer_id != -1){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}
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
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	file_list = gtk_blist_new_with_titles (items, titles);
	gtk_container_add (GTK_CONTAINER (sw), file_list);
	gtk_widget_show (file_list);

	panel_configure_file_list (panel, sw, file_list);
	g_free (titles);

	gtk_signal_connect_after (GTK_OBJECT (sw), "size_allocate",
				  GTK_SIGNAL_FUNC (panel_file_list_size_allocate_hook),
				  panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "select_row",
			    GTK_SIGNAL_FUNC (panel_file_list_select_row),
			    panel);

	/* Set up drag and drop */

	load_dnd_icons ();

	gtk_drag_dest_set (GTK_WIDGET (file_list), GTK_DEST_DEFAULT_ALL,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

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
	
	/* These implement our drag-start activation code.  We need to
	 * manually activate the drag as the DnD code in Gtk+ will
	 * make the scrollbars in the CList activate drags when they
	 * are moved.
	 */
	gtk_signal_connect (GTK_OBJECT (file_list), "button_press_event",
			    GTK_SIGNAL_FUNC (panel_clist_button_press), panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "button_release_event",
			    GTK_SIGNAL_FUNC (panel_clist_button_release), panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "motion_notify_event",
			    GTK_SIGNAL_FUNC (panel_widget_motion), panel);
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

	if (event)
		printf ("Icon list select icon, event->type= %d %d %d\n", event->type, GDK_BUTTON_PRESS, GDK_2BUTTON_PRESS);
	if (!event)
		return;

	switch (event->type){
	case GDK_BUTTON_PRESS:
		if (event->button.button == 2){
			char *fullname;

			if (S_ISDIR (panel->dir.list [index].buf.st_mode) ||
			    panel->dir.list [index].f.link_to_dir){
				fullname = concat_dir_and_file (panel->cwd, panel->dir.list [index].fname);
				new_panel_at (fullname);
				free (fullname);
			}
			break;
		} 
		if (event->button.button == 3)
			gpopup_do_popup ((GdkEventButton *) event, panel, NULL, index, panel->dir.list[index].fname);
		break;

	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1)
			do_enter (panel);
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
panel_icon_renamed (GtkWidget *widget, int index, char *dest, WPanel *panel)
{
	char *source;

	source = panel->dir.list [index].fname;
	if (mc_rename (source, dest) == 0){
		free (panel->dir.list [index].fname);
		panel->dir.list [index].fname = strdup (dest);
		return TRUE;
	} else
		return FALSE;
}

static void
load_imlib_icons (void)
{
	static int loaded;

	if (loaded)
		return;

	loaded = 1;
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

	icon = gnome_icon_list_get_icon_at (gil, event->x, event->y);

	if (icon == -1)
		panel->maybe_start_drag = 0;
	else {
		if (event->button != 3)
			panel->maybe_start_drag = event->button;
	}

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
	GnomeIconList *ilist;
	GtkStyle *style;
			       
	ilist = GNOME_ICON_LIST (gnome_icon_list_new (90, NULL, TRUE));
	/* Set the background of the icon list to white */
	style = gtk_style_copy (gtk_widget_get_style (GTK_WIDGET (ilist)));
	style->bg [GTK_STATE_NORMAL] = style->bg [GTK_STATE_PRELIGHT];
	gtk_widget_set_style (GTK_WIDGET (ilist), style);
		
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

	load_imlib_icons ();
	load_dnd_icons ();

	gtk_drag_dest_set (GTK_WIDGET (ilist), GTK_DEST_DEFAULT_ALL,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

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

	return GTK_WIDGET (ilist);
}

static void
panel_switch_new_display_mode (WPanel *panel)
{
	GtkWidget *old_list = panel->list;

	if (!old_list)
		return;

	panel->list = panel_create_file_list (panel);
	gtk_widget_destroy (old_list);
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->list, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (panel->list);
	panel_update_contents (panel);
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
	 * will let you expand it.
	 */
	gtk_widget_set_usize (GTK_WIDGET (in->widget.wdata), 320, -1);
	return GTK_WIDGET (in->widget.wdata);
}

/* FIXME: for now, this list is hardcoded.  We want a way to let the user configure it. */

static struct filter_item {
	char *text;
	char *glob;
} filter_items [] = {
	{ N_("All files"),
	  "*" },
	{ N_("Archives and compressed files"),
	  "*.(tar|gz|tgz|taz|zip|lha|zoo|pak|sit|arc|arj|rar|huf|lzh)" },
	{ N_("RPM/DEB files"),
	  "*.(rpm|deb)" },
	{ N_("Text/Document files"),
	  "*.(txt|tex|doc|rtf)" },
	{ N_("HTML and SGML files"),
	  "*.(html|htm|sgml|sgm)" },
	{ N_("Postscript and PDF files"),
	  "*.(ps|pdf)" },
	{ N_("Spreadsheet files"),
	  "*.(xls|wks|wk1)" },
	{ N_("Image files"),
	  "*.(png|jpg|jpeg|xcf|gif|tif|tiff|xbm|xpm|pbm|pgm|ppm|tga|rgb|iff|lbm|ilbm|"
	  "bmp|pcx|pic|pict|psd|gbr|pat|ico|fig|cgm|rle|fits)" },
	{ N_("Video/animation files"),
	  "*.(mpg|mpeg|mov|avi|fli|flc|flh|flx|dl)" },
	{ N_("Audio files"),
	  "*.(au|wav|mp3|snd|mod|s3m|ra)" },
	{ N_("C program files"),
	  "*.[ch]" },
	{ N_("C++ program files"),
	  "*.(cc|C|cpp|cxx|h|H)" },
	{ N_("Objective-C program files"),
	  "*.[mh]" },
	{ N_("Scheme program files"),
	  "*.scm" },
	{ N_("Assembler program files"),
	  "*.(s|S|asm)" },
	{ N_("Misc. program files"),
	  "*.(awk|sed|lex|l|y|sh|idl|pl|py|am|in|f|el|bas|pas|java|sl|p|m4|tcl|pov)" },
	{ N_("Font files"),
	  "*.(pfa|pfb|afm|ttf|fon|pcf|pcf.gz|spd)" }
};

static GtkWidget *filter_menu;

static void
filter_item_select (GtkWidget *widget, gpointer data)
{
	/* FIXME: the hintbar resizes horribly and screws the panel */
#if 0
	struct filter_item *fi = gtk_object_get_user_data (GTK_OBJECT (widget));

	set_hintbar (easy_patterns ? fi->glob : fi->regexp);
#endif
}

static void
filter_item_deselect (GtkWidget *widget, gpointer data)
{
/*	set_hintbar (""); */
}

static void
filter_item_activate (GtkWidget *widget, gpointer data)
{
	struct filter_item *fi = gtk_object_get_user_data (GTK_OBJECT (widget));
	WPanel *panel = data;
	char *pattern;

	if (easy_patterns)
		pattern = g_strdup (fi->glob);
	else {
		/* This is sort of a hack to force convert_pattern() to actually convert the thing */

		easy_patterns = 1;
		pattern = convert_pattern (fi->glob, match_file, 0);
		easy_patterns = 0;
	}

	set_panel_filter_to (panel, pattern);
}

static void
build_filter_menu (WPanel *panel, GtkWidget *button)
{
	GtkWidget *item;
	int i;

	if (filter_menu)
		return;

	/* FIXME: the filter menu is global, and it is never destroyed */

	filter_menu = gtk_menu_new ();

	gtk_object_set_user_data (GTK_OBJECT (filter_menu), button);

	for (i = 0; i < ELEMENTS (filter_items); i++) {
		item = gtk_menu_item_new_with_label (_(filter_items[i].text));
		gtk_object_set_user_data (GTK_OBJECT (item), &filter_items[i]);

		gtk_signal_connect (GTK_OBJECT (item), "select",
				    (GtkSignalFunc) filter_item_select,
				    panel);
		gtk_signal_connect (GTK_OBJECT (item), "deselect",
				    (GtkSignalFunc) filter_item_deselect,
				    panel);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    (GtkSignalFunc) filter_item_activate,
				    panel);

		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (filter_menu), item);
	}
}

static void
position_filter_popup (GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	int screen_width, screen_height;
	GtkWidget *wmenu = GTK_WIDGET (menu);
	GtkWidget *button = GTK_WIDGET (data);

	/* This code is mostly ripped off from gtkmenu.c - Federico */

	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	gdk_window_get_origin (button->window, x, y);

	*y += button->allocation.height;

	if ((*x + wmenu->requisition.width) > screen_width)
		*x -= (*x + wmenu->requisition.width) - screen_width;

	if ((*y + wmenu->requisition.height) > screen_height)
		*y -= (*y + wmenu->requisition.height) - screen_height;

	if (*y < 0)
		*y = 0;
}

static void
show_filter_popup (GtkWidget *button, gpointer data)
{
	WPanel *panel;

	panel = data;

	build_filter_menu (panel, button);

	gtk_menu_popup (GTK_MENU (filter_menu), NULL, NULL,
			position_filter_popup,
			button,
			1,
			GDK_CURRENT_TIME);
}

void
display_mini_info (WPanel *panel)
{
	GtkLabel *label = GTK_LABEL (panel->ministatus);

	if (panel->searching) {
		char *buf;

		buf = g_strdup_printf (_("Search: %s"), panel->search_buffer);
		gtk_label_set (label, buf);
		g_free (buf);
		return;
	}

	if (panel->marked){
		char *buf;

		buf = g_strdup_printf ((panel->marked == 1) ? _("%s bytes in %d file") : _("%s bytes in %d files"),
				       size_trunc_sep (panel->total),
				       panel->marked);
		gtk_label_set (label, buf);
		g_free (buf);
		return;
	}

	if (S_ISLNK (panel->dir.list [panel->selected].buf.st_mode)){
		char *link, link_target [MC_MAXPATHLEN];
		int  len;

		link = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
		len = mc_readlink (link, link_target, MC_MAXPATHLEN);
		free (link);

		if (len > 0){
			link_target [len] = 0;
			/* FIXME: Links should be handled differently */
			/* str = copy_strings ("-> ", link_target, NULL); */
			gtk_label_set (label, " ");
			   /*free (str); */
		} else
			gtk_label_set (label, _("<readlink failed>"));
		return;
	}

	if (panel->estimated_total > 8){
		int  len = panel->estimated_total;
		char *buffer;

		buffer = xmalloc (len + 2, "display_mini_info");
		format_file (buffer, panel, panel->selected, panel->estimated_total-2, 0, 1);
		buffer [len] = 0;
		gtk_label_set (label, buffer);
		free (buffer);
	}
	if (panel->list_type == list_icons){
		if (panel->marked == 0){
			gtk_label_set (label, " ");
		}
	}
}

static GtkWidget *
panel_create_filter (Dlg_head *h, WPanel *panel, void **filter_w)
{
	GtkWidget *fhbox;
	GtkWidget *button;
	GtkWidget *arrow;
	GtkWidget *label;
	GtkWidget *ihbox;
	WInput *in;

	fhbox = gtk_hbox_new (FALSE, 0);

	/* Filter popup button */

	button = gtk_button_new ();
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    (GtkSignalFunc) show_filter_popup,
			    panel);
	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
	gtk_box_pack_start (GTK_BOX (fhbox), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	ihbox = gtk_hbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (button), ihbox);
	gtk_widget_show (ihbox);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (ihbox), arrow, TRUE, TRUE, 0);
	gtk_widget_show (arrow);

	label = gtk_label_new (_("Filter"));
	gtk_box_pack_start (GTK_BOX (ihbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* Filter input line */

	in = input_new (0, 0, 0, 10, "", "filter");
	add_widget (h, in);

	/* Force the creation of the gtk widget */
	send_message_to (h, (Widget *) in, WIDGET_INIT, 0);
	*filter_w = in;

	gtk_box_pack_start (GTK_BOX (fhbox), GTK_WIDGET (in->widget.wdata), TRUE, TRUE, 0);

	return fhbox;
}

/* Signal handler for DTree's "directory_changed" signal */
static void
panel_chdir (GtkDTree *dtree, char *path, WPanel *panel)
{
	if (!panel->dragging)
		do_panel_cd (panel, path, cd_exact);
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
	WPanel *panel = data;
	GtkDTree *dtree = GTK_DTREE (panel->tree);
	GtkCTreeNode *node;
	int row, col;
	int r;

	r = gtk_clist_get_selection_info (
		GTK_CLIST (panel->tree),
		panel->drag_motion_x,
		panel->drag_motion_y,
		&row, &col);

	if (!r)
		return FALSE;

	node = gtk_ctree_node_nth (GTK_CTREE (panel->tree), row);
	if (!node)
		return FALSE;

	if (!GTK_CTREE_ROW (node)->expanded)
		dtree->auto_expanded_nodes = g_list_append (dtree->auto_expanded_nodes, node);
	gtk_ctree_expand (GTK_CTREE (panel->tree), node);

	return FALSE;
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

	if (y < 10){
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (GTK_WIDGET (dtree)->allocation.height-20)){
			if (va->value < va->upper)
				return TRUE;
		}
	}

	return FALSE;
}

/**
 * panel_tree_scrolling_is_desirable:
 *
 * Give a node of the tree, this panel checks to see if we've
 * auto-expanded any rows that aren't parents of this node,
 * and, if so, collapses them back.
 */
static void
panel_tree_check_auto_expand (WPanel *panel, GtkCTreeNode *current)
{
	GtkDTree *dtree = GTK_DTREE (panel->tree);
	GtkCList *clist = GTK_CLIST (dtree);
	GList *tmp_list = dtree->auto_expanded_nodes;
	GList *free_list;
	gint row, old_y, new_y;

	if (current) {
		while (tmp_list) {
			GtkCTreeNode *parent_node = tmp_list->data;
			GtkCTreeNode *tmp_node = current;

			while (tmp_node) {
				if (tmp_node == parent_node)
					break;
				tmp_node = GTK_CTREE_ROW (tmp_node)->parent;
			}
			if (!tmp_node) /* parent not found */
				break;
			
			tmp_list = tmp_list->next;
		}
	}

	/* Collapse the rows as necessary. If possible, try to do
	 * so that the "current" stays the same place on the
	 * screen
	 */
	if (tmp_list) {
		if (current) {
			row = g_list_position (clist->row_list, (GList *)current);
			old_y = row * clist->row_height - clist->vadjustment->value;
		}
  
		free_list = tmp_list;
		while (tmp_list) {
			gtk_ctree_collapse (GTK_CTREE (panel->tree), tmp_list->data);
			tmp_list = tmp_list->next;
		}

		if (current) {
			row = g_list_position (clist->row_list, (GList *)current);
			new_y = row * clist->row_height - clist->vadjustment->value;

			if (new_y != old_y) {
				gtk_adjustment_set_value (clist->vadjustment,
							  clist->vadjustment->value + new_y - old_y);
			}
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
 * panel_tree_scroll:
 *
 * Timer callback invoked to scroll the tree window
 */
static gboolean
panel_tree_scroll (gpointer data)
{
	WPanel *panel = data;
	GtkAdjustment *va;

	va = scrolled_window_get_vadjustment (panel->tree_scrolled_window);

	if (panel->drag_motion_y < 10)
		gtk_adjustment_set_value (va, va->value - va->step_increment);
	else{
		gtk_adjustment_set_value (va, va->value + va->step_increment);
	}
	return TRUE;
}

/**
 * panel_tree_drag_motion:
 *
 * This routine is invoked by GTK+ when an item is being dragged on
 * top of our widget.  We setup a timed function that will open the
 * Tree node
 */
static gboolean
panel_tree_drag_motion (GtkWidget *widget, GdkDragContext *ctx, int x, int y, guint time, void *data)
{
	WPanel *panel = data;
	int r, row, col;

	if (panel_setup_drag_motion (panel, x, y, panel_tree_scrolling_is_desirable, panel_tree_scroll))
		return TRUE;

	r = gtk_clist_get_selection_info (
		GTK_CLIST (widget), x, y, &row, &col);

	if (r) {
		GtkCTreeNode *current;
		current = gtk_ctree_node_nth (GTK_CTREE (widget), row);
		panel_tree_check_auto_expand (panel, current);
	} else
		panel_tree_check_auto_expand (panel, NULL);
		
	panel->timer_id = gtk_timeout_add (400, tree_drag_open_directory, data);
	return TRUE;
}

/**
 * panel_tree_drag_leave:
 *
 * Invoked by GTK+ when the dragging cursor has abandoned our widget.
 * We kill any pending timers.
 */
static void
panel_tree_drag_leave (GtkWidget *widget, GdkDragContext *ctx, guint time, void *data)
{
	WPanel *panel = data;

	if (panel->timer_id != -1){
		gtk_timeout_remove (panel->timer_id);
		panel->timer_id = -1;
	}
	panel_tree_check_auto_expand (panel, NULL);
}

/**
 * panel_tree_drag_begin:
 *
 * callback invoked when the drag action starts from the Tree
 */
static void
panel_tree_drag_begin (GtkWidget *widget, GdkDragContext *context, WPanel *panel)
{
	GtkDTree *dtree = GTK_DTREE (widget);

	panel->dragging = 1;
	dtree->drag_dir = g_strdup (dtree->current_path);
	printf ("This is the directory being dragged: %s\n", dtree->current_path);

}

/**
 * panel_tree_drag_end:
 *
 * callback invoked when the drag action initiated by the tree finishes.
 */
static void
panel_tree_drag_end (GtkWidget *widget, GdkDragContext *context, WPanel *panel)
{
	GtkDTree *dtree = GTK_DTREE (widget);

	panel->dragging = 0;
	g_free (dtree->current_path);
	dtree->current_path = NULL;
}

/**
 * panel_tree_drag_data_get:
 *
 * Invoked when the tree is required to provide the dragged data
 */
static void
panel_tree_drag_data_get (GtkWidget *widget, GdkDragContext *context,
			  GtkSelectionData *selection_data, guint info,
			  guint32 time)
{
	GtkDTree *dtree = GTK_DTREE (widget);
	char *data;

	switch (info){
	case TARGET_URI_LIST:
	case TARGET_TEXT_PLAIN:
		data = copy_strings ("file:", dtree->drag_dir, NULL);
		gtk_selection_data_set (
			selection_data, selection_data->target, 8,
			data, strlen (data)+1);
		break;
	}
}

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

	gtk_signal_connect (GTK_OBJECT (tree), "directory_changed",
			    GTK_SIGNAL_FUNC (panel_chdir), panel);

	gtk_drag_dest_set (GTK_WIDGET (tree), GTK_DEST_DEFAULT_ALL,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

	/*
	 * Drag and drop signals.
	 */

	/* Data has been dropped signal handler */
	gtk_signal_connect (GTK_OBJECT (tree), "drag_data_received",
			    GTK_SIGNAL_FUNC (panel_tree_drag_data_received), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_begin",
			    GTK_SIGNAL_FUNC (panel_tree_drag_begin), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_end",
			    GTK_SIGNAL_FUNC (panel_tree_drag_end), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_data_get",
			    GTK_SIGNAL_FUNC (panel_tree_drag_data_get), panel);

	/* Make directories draggable */
	gtk_drag_source_set (GTK_WIDGET (tree), GDK_BUTTON1_MASK,
			     drag_types, ELEMENTS (drag_types),
			     GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK);

	/* Mouse is being moved over ourselves */
	gtk_signal_connect (GTK_OBJECT (tree), "drag_motion",
			    GTK_SIGNAL_FUNC (panel_tree_drag_motion), panel);
	gtk_signal_connect (GTK_OBJECT (tree), "drag_leave",
			    GTK_SIGNAL_FUNC (panel_tree_drag_leave), panel);

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

	pane = gtk_hpaned_new ();

	/*
	 * Hack: set the default startup size for the pane without
	 * using _set_usize which would set the minimal size
	 */
	GTK_PANED (pane)->child1_size = 20 * gdk_string_width (tree_font, "W");
	GTK_PANED (pane)->position_set = TRUE;

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

static GtkWidget *
button_switch_to (char **icon, GtkSignalFunc fn, void *closure)
{
	GtkWidget *button, *pix;

	button = gtk_button_new ();
	pix = gnome_pixmap_new_from_xpm_d (icon);
	gtk_container_add (GTK_CONTAINER (button), pix);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", fn, closure);

	return button;
}

static void
do_switch_to_iconic (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type == list_icons)
		return;
	panel->list_type = list_icons;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}

static void
do_switch_to_listing (GtkWidget *widget, WPanel *panel)
{
	if (panel->list_type != list_icons)
		return;
	panel->list_type = list_full;
	set_panel_formats (panel);
	paint_panel (panel);
	do_refresh ();
}

static GtkWidget *
button_switch_to_icon (WPanel *panel)
{
	return button_switch_to (listing_iconic_xpm, GTK_SIGNAL_FUNC (do_switch_to_iconic), panel);
}

static GtkWidget *
button_switch_to_listing (WPanel *panel)
{
	return button_switch_to (listing_list_xpm, GTK_SIGNAL_FUNC (do_switch_to_listing), panel);
}

void
x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel)
{
	GtkWidget *status_line, *filter, *vbox, *ministatus_box;
	GtkWidget *frame, *cwd, *back_p, *fwd_p, *up_p;
	GtkWidget *hbox, *evbox, *dock, *box;
	GtkWidget *table_frame;
	int display;
	
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
	panel->scrollbar = gtk_vscrollbar_new (GNOME_ICON_LIST (panel->icons)->adj);
	gtk_widget_show (panel->scrollbar);

	panel->list  = panel_create_file_list (panel);
	gtk_widget_ref (panel->icons);
	gtk_widget_ref (panel->list);

	box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), panel->icons, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (box), panel->scrollbar, FALSE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (panel->notebook), box, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (panel->notebook), panel->list, NULL);
	gtk_notebook_set_page (GTK_NOTEBOOK (panel->notebook), panel->list_type == list_icons ? 0 : 1);
	gtk_widget_show_all (box);
	gtk_widget_show (panel->list);
	gtk_widget_show (panel->notebook);
	
	/*
	 * Pane
	 */
	panel->pane = create_and_setup_pane (panel);
	gtk_paned_add1 (GTK_PANED (panel->pane), panel->tree_scrolled_window);

	/*
	 * Filter
	 */
	filter = panel_create_filter (h, panel, &panel->filter_w);

	/*
	 * Current Working directory
	 */
	cwd = panel_create_cwd (h, panel, &panel->current_dir);

	/* We do not want the focus by default  (and the previos add_widget just gave it to us) */
	h->current = h->current->prev;

	/*
	 * History buttons, and updir.
	 */
	back_p = gnome_stock_pixmap_widget_new (panel->xwindow, GNOME_STOCK_MENU_BACK);
	fwd_p  = gnome_stock_pixmap_widget_new (panel->xwindow, GNOME_STOCK_MENU_FORWARD);
	up_p   = gnome_stock_pixmap_widget_new (panel->xwindow, GNOME_STOCK_MENU_UP);
	panel->up_b     = gtk_button_new ();
	panel->back_b   = gtk_button_new ();
	panel->fwd_b    = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (panel->back_b), back_p);
	gtk_container_add (GTK_CONTAINER (panel->fwd_b), fwd_p);
	gtk_container_add (GTK_CONTAINER (panel->up_b), up_p);
	gtk_signal_connect (GTK_OBJECT (panel->back_b), "clicked", GTK_SIGNAL_FUNC(panel_back), panel);
	gtk_signal_connect (GTK_OBJECT (panel->fwd_b), "clicked", GTK_SIGNAL_FUNC(panel_fwd), panel);
	gtk_signal_connect (GTK_OBJECT (panel->up_b), "clicked", GTK_SIGNAL_FUNC(panel_up), panel);
	panel_update_marks (panel);


	/*
	 * Here we make the toolbars
	 */
	status_line = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gtk_container_set_border_width (GTK_CONTAINER (status_line), 3);
	evbox = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (evbox), hbox);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Location:")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), cwd, FALSE, FALSE, 0);
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   evbox,
				   NULL, NULL);
	gtk_toolbar_append_space (GTK_TOOLBAR (status_line));
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   panel->back_b,
				   "Go to the previous directory.", NULL);
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   panel->up_b,
				   "Go up a level in the directory heirarchy.", NULL);
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   panel->fwd_b,
				   "Go to the next directory.", NULL);
	gtk_toolbar_append_space (GTK_TOOLBAR (status_line));
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   button_switch_to_icon (panel),
				   "Switch view to icon view.", NULL);
	gtk_toolbar_append_widget (GTK_TOOLBAR (status_line),
				   button_switch_to_listing (panel),
				   "Switch view to detailed view.", NULL);
	dock =  gnome_dock_item_new ("gmc-toolbar", GNOME_DOCK_ITEM_BEH_EXCLUSIVE | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL);
	gtk_container_add (GTK_CONTAINER(dock),status_line);
	gnome_dock_add_item (GNOME_DOCK(GNOME_APP (panel->xwindow)->dock),
			     GNOME_DOCK_ITEM (dock), GNOME_DOCK_TOP, 1, 0, 0, FALSE);
	gtk_widget_show_all (dock);


	/*
	 * ministatus
	 */
	panel->ministatus = gtk_label_new (" "); /* was a cliplabel */
	gtk_widget_set_usize (panel->ministatus, 0, -1);
	gtk_misc_set_alignment (GTK_MISC (panel->ministatus), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (panel->ministatus), 3, 0);
	gtk_widget_show (panel->ministatus);
	gtk_label_set_justify (GTK_LABEL (panel->ministatus), GTK_JUSTIFY_LEFT);

	/*
	 * The statusbar
	 * This status bar now holds the  ministatus.
	 */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

	panel->status = gtk_label_new (""); /* used to be a cliplabel */


	/* we set up the status_bar */
	gtk_misc_set_alignment (GTK_MISC (panel->status), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (panel->status), 3, 0);
	gtk_container_add (GTK_CONTAINER (frame), panel->ministatus);
	gtk_label_set_justify (GTK_LABEL (panel->status), GTK_JUSTIFY_LEFT);
	gtk_widget_show_all (frame);


	panel->view_table = gtk_table_new (1, 1, 0);
	gtk_widget_show (panel->view_table);

	/*
	 * Put the icon list and the file listing in a nice frame
	 */
	table_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (table_frame), GTK_SHADOW_IN);
	gtk_widget_show (table_frame);
	gtk_container_add (GTK_CONTAINER (table_frame), panel->view_table);

	/* Add both the icon view and the listing view */
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->notebook, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);

	gtk_table_attach (GTK_TABLE (panel->table), panel->pane, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);

	gtk_paned_add2 (GTK_PANED (panel->pane), table_frame);

#if 0
	gtk_table_attach (GTK_TABLE (panel->table), status_line, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#endif
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

#endif
	gtk_table_attach (GTK_TABLE (panel->table), frame, 0, 1, 3, 4,
			  GTK_EXPAND | GTK_FILL,
			  0, 0, 0);

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
    static char buffer [20];

    if (!num)
        return "New Left Panel";
    else if (num == 1)
        return "New Right Panel";
    else {
        sprintf (buffer, "%ith Panel", num);
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
		free (hint);
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
	if (!panel->notebook)
		return;

	gtk_notebook_set_page (
		GTK_NOTEBOOK (panel->notebook),
		panel->list_type == list_icons ? 0 : 1);
}

/* Releases all of the X resources allocated */
void
x_panel_destroy (WPanel *panel)
{
	gtk_widget_destroy (GTK_WIDGET (panel->xwindow));
}
