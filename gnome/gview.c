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
#define WANT_WIDGETS
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

void
view_percent (WView *view, int p, int w)
{
    int percent;
    char buffer [40];
    
    percent = (view->s.st_size == 0 || view->last_byte == view->last) ? 100 :
        (p > (INT_MAX/100) ?
         p / (view->s.st_size / 100) :
	 p * 100 / view->s.st_size);

    sprintf (buffer, "%3d%%", percent);
    gtk_label_set (GTK_LABEL (view->gtk_percent), buffer);
}

void
view_status (WView *view)
{
	char buffer [80];

	if (view->hex_mode)
		sprintf (buffer, "Offset 0x$08x", view->edit_cursor);
	else
		sprintf (buffer, "Col %d", -view->start_col);
	gtk_label_set (GTK_LABEL (view->gtk_offset), buffer);

	sprintf (buffer, "%s bytes", size_trunc (view->s.st_size));
	gtk_label_set (GTK_LABEL (view->gtk_bytes), buffer);

	gtk_label_set (GTK_LABEL (view->gtk_flags), view->growing_buffer ? "[grow]" : "");
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
my_test (void)
{
	GtkWidget *frame;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (frame), gtk_label_new ("Hello Wordl!"));
	return frame;
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
	view->gtk_flags   = prepare_status (s);
	view->gtk_percent = prepare_status (s);

	gtk_label_set (GTK_LABEL (view->gtk_fname),
		       view->filename ? view->filename : view->command ? view->command : "");
	
	GTK_BOX (s)->spacing = 2;
	
	return s;
}

int
view (char *_command, char *_file, int *move_dir_p, int start_line)
{
	Dlg_head  *our_dlg;
	GtkWidget *toplevel, *status;
	GtkVBox   *vbox;
	WView *wview;
	int   midnight_colors [4];
	int   error;
	
	/* Create dialog and widgets, put them on the dialog */
	our_dlg = create_dlg (0, 0, 0, 0, midnight_colors,
			      gnome_view_callback, "[Internal File Viewer]",
			      "view",
			      DLG_NO_TED);
	
	toplevel = GTK_WIDGET (our_dlg->wdata);
	vbox   = GTK_VBOX (gtk_vbox_new (0, 0));
	
	gtk_window_set_policy (GTK_WINDOW (toplevel), TRUE, TRUE, TRUE);
	gtk_container_add (GTK_CONTAINER (toplevel), GTK_WIDGET (vbox));
	
	gtk_window_set_title (GTK_WINDOW (toplevel),
			      _command ? _command : _file);
	wview = view_new (0, 0, 80, 25, 0);
	
	add_widget (our_dlg, wview);
	
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
	
	init_dlg (our_dlg);
	gtk_box_pack_start (GTK_BOX (vbox), status, 0, 1, 0);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (wview->widget.wdata), 1, 1, 0);
	gtk_widget_show_all (toplevel);
	
	return 1;
}

