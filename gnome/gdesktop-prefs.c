/* Desktop preferences box for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <gnome.h>
#include "gdesktop-prefs.h"
#include "gdesktop.h"


/* Size of the icon position box */
#define ICON_POS_WIDTH 120
#define ICON_POS_HEIGHT 90
#define ICON_POS_INTERVAL 10


struct point {
	int x, y;
};

/* Structure to hold preferences information */
struct _GDesktopPrefs {
	/* Property box we are attached to */
	GnomePropertyBox *pbox;

	/* Canvas for the miniature icons */
	GtkWidget *canvas;

	/* Group that holds the miniature icons */
	GnomeCanvasItem *group;

	/* Icon position flags */
	guint right_to_left : 1;
	guint bottom_to_top : 1;
	guint rows_not_columns : 1;

	/* Icon positioning options */
	GtkWidget *auto_placement;
	GtkWidget *snap_icons;

	/* Icon shape options */
	GtkWidget *use_shaped_icons;
	GtkWidget *use_shaped_text;
};


/* Creates the canvas items that represent a mini-icon */
static void
create_mini_icon (GnomeCanvasGroup *group, int x, int y, guint color)
{
	gnome_canvas_item_new (group,
			       gnome_canvas_rect_get_type (),
			       "x1", (double) (x - 1),
			       "y1", (double) (y - 2),
			       "x2", (double) (x + 1),
			       "y2", (double) y,
			       "fill_color_rgba", color,
			       NULL);

	gnome_canvas_item_new (group,
			       gnome_canvas_rect_get_type (),
			       "x1", (double) (x - 2),
			       "y1", (double) (y + 2),
			       "x2", (double) (x + 2),
			       "y2", (double) (y + 3),
			       "fill_color_rgba", color,
			       NULL);
}

/* Re-create the icon position mini-icons */
static void
create_mini_icons (GDesktopPrefs *dp, int r2l, int b2t, int rows)
{
	GtkStyle *style;
	GdkColor *c;
	guint color;
	int i, j;
	int max_j;
	int x, y;
	int dx, dy;
	int ox, oy;

	dp->right_to_left = r2l ? TRUE : FALSE;
	dp->bottom_to_top = b2t ? TRUE : FALSE;
	dp->rows_not_columns = rows ? TRUE : FALSE;

	/* Compute color for mini icons */

	style = gtk_widget_get_style (GTK_WIDGET (dp->canvas));
	c = &style->fg[GTK_STATE_NORMAL];
	color = ((c->red & 0xff00) << 8) | (c->green & 0xff00) | (c->blue >> 8) | 0xff;

	if (dp->group)
		gtk_object_destroy (GTK_OBJECT (dp->group));

	dp->group = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (dp->canvas)),
					   gnome_canvas_group_get_type (),
					   NULL);

	if (dp->right_to_left) {
		ox = ICON_POS_WIDTH - ICON_POS_INTERVAL;
		dx = -ICON_POS_INTERVAL;
	} else {
		ox = ICON_POS_INTERVAL;
		dx = ICON_POS_INTERVAL;
	}

	if (dp->bottom_to_top) {
		oy = ICON_POS_HEIGHT - ICON_POS_INTERVAL;
		dy = -ICON_POS_INTERVAL;
	} else {
		oy = ICON_POS_INTERVAL;
		dy = ICON_POS_INTERVAL;
	}

	if (dp->rows_not_columns)
		y = oy;
	else
		x = ox;

	for (i = 0; i < 2; i++) {
		if (dp->rows_not_columns) {
			x = ox;
			max_j = (i == 1) ? (ICON_POS_WIDTH / 2) : ICON_POS_WIDTH;
		} else {
			y = oy;
			max_j = (i == 1) ? (ICON_POS_HEIGHT / 2) : ICON_POS_HEIGHT;
		}

		for (j = ICON_POS_INTERVAL; j < max_j; j += ICON_POS_INTERVAL) {
			create_mini_icon (GNOME_CANVAS_GROUP (dp->group), x, y, color);

			if (dp->rows_not_columns)
				x += dx;
			else
				y += dy;
		}

		if (dp->rows_not_columns)
			y += dy;
		else
			x += dx;
	}
}

/* Handler for button presses on the icon position canvas */
static gint
button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GDesktopPrefs *dp;
	int r2l;
	int b2t;
	int dx, dy;
	int rows;

	dp = data;

	if (!(event->type == GDK_BUTTON_PRESS && event->button == 1))
		return FALSE;

	if (event->x > ICON_POS_WIDTH / 2) {
		r2l = TRUE;
		dx = ICON_POS_WIDTH - event->x;
	} else {
		r2l = FALSE;
		dx = event->x;
	}

	if (event->y > ICON_POS_HEIGHT / 2) {
		b2t = TRUE;
		dy = ICON_POS_HEIGHT - event->y;
	} else {
		b2t = FALSE;
		dy = event->y;
	}

	if (dx < dy)
		rows = FALSE;
	else
		rows = TRUE;

	create_mini_icons (dp, r2l, b2t, rows);
	gnome_property_box_changed (dp->pbox);
	return TRUE;
}

/* Creates the canvas widget and items to configure icon position */
static GtkWidget *
create_icon_pos (GDesktopPrefs *dp)
{
	dp->canvas = gnome_canvas_new ();
	gtk_widget_set_usize (dp->canvas, ICON_POS_WIDTH, ICON_POS_HEIGHT);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (dp->canvas),
					0.0, 0.0, ICON_POS_WIDTH, ICON_POS_HEIGHT);

	gtk_signal_connect (GTK_OBJECT (dp->canvas), "button_press_event",
			    GTK_SIGNAL_FUNC (button_press),
			    dp);

	create_mini_icons (dp, desktop_arr_r2l, desktop_arr_b2t, desktop_arr_rows);

	return dp->canvas;
}

/* Callback used to notify a property box when a toggle button is toggled */
static void
toggled (GtkWidget *w, gpointer data)
{
	gnome_property_box_changed (GNOME_PROPERTY_BOX (data));
}

/* Creates a check box that will notify a property box when it changes */
static GtkWidget *
create_check_box (char *text, int state, GnomePropertyBox *pbox)
{
	GtkWidget *w;

	w = gtk_check_button_new_with_label (text);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), state);
	gtk_signal_connect (GTK_OBJECT (w), "toggled",
			    GTK_SIGNAL_FUNC (toggled),
			    pbox);

	return w;
}

/* Creates the widgets that are used to configure icon position and snapping */
static GtkWidget *
create_position_widgets (GDesktopPrefs *dp)
{
	GtkWidget *vbox;
	GtkWidget *w;
	GtkWidget *frame;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

	/* Icon position */

	w = gtk_label_new (_("Icon position"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	w = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (w), frame);

	w = create_icon_pos (dp);
	gtk_container_add (GTK_CONTAINER (frame), w);

	/* Snap and placement */

	dp->auto_placement = create_check_box (_("Automatic icon placement"),
					       desktop_auto_placement, dp->pbox);
	gtk_box_pack_start (GTK_BOX (vbox), dp->auto_placement, FALSE, FALSE, 0);

	dp->snap_icons = create_check_box (_("Snap icons to grid"),
					   desktop_snap_icons, dp->pbox);
	gtk_box_pack_start (GTK_BOX (vbox), dp->snap_icons, FALSE, FALSE, 0);

	return vbox;
}

/* Creates the widgets that are used to configure the icon shape */
static GtkWidget *
create_shape_widgets (GDesktopPrefs *dp)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

	dp->use_shaped_icons = create_check_box (_("Use shaped icons"),
						 desktop_use_shaped_icons, dp->pbox);
	gtk_box_pack_start (GTK_BOX (vbox), dp->use_shaped_icons, FALSE, FALSE, 0);

	dp->use_shaped_text = create_check_box (_("Use shaped text"),
						desktop_use_shaped_text, dp->pbox);
	gtk_box_pack_start (GTK_BOX (vbox), dp->use_shaped_text, FALSE, FALSE, 0);

	return vbox;
}

/* Callback to destroy the desktop preferences data */
static void
destroy (GtkObject *object, gpointer data)
{
	GDesktopPrefs *dp;

	dp = data;
	g_free (dp);
}

/* Creates all the widgets in the desktop preferences page */
static GtkWidget *
create_widgets (GDesktopPrefs *dp)
{
	GtkWidget *hbox;
	GtkWidget *w;

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
	gtk_signal_connect (GTK_OBJECT (hbox), "destroy",
			    GTK_SIGNAL_FUNC (destroy),
			    dp);

	w = create_position_widgets (dp);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	w = create_shape_widgets (dp);
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);
	return hbox;
}

/* Creates the desktop preferences page */
GDesktopPrefs *
desktop_prefs_new (GnomePropertyBox *pbox)
{
	GDesktopPrefs *dp;
	GtkWidget *page;

	g_return_val_if_fail (pbox != NULL, NULL);

	dp = g_new0 (GDesktopPrefs, 1);

	dp->pbox = pbox;

	page = create_widgets (dp);
	gnome_property_box_append_page (pbox, page, gtk_label_new (_("Desktop")));

	return dp;
}

/* Applies the changes in the desktop preferences page */
void
desktop_prefs_apply (GDesktopPrefs *dp)
{
	g_return_if_fail (dp != NULL);

	desktop_arr_r2l = dp->right_to_left;
	desktop_arr_b2t = dp->bottom_to_top;
	desktop_arr_rows = dp->rows_not_columns;

	desktop_auto_placement = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dp->auto_placement));

	desktop_snap_icons = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dp->snap_icons));

	desktop_use_shaped_icons = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dp->use_shaped_icons));

	desktop_use_shaped_text = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dp->use_shaped_text));

	desktop_reload_icons (FALSE, 0, 0);
}
