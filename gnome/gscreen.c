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
#include "gdesktop.h"
#include "gdnd.h"
#include "gtkdtree.h"
#include "gpageprop.h"
#include "gpopup.h"
#include "gcmd.h"
#include "gcliplabel.h"
#include "gicon.h"
#include "../vfs/vfs.h"
#include <gdk/gdkprivate.h>

/* Whether to display the tree view on the left */
int tree_panel_visible = -1;

/* The pixmaps */
#include "dir-close.xpm"
#include "link.xpm"
#include "dev.xpm"


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
	char  **texts;

	texts = g_new (char *, items+1);

	gtk_clist_freeze (GTK_CLIST (cl));
	gtk_clist_clear (GTK_CLIST (cl));

	/* which column holds the type information */
	type_col = -1;

	g_assert (items == cl->columns);
		  
	texts [items] = NULL;
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

		color = file_compute_color (NORMAL, fe);
		panel_file_list_set_row_colors (cl, i, color);

		if (type_col != -1)
			panel_file_list_set_type_bitmap (cl, i, type_col, color, fe);

		if (fe->f.marked)
			gtk_clist_select_row (cl, i, 0);
		
	}
	/* This is needed as the gtk_clist_append changes selected under us :-( */
	panel->selected = selected;
	select_item (panel);
	gtk_clist_thaw (GTK_CLIST (cl));
	 g_free (texts);
}

/*
 * Icon view: load the panel contents
 */
static void
panel_fill_panel_icons (WPanel *panel)
{
	GnomeIconList *icons = ILIST_FROM_SW (panel->icons);
	const int top       = panel->count;
	const int selected  = panel->selected;
	int i;
	GdkImlibImage *image;

	gnome_icon_list_freeze (icons);
	gnome_icon_list_clear (icons);

	for (i = 0; i < top; i++){
		file_entry *fe = &panel->dir.list [i];
		int p;
		
		image = gicon_get_icon_for_file (panel->cwd, fe, TRUE);
		p = gnome_icon_list_append_imlib (icons, image, fe->fname);
		if (fe->f.marked)
			gnome_icon_list_select_icon (icons, p);
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
	
	if (panel->list_type == list_icons){
		GnomeIconList *list = ILIST_FROM_SW (panel->icons);

		gnome_icon_list_select_icon (list, panel->selected);

		if (list->icon_list){
			if (GTK_WIDGET (list)->allocation.x != -1)
				if (gnome_icon_list_icon_is_visible (list, panel->selected) != GTK_VISIBILITY_FULL)
					gnome_icon_list_moveto (list, panel->selected, 0.5);
		}
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
	char_width = gdk_string_width (sw->style->font, "xW") / 2;
	for (format = panel->format; format; format = format->next) {
		format->field_len = format->requested_field_len;
		if (!format->use_in_gui)
			continue;

		if (format->use_in_gui == 2)
			used_columns += 2;
		else
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

static int
panel_file_list_press_row (GtkWidget *file_list, GdkEvent *event, WPanel *panel)
{
	/* FIXME: This is still very broken. */
	if (event->type == GDK_BUTTON_PRESS && event->button.button == 3) {
		gint row, column;

		if (gtk_clist_get_selection_info (GTK_CLIST (file_list),
						  event->button.x, event->button.y,
						  &row, &column)) {
			gtk_clist_select_row (GTK_CLIST (file_list), row, 0);
#if 1
			gpopup_do_popup2 ((GdkEventButton *) event, panel, NULL);
#else
			gpopup_do_popup ((GdkEventButton *) event, panel,
					 NULL, row, panel->dir.list[row].fname);
#endif
		} else
			file_list_popup ((GdkEventButton *) event, panel);
	}
	return TRUE;
}

static void
panel_file_list_select_row (GtkWidget *file_list, int row, int column, GdkEvent *event, WPanel *panel)
{
	panel->selected = row;
	do_file_mark (panel, row, 1);
	display_mini_info (panel);
	execute_hooks (select_file_hook);

	if (!event)
		return;

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 2){
			char *fullname;

			if (S_ISDIR (panel->dir.list [row].buf.st_mode) ||
			    panel->dir.list [row].f.link_to_dir){
				fullname = concat_dir_and_file (panel->cwd,
								panel->dir.list [row].fname);
				new_panel_at (fullname);
				 g_free (fullname);
			}
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
panel_file_list_unselect_row (GtkWidget *widget, int row, int columns, GdkEvent *event, WPanel *panel)
{
	do_file_mark (panel, row, 0);
	display_mini_info (panel);
	if (panel->marked == 0)
		panel->selected = 0;
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

	create_pixmap (DIRECTORY_CLOSE_XPM, &icon_directory_pixmap, &icon_directory_mask);
	create_pixmap (link_xpm, &icon_link_pixmap, &icon_link_mask);
	create_pixmap (dev_xpm, &icon_dev_pixmap, &icon_dev_mask);
}

typedef gboolean (*desirable_fn)(WPanel *p, int x, int y);
typedef gboolean (*scroll_fn)(gpointer data);

static gboolean
panel_setup_drag_scroll (WPanel *panel, int x, int y, desirable_fn desirable, scroll_fn scroll)
{
	panel_cancel_drag_scroll (panel);

	panel->drag_motion_x = x;
	panel->drag_motion_y = y;

	if ((*desirable)(panel, x, y)){
		panel->timer_id = gtk_timeout_add (60, scroll, panel);
		return TRUE;
	}

	return FALSE;
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

	gtk_clist_set_selection_mode (GTK_CLIST (file_list), GTK_SELECTION_EXTENDED);

	for (i = 0, format = panel->format; format; format = format->next) {
		GtkJustification just;

		if (!format->use_in_gui)
			continue;

		/* Set desired justification */
		switch (HIDE_FIT(format->just_mode)){
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

	/* Configure the scrolbars */
	adjustment = GTK_OBJECT (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw)));
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
	file_entry *fe;
	char *file;
	int free_file, free_fe;
	int idx;
	gboolean reload;

	idx = gnome_icon_list_get_icon_at (gil, x, y);
	if (idx == -1) {
		file = panel->cwd;
		fe = file_entry_from_file (file);
		if (!fe)
			return; /* eeeek */

		free_file = FALSE;
		free_fe = TRUE;
	} else {
		fe = &panel->dir.list[idx];

		if (S_ISDIR (fe->buf.st_mode) || fe->f.link_to_dir){
			file = g_concat_dir_and_file (panel->cwd, panel->dir.list[idx].fname);
			free_file = TRUE;
		} else {
			file = panel->cwd;
			free_file = FALSE;
		}
		
		free_fe = FALSE;
	}

	reload = gdnd_perform_drop (context, selection_data, file, fe);

	if (free_file)
		g_free (file);

	if (free_fe)
		file_entry_free (fe);

	if (reload) {
		update_one_panel_widget (panel, 0, UP_KEEPSEL);
		panel_update_contents (panel);
	}
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
	file_entry *fe;
	char *file;
	int free_file, free_fe;
	int row;
	int reload;

	if (gtk_clist_get_selection_info (clist, x, y, &row, NULL) == 0) {
		file = panel->cwd;
		fe = file_entry_from_file (file);
		if (!fe)
			return; /* eeeek */

		free_file = FALSE;
		free_fe = TRUE;
	} else {
		g_assert (row < panel->count);

		fe = &panel->dir.list[row];

		if (S_ISDIR (fe->buf.st_mode) || fe->f.link_to_dir){
			file = g_concat_dir_and_file (panel->cwd, panel->dir.list[row].fname);
			free_file = TRUE;
		} else {
			file = panel->cwd;
			free_file = FALSE;
		}
		
		free_fe = FALSE;
	}

	reload = gdnd_perform_drop (context, selection_data, file, fe);

	if (free_file)
		g_free (file);

	if (free_fe)
		file_entry_free (fe);

	if (reload) {
		update_one_panel_widget (panel, 0, UP_KEEPSEL);
		panel_update_contents (panel);
	}
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

	if (!gtk_clist_get_selection_info (GTK_CLIST (dtree), x, y, &row, &col))
		return;

	node = gtk_ctree_node_nth (GTK_CTREE (dtree), row);
	if (!node)
		return;
	gtk_ctree_expand_recursive (GTK_CTREE (dtree), node);

	path = gtk_dtree_get_row_path (dtree, node, 0);
	fe = file_entry_from_file (path);
	if (!fe)
		return; /* eeeek */

	gdnd_perform_drop (context, selection_data, path, fe);

	file_entry_free (fe);
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
	if (event->window != GTK_CLIST (widget)->clist_window)
		return FALSE;

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

	if (!panel->maybe_start_drag)
		return FALSE;

	if ((abs (event->x - panel->click_x) < 4) ||
	    (abs (event->y - panel->click_y) < 4))
		return FALSE;

	list = gtk_target_list_new (drag_types, ELEMENTS (drag_types));

	context = gtk_drag_begin (widget, list,
				  (GDK_ACTION_COPY | GDK_ACTION_MOVE
				   | GDK_ACTION_LINK | GDK_ACTION_ASK),
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
	panel_cancel_drag_scroll (panel);
	panel->dragging = 0;
}

/* Wrapper for the motion_notify callback; it ignores motion events from the
 * clist if they do not come from the clist_window.
 */
static int
panel_clist_motion (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	if (event->window != GTK_CLIST (widget)->clist_window)
		return FALSE;

	panel_widget_motion (widget, event, data);

	/* We have to stop the motion event from ever reaching the clist.
	 * Otherwise it will begin dragging/selecting rows when we don't want
	 * that.  Yes, the clist widget sucks.
	 */

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "motion_notify_event");
	return TRUE;
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

	if (y < 10){
		if (va->value > va->lower)
			return TRUE;
	} else {
		if (y > (GTK_WIDGET (panel->list)->allocation.height - 10)){
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

/* Convenience function to return whether we are on a valid drop area in a
 * GtkCList.
 */
static int
can_drop_on_clist (WPanel *panel, int x, int y)
{
	GtkCList *clist;
	int border_width;

	clist = CLIST_FROM_SW (panel->list);

	border_width = GTK_CONTAINER (clist)->border_width;

	if (y < border_width + clist->column_title_area.y + clist->column_title_area.height)
		return FALSE;
	else
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
	GdkDragAction action;
	GtkWidget *source_widget;
	gint idx;
	file_entry *fe;

	panel = data;

	if (!can_drop_on_clist (panel, x, y)) {
		gdk_drag_status (context, 0, time);
		return TRUE;
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

	action = gdnd_validate_action (context,
				       FALSE,
				       source_widget != NULL,
				       source_widget == widget,
				       panel->cwd,
				       fe,
				       fe ? fe->f.marked : FALSE);

	gdk_drag_status (context, action, time);
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

	panel = data;

	/* Set up auto-scrolling */

	panel_setup_drag_scroll (panel, x, y,
				 panel_icon_list_scrolling_is_desirable,
				 panel_icon_list_scroll);

	/* Validate the drop */

	gdnd_find_panel_by_drag_context (context, &source_widget);

	idx = gnome_icon_list_get_icon_at (GNOME_ICON_LIST (widget), x, y);
	fe = (idx == -1) ? NULL : &panel->dir.list[idx];

	action = gdnd_validate_action (context,
				       FALSE,
				       source_widget != NULL,
				       source_widget == widget,
				       panel->cwd,
				       fe,
				       fe ? fe->f.marked : FALSE);

	gdk_drag_status (context, action, time);
	return TRUE;
}

/**
 * panel_icon_list_drag_leave:
 *
 * Invoked when the dragged object has left our region
 */
static void
panel_icon_list_drag_leave (GtkWidget *widget, GdkDragContext *ctx, guint time, void *data)
{
	WPanel *panel = data;

	panel_cancel_drag_scroll (panel);
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

	file_list = gtk_clist_new_with_titles (items, titles);
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
	gtk_signal_connect (GTK_OBJECT (file_list), "unselect_row",
			    GTK_SIGNAL_FUNC (panel_file_list_unselect_row),
			    panel);
#if 1
	gtk_signal_connect_after (GTK_OBJECT (file_list), "button_press_event",
				  GTK_SIGNAL_FUNC (panel_file_list_press_row),
				  panel);
#endif
	gtk_clist_set_button_actions (GTK_CLIST (file_list), 1, GTK_BUTTON_SELECTS | GTK_BUTTON_DRAGS);
	gtk_clist_set_button_actions (GTK_CLIST (file_list), 2, GTK_BUTTON_SELECTS);
	
	/* Set up drag and drop */

	load_dnd_icons ();

	gtk_drag_dest_set (GTK_WIDGET (file_list),
			   GTK_DEST_DEFAULT_DROP,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
#if 0
	/* Make directories draggable */
	gtk_drag_source_set (GTK_WIDGET (file_list), GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			     drag_types, ELEMENTS (drag_types),
			     GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK);
#endif
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
			    GTK_SIGNAL_FUNC (panel_clist_motion), panel);
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

	if (!event)
		return;

	switch (event->type){
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
#if 1
			gpopup_do_popup2 ((GdkEventButton *) event, panel, NULL);
#else
			gpopup_do_popup ((GdkEventButton *) event, panel,
					 NULL, index, panel->dir.list[index].fname);
#endif
		}
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
		g_free (panel->dir.list [index].fname);
		panel->dir.list [index].fname = g_strdup (dest);
		return TRUE;
	} else
		return FALSE;
}

/* Callback for rescanning the cwd */
static void
handle_rescan_directory (GtkWidget *widget, gpointer data)
{
	reread_cmd ();
}

/* The popup menu for file panels */
static GnomeUIInfo file_list_popup_items[] = {
	GNOMEUIINFO_ITEM_NONE (N_("Rescan Directory"), N_("Reloads the current directory"),
			       handle_rescan_directory),
	GNOMEUIINFO_ITEM_NONE (N_("New folder"), N_("Creates a new folder here"),
			       gnome_mkdir_cmd),
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

	if (icon == -1) {
		if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
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
		if (y > (GTK_WIDGET (dtree)->allocation.height - 10)){
			if (va->value < va->upper - va->page_size)
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
#if 0
	/* This behaviour is confusing --jrb and quartic (and MS, apparently)*/
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
#endif
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

	if (panel_setup_drag_scroll (panel, x, y, panel_tree_scrolling_is_desirable, panel_tree_scroll))
		return TRUE;

	r = gtk_clist_get_selection_info (
		GTK_CLIST (widget), x, y, &row, &col);

	if (r) {
		GtkCTreeNode *current;
		current = gtk_ctree_node_nth (GTK_CTREE (widget), row);
		panel_tree_check_auto_expand (panel, current);
	} else
		panel_tree_check_auto_expand (panel, NULL);
	GTK_DTREE (widget)->internal = TRUE;
	gtk_clist_select_row (GTK_CLIST (widget), row, 0);
	GTK_DTREE (widget)->internal = FALSE;
	
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
		data = g_strconcat ("file:", dtree->drag_dir, NULL);
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
			    (GtkSignalFunc) panel_chdir, panel);
	gtk_signal_connect (GTK_OBJECT (tree), "scan_begin",
			    (GtkSignalFunc) panel_tree_scan_begin, panel);
	gtk_signal_connect (GTK_OBJECT (tree), "scan_end",
			    (GtkSignalFunc) panel_tree_scan_end, panel);

	gtk_drag_dest_set (GTK_WIDGET (tree), GTK_DEST_DEFAULT_ALL,
			   drop_types, ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

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
	gtk_drag_source_set (GTK_WIDGET (tree), GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			     drag_types, ELEMENTS (drag_types),
			     GDK_ACTION_LINK | GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK | GDK_ACTION_DEFAULT);

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
	
	/*
	 * Hack: set the default startup size for the pane without
	 * using _set_usize which would set the minimal size
	 */
	GTK_PANED (pane)->child1_size = size;
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
	GtkWidget *status_line, *filter, *vbox, *ministatus_box;
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
	copy_uiinfo_widgets (panel_view_toolbar_uiinfo, &panel->view_toolbar_items);

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
	gtk_box_pack_start (GTK_BOX (status_line),
                            cwd, TRUE, TRUE, 0);

	dock =  gnome_dock_item_new ("gmc-toolbar1",
                                     (GNOME_DOCK_ITEM_BEH_EXCLUSIVE
                                      | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL));
	gtk_container_add (GTK_CONTAINER(dock), status_line);
	gnome_dock_add_item (GNOME_DOCK(GNOME_APP (panel->xwindow)->dock),
			     GNOME_DOCK_ITEM (dock), GNOME_DOCK_TOP, 1, 0, 0, FALSE);

	gtk_widget_show_all (dock);

	panel->view_table = gtk_table_new (1, 1, 0);
	gtk_widget_show (panel->view_table);

	/*
	 * The status bar.
	 */
	ministatus_box = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME(ministatus_box), GTK_SHADOW_IN);

	panel->status = gtk_label_new (_("Show all files"));
	gtk_misc_set_alignment (GTK_MISC (panel->status), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (panel->status), 2, 0);

	gtk_box_pack_start (GTK_BOX (panel->ministatus), ministatus_box, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER(ministatus_box), panel->status);

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
