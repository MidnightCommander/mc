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
#include "x.h"
#include "dir.h"
#include "panel.h"
#include "command.h"
#include "panel.h"		/* current_panel */
#include "command.h"		/* cmdline */
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
#include "gpageprop.h"
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

static char *drag_types [] = { "text/plain", "file:ALL", "url:ALL" };
static char *drop_types [] = { "url:ALL" };

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

/* GtkWidgets with the shaped windows for dragging */
GtkWidget *drag_directory    = NULL;
GtkWidget *drag_directory_ok = NULL;
GtkWidget *drag_multiple     = NULL;
GtkWidget *drag_multiple_ok  = NULL;

typedef void (*context_menu_callback)(GtkWidget *, WPanel *);

/*
 * Flags for the context-sensitive popup menus
 */
#define F_ALL         1
#define F_REGULAR     2
#define F_SYMLINK     4
#define F_SINGLE      8
#define F_NOTDIR     16

static void panel_file_list_configure_contents (GtkWidget *file_list, WPanel *panel, int main_width, int height);

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
	GtkCList *cl        = GTK_CLIST (panel->list);
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
	select_item (panel);
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
	panel_file_list_set_row_colors (GTK_CLIST (panel->list), index, color);
}

void
x_select_item (WPanel *panel)
{
	if (panel->list_type == list_icons){
		GnomeIconList *list = GNOME_ICON_LIST (panel->icons);
		
		do_file_mark (panel, panel->selected, 1);
		display_mini_info (panel);
		gnome_icon_list_select_icon (list, panel->selected);

		if (list->icon_rows){
			if (gnome_icon_list_icon_is_visible (list, panel->selected) != GTK_VISIBILITY_FULL)
				gnome_icon_list_moveto (list, panel->selected, 0.5);
		}
	} else {
		GtkCList *clist = GTK_CLIST (panel->list);
		int color, marked;
		
		if (panel->dir.list [panel->selected].f.marked)
			marked = 1;
		else
			marked = 0;
		
		color = file_compute_color (marked ? MARKED_SELECTED : SELECTED, &panel->dir.list [panel->selected]);
		panel_file_list_set_row_colors (GTK_CLIST (panel->list), panel->selected, color);
		
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
		panel_file_list_set_row_colors (GTK_CLIST (panel->list), panel->selected, color);
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
panel_file_list_configure_contents (GtkWidget *file_list, WPanel *panel, int main_width, int height)
{
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
	for (format = panel->format; format; format = format->next){
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
	if (GTK_WIDGET_VISIBLE (GTK_CLIST (file_list)->vscrollbar)){
		int scrollbar_width = GTK_WIDGET (GTK_CLIST (file_list)->vscrollbar)->requisition.width;
		int scrollbar_space = GTK_CLIST_CLASS (GTK_OBJECT (file_list)->klass)->scrollbar_spacing;

		lost_pixels += scrollbar_space + scrollbar_width;
	}
	char_width = gdk_string_width (file_list->style->font, "xW") / 2;
	width = main_width - lost_pixels;
	extra_pixels  = width % char_width;
	usable_pixels = width - extra_pixels;
	total_columns = usable_pixels / char_width;
	extra_columns = total_columns - used_columns;
	if (extra_columns > 0){
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
	if (used_columns > total_columns){
		expand_space = 0;
		shrink_space = (used_columns - total_columns) / items;
	} else
		shrink_space = 0;
	
	gtk_clist_freeze (GTK_CLIST (file_list));
	for (i = 0, format = panel->format; format; format = format->next){
		if (!format->use_in_gui)
			continue;

		format->field_len += (format->expand ? expand_space : 0) - shrink_space;
		gtk_clist_set_column_width (GTK_CLIST (file_list), i, format->field_len * char_width);
		i++;
	}
	gtk_clist_thaw (GTK_CLIST (file_list));
}

static void
panel_action_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (_(" Open with..."),
				       _("Enter extra arguments:"), panel->dir.list [panel->selected].fname);
	if (!command)
		return;
	execute (command);
	free (command);
}

static void
panel_action_open (GtkWidget *widget, WPanel *panel)
{
	if (do_enter (panel))
		return;
	panel_action_open_with (widget, panel);
}

void
panel_action_view (GtkWidget *widget, WPanel *panel)
{
	view_cmd (panel);
}

void
panel_action_view_unfiltered (GtkWidget *widget, WPanel *panel)
{
	view_simple_cmd (panel);
}

void
panel_action_properties (GtkWidget *widget, WPanel *panel)
{
	file_entry *fe = &panel->dir.list [panel->selected];
	char *full_name = concat_dir_and_file (panel->cwd, fe->fname);
	
	if (item_properties (GTK_WIDGET (panel->list), full_name, NULL) != 0){
		reread_cmd ();
	}
	free (full_name);
}

/*
 * The context menu: text displayed, condition that must be met and
 * the routine that gets invoked upon activation.
 */
static struct {
	char *text;
	int  flags;
	context_menu_callback callback;
} file_actions [] = {
	{ N_("Properties"),      F_SINGLE,   	    panel_action_properties },
	{ "",                    F_SINGLE,   	    NULL },
	{ N_("Open"),            F_ALL,      	    panel_action_open },
	{ N_("Open with"),       F_ALL,      	    panel_action_open_with },
	{ N_("View"),            F_NOTDIR,   	    panel_action_view },
	{ N_("View unfiltered"), F_NOTDIR,     	    panel_action_view_unfiltered },  
	{ "",                    0,          	    NULL },
	{ N_("Link..."),         F_REGULAR | F_SINGLE, (context_menu_callback) link_cmd },
	{ N_("Symlink..."),      F_SINGLE,             (context_menu_callback) symlink_cmd },
	{ N_("Edit symlink..."), F_SYMLINK,            (context_menu_callback) edit_symlink_cmd },
	{ NULL, 0, NULL },
};

/*
 * context menu, constant entries */
static struct {
	char *text;
	context_menu_callback callback;
} common_actions [] = {
	{ N_("Copy..."),         (context_menu_callback) copy_cmd },
	{ N_("Rename/move.."),   (context_menu_callback) ren_cmd },
	{ N_("Delete..."),       (context_menu_callback) delete_cmd },
	{ NULL, NULL }
};

static GtkWidget *
create_popup_submenu (WPanel *panel, int row, char *filename)
{
	static int submenu_translated;
	GtkWidget *menu;
	int i;
	
	if (!submenu_translated){
		/* FIXME translate it */
		submenu_translated = 1;
	}
	
	menu = gtk_menu_new ();
	for (i = 0; file_actions [i].text; i++){
		GtkWidget *item;

		/* Items with F_ALL bypass any other condition */
		if (!(file_actions [i].flags & F_ALL)){

			/* Items with F_SINGLE require that ONLY ONE marked files exist */
			if (file_actions [i].flags & F_SINGLE){
				if (panel->marked > 1)
					continue;
			}

			/* Items with F_NOTDIR requiere that the selection is not a directory */
			if (file_actions [i].flags & F_NOTDIR){
				struct stat *s = &panel->dir.list [row].buf;

				if (panel->dir.list [row].f.link_to_dir)
					continue;
				
				if (S_ISDIR (s->st_mode))
					continue;
			}
			
			/* Items with F_REGULAR do not accept any strange file types */
			if (file_actions [i].flags & F_REGULAR){
				struct stat *s = &panel->dir.list [row].buf;
				
				if (S_ISLNK (panel->dir.list [row].f.link_to_dir))
					continue;
				if (S_ISSOCK (s->st_mode) || S_ISCHR (s->st_mode) ||
				    S_ISFIFO (s->st_mode) || S_ISBLK (s->st_mode))
					continue;
			}

			/* Items with F_SYMLINK only operate on symbolic links */
			if (file_actions [i].flags & F_SYMLINK){
				if (!S_ISLNK (panel->dir.list [row].buf.st_mode))
					continue;
			}
		}
		if (*file_actions [i].text)
			item = gtk_menu_item_new_with_label (_(file_actions [i].text));
		else
			item = gtk_menu_item_new ();

		gtk_widget_show (item);
		if (file_actions [i].callback){
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC(file_actions [i].callback), panel);
		}

		gtk_menu_append (GTK_MENU (menu), item);
	}
	return menu;
}

/*
 * Ok, this activates a menu popup action for a filename
 * it is kind of hackish, it gets the desired action from the
 * item, so it has to peek inside the item to retrieve the label
 */
static void
popup_activate_by_string (GtkMenuItem *item, WPanel *panel)
{
	char *filename = panel->dir.list [panel->selected].fname;
	char *action;
	int movedir;
	
	g_return_if_fail (GTK_IS_MENU_ITEM (item));
	g_return_if_fail (GTK_IS_LABEL (GTK_BIN (item)->child));

	action = GTK_LABEL (GTK_BIN (item)->child)->label;

	regex_command (filename, action, NULL, &movedir);
}

static void
file_popup_add_context (GtkMenu *menu, WPanel *panel, char *filename)
{
	GtkWidget *item;
	char *p, *q;
	int c, i;

	for (i = 0; common_actions [i].text; i++){
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (_(common_actions [i].text));
		gtk_widget_show (item);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (common_actions [i].callback), panel);
		gtk_menu_append (GTK_MENU (menu), item);
	}
	
	p = regex_command (filename, NULL, NULL, NULL);
	if (!p)
		return;
	
	item = gtk_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_append (menu, item);
	
	for (;;){
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		q = p;
		while (*q && *q != '=' && *q != '\t')
			q++;
		c = *q;
		*q = 0;
		
		item = gtk_menu_item_new_with_label (p);
		gtk_widget_show (item);
		gtk_signal_connect (GTK_OBJECT(item), "activate",
				    GTK_SIGNAL_FUNC(popup_activate_by_string), panel);
		gtk_menu_append (menu, item);
		if (!c)
			break;
		p = q + 1;
	}
}

static void
file_popup (GdkEvent *event, WPanel *panel, int row, char *filename)
{
	GtkWidget *menu = gtk_menu_new ();
	GtkWidget *submenu;
	GtkWidget *item;
	
	item = gtk_menu_item_new_with_label ( (panel->marked > 1)?"...":filename );
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);

	submenu = create_popup_submenu (panel, row, filename);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	
	file_popup_add_context (GTK_MENU (menu), panel, filename);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, event->button.time);
}

static void
internal_select_item (GtkWidget *file_list, WPanel *panel, int row)
{
	unselect_item (panel);
	panel->selected = row;

	gtk_signal_handler_block_by_data (GTK_OBJECT (file_list), panel);
	select_item (panel);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (file_list), panel);
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
	case GDK_BUTTON_PRESS:
		gtk_clist_unselect_row (GTK_CLIST (panel->list), row, 0);
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
			file_popup (event, panel, row, panel->dir.list[row].fname);
			break;
		}

		break;

	case GDK_2BUTTON_PRESS:
		gtk_clist_unselect_row (GTK_CLIST (panel->list), row, 0);
		if (event->button.button == 1)
			do_enter (panel);
		break;

	default:
		break;
	}
}

/* Figure out the number of visible lines in the panel */
static void
panel_file_list_compute_lines (GtkCList *file_list, WPanel *panel, int height)
{
	int lost_pixels = 0;
	
	if (GTK_WIDGET_VISIBLE (file_list->hscrollbar)){
		int scrollbar_width = GTK_WIDGET (file_list->hscrollbar)->requisition.width;
		int scrollbar_space = GTK_CLIST_CLASS (GTK_OBJECT (file_list)->klass)->scrollbar_spacing;

		lost_pixels = scrollbar_space + scrollbar_width;
	}
	panel->widget.lines = (height-lost_pixels) /
		(GTK_CLIST (file_list)->row_height + CELL_SPACING);
}

static void
panel_file_list_size_allocate_hook (GtkWidget *file_list, GtkAllocation *allocation, WPanel *panel)
{
	gtk_signal_handler_block_by_data (GTK_OBJECT (file_list), panel);
	panel_file_list_configure_contents (file_list, panel, allocation->width, allocation->height);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (file_list), panel);
	
	panel_file_list_compute_lines (GTK_CLIST (file_list), panel, allocation->height);
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

static void
panel_create_pixmaps (GtkWidget *parent)
{
	GdkColor color = gtk_widget_get_style (parent)->bg [GTK_STATE_NORMAL];

	pixmaps_ready = 1;
	icon_directory_pixmap = gdk_pixmap_create_from_xpm_d (parent->window, &icon_directory_mask, &color, directory_xpm);
	icon_link_pixmap      = gdk_pixmap_create_from_xpm_d (parent->window, &icon_link_mask,      &color, link_xpm);
	icon_dev_pixmap       = gdk_pixmap_create_from_xpm_d (parent->window, &icon_dev_mask,       &color, dev_xpm);
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
panel_configure_file_list (WPanel *panel, GtkWidget *file_list)
{
	format_e *format = panel->format;
	GtkCList *cl = GTK_CLIST (file_list);
	GtkObject *adjustment;
	int i;

	/* Set sorting callback */
	gtk_signal_connect (GTK_OBJECT (file_list), "click_column",
			    GTK_SIGNAL_FUNC (panel_file_list_column_callback), panel);

	/* Configure the CList */

	gtk_clist_set_selection_mode (cl, GTK_SELECTION_SINGLE);
	gtk_clist_set_policy (cl, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	for (i = 0, format = panel->format; format; format = format->next){
		GtkJustification just;

		if (!format->use_in_gui)
			continue;

		/* Set desired justification */
		if (format->just_mode == J_LEFT)
			just = GTK_JUSTIFY_LEFT;
		else
			just = GTK_JUSTIFY_RIGHT;
		
		gtk_clist_set_column_justification (cl, i, just);
		i++;
	}

	/* Configure the scrolbars */
	adjustment = GTK_OBJECT (gtk_range_get_adjustment (GTK_RANGE (cl->vscrollbar)));
	gtk_signal_connect_after (GTK_OBJECT(adjustment), "value_changed", 
				  GTK_SIGNAL_FUNC (panel_file_list_scrolled), panel);
}

static void *
panel_build_selected_file_list (WPanel *panel, int *file_list_len)
{
	if (panel->marked){
		char *data, *copy;
		int i, total_len = 0;
		int cwdlen = strlen (panel->cwd) + 1;
		
		/* first pass, compute the length */
		for (i = 0; i < panel->count; i++)
			if (panel->dir.list [i].f.marked)
				total_len += (cwdlen + panel->dir.list [i].fnamelen + 1);

		data = copy = xmalloc (total_len, "build_selected_file_list");
		for (i = 0; i < panel->count; i++)
			if (panel->dir.list [i].f.marked){
				strcpy (copy, panel->cwd);
				copy [cwdlen-1] = '/';
				strcpy (&copy [cwdlen], panel->dir.list [i].fname);
				copy += panel->dir.list [i].fnamelen + 1 + cwdlen;
			}
		*file_list_len = total_len;
		return data;
	} else {
		char *fullname = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
		
		*file_list_len = strlen (fullname) + 1;
		return fullname;
	}
}
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
		filename = concat_dir_and_file (panel->cwd, panel->dir.list [i].fname);
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

		if (S_ISDIR (panel->dir.list [row].buf.st_mode))
			drop_dir = concat_dir_and_file (panel->cwd, panel->dir.list [row].fname);
		else 
			drop_dir = panel->cwd;
	}
	drop_on_directory (data, drop_dir, 0);

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
	
	icons->last_clicked = -1;
	panel_drag_begin (widget, event, panel);
}

static void
panel_artificial_drag_start (GtkCList *window, GdkEventMotion *event)
{
	artificial_drag_start (window->clist_window, event->x, event->y);
}

static GtkWidget *
load_transparent_image (char *base)
{
	char *f = concat_dir_and_file (ICONDIR, base);
	GtkWidget *w;

	w = make_transparent_window (f);
	g_free (f);
	return w;
}

static void
load_dnd_icons (void)
{
	GdkPoint hotspot = { 5, 5 };

	if (!drag_directory)
		drag_directory = load_transparent_image ("not.png");
	
	if (!drag_directory_ok)
		drag_directory_ok = load_transparent_image ("directory.xpm");
	
	if (!drag_multiple)
		drag_multiple = load_transparent_image ("not.png");
	
	if (!drag_multiple_ok)
		drag_multiple_ok = load_transparent_image ("multi-ok.png");

	if (drag_directory && drag_directory_ok)
		gdk_dnd_set_drag_shape (drag_directory->window, &hotspot,
					drag_directory_ok->window, &hotspot);	
}

/*
 * Pixmaps can only be loaded once the window has been realized, so
 * this is why this hook is here
 */
static void
panel_realized (GtkWidget *file_list, WPanel *panel)
{
	GtkObject *obj = GTK_OBJECT (file_list);

	load_dnd_icons ();
	
	/* DND: Drag setup */
	gtk_signal_connect (obj, "drag_request_event", GTK_SIGNAL_FUNC (panel_clist_drag_request), panel);
	gtk_signal_connect (obj, "drag_begin_event",   GTK_SIGNAL_FUNC (panel_drag_begin), panel);

	gdk_window_dnd_drag_set (GTK_CLIST (file_list)->clist_window, TRUE, drag_types, ELEMENTS (drag_types));
 
	/* DND: Drop setup */
	gtk_signal_connect (obj, "drop_data_available_event", GTK_SIGNAL_FUNC (panel_clist_drop_data_available), panel);

	/* Artificial way of getting drag to start without leaving the widget boundary */
	gtk_signal_connect (obj, "motion_notify_event",
			    GTK_SIGNAL_FUNC (panel_artificial_drag_start), panel);
	gdk_window_dnd_drop_set (GTK_CLIST (file_list)->clist_window, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
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
	GtkCList  *clist;
	gchar     **titles;
	int       i;

	titles = g_new (char *, items);
	for (i = 0; i < items; format = format->next)
		if (format->use_in_gui)
			titles [i++] = format->title;

	file_list = gtk_blist_new_with_titles (items, titles);
	clist = GTK_CLIST (file_list);
	panel_configure_file_list (panel, file_list);
	free (titles);

	gtk_signal_connect_after (GTK_OBJECT (file_list), "size_allocate",
				  GTK_SIGNAL_FUNC (panel_file_list_size_allocate_hook),
				  panel);
	gtk_signal_connect (GTK_OBJECT (file_list), "realize",
			    GTK_SIGNAL_FUNC (panel_realized),
			    panel);

	gtk_signal_connect (GTK_OBJECT (file_list), "select_row",
			    GTK_SIGNAL_FUNC (panel_file_list_select_row),
			    panel);
	return file_list;
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
		if (event->button.button == 3){
			file_popup (event, panel, index, panel->dir.list [index].fname);
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
		  
static void
panel_icon_list_artificial_drag_start (GtkObject *obj, GdkEventMotion *event)
{
	GnomeIconList *ilist = GNOME_ICON_LIST (obj);

	artificial_drag_start (ilist->ilist_window, event->x, event->y);
}

/*
 * Icon view drag request handler
 */
static void
panel_icon_list_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel)
{
	GnomeIconList *ilist = GNOME_ICON_LIST (widget);
	char *data;
	int len;

	panel_drag_request (widget, event, panel, &len, &data);

	if (len && data){
		gdk_window_dnd_data_set (ilist->ilist_window, (GdkEvent *) event, data, len);
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
	
	gdk_window_get_origin (ilist->ilist_window, &winx, &winy);
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
	drop_on_directory (data, drop_dir, 0);

	if (drop_dir != panel->cwd)
		free (drop_dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
}

/*
 * Setup for the icon view
 */
static void
panel_icon_list_realized (GtkObject *obj, WPanel *panel)
{
	GnomeIconList *icon = GNOME_ICON_LIST (obj);
	
	load_imlib_icons ();
	load_dnd_icons ();

	/* DND: Drag setup */
	gtk_signal_connect (obj, "drag_request_event", GTK_SIGNAL_FUNC (panel_icon_list_drag_request), panel);
	gtk_signal_connect (obj, "drag_begin_event", GTK_SIGNAL_FUNC (panel_icon_list_drag_begin), panel);
	gdk_window_dnd_drag_set (icon->ilist_window, TRUE, drag_types, ELEMENTS (drag_types));
	
	/* DND: Drop setup */
	gtk_signal_connect (obj, "drop_data_available_event",
			    GTK_SIGNAL_FUNC (panel_icon_list_drop_data_available), panel);
	
	gtk_signal_connect (obj, "motion_notify_event",
			    GTK_SIGNAL_FUNC (panel_icon_list_artificial_drag_start), panel);
	gdk_window_dnd_drop_set (icon->ilist_window, TRUE, drop_types, ELEMENTS (drop_types), FALSE);
}

/*
 * Create and setup the icon field display
 */
static GtkWidget *
panel_create_icon_display (WPanel *panel)
{
	GnomeIconList *icon_field;

	icon_field = GNOME_ICON_LIST (gnome_icon_list_new ());
	gnome_icon_list_set_separators (icon_field, " /-_.");
	
	gnome_icon_list_set_selection_mode (icon_field, GTK_SELECTION_MULTIPLE);

	gtk_signal_connect (GTK_OBJECT (icon_field), "select_icon",
			    GTK_SIGNAL_FUNC (panel_icon_list_select_icon), panel);
	gtk_signal_connect (GTK_OBJECT (icon_field), "unselect_icon",
			    GTK_SIGNAL_FUNC (panel_icon_list_unselect_icon), panel);

	gtk_signal_connect (GTK_OBJECT (icon_field), "realize",
			    GTK_SIGNAL_FUNC (panel_icon_list_realized), panel);
	return GTK_WIDGET (icon_field);
}

void
panel_switch_new_display_mode (WPanel *panel)
{
	GtkWidget *old_list = panel->list;

	if (!old_list)
		return;
	
	panel->list = panel_create_file_list (panel);
	gtk_widget_destroy (old_list);
	gtk_table_attach (GTK_TABLE (panel->table), panel->list, 0, 1, 1, 2,
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

	if (panel->searching){
		char *str = copy_strings (N_("Search: "), panel->search_buffer, NULL);
		
		gtk_clip_label_set (label, str);
		free (str);
		return;
	}

	if (panel->marked){
		char buffer [120];
		
		sprintf (buffer, N_(" %s bytes in %d file%s"),
			 size_trunc_sep (panel->total), panel->marked,
			 panel->marked == 1 ? "" : "s");
		
		gtk_clip_label_set (label, buffer);
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
			gtk_clip_label_set (label, str);
			free (str);
		} else 
			gtk_clip_label_set (label, N_("<readlink failed>"));
		return;
	}

	if (panel->estimated_total){
		int  len = panel->estimated_total;
		char *buffer;

		buffer = xmalloc (len + 2, "display_mini_info");
		format_file (buffer, panel, panel->selected, panel->estimated_total-2, 0, 1);
		buffer [len] = 0;
		gtk_clip_label_set (label, buffer);
		free (buffer);
	}
	if (panel->list_type == list_icons){
		if (panel->marked == 0){
			gtk_clip_label_set (label, "");
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

	label = gtk_label_new (N_("Filter"));
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
	GtkWidget *status_line, *filter, *vbox;
	GtkWidget *frame, *cwd, *back_p, *fwd_p;
	GtkWidget *display;

	panel->xwindow = gtk_widget_get_toplevel (GTK_WIDGET (panel->widget.wdata));
	
	panel->table = gtk_table_new (2, 1, 0);

	panel->icons = panel_create_icon_display (panel);
	panel->list  = panel_create_file_list (panel);
	gtk_widget_ref (panel->icons);
	gtk_widget_ref (panel->list);

	if (panel->list_type == list_icons)
		display = panel->icons;
	else
		display = panel->list;

	filter = panel_create_filter (h, panel, &panel->filter_w);
	cwd = panel_create_cwd (h, panel, &panel->current_dir);
	
	/* We do not want the focus by default  (and the previos add_widget just gave it to us) */
	h->current = h->current->prev;

	/* buttons */
	back_p = gnome_stock_pixmap_widget_new (panel->xwindow, GNOME_STOCK_MENU_BACK);
	fwd_p  = gnome_stock_pixmap_widget_new (panel->xwindow, GNOME_STOCK_MENU_FORWARD);
	
	panel->up_b    = gtk_button_new_with_label ("up");
	panel->back_b   = gtk_button_new ();
	panel->fwd_b    = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (panel->back_b), back_p);
	gtk_container_add (GTK_CONTAINER (panel->fwd_b), fwd_p);
	gtk_signal_connect (GTK_OBJECT (panel->back_b), "clicked", GTK_SIGNAL_FUNC(panel_back), panel);
	gtk_signal_connect (GTK_OBJECT (panel->fwd_b), "clicked", GTK_SIGNAL_FUNC(panel_fwd), panel);
	gtk_signal_connect (GTK_OBJECT (panel->up_b), "clicked", GTK_SIGNAL_FUNC(panel_up), panel);
	panel_update_marks (panel);
	
	/* ministatus */
	panel->ministatus = gtk_clip_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (panel->ministatus), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (panel->ministatus), 3, 0);
	gtk_widget_show (panel->ministatus);
	
	status_line = gtk_hbox_new (0, 0);
	gtk_container_border_width (GTK_CONTAINER (status_line), 3);
	
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
	
	/* The statusbar */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_border_width (GTK_CONTAINER (frame), 3);

	panel->status = gtk_clip_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (panel->status), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (panel->status), 3, 0);
	gtk_container_add (GTK_CONTAINER (frame), panel->status);
	gtk_label_set_justify (GTK_LABEL (panel->status), GTK_JUSTIFY_LEFT);
	gtk_widget_show_all (frame);

	/* Add both the icon view and the listing view */
	gtk_table_attach (GTK_TABLE (panel->table), panel->icons, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_table_attach (GTK_TABLE (panel->table), panel->list, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	gtk_widget_show (display);
	
	gtk_table_attach (GTK_TABLE (panel->table), status_line, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_table_attach (GTK_TABLE (panel->table), panel->ministatus, 0, 1, 2, 3,
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

	/* This is a bug workaround for the icon list, as the icon */
	gtk_widget_queue_resize (panel->icons);
			     
	if (!pixmaps_ready){
		if (!GTK_WIDGET_REALIZED (panel->list))
			gtk_widget_realize (panel->list);
		panel_create_pixmaps (panel->list);
	}

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
		if (panel->icons)
			gtk_widget_show (panel->icons);
		if (panel->list)
			gtk_widget_hide (panel->list);
	} else {
		panel_switch_new_display_mode (panel);
		if (panel->list)
			gtk_widget_show (panel->list);
		if (panel->icons)
			gtk_widget_hide (panel->icons);
	}
}

/* Releases all of the X resources allocated */
void
x_panel_destroy (WPanel *panel)
{
	gtk_widget_destroy (GTK_WIDGET (panel->xwindow));
}
