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
#include "panel.h"
#include "command.h"
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
#include "gtree.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "gcliplabel.h"
#include "gblist.h"
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

/* These are big images used in the Icon View,  for the gnome_icon_list */
static GdkImlibImage *icon_view_directory;
static GdkImlibImage *icon_view_executable;
static GdkImlibImage *icon_view_symlink;
static GdkImlibImage *icon_view_device;
static GdkImlibImage *icon_view_regular;
static GdkImlibImage *icon_view_core;
static GdkImlibImage *icon_view_sock;

#ifdef OLD_DND
static char *drag_types [] = { "text/plain", "file:ALL", "url:ALL" };
static char *drop_types [] = { "url:ALL" };
#endif

static GtkTargetEntry drag_types [] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/url-list", 0, TARGET_URL_LIST },
	{ "text/plain",    0, TARGET_TEXT_PLAIN },
};

static GtkTargetEntry drop_types [] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/url-list", 0, TARGET_URL_LIST },
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

		
		switch (file_entry_color (fe)){
		case DIRECTORY_COLOR:
			image = icon_view_directory;
			break;
			
		case LINK_COLOR:
			image = icon_view_symlink;
			break;
			
		case DEVICE_COLOR:
			image = icon_view_device;
			break;
			
		case SPECIAL_COLOR:
			image = icon_view_sock;
			break;
			
		case EXECUTABLE_COLOR:
			image = icon_view_executable;
			break;
				
		case CORE_COLOR:
			image = icon_view_core;
			break;
			
		case STALLED_COLOR:
		case NORMAL_COLOR:
		default:
			image = icon_view_regular;
		}
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

	gtk_dtree_do_select_dir (GTK_DTREE (panel->tree), panel->cwd); 

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

	/* Hack: the default mini-info display only gets displayed
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

	printf ("Selecting %d %p, %d\n", row, event, event ? event->type : -1);
	if (!event) {
		internal_select_item (file_list, panel, row);
		return;
	}
	
	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		printf ("1\n");
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
#if 1
			file_popup (&event->button, panel, NULL, row, panel->dir.list[row].fname);
#else
			/* FIXME: this should happen on button press, not button release */
			gpopup_do_popup (panel->dir.list[row].fname, panel, (GdkEventButton *) event);
#endif
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
		data = copy = xmalloc (total_len, "build_selected_file_list");
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
			
	switch (info){
	case TARGET_URL_LIST:
	case TARGET_URI_LIST:
	case TARGET_TEXT_PLAIN:
		data = panel_build_selected_file_list (panel, &len);

		gtk_selection_data_set (
			selection_data, selection_data->target, 8,
			data, len);
		break;
	}
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

#ifdef OLD_DND
/*
 * Handler for text/plain and url:ALL drag types
 *
 * Sets the drag information to the filenames selected on the panel
 */
static void
panel_transfer_file_names (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel, int *len, char **data)
{
	*data = panel_build_selected_file_list (panel, len);
}

/*
 * Handler for file:ALL type (target application only understands local pathnames)
 *
 * Makes local copies of the files and transfers the filenames.
 */
static void
panel_make_local_copies_and_transfer (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel,
				      int *len, char **data)
{
	char *filename, *localname;
	int i;
	
	if (panel->marked){
		char **local_names_array, *p;
		int j, total_len;

		/* First assemble all of the filenames */
		local_names_array = malloc (sizeof (char *) * panel->marked);
		total_len = j = 0;
		for (i = 0; i < panel->count; i++){
			char *filename;

			if (!panel->dir.list [i].f.marked)
				continue;
			
			filename = concat_dir_and_file (panel->cwd, panel->dir.list [i].fname);
			localname = mc_getlocalcopy (filename);
			total_len += strlen (localname) + 1;
			local_names_array [j++] = localname;
			free (filename);
		}
		*len = total_len;
		*data = p = malloc (total_len);
		for (i = 0; i < j; i++){
			strcpy (p, local_names_array [i]);
			g_free (local_names_array [i]);
			p += strlen (p) + 1;
		}
	} else {
		filename = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
		localname = mc_getlocalcopy (filename);
		free (filename);
		*data = localname;
		*len = strlen (localname + 1);
	}
}

static void
panel_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel, int *len, char **data)
{
	*len = 0;
	*data = 0;
	
	if ((strcmp (event->data_type, "text/plain") == 0) ||
	    (strcmp (event->data_type, "url:ALL") == 0)){
		panel_transfer_file_names (widget, event, panel, len, data);
	} else if (strcmp (event->data_type, "file:ALL") == 0){
		if (vfs_file_is_local (panel->cwd))
			panel_transfer_file_names (widget, event, panel, len, data);
		else
			panel_make_local_copies_and_transfer (widget, event, panel, len, data);
	}
}

/*
 * Listing mode: drag request handler
 */
static void
panel_clist_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel)
{
	GdkWindowPrivate *clist_window = (GdkWindowPrivate *) (GTK_WIDGET (widget)->window);
	GdkWindowPrivate *clist_areaw  = (GdkWindowPrivate *) (GTK_CLIST (widget)->clist_window);
	char *data;
	int len;

	panel_drag_request (widget, event, panel, &len, &data);
	
	/* Now transfer the DnD information */
	if (len && data){
		if (clist_window->dnd_drag_accepted)
			gdk_window_dnd_data_set ((GdkWindow *)clist_window, (GdkEvent *) event, data, len);
		else
			gdk_window_dnd_data_set ((GdkWindow *)clist_areaw, (GdkEvent *) event, data, len);
		free (data);
	}
}

/*
 * Invoked when a drop has happened on the panel
 */
static void
panel_clist_drop_data_available (GtkWidget *widget, GdkEventDropDataAvailable *data, WPanel *panel)
{
	gint winx, winy;
	gint dropx, dropy;
	gint row;
	char *drop_dir;
	
	gdk_window_get_origin (GTK_CLIST (widget)->clist_window, &winx, &winy);
	dropx = data->coords.x - winx;
	dropy = data->coords.y - winy;

	if (dropx < 0 || dropy < 0)
		return;

	if (gtk_clist_get_selection_info (GTK_CLIST (widget), dropx, dropy, &row, NULL) == 0)
		drop_dir = panel->cwd;
	else {
		g_assert (row < panel->count);

	}
#if 0
	drop_on_directory (data, drop_dir, 0);
#endif

	if (drop_dir != panel->cwd)
		free (drop_dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
}

static void
panel_drag_begin (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	GdkPoint hotspot = { 15, 15 };

	if (panel->marked > 1){
		if (drag_multiple && drag_multiple_ok){
			gdk_dnd_set_drag_shape (drag_multiple->window, &hotspot,
						drag_multiple_ok->window, &hotspot);
			gtk_widget_show (drag_multiple);
			gtk_widget_show (drag_multiple_ok);
		}
			
	} else {
		if (drag_directory && drag_directory_ok)
			gdk_dnd_set_drag_shape (drag_directory->window, &hotspot,
						drag_directory_ok->window, &hotspot);	
			gtk_widget_show (drag_directory_ok);
			gtk_widget_show (drag_directory);
	}

}

static void
panel_icon_list_drag_begin (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	GnomeIconList *icons = GNOME_ICON_LIST (panel->icons);
	
	icons->last_clicked = NULL;
	panel_drag_begin (widget, event, panel);
}

static void
panel_artificial_drag_start (GtkCList *window, GdkEventMotion *event)
{
	artificial_drag_start (window->clist_window, event->x, event->y);
}
#endif /* OLD_DND */


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

#define MAX(a,b) ((a > b) ? a : b)

static int
panel_widget_motion (GtkWidget *widget, GdkEventMotion *event, WPanel *panel)
{
	GtkTargetList *list;
	GdkDragAction action;
	GdkDragContext *context;
	
	if (!panel->maybe_start_drag)
		return FALSE;

	if (panel->maybe_start_drag == 3)
		return FALSE;
	
	if ((abs (event->x - panel->click_x) < 4) || 
	    (abs (event->y - panel->click_y) < 4))
		return FALSE;
	
	list = gtk_target_list_new (drag_types, ELEMENTS (drag_types));

	/* Control+Shift = LINK */
	if ((event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == (GDK_SHIFT_MASK | GDK_CONTROL_MASK))
		action = GDK_ACTION_LINK;
	else if (event->state & (GDK_SHIFT_MASK))
		action = GDK_ACTION_MOVE;
	else
		action = GDK_ACTION_COPY;
		
	context = gtk_drag_begin (widget, list, action, panel->maybe_start_drag, (GdkEvent *) event);
	gtk_drag_set_icon_default (context);
	
	return FALSE;
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

	/* These implement our drag-start activation code.  We need to manually activate the drag as
	 * the DnD code in Gtk+ will make the scrollbars in the CList activate drags when they are
	 * moved.
	 */
	gtk_signal_connect (GTK_OBJECT (file_list), "button_press_event",
			    GTK_SIGNAL_FUNC (panel_clist_button_press), panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "button_release_event",
			    GTK_SIGNAL_FUNC (panel_clist_button_release), panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "motion_notify_event",
			    GTK_SIGNAL_FUNC (panel_widget_motion), panel);

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

	if (!event)
		return;
	
	switch (event->type){
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
#if 1
			file_popup (&event->button, panel, NULL, index, panel->dir.list [index].fname);
#else
			gpopup_do_popup (panel->dir.list[index].fname, panel, (GdkEventButton *) event);
#endif
			return;
		}
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

static GdkImlibImage *
load_image_icon_view (char *base)
{
	GdkImlibImage *im;
	char *f = concat_dir_and_file (ICONDIR, base);

	im = gdk_imlib_load_image (f);
	g_free (f);
	return im;
}

void
load_imlib_icons (void)
{
	static int loaded;

	if (loaded)
		return;
	
	icon_view_directory  = load_image_icon_view ("i-directory.png");
	icon_view_executable = load_image_icon_view ("i-executable.png");
	icon_view_symlink    = load_image_icon_view ("i-symlink.png");
	icon_view_device     = load_image_icon_view ("i-device.png");
	icon_view_regular    = load_image_icon_view ("i-regular.png");
	icon_view_core       = load_image_icon_view ("i-core.png");
	icon_view_sock       = load_image_icon_view ("i-sock.png");

	loaded = 1;
}

#if OLD_DND
static void
panel_icon_list_artificial_drag_start (GtkObject *obj, GdkEventMotion *event)
{
	artificial_drag_start (GTK_WIDGET (obj)->window, event->x, event->y);
}

/*
 * Icon view drag request handler
 */
static void
panel_icon_list_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel)
{
	char *data;
	int len;

	panel_drag_request (widget, event, panel, &len, &data);

	if (len && data){
		gdk_window_dnd_data_set (widget->window, (GdkEvent *) event, data, len);
		free (data);
	}
}

static void
panel_icon_list_drop_data_available (GtkWidget *widget, GdkEventDropDataAvailable *data, WPanel *panel)
{
	GnomeIconList *ilist = GNOME_ICON_LIST (widget);
	gint winx, winy;
	gint dropx, dropy;
	gint item;
	char *drop_dir;
	
	gdk_window_get_origin (widget->window, &winx, &winy);
	dropx = data->coords.x - winx;
	dropy = data->coords.y - winy;

	if (dropx < 0 || dropy < 0)
		return;

	item = gnome_icon_list_get_icon_at (ilist, dropx, dropy);
	if (item == -1)
		drop_dir = panel->cwd;
	else {
		g_assert (item < panel->count);

		if (S_ISDIR (panel->dir.list [item].buf.st_mode))
			drop_dir = concat_dir_and_file (panel->cwd, panel->dir.list [item].fname);
		else 
			drop_dir = panel->cwd;
	}
#if 0
	drop_on_directory (data, drop_dir, 0);
#endif

	if (drop_dir != panel->cwd)
		free (drop_dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
}
#endif

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
	else
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
	GnomeIconList *ilist;

	ilist = GNOME_ICON_LIST (gnome_icon_list_new (90, NULL, TRUE));
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

void
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
			char *str;

			link_target [len] = 0;
			str = copy_strings ("-> ", link_target, NULL);
			gtk_label_set (label, str);
			free (str);
		} else 
			gtk_label_set (label, _("<readlink failed>"));
		return;
	}

	if (panel->estimated_total){
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
			gtk_label_set (label, "");
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

/* Signal handler for DTree's "directory_changed" signal */
static void
panel_chdir (GtkDTree *dtree, char *path, WPanel *panel)
{
	do_panel_cd (panel, path, cd_exact);
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
	GtkWidget *frame, *cwd, *back_p, *fwd_p;
	GtkWidget *display, *tree_scrolled_window;
		
	panel->xwindow = gtk_widget_get_toplevel (GTK_WIDGET (panel->widget.wdata));
	
	panel->table = gtk_table_new (2, 1, 0);

	/*
	 * Tree View
	 */
	tree_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (tree_scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	panel->tree = gtk_dtree_new ();
	gtk_signal_connect (GTK_OBJECT (panel->tree), "directory_changed",
			    GTK_SIGNAL_FUNC (panel_chdir), panel);
	gtk_container_add (GTK_CONTAINER (tree_scrolled_window), panel->tree);
	gtk_widget_show_all (tree_scrolled_window);
	
	/*
	 * Icon and Listing display
	 */
	panel->icons = panel_create_icon_display (panel);
	panel->scrollbar = gtk_vscrollbar_new (GNOME_ICON_LIST (panel->icons)->adj);
	gtk_widget_show (panel->scrollbar);
	
	panel->list  = panel_create_file_list (panel);
	gtk_widget_ref (panel->icons);
	gtk_widget_ref (panel->list);

	if (panel->list_type == list_icons)
		display = panel->icons;
	else
		display = panel->list;

	/*
	 * Pane
	 */
	panel->pane = gtk_hpaned_new ();
	gtk_widget_show (panel->pane);

	gtk_paned_add1 (GTK_PANED (panel->pane), tree_scrolled_window);
	
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
	
	panel->up_b    = gtk_button_new_with_label (_("Up"));
	panel->back_b   = gtk_button_new ();
	panel->fwd_b    = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (panel->back_b), back_p);
	gtk_container_add (GTK_CONTAINER (panel->fwd_b), fwd_p);
	gtk_signal_connect (GTK_OBJECT (panel->back_b), "clicked", GTK_SIGNAL_FUNC(panel_back), panel);
	gtk_signal_connect (GTK_OBJECT (panel->fwd_b), "clicked", GTK_SIGNAL_FUNC(panel_fwd), panel);
	gtk_signal_connect (GTK_OBJECT (panel->up_b), "clicked", GTK_SIGNAL_FUNC(panel_up), panel);
	panel_update_marks (panel);
	
	/*
	 * ministatus
	 */
	panel->ministatus = gtk_label_new (""); /* was a cliplabel */
	gtk_widget_set_usize (panel->ministatus, 0, -1);
	gtk_misc_set_alignment (GTK_MISC (panel->ministatus), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (panel->ministatus), 3, 0);
	gtk_widget_show (panel->ministatus);

	/*
	 * Status line and packing of all of the toys
	 */
	status_line = gtk_hbox_new (0, 0);
	gtk_container_set_border_width (GTK_CONTAINER (status_line), 3);
	
	gtk_label_set_justify (GTK_LABEL (panel->ministatus), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (status_line), panel->back_b, 0, 0, 2);
	gtk_box_pack_start (GTK_BOX (status_line), panel->up_b, 0, 0, 2);
	gtk_box_pack_start (GTK_BOX (status_line), panel->fwd_b, 0, 0, 2);
	gtk_box_pack_start (GTK_BOX (status_line), cwd, 1, 1, 5);
	gtk_box_pack_start (GTK_BOX (status_line), button_switch_to_icon (panel), 0, 0, 2);
	gtk_box_pack_start (GTK_BOX (status_line), button_switch_to_listing (panel), 0, 0, 2);
#if 0
	/* Remove the filter for now, until I add another toolbar */
	gtk_box_pack_end   (GTK_BOX (status_line), filter, 0, 0, 0);
#endif
	gtk_widget_show_all (status_line);
	
	/*
	 * The statusbar
	 */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

	panel->status = gtk_label_new (""); /* used to be a cliplabel */
	gtk_misc_set_alignment (GTK_MISC (panel->status), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (panel->status), 3, 0);
	gtk_container_add (GTK_CONTAINER (frame), panel->status);
	gtk_label_set_justify (GTK_LABEL (panel->status), GTK_JUSTIFY_LEFT);
	gtk_widget_show_all (frame);


	panel->view_table = gtk_table_new (1, 1, 0);
	gtk_widget_show (panel->view_table);
	
	/* Add both the icon view and the listing view */
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->icons, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->scrollbar, 1, 2, 0, 1,
			  0,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_table_attach (GTK_TABLE (panel->view_table), panel->list, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (display);

	gtk_table_attach (GTK_TABLE (panel->table), panel->pane, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);

	gtk_paned_add2 (GTK_PANED (panel->pane), panel->view_table);
	
	gtk_table_attach (GTK_TABLE (panel->table), status_line, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

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
	if (panel->list_type == list_icons){
		if (panel->icons){
			gtk_widget_show (panel->icons);
			gtk_widget_show (panel->scrollbar);
		}
		if (panel->list)
			gtk_widget_hide (panel->list);
	} else {
		panel_switch_new_display_mode (panel);
		if (panel->list){
			gtk_widget_show (panel->list);
			gtk_widget_show (panel->scrollbar);
		}
		if (panel->icons){
			gtk_widget_hide (panel->icons);
			gtk_widget_hide (panel->scrollbar);
		}
	}
}

/* Releases all of the X resources allocated */
void
x_panel_destroy (WPanel *panel)
{
	gtk_widget_destroy (GTK_WIDGET (panel->xwindow));
}



