/* GNU Midnight Commander -- GNOME edition
 *
 * Directory display routines
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Author: Miguel de Icaza
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

#include "directory.xpm"

/* This is used to initialize our pixmaps */
static int pixmaps_ready;
GdkPixmap *directory_pixmap;
GdkBitmap *directory_mask;

/* button bindings, mc text mode (1) or gui-like  */
static int mc_bindings;

void
repaint_file (WPanel *panel, int file_index, int move, int attr, int isstatus)
{
}

void
show_dir (WPanel *panel)
{
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;
	GList *list;
	char  *string;

	list = g_list_alloc ();
	g_list_append (list, panel->cwd);

	printf ("show_dir to %s %s\n", panel->cwd, panel->filter ? panel->filter : "<no-filt>");
}

static void
panel_file_list_set_type_bitmap (GtkCList *cl, int row, int column, int color, file_entry *fe)
{
	/* Here, add more icons */
	switch (color){
	case DIRECTORY_COLOR:
		gtk_clist_set_pixmap (cl, row, column, directory_pixmap, directory_mask);
		break;
	}
}

static void
panel_file_list_set_row_colors (GtkCList *cl, int row, int color_pair)
{
	if (gmc_color_pairs [color_pair].fore)
		gtk_clist_set_foreground (cl, row, gmc_color_pairs [color_pair].fore);
	if (gmc_color_pairs [color_pair].back)
		gtk_clist_set_background (cl, row, gmc_color_pairs [color_pair].back);
}

void
x_fill_panel (WPanel *panel)
{
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;
	const int top       = panel->count;
	const int items     = panel->format->items;
	GtkCList *cl        = GTK_CLIST (g->list);
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
			char *text;

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
		
		color = file_entry_color (fe);
		panel_file_list_set_row_colors (cl, i, color);
		if (type_col != -1)
			panel_file_list_set_type_bitmap (cl, i, type_col, color, fe);
	}
	gtk_clist_thaw (GTK_CLIST (cl));
	free (texts);
}

void
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

void
x_panel_select_item (WPanel *panel, int index, int value)
{
	/* Not required */
}

void
x_select_item (WPanel *panel)
{
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;
	GtkCList *clist = GTK_CLIST (g->list);
	
	gtk_clist_select_row (clist, panel->selected, 0);
	
	if (!gtk_clist_row_isvisable (clist, panel->selected)){
		gtk_clist_moveto (clist, panel->selected, 0, 0.5, 0.0);
	}
}

void
x_unselect_item (WPanel *panel)
{
#if 0
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;
	gtk_clist_unselect_row (GTK_CLIST (g->list), panel->selected, 0);
#endif
}

void
x_filter_changed (WPanel *panel)
{
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;

	if (panel->filter){
		char *string;
		
		string = g_copy_strings ("Filter: ", panel->filter, NULL);
		gtk_label_set (GTK_LABEL (g->filter), string);
		g_free (string);
	} else
		gtk_label_set (GTK_LABEL (g->filter), "No filter");
}

void
x_adjust_top_file (WPanel *panel)
{
	g_panel_contents *g = (g_panel_contents *) panel->widget.wdata;

	gtk_clist_moveto (GTK_CLIST (g->list), panel->top_file, 0, 0.0, 0.0);
	panel->top_file;
}

#define COLUMN_INSET 3
#define CELL_SPACING 1

GtkWidget
panel_file_list_configure_contents (GtkWidget *file_list, WPanel *panel, int main_width, int height)
{
	format_e *format = panel->format;
	int i, used_columns, expandables, items;
	int char_width, usable_pixels, extra_pixels, width;
	int total_columns, extra_columns;
	int expand_space, extra_space, shrink_space;
	int lost_pixels;
	
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
internal_select_item (GtkWidget *file_list, WPanel *panel, int row)
{
	if (panel->selected == row)
		return;
	
	gtk_signal_handler_block_by_data (GTK_OBJECT (file_list), panel);
	unselect_item (panel);
	panel->selected = row;
	select_item (panel);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (file_list), panel);
}

void
panel_file_list_select_row (GtkWidget *file_list, int row, int column, GdkEvent *event, WPanel *panel)
{
	if (!event){
		internal_select_item (file_list, panel, row);
		return;
	}
	
	if (event->type == GDK_BUTTON_PRESS)
		internal_select_item (file_list, panel, row);

	if (event->type == GDK_2BUTTON_PRESS){
		do_enter (panel);
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
	panel_file_list_configure_contents (file_list, panel, allocation->width, allocation->height);
	
	/* Set the selection callback */
	gtk_signal_connect (GTK_OBJECT (file_list), "select_row",
			    GTK_SIGNAL_FUNC (panel_file_list_select_row), panel);

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

void
panel_create_pixmaps (GtkWidget *parent)
{
	GdkColor color = gtk_widget_get_style (parent)->bg [GTK_STATE_NORMAL];

	pixmaps_ready = 1;
	directory_pixmap = gdk_pixmap_create_from_xpm_d (parent->window, &directory_mask, &color, directory_xpm);
}

static void
panel_file_list_scrolled (GtkAdjustment *adj, WPanel *panel)
{
	if (!GTK_IS_ADJUSTMENT (adj)){
		fprintf (stderr, "CRAP!\n");
		exit (1);
	}
}

void
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
	gtk_clist_set_selection_mode (cl, GTK_SELECTION_BROWSE);
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

void
panel_list_mc_bindings (GtkWidget *file_list, int row, int col, GdkEvent *event, WPanel *panel)
{
}

void
panel_action_open (GtkWidget *widget, WPanel *panel)
{
	do_enter (panel);
}

void
panel_action_open_with (GtkWidget *widget, WPanel *panel)
{
	char *command;
	
	command = input_expand_dialog (" Open with...", "Enter extra arguments:", panel->dir.list [panel->selected].fname);
	execute (command);
	free (command);
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
panel_action_copy (GtkWidget *widget, WPanel *panel)
{
}

void
panel_action_rename (GtkWidget *widget, WPanel *panel)
{
}

void
panel_action_delete (GtkWidget *widget, WPanel *panel)
{
}

typedef void (*context_menu_callback)(GtkWidget *, WPanel *);
	
struct {
	char *text;
	context_menu_callback callback;
} file_actions [] = {
	{ "Info",            NULL },
	{ "",                NULL },
	{ "Open",            panel_action_open },
	{ "Open with",       panel_action_open_with },
	{ "View",            panel_action_view },
	{ "View unfiltered", panel_action_view_unfiltered },  
	{ "",                NULL },
	{ "Copy...",         (context_menu_callback) copy_cmd },
	{ "Rename/move..",   (context_menu_callback) ren_cmd },
	{ "Delete...",       (context_menu_callback) delete_cmd },
	{ NULL, NULL },
};
	
GtkWidget *
create_popup_submenu (WPanel *panel, char *filename)
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

		if (*file_actions [i].text)
			item = gtk_menu_item_new_with_label (file_actions [i].text);
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
	int c;
	
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
file_popup (GdkEvent *event, WPanel *panel, char *filename)
{
	GtkWidget *menu = gtk_menu_new ();
	GtkWidget *submenu;
	GtkWidget *item;
	
	item = gtk_menu_item_new_with_label (filename);
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);

	submenu = create_popup_submenu (panel, filename);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	
	file_popup_add_context (GTK_MENU (menu), panel, filename);
	
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, event->button.time);
	gtk_widget_show (menu);
}

void
panel_list_new_bindings (GtkWidget *file_list, int row, int col, GdkEvent *event, WPanel *panel)
{
	char *filename = panel->dir.list [row].fname;

	unselect_item (panel);
	panel->selected = row;
	select_item (panel);
	
	if (event->button.button == 3){
		file_popup (event, panel, filename);
	}
}

void
panel_file_list_row_selected (GtkWidget *file_list, int row, int col, GdkEvent *event, WPanel *panel)
{
	if (!event)
		return;

	if (mc_bindings)
		panel_list_mc_bindings (file_list, row, col, event, panel);
	else
		panel_list_new_bindings (file_list, row, col, event, panel);
}

GtkWidget *
panel_create_file_list (WPanel *panel)
{
	const int items = panel->format->items;
	format_e  *format = panel->format;
	GtkWidget *file_list;
	gchar     **titles;
	int       i;

	titles = g_new (char *, items);
	for (i = 0; i < items; format = format->next)
		if (format->use_in_gui)
			titles [i++] = format->title;

	file_list = gtk_clist_new_with_titles (items, titles);
	panel_configure_file_list (panel, file_list);
	free (titles);
	
	gtk_signal_connect (GTK_OBJECT (file_list),
			    "size_allocate",
			    GTK_SIGNAL_FUNC (panel_file_list_size_allocate_hook),
			    panel);

	gtk_signal_connect (GTK_OBJECT (file_list),
			    "select_row",
			    GTK_SIGNAL_FUNC (panel_file_list_row_selected),
			    panel);

	return file_list;
}

GtkWidget *
panel_create_cwd (WPanel *panel)
{
	GtkWidget *option_menu;

	option_menu = gtk_combo_new ();
	
	return option_menu;
}

void
panel_change_filter (GtkWidget *button, WPanel *panel)
{
	fprintf (stderr, "Change filter: not yet hooked\n");
}

GtkWidget *
panel_create_filter (WPanel *panel, GtkWidget **label)
{
	GtkWidget *filter;

	*label = gtk_label_new ("");
	gtk_widget_show (*label);
	filter = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (filter), *label);
	gtk_signal_connect (GTK_OBJECT (filter), "clicked", GTK_SIGNAL_FUNC (panel_change_filter), panel);
	return filter;
}

void
x_create_panel (Dlg_head *h, widget_data parent, WPanel *panel)
{
	g_panel_contents *g = g_new (g_panel_contents, 1);
	GtkWidget *table, *status_line, *filter_w, *statusbar;

	g->table = gtk_table_new (2, 1, 0);
	gtk_widget_show (g->table);
	
	g->list  = panel_create_file_list (panel);
	gtk_widget_show (g->list);

	g->current_dir = panel_create_cwd (panel);
	gtk_widget_show (g->current_dir);

	filter_w = panel_create_filter (panel, &g->filter);
	gtk_widget_show (filter_w);

	status_line = gtk_hbox_new (0, 0);
	gtk_widget_show (status_line);
	
	gtk_box_pack_start (GTK_BOX (status_line), g->current_dir, 0, 0, 0);
	gtk_box_pack_end   (GTK_BOX (status_line), filter_w, 0, 0, 0);

	g->status = statusbar = gtk_label_new ("");
	gtk_widget_show (statusbar);
	
	gtk_table_attach (GTK_TABLE (g->table), g->list, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0);
	
	gtk_table_attach (GTK_TABLE (g->table), status_line, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_table_attach (GTK_TABLE (g->table), statusbar, 0, 1, 2, 3,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0, 0, 0);
	
	gtk_widget_show (g->table);
	panel->widget.wdata = (widget_data) g;

	/* Now, insert our table in our parent */
	gtk_container_add (GTK_CONTAINER (gtk_object_get_data (GTK_OBJECT (h->wdata), "parent-container")),
			   g->table);
	
	if (!pixmaps_ready){
		if (!GTK_WIDGET_REALIZED (g->list))
			gtk_widget_realize (g->list);
		panel_create_pixmaps (g->list);
	}
}

void
panel_update_cols (Widget *panel, int frame_size)
{
	panel->cols  = 60;
	panel->lines = 20;
}

char *get_nth_panel_name (int num)
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
		set_hintbar ("The GNOME Midnight Commander " VERSION " (C) 1995-1998 the FSF");
	
}

void
paint_frame (WPanel *panel)
{
}
