/*
 * The GNOME file viewer frontend
 * (C) The Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include "x.h"
#include "gmc-chargrid.h"
#include "dlg.h"
#define WANT_WIDGETS /* bleah */
#include "view.h"

enum {
	CONTEXT_FILENAME,
	CONTEXT_POSITION,
	CONTEXT_BYTES,
	CONTEXT_GROW,
	CONTEXT_PERCENT
};

void
x_init_view (WView *view)
{
	view->current_x = view->current_y = 0;
	view->sadj = 0;
}

void
x_destroy_view (WView *view)
{
	gtk_widget_destroy (GTK_WIDGET (view->widget.wdata));
}

static void
viewer_size_changed (GtkWidget *widget, guint cols, guint lines, WView *view)
{
	widget_set_size (&view->widget, 0, 0, lines, cols);
	view_update_bytes_per_line (view);
	dlg_redraw (view->widget.parent);
}

void
x_create_viewer (WView *view)
{
	GtkWidget *viewer;
	guint lines, cols;
	
	viewer = gmc_char_grid_new ();
	view->widget.wdata = (widget_data) viewer;
	gtk_signal_connect (GTK_OBJECT (viewer), "size_changed",
			    GTK_SIGNAL_FUNC (viewer_size_changed),
			    view);
	gtk_widget_show (viewer);
	gmc_char_grid_get_size (GMC_CHAR_GRID (viewer), &cols, &lines);
	widget_set_size (&view->widget, 0, 0, lines, cols);
}

void
x_focus_view (WView *view)
{
}

static void
scrollbar_moved (GtkAdjustment *adj, WView *view)
{
	if (adj->value == view->start_display)
		return;
	
	if (adj->value < view->start_display){
		while (adj->value < view->start_display)
			view_move_backward (view, 1);
	} else {
		while (adj->value > view->start_display)
			view_move_forward (view, 1);
	}

	/* Update the ajd->value */
	gtk_signal_handler_block_by_func (
		GTK_OBJECT (view->sadj),
		GTK_SIGNAL_FUNC (scrollbar_moved),
		view);

	gtk_adjustment_set_value (adj, view->start_display);
	
	gtk_signal_handler_unblock_by_func (
		GTK_OBJECT (view->sadj),
		GTK_SIGNAL_FUNC (scrollbar_moved),
		view);
	
	/* To force a display */
	view->dirty = max_dirt_limit + 1;
	view_update (view, 0);
}

void
view_percent (WView *view, int p, int w, gboolean update_gui)
{
    int percent;
    char buffer [40];

    percent = (view->s.st_size == 0 || view->last_byte == view->last) ? 100 :
        (p > (INT_MAX/100) ?
         p / (view->s.st_size / 100) :
	 p * 100 / view->s.st_size);

    g_snprintf (buffer, sizeof (buffer), "%3d%%", percent);
    if (strcmp (buffer, GTK_LABEL (view->gtk_percent)->label))
	    gtk_label_set (GTK_LABEL (view->gtk_percent), buffer);

    if (!update_gui)
	    return;
    
    if (view->sadj){
	    GtkAdjustment *adj = GTK_ADJUSTMENT (view->sadj);
	    
	    if ((int) adj->upper != view->last_byte){
		    adj->upper = view->last_byte;
		    adj->step_increment = 1.0;
		    adj->page_increment = 
			    adj->page_size = view->last - view->start_display;
		    gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
	    }
	    if ((int) adj->value != view->start_display){
		    gtk_adjustment_set_value (adj, view->start_display);
	    }
    }
}

void
view_status (WView *view, gboolean update_gui)
{
	char buffer [80];
	
	if (view->hex_mode)
		g_snprintf (buffer, sizeof (buffer), _("Offset 0x%08lx"), view->edit_cursor);
	else
		g_snprintf (buffer, sizeof (buffer), _("Col %d"), -view->start_col);
	if (strcmp (buffer, GTK_LABEL (view->gtk_offset)->label))
		gtk_label_set (GTK_LABEL (view->gtk_offset), buffer);

	g_snprintf (buffer, sizeof (buffer), _("%s bytes"), size_trunc (view->s.st_size));
	if (strcmp (buffer, GTK_LABEL (view->gtk_bytes)->label))
		gtk_label_set (GTK_LABEL (view->gtk_bytes), buffer);

	if (view->hex_mode)
		view_percent (view, view->edit_cursor - view->first, 0, update_gui);
	else
		view_percent (view, view->start_display - view->first, 0, update_gui);
}

void
view_add_character (WView *view, int c)
{
	gmc_char_grid_put_char ((GmcCharGrid *) (view->widget.wdata),
				view->current_x, view->current_y,
				gmc_color_pairs [view->color].fore,
				gmc_color_pairs [view->color].back, c);
}

void
view_add_one_vline ()
{
}

void
view_add_string (WView *view, char *s)
{
	gmc_char_grid_put_string ((GmcCharGrid *)(view->widget.wdata),
				  view->current_x, view->current_y,
				  gmc_color_pairs [view->color].fore,
				  gmc_color_pairs [view->color].back, s);
}

void
view_gotoyx (WView *view, int r, int c)
{
	view->current_y = r;
	view->current_x = c;
}

void
view_set_color (WView *view, int font)
{
	view->color = font;
}

void
view_freeze (WView *view)
{
	gmc_char_grid_freeze (GMC_CHAR_GRID (view->widget.wdata));
}

void
view_thaw (WView *view)
{
	gmc_char_grid_thaw (GMC_CHAR_GRID (view->widget.wdata));
}

void
view_display_clean (WView *view, int h, int w)
{
	gmc_char_grid_clear (GMC_CHAR_GRID (view->widget.wdata), 0, 0, w, h, NULL);
}

static int
gnome_view_callback (struct Dlg_head *h, int id, int msg)
{
    return default_dlg_callback (h, id, msg);
}

static GtkWidget *
prepare_status (GtkWidget *s)
{
	GtkWidget *frame, *label;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	label = gtk_label_new ("");
	gtk_container_add (GTK_CONTAINER (frame), label);
	gtk_box_pack_start_defaults (GTK_BOX (s), frame);
	
	return label;
}

static GtkWidget *
gview_status (WView *view)
{
	GtkWidget *s;

	s = gtk_hbox_new (0, 0);
	view->gtk_fname   = prepare_status (s);
	
	view->gtk_offset  = prepare_status (s);
	view->gtk_bytes   = prepare_status (s);
	if (view->growing_buffer){
		view->gtk_flags   = prepare_status (s);
		gtk_label_set (GTK_LABEL (view->gtk_flags), "[grow]");
	}
	view->gtk_percent = prepare_status (s);

	gtk_label_set (GTK_LABEL (view->gtk_fname),
		       view->filename ? view->filename : view->command ? view->command : "");
	
	GTK_BOX (s)->spacing = 2;
	
	return s;
}

static void
gview_quit (GtkWidget *widget, WView *view)
{
	dlg_run_done (view->widget.parent);
	destroy_dlg (view->widget.parent);
}

static void
gnome_normal_search_cmd (GtkWidget *widget, WView *view)
{
	normal_search_cmd (view);
}

static void
gnome_regexp_search_cmd (GtkWidget *widget, WView *view)
{
	regexp_search_cmd (view);
}

static void
gnome_continue_search (GtkWidget *widget, WView *view)
{
	continue_search (view);
}

static void
gnome_goto_line (GtkWidget *widget, WView *view)
{
	goto_line (view);
}

static void
gnome_toggle_wrap (GtkWidget *widget, WView *view)
{
	toggle_wrap_mode (view);
}

static void
gnome_toggle_format (GtkWidget *widget, WView *view)
{
	change_nroff (view);
}

static void
gnome_toggle_hex (GtkWidget *widget, WView *view)
{
	toggle_hex_mode (view);
}

static void
gnome_monitor (GtkWidget *widget, WView *view)
{
	set_monitor (view, 1);
}

GnomeUIInfo gview_file_menu [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("_Goto line"),
				N_("Jump to a specified line number"),
				&gnome_goto_line, GNOME_STOCK_PIXMAP_JUMP_TO),
	GNOMEUIINFO_ITEM (N_("_Monitor file"), N_("Monitor file growing"),            &gnome_monitor, NULL),
	GNOMEUIINFO_MENU_CLOSE_ITEM(gview_quit, NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo gview_search_menu [] = {
	GNOMEUIINFO_MENU_FIND_ITEM(gnome_normal_search_cmd, NULL),
	GNOMEUIINFO_ITEM_STOCK (N_("Regexp search"),
				N_("Regular expression search"),
				gnome_regexp_search_cmd, GNOME_STOCK_MENU_SEARCH),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM(gnome_continue_search, NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo gview_mode_menu [] = {
#define WRAP_POS 0
	GNOMEUIINFO_TOGGLEITEM (N_("_Wrap"),
				N_("Wrap the text"), gnome_toggle_wrap, NULL),
#if 0
	/* Can not use this one yet, as it destroys the viewer, need to fix that */
	GNOMEUIINFO_TOGGLEITEM (N_("_Parsed view"), NULL, gnome_toggle_parse, NULL),
#endif
#define FORMAT_POS 1
	GNOMEUIINFO_TOGGLEITEM (N_("_Formatted"),   NULL, gnome_toggle_format, NULL),
#define HEX_POS 2
	GNOMEUIINFO_TOGGLEITEM (N_("_Hex"),         NULL, gnome_toggle_hex, NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo gview_top_menu [] = {
        GNOMEUIINFO_MENU_FILE_TREE( &gview_file_menu ),
	GNOMEUIINFO_SUBTREE (N_ ("_Search"), &gview_search_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE( &gview_mode_menu),
        GNOMEUIINFO_END
};

static int
quit_view (GtkWidget *widget, GdkEvent *event, WView *view)
{
	gview_quit (widget, view);
	return TRUE;
}
	   
int
view (char *_command, char *_file, int *move_dir_p, int start_line)
{
	Dlg_head   *our_dlg;
	GtkWidget  *toplevel, *status, *scrollbar, *hbox;
	GtkVBox    *vbox;
	WView      *wview;
	WButtonBar *bar;
	int        midnight_colors [4];
	int        error;
	
	/* Create dialog and widgets, put them on the dialog */
	our_dlg = create_dlg (0, 0, 0, 0, midnight_colors,
			      gnome_view_callback, "[Internal File Viewer]",
			      "view",
			      DLG_NO_TED | DLG_GNOME_APP);
	
	toplevel = GTK_WIDGET (our_dlg->wdata);
	vbox   = GTK_VBOX (gtk_vbox_new (0, 0));
	
	gtk_window_set_policy (GTK_WINDOW (toplevel), TRUE, TRUE, TRUE);
	gnome_app_set_contents (GNOME_APP (toplevel), GTK_WIDGET (vbox));

	gtk_window_set_title (GTK_WINDOW (toplevel),
			      _command ? _command : _file);
	wview = view_new (0, 0, 80, 25, 0);

	bar = buttonbar_new (1);
	
	add_widget (our_dlg, wview);
	add_widget (our_dlg, bar);
	
	error = view_init (wview, _command, _file, start_line);
	if (move_dir_p)
		*move_dir_p = 0;

	/* Please note that if you add another widget,
	 * you have to modify view_adjust_size to
	 * be aware of it
	 */
	if (error)
		return !error;

	status = gview_status (wview);
	gnome_app_create_menus_with_data (GNOME_APP (toplevel), gview_top_menu, wview);

	/* Setup the menus checkboxes correctly */
	GTK_CHECK_MENU_ITEM (gview_mode_menu [WRAP_POS].widget)->active = wview->wrap_mode;
	GTK_CHECK_MENU_ITEM (gview_mode_menu [FORMAT_POS].widget)->active = wview->viewer_nroff_flag;
	GTK_CHECK_MENU_ITEM (gview_mode_menu [HEX_POS].widget)->active = wview->hex_mode;
		
	init_dlg (our_dlg);
	
	gtk_box_pack_start (GTK_BOX (vbox), status, 0, 1, 0);

	wview->sadj = gtk_adjustment_new (0.0, 0.0, 1000000.0, 1.0, 25.0, 25.0);
	scrollbar = gtk_vscrollbar_new (wview->sadj);
	gtk_signal_connect (GTK_OBJECT (wview->sadj), "value_changed",
			    GTK_SIGNAL_FUNC(scrollbar_moved), wview);

	gtk_signal_connect (GTK_OBJECT (toplevel), "delete_event",
			    GTK_SIGNAL_FUNC (quit_view), wview);
	
	hbox = gtk_hbox_new (0, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, 1, 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (wview->widget.wdata), 1, 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scrollbar), 0, 1, 0);
	
	gtk_widget_show_all (toplevel);
	
	return 1;
}

