/* gtkedit.c -
 front end for gtk/gnome version

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define _GTK_EDIT_C

#include <config.h>
#include <gnome.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include "gdk/gdkkeysyms.h"
#include "gtk/gtkmain.h"
#include "gtk/gtkselection.h"
#include "gtk/gtksignal.h"
#include "edit.h"
#include "mousemark.h"

#define EDIT_BORDER_ROOM         1
#define MIN_EDIT_WIDTH_LINES     20
#define MIN_EDIT_HEIGHT_LINES    10

#define GDK_FONT_XFONT(font) (((GdkFontPrivate*) font)->xfont)

int edit_key_emulation = 0;
int column_highlighting = 0;

static GtkWidgetClass *parent_class = NULL;

WEdit *edit_init (WEdit * edit, int lines, int columns, const char *filename, const char *text, const char *dir, unsigned long text_size);
void edit_destroy_callback (CWidget * w);
int edit_translate_key (unsigned int x_keycode, long x_key, int x_state, int *cmd, int *ch);
void gtk_edit_alloc_colors (GtkEdit *edit, GdkColormap *colormap);
static void gtk_edit_set_position (GtkEditable *editable, gint position);
void edit_mouse_mark (WEdit * edit, XEvent * event, int double_click);
void edit_move_to_prev_col (WEdit * edit, long p);
int edit_load_file_from_filename (WEdit *edit, char *exp);
static void gtk_edit_set_selection (GtkEditable * editable, gint start, gint end);


guchar gtk_edit_font_width_per_char[256];
int gtk_edit_option_text_line_spacing;
int gtk_edit_option_font_ascent;
int gtk_edit_option_font_descent;
int gtk_edit_option_font_mean_width;
int gtk_edit_fixed_font;

static void clear_focus_area (GtkEdit *edit, gint area_x, gint area_y, gint area_width, gint area_height)
{
    return;
}

void gtk_edit_freeze (GtkEdit *edit)
{
    return;
}

void       gtk_edit_insert          (GtkEdit       *edit,
				     GdkFont       *font,
				     GdkColor      *fore,
				     GdkColor      *back,
				     const char    *chars,
				     gint           length)
{
    while (length-- > 0)
	edit_insert (GTK_EDIT (edit)->editor, *chars++);
}

void       gtk_edit_set_editable    (GtkEdit       *text,
				     gint           editable)
{
    return;
}

void       gtk_edit_thaw            (GtkEdit       *text)
{
    return;
}


void gtk_edit_configure_font_dimensions (GtkEdit * edit)
{
    XFontStruct *f;
    XCharStruct s;
    char *p;
    char q[256];
    unsigned char t;
    int i, direction;
    f = GDK_FONT_XFONT (edit->editable.widget.style->font);
    p = _ ("The Quick Brown Fox Jumps Over The Lazy Dog");
    for (i = ' '; i <= '~'; i++)
	q[i - ' '] = i;
    if (XTextWidth (f, "M", 1) == XTextWidth (f, "M", 1))
	gtk_edit_fixed_font = 1;
    else
	gtk_edit_fixed_font = 0;
    XTextExtents (f, q, '~' - ' ', &direction, &gtk_edit_option_font_ascent, &gtk_edit_option_font_descent, &s);
    gtk_edit_option_font_mean_width = XTextWidth (f, p, strlen (p)) / strlen (p);
    for (i = 0; i < 256; i++) {
	t = (unsigned char) i;
	if (i > f->max_char_or_byte2 || i < f->min_char_or_byte2) {
	    gtk_edit_font_width_per_char[i] = 0;
	} else {
	    gtk_edit_font_width_per_char[i] = XTextWidth (f, (char *) &t, 1);
	}
    }
}

void gtk_edit_set_adjustments (GtkEdit * edit,
			       GtkAdjustment * hadj,
			       GtkAdjustment * vadj)
{
    g_return_if_fail (edit != NULL);
    g_return_if_fail (GTK_IS_EDIT (edit));

    if (edit->hadj && (edit->hadj != hadj)) {
	gtk_signal_disconnect_by_data (GTK_OBJECT (edit->hadj), edit);
	gtk_object_unref (GTK_OBJECT (edit->hadj));
    }
    if (edit->vadj && (edit->vadj != vadj)) {
	gtk_signal_disconnect_by_data (GTK_OBJECT (edit->vadj), edit);
	gtk_object_unref (GTK_OBJECT (edit->vadj));
    }
    if (!hadj)
	hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

    if (!vadj)
	vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

    if (edit->hadj != hadj) {
	edit->hadj = hadj;
	gtk_object_ref (GTK_OBJECT (edit->hadj));
	gtk_object_sink (GTK_OBJECT (edit->hadj));

#if 0
	gtk_signal_connect (GTK_OBJECT (edit->hadj), "changed",
			    (GtkSignalFunc) gtk_edit_adjustment,
			    edit);
	gtk_signal_connect (GTK_OBJECT (edit->hadj), "value_changed",
			    (GtkSignalFunc) gtk_edit_adjustment,
			    edit);
	gtk_signal_connect (GTK_OBJECT (edit->hadj), "disconnect",
			    (GtkSignalFunc) gtk_edit_disconnect,
			    edit);
#endif
    }
    if (edit->vadj != vadj) {
	edit->vadj = vadj;
	gtk_object_ref (GTK_OBJECT (edit->vadj));
	gtk_object_sink (GTK_OBJECT (edit->vadj));

#if 0
	gtk_signal_connect (GTK_OBJECT (edit->vadj), "changed",
			    (GtkSignalFunc) gtk_edit_adjustment,
			    edit);
	gtk_signal_connect (GTK_OBJECT (edit->vadj), "value_changed",
			    (GtkSignalFunc) gtk_edit_adjustment,
			    edit);
	gtk_signal_connect (GTK_OBJECT (edit->vadj), "disconnect",
			    (GtkSignalFunc) gtk_edit_disconnect,
			    edit);
#endif
    }
}

GtkWidget *gtk_edit_new (GtkAdjustment * hadj,
			 GtkAdjustment * vadj)
{
    GtkEdit *edit;
    edit = gtk_type_new (gtk_edit_get_type ());
    gtk_edit_set_adjustments (edit, hadj, vadj);
    gtk_edit_configure_font_dimensions (edit);
    return GTK_WIDGET (edit);
}

static void gtk_edit_realize (GtkWidget * widget)
{
    GtkEdit *edit;
    GtkEditable *editable;
    GdkWindowAttr attributes;
    GdkColormap *colormap;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));

    edit = GTK_EDIT (widget);
    editable = GTK_EDITABLE (widget);
    GTK_WIDGET_SET_FLAGS (edit, GTK_REALIZED);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual (widget);
    colormap = attributes.colormap = gtk_widget_get_colormap (widget);
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |= (GDK_EXPOSURE_MASK |
			      GDK_BUTTON_PRESS_MASK |
			      GDK_BUTTON_RELEASE_MASK |
			      GDK_BUTTON_MOTION_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_LEAVE_NOTIFY_MASK |
			      GDK_KEY_RELEASE_MASK |
			      GDK_KEY_PRESS_MASK);
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gdk_window_set_user_data (widget->window, edit);

    attributes.x = (widget->style->klass->xthickness + EDIT_BORDER_ROOM);
    attributes.y = (widget->style->klass->ythickness + EDIT_BORDER_ROOM);
    attributes.width = widget->allocation.width - attributes.x * 2;
    attributes.height = widget->allocation.height - attributes.y * 2;

    edit->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
    gdk_window_set_user_data (edit->text_area, edit);

    widget->style = gtk_style_attach (widget->style, widget->window);

    gtk_edit_alloc_colors (edit, colormap);

    /* Can't call gtk_style_set_background here because it's handled specially */
    gdk_window_set_background (widget->window, &edit->color[1]);
    gdk_window_set_background (edit->text_area, &edit->color[1]);

    edit->gc = gdk_gc_new (edit->text_area);
    gdk_gc_set_exposures (edit->gc, TRUE);
    gdk_gc_set_foreground (edit->gc, &edit->color[26]);
    gdk_gc_set_background (edit->gc, &edit->color[1]);


    gdk_window_show (edit->text_area);

    if (editable->selection_start_pos != editable->selection_end_pos)
	gtk_editable_claim_selection (editable, TRUE, GDK_CURRENT_TIME);

#if 0
    if ((widget->allocation.width > 1) || (widget->allocation.height > 1))
	recompute_geometry (edit);
#endif
}

static void gtk_edit_unrealize (GtkWidget * widget)
{
    GtkEdit *edit;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));

    edit = GTK_EDIT (widget);

    gdk_window_set_user_data (edit->text_area, NULL);
    gdk_window_destroy (edit->text_area);
    edit->text_area = NULL;

    gdk_gc_destroy (edit->gc);
    edit->gc = NULL;

    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
	(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void gtk_edit_destroy (GtkObject * object)
{
    GtkEdit *edit;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GTK_IS_EDIT (object));

    edit = (GtkEdit *) object;
    if (edit->hadj) {
	gtk_object_unref (GTK_OBJECT (edit->hadj));
	edit->hadj = NULL;
    }
    if (edit->vadj) {
	gtk_object_unref (GTK_OBJECT (edit->vadj));
	edit->vadj = NULL;
    }
    if (edit->timer) {
	gtk_timeout_remove (edit->timer);
	edit->timer = 0;
    }
    edit_clean (edit->editor);
    if (edit->editor) {
	free (edit->editor);
	edit->editor = NULL;
    }
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void gtk_edit_style_set (GtkWidget * widget,
				GtkStyle * previous_style)
{
    GtkEdit *edit;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));

    edit = GTK_EDIT (widget);
    if (GTK_WIDGET_REALIZED (widget)) {
	gdk_window_set_background (widget->window, &edit->color[1]);
	gdk_window_set_background (edit->text_area, &edit->color[1]);

#if 0
	if ((widget->allocation.width > 1) || (widget->allocation.height > 1))
	    recompute_geometry (edit);
#endif
    }
    if (GTK_WIDGET_DRAWABLE (widget))
	gdk_window_clear (widget->window);
}

static void gtk_edit_draw_focus (GtkWidget * widget)
{
    GtkEdit *edit;
    gint width, height;
    gint x, y;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));

    edit = GTK_EDIT (widget);

    if (GTK_WIDGET_DRAWABLE (widget)) {
	gint ythick = widget->style->klass->ythickness;
	gint xthick = widget->style->klass->xthickness;
	gint xextra = EDIT_BORDER_ROOM;
	gint yextra = EDIT_BORDER_ROOM;

/*      TDEBUG (("in gtk_edit_draw_focus\n")); */

	x = 0;
	y = 0;
	width = widget->allocation.width;
	height = widget->allocation.height;

	if (GTK_WIDGET_HAS_FOCUS (widget)) {
	    x += 1;
	    y += 1;
	    width -= 2;
	    height -= 2;
	    xextra -= 1;
	    yextra -= 1;

	    gdk_draw_rectangle (widget->window,
				widget->style->fg_gc[GTK_STATE_NORMAL],
				FALSE, 0, 0,
				widget->allocation.width - 1,
				widget->allocation.height - 1);
	}
	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_IN,
			 x, y, width, height);

	x += xthick;
	y += ythick;
	width -= 2 * xthick;
	height -= 2 * ythick;

	if (widget->style->bg_pixmap[GTK_STATE_NORMAL]) {
	    /* top rect */
	    clear_focus_area (edit, x, y, width, yextra);
	    /* left rect */
	    clear_focus_area (edit, x, y + yextra,
			      xextra, y + height - 2 * yextra);
	    /* right rect */
	    clear_focus_area (edit, x + width - xextra, y + yextra,
			      xextra, height - 2 * ythick);
	    /* bottom rect */
	    clear_focus_area (edit, x, x + height - yextra, width, yextra);
	} else if (!GTK_WIDGET_HAS_FOCUS (widget)) {
	    gdk_draw_rectangle (widget->window,
			 widget->style->base_gc[GTK_STATE_NORMAL], FALSE,
				x, y,
				width - 1,
				height - 1);
	}
    }
#if 0
    else {
	TDEBUG (("in gtk_edit_draw_focus (undrawable !!!)\n"));
    }
#endif
}

static void gtk_edit_size_request (GtkWidget * widget,
				   GtkRequisition * requisition)
{
    gint xthickness;
    gint ythickness;
    gint char_height;
    gint char_width;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));
    g_return_if_fail (requisition != NULL);

    xthickness = widget->style->klass->xthickness + EDIT_BORDER_ROOM;
    ythickness = widget->style->klass->ythickness + EDIT_BORDER_ROOM;

    char_height = MIN_EDIT_HEIGHT_LINES * (widget->style->font->ascent +
					   widget->style->font->descent);

    char_width = MIN_EDIT_WIDTH_LINES * (gdk_text_width (widget->style->font,
					    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
							 26)
					 / 26);

    requisition->width = char_width + xthickness * 2;
    requisition->height = char_height + ythickness * 2;
}

static void gtk_edit_size_allocate (GtkWidget * widget,
				    GtkAllocation * allocation)
{
    GtkEdit *edit;
    GtkEditable *editable;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));
    g_return_if_fail (allocation != NULL);

    edit = GTK_EDIT (widget);
    editable = GTK_EDITABLE (widget);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget)) {
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

	gdk_window_move_resize (edit->text_area,
		     widget->style->klass->xthickness + EDIT_BORDER_ROOM,
		     widget->style->klass->ythickness + EDIT_BORDER_ROOM,
	   widget->allocation.width - (widget->style->klass->xthickness +
				       EDIT_BORDER_ROOM) * 2,
	  widget->allocation.height - (widget->style->klass->ythickness +
				       EDIT_BORDER_ROOM) * 2);

#if 0
	recompute_geometry (edit);
#endif
    }
}

static void gtk_edit_draw (GtkWidget * widget,
			   GdkRectangle * area)
{
    GtkEdit *edit;
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_EDIT (widget));
    g_return_if_fail (area != NULL);

    if (GTK_WIDGET_DRAWABLE (widget)) {
/* convert the gtk expose to a coolwidget expose */
	XExposeEvent xexpose;
	xexpose.x = area->x;
	xexpose.y = area->y;
	xexpose.width = area->width;
	xexpose.height = area->height;
	edit = GTK_EDIT (widget);
	edit_render_expose (edit->editor, &xexpose);
	edit_status (edit->editor);
    }
}

void gtk_edit_set_colors (GtkEdit *win)
{
    edit_set_foreground_colors (
				 color_palette (option_editor_fg_normal),
				   color_palette (option_editor_fg_bold),
				   color_palette (option_editor_fg_italic)
	);
    edit_set_background_colors (
				 color_palette (option_editor_bg_normal),
			       color_palette (option_editor_bg_abnormal),
				 color_palette (option_editor_bg_marked),
			color_palette (option_editor_bg_marked_abnormal),
			     color_palette (option_editor_bg_highlighted)
	);
    edit_set_cursor_color (
			      color_palette (option_editor_fg_cursor)
	);
}

static gint
 gtk_edit_expose (GtkWidget * widget,
		  GdkEventExpose * event)
{
    GtkEdit *edit;
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_EDIT (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (GTK_WIDGET_DRAWABLE (widget)) {
/* convert the gtk expose to a coolwidget expose */
	XExposeEvent xexpose;
	xexpose.x = event->area.x;
	xexpose.y = event->area.y;
	xexpose.width = event->area.width;
	xexpose.height = event->area.height;
	edit = GTK_EDIT (widget);
	gtk_edit_set_colors (edit);
	edit_render_expose (edit->editor, &xexpose);
	edit_status (edit->editor);
    }
    return FALSE;
}

void edit_update_screen (WEdit * e)
{
    if (!e)
	return;
    if (!e->force)
	return;

    edit_scroll_screen_over_cursor (e);
    edit_update_curs_row (e);
    edit_update_curs_col (e);
#if 0
    update_scroll_bars (e);
#endif
    edit_status (e);

    if (e->force & REDRAW_COMPLETELY)
	e->force |= REDRAW_PAGE;

/* pop all events for this window for internal handling */
    if (e->force & (REDRAW_CHAR_ONLY | REDRAW_COMPLETELY)) {
	edit_render_keypress (e);
#if 0
    } else if (CCheckWindowEvent (e->widget->winid, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, 0)
	       || CKeyPending ()) {
	e->force |= REDRAW_PAGE;
	return;
#endif
    } else {
	edit_render_keypress (e);
    }
}


void gtk_edit_mouse_redraw (WEdit * edit, long click)
{
    edit->force |= REDRAW_PAGE | REDRAW_LINE;
    edit_update_curs_row (edit);
    edit_update_curs_col (edit);
    edit->prev_col = edit_get_col (edit);
    edit_update_screen (edit);
    edit->search_start = click;
}

/* returns the position in the edit buffer of a window click */
static long edit_get_click_pos (WEdit * edit, int x, int y)
{
    long click;
/* (1) goto to left margin */
    click = edit_bol (edit, edit->curs1);

/* (1) move up or down */
    if (y > (edit->curs_row + 1))
	click = edit_move_forward (edit, click, y - (edit->curs_row + 1), 0);
    if (y < (edit->curs_row + 1))
	click = edit_move_backward (edit, click, (edit->curs_row + 1) - y);

/* (3) move right to x pos */
    click = edit_move_forward3 (edit, click, x - edit->start_col - 1, 0);
    return click;
}

static void edit_translate_xy (int xs, int ys, int *x, int *y)
{
    *x = xs - EDIT_TEXT_HORIZONTAL_OFFSET;
    *y = (ys - EDIT_TEXT_VERTICAL_OFFSET - option_text_line_spacing / 2 - 1) / FONT_PIX_PER_LINE + 1;
}

static gint gtk_edit_button_press_release (GtkWidget * widget,
					   GdkEventButton * event)
{
    GtkEditable *editable;
    GtkEdit *edit;
    WEdit *e;
    static GdkAtom ctext_atom = GDK_NONE;
    int x_text = 0, y_text = 0;
    long mouse_pos;
    static long button_down_pos;
    static long dragging = 0;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_EDIT (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (ctext_atom == GDK_NONE)
	ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

    edit = GTK_EDIT (widget);
    e = edit->editor;
    editable = GTK_EDITABLE (widget);

    if (!GTK_WIDGET_HAS_FOCUS (widget))
	if (!(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
	    gtk_widget_grab_focus (widget);

    edit_translate_xy (event->x, event->y, &x_text, &y_text);
    mouse_pos = edit_get_click_pos (e, x_text, y_text);

    switch (event->type) {
    case GDK_BUTTON_RELEASE:
	if (!dragging)
	    return FALSE;
    case GDK_MOTION_NOTIFY:
	if (mouse_pos == button_down_pos)
	    return FALSE;
	dragging = 1;
	edit_cursor_move (e, mouse_pos - e->curs1);
	gtk_edit_set_selection (GTK_EDITABLE (widget), mouse_pos, button_down_pos);
	gtk_edit_mouse_redraw (e, mouse_pos);
	break;
    case GDK_BUTTON_PRESS:
	dragging = 0;
	if (event->button == 2) {
	    edit_cursor_move (e, mouse_pos - e->curs1);
	    editable->current_pos = mouse_pos;
	    gtk_selection_convert (GTK_WIDGET (edit), GDK_SELECTION_PRIMARY,
				   ctext_atom, event->time);
	    gtk_edit_mouse_redraw (e, mouse_pos);
	    editable->selection_start_pos = e->mark1;
	    editable->selection_end_pos = e->mark2;
	    return FALSE;
	}
	button_down_pos = mouse_pos;
#if 0
	if (editable->has_selection)
	    if (mouse_pos >= editable->selection_start_pos
		&& mouse_pos < editable->selection_end_pos)
		gtk_edit_set_selection (GTK_EDITABLE (widget), mouse_pos, mouse_pos);
#endif
	edit_cursor_move (e, mouse_pos - e->curs1);
	gtk_edit_mouse_redraw (e, mouse_pos);
	break;
    case GDK_2BUTTON_PRESS:
	dragging = 0;
	edit_cursor_move (e, mouse_pos - e->curs1);
	edit_right_word_move (e);
	mouse_pos = e->curs1;
	edit_left_word_move (e);
	button_down_pos = e->curs1;
	gtk_edit_set_selection (GTK_EDITABLE (widget), mouse_pos, button_down_pos);
	gtk_edit_mouse_redraw (e, mouse_pos);
	break;
    case GDK_3BUTTON_PRESS:
	dragging = 0;
	mouse_pos = edit_bol (e, mouse_pos);
	edit_cursor_move (e, mouse_pos - e->curs1);
	button_down_pos = edit_eol (e, mouse_pos) + 1;
	gtk_edit_set_selection (GTK_EDITABLE (widget), mouse_pos, button_down_pos);
	gtk_edit_mouse_redraw (e, mouse_pos);
	break;
    default:
	dragging = 0;
	break;
    }
    editable->current_pos = e->curs1;
    return FALSE;
}

static gint
 gtk_edit_button_motion (GtkWidget * widget,
			 GdkEventMotion * event)
{
    return gtk_edit_button_press_release (widget, (GdkEventButton *) event);
}

static guint toggle_bit (guint x, guint mask)
{
    unsigned long m = -1;
    if ((x & mask))
	return x & (m - mask);
    else
	return x | mask;
}

int mod_type_key (guint x)
{
    switch ((guint) x) {
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
	return 1;
    }
    return 0;
}

/* get a 15 bit "almost unique" key sym that includes keyboard modifier
   info in the top 3 bits */
short key_sym_mod (gint key, gint state)
{
    if (key && !mod_type_key (key)) {
	key = toggle_bit (key, 0x1000 * ((state & GDK_SHIFT_MASK) != 0));
	key = toggle_bit (key, 0x2000 * ((state & GDK_CONTROL_MASK) != 0));
	key = toggle_bit (key, 0x4000 * ((state & GDK_MOD1_MASK) != 0));
	key &= 0x7FFF;
    } else
	key = 0;
    return key;
}

static gint gtk_edit_key_press (GtkWidget * widget, GdkEventKey * event)
{
    GtkEditable *editable;
    GtkEdit *edit;
    WEdit *e;
    gint command = 0, insert = -1, r = 0;
    guint key, state;
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_EDIT (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    edit = GTK_EDIT (widget);
    editable = GTK_EDITABLE (widget);
    e = edit->editor;
    if (!edit_translate_key (0, event->keyval, event->state, &command, &insert)) {
	return FALSE;
    }
    key = event->keyval;
    state = event->state;
    if (!command && insert < 0) {	/* no translation took place, so lets see if we have a macro */
	if ((key == GDK_r || key == GDK_R) && (state & ControlMask)) {
	    command = e->macro_i < 0 ? CK_Begin_Record_Macro : CK_End_Record_Macro;
	} else {
	    command = key_sym_mod (key, state);
	    if (command > 0)
		command = CK_Macro (command);
	}
    }
    r = edit_execute_key_command (e, command, insert);
    if (r)
	edit_update_screen (e);
    editable->selection_start_pos = e->mark1;
    editable->selection_end_pos = ((e->mark2 < 0) ? e->curs1 : e->mark2);
    editable->has_selection = editable->selection_start_pos != editable->selection_end_pos;
    editable->current_pos = e->curs1;
    return r;
}

/**********************************************************************/
/*                              Widget Crap                           */
/**********************************************************************/

char *home_dir = 0;

static void get_home_dir (void)
{
    if (home_dir)		/* already been set */
	return;
    home_dir = getenv ("HOME");
    if (home_dir)
	if (*home_dir) {
	    home_dir = strdup (home_dir);
	    return;
	}
    home_dir = (getpwuid (geteuid ()))->pw_dir;
    if (home_dir)
	if (*home_dir) {
	    home_dir = strdup (home_dir);
	    return;
	}
    fprintf (stderr, _("gtkedit.c: HOME environment variable not set and no passwd entry - aborting\n"));
    abort ();
}

static gchar *gtk_edit_get_chars (GtkEditable * editable,
				  gint start_pos,
				  gint end_pos)
{
    GtkEdit *edit;
    gchar *retval;
    int i;
    g_return_val_if_fail (editable != NULL, NULL);
    g_return_val_if_fail (GTK_IS_EDIT (editable), NULL);
    edit = GTK_EDIT (editable);
    if (end_pos < 0)
	end_pos = edit->editor->last_byte;
    if ((start_pos < 0) ||
	(end_pos > edit->editor->last_byte) ||
	(end_pos < start_pos))
	return 0;
    retval = malloc (end_pos - start_pos + 1);
    retval[end_pos - start_pos] = '\0';
    for (i = 0; start_pos < end_pos; start_pos++, i++)
	retval[i] = (gchar) edit_get_byte (edit->editor, start_pos);
    return retval;
}

static void gtk_edit_set_selection (GtkEditable * editable,
				    gint start,
				    gint end)
{
    GtkEdit *edit = GTK_EDIT (editable);
    WEdit *e;
    e = edit->editor;
    if (end < 0)
	end = edit->editor->last_byte;
    editable->selection_start_pos = e->mark1 = MIN (start, end);
    editable->selection_end_pos = e->mark2 = MAX (start, end);
    editable->has_selection = (e->mark2 != e->mark1);
    gtk_edit_mouse_redraw (e, e->curs1);
    gtk_editable_claim_selection (editable, TRUE, GDK_CURRENT_TIME);
}

static void gtk_edit_insert_text (GtkEditable * editable,
				  const gchar * new_text,
				  gint new_text_length,
				  gint * position)
{
    GtkEdit *edit = GTK_EDIT (editable);
    edit_cursor_move (edit->editor, *position - edit->editor->curs1);
    while (new_text_length--)
	edit_insert_ahead (edit->editor, new_text[new_text_length]);
    *position = edit->editor->curs1;
}

static void gtk_edit_class_init (GtkEditClass * class)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;
    GtkEditableClass *editable_class;

    object_class = (GtkObjectClass *) class;
    widget_class = (GtkWidgetClass *) class;
    editable_class = (GtkEditableClass *) class;

    parent_class = gtk_type_class (gtk_editable_get_type ());

    object_class->destroy = gtk_edit_destroy;

    widget_class->realize = gtk_edit_realize;
    widget_class->unrealize = gtk_edit_unrealize;
    widget_class->style_set = gtk_edit_style_set;
    widget_class->draw_focus = gtk_edit_draw_focus;
    widget_class->size_request = gtk_edit_size_request;
    widget_class->size_allocate = gtk_edit_size_allocate;
    widget_class->draw = gtk_edit_draw;
    widget_class->expose_event = gtk_edit_expose;
    widget_class->button_press_event = gtk_edit_button_press_release;
    widget_class->button_release_event = gtk_edit_button_press_release;
    widget_class->motion_notify_event = gtk_edit_button_motion;

    widget_class->key_press_event = gtk_edit_key_press;
#if 0
    widget_class->focus_in_event = gtk_edit_focus_in;
    widget_class->focus_out_event = gtk_edit_focus_out;
#endif
    widget_class->focus_in_event = 0;
    widget_class->focus_out_event = 0;

    editable_class->insert_text = gtk_edit_insert_text;
#if 0
    editable_class->delete_text = gtk_edit_delete_text;
    editable_class->update_text = gtk_edit_update_text;
#else
    editable_class->delete_text = 0;
    editable_class->update_text = 0;
#endif

    editable_class->get_chars = gtk_edit_get_chars;
    editable_class->set_selection = gtk_edit_set_selection;
    editable_class->set_position = gtk_edit_set_position;


#if 0
    editable_class->set_position = 0;
#endif

    get_home_dir ();
}

static void gtk_edit_init (GtkEdit * edit)
{
    static made_directory = 0;

    GTK_WIDGET_SET_FLAGS (edit, GTK_CAN_FOCUS);

    edit->editor = edit_init (0, 80, 25, 0, "", "/", 0);
    edit->editor->macro_i = -1;
    edit->editor->widget = edit;
    edit->timer = 0;
    edit->menubar = 0;
    edit->status = 0;
    edit->options = 0;

    gtk_edit_configure_font_dimensions (edit);

    if (!made_directory) {
	mkdir (catstrs (home_dir, EDIT_DIR, 0), 0700);
	made_directory = 1;
    }
    GTK_EDITABLE (edit)->editable = TRUE;
}

guint
gtk_edit_get_type (void)
{
    static guint edit_type = 0;

    if (!edit_type) {
	GtkTypeInfo edit_info =
	{
	    "GtkEdit",
	    sizeof (GtkEdit),
	    sizeof (GtkEditClass),
	    (GtkClassInitFunc) gtk_edit_class_init,
	    (GtkObjectInitFunc) gtk_edit_init,
	    (GtkArgSetFunc) NULL,
	    (GtkArgGetFunc) NULL,
	};

	edit_type = gtk_type_unique (gtk_editable_get_type (), &edit_info);
    }
    return edit_type;
}

#include <libgnomeui/gtkcauldron.h>
#include <libgnomeui/gnome-stock.h>

char *gtk_edit_dialog_get_save_file (guchar * dir, guchar * def, guchar * title)
{
    char *s;
    s = gtk_dialog_cauldron (
				title, GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_GRAB,
	    " ( ( (Filename:)d | %Fgxf )f )xf / ( %Bxfgrq || %Bxfgq )f ",
				&def, "filename", title,
				GNOME_STOCK_BUTTON_OK,
				GNOME_STOCK_BUTTON_CANCEL
	);
    if (s == GTK_CAULDRON_ESCAPE || !s || s == GNOME_STOCK_BUTTON_CANCEL)
	return 0;
    return def;
}

char *gtk_edit_dialog_get_load_file (guchar * dir, guchar * def, guchar * title)
{
    char *s;
    s = gtk_dialog_cauldron (
				title, GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_GRAB,
	    " ( ( (Filename:)d | %Fgxf )f )xf / ( %Bxfgrq || %Bxfgq )f ",
				&def, "filename", title,
				GNOME_STOCK_BUTTON_OK,
				GNOME_STOCK_BUTTON_CANCEL
	);
    if (s == GTK_CAULDRON_ESCAPE || !s || s == GNOME_STOCK_BUTTON_CANCEL)
	return 0;
    return def;
}

void gtk_edit_dialog_message (guchar * heading, char *fmt,...)
{
    gchar s[8192];
    va_list ap;
    va_start (ap, fmt);
    vsprintf (s, fmt, ap);
    va_end (ap);
    gtk_dialog_cauldron (
			    heading, GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_GRAB,
			    " [ ( %Ld )xf ]xf / ( %Bxfgq )f ",
			    s,
			    GNOME_STOCK_BUTTON_CANCEL
	);
    return;
}

int gtk_edit_dialog_query (guchar * heading, guchar * first,...)
{
    char *buttons[16];
    char s[1024], *r;
    int n;
    va_list ap;
    va_start (ap, first);
    n = 0;
    while ((buttons[n++] = va_arg (ap, char *)) && n < 15);
    va_end (ap);
    buttons[n] = 0;
    strcpy (s, " [ ( %Lxf )xf ]xf / ( ");
    n = 0;
    while (buttons[n]) {
	strcat (s, " %Bqxf ");
	if (!buttons[n])
	    break;
	strcat (s, " ||");
	n++;
    }
    strcat (s, " )f");
    r = gtk_dialog_cauldron (heading, GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_IGNOREENTER | GTK_CAULDRON_GRAB, s, first,
			  buttons[0], buttons[1], buttons[2], buttons[3],
			  buttons[4], buttons[5], buttons[6], buttons[7],
			buttons[8], buttons[9], buttons[10], buttons[11],
		      buttons[12], buttons[13], buttons[14], buttons[15]
	);
    n = 0;
    if (r == GTK_CAULDRON_ESCAPE || !r || r == GNOME_STOCK_BUTTON_CANCEL)
	return -1;
    while (buttons[n]) {
	if (!strcmp (buttons[n], r))
	    return n;
	n++;
    }
    return -1;
}

void gtk_edit_dialog_error (guchar * heading, char *fmt, ...)
{
    gchar s[8192];
    va_list ap;
    va_start (ap, fmt);
    vsprintf (s, fmt, ap);
    va_end (ap);
    gtk_dialog_cauldron (
			    heading, GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_GRAB,
			    " [ ( %Ld )xf ]xf / ( %Bxfgq )f",
			    s,
			    GNOME_STOCK_BUTTON_CANCEL
	);
    return;
}



struct color_matrix_struct {
    unsigned int R, G, B;
} color_matrix[27] =
{
    {0, 0, 0},
    {0, 0, 128},
    {0, 0, 255},
    {0, 139, 0},
    {0, 139, 139},
    {0, 154, 205},
    {0, 255, 0},
    {0, 250, 154},
    {0, 255, 255},
    {139, 37, 0},
    {139, 0, 139},
    {125, 38, 205},
    {139, 117, 0},
    {127, 127, 127},
    {123, 104, 238},
    {127, 255, 0},
    {135, 206, 235},
    {127, 255, 212},
    {238, 0, 0},
    {238, 18, 137},
    {238, 0, 238},
    {205, 102, 0},
    {248, 183, 183},
    {224, 102, 255},
    {238, 238, 0},
    {238, 230, 133},
    {248, 248, 255}
};

void gtk_edit_alloc_colors (GtkEdit *edit, GdkColormap *colormap)
{
    int i;
    for (i = 0; i < 27; i++) {
	edit->color[i].red = (gushort) color_matrix[i].R << 8;
	edit->color[i].green = (gushort) color_matrix[i].G << 8;
	edit->color[i].blue = (gushort) color_matrix[i].B << 8;
	if (!gdk_color_alloc (colormap, &edit->color[i]))
	    g_warning ("cannot allocate color");
    }
    edit->color_last_pixel = 27;
}

int allocate_color (WEdit *edit, gchar *color)
{
    GtkEdit *win;
    win = (GtkEdit *) edit->widget;
    if (!color)
	return NO_COLOR;
    if (*color >= '0' && *color <= '9') {
	return atoi (color);
    } else {
	int i;
	GdkColor c;
	if (!color)
	    return NO_COLOR;
	if (!gdk_color_parse (color, &c))
	    return NO_COLOR;
	if (!gdk_color_alloc (gtk_widget_get_colormap(GTK_WIDGET (edit->widget)), &c))
	    return NO_COLOR;
	for (i = 0; i < (GTK_EDIT (edit->widget))->color_last_pixel; i++)
	    if (color_palette (i) == c.pixel)
		return i;
	GTK_EDIT (edit->widget)->color[(GTK_EDIT (edit->widget))->color_last_pixel].pixel = c.pixel;
	return (GTK_EDIT (edit->widget))->color_last_pixel++;
    }
}

static void gtk_edit_set_position (GtkEditable * editable, gint position)
{
    WEdit *edit;
    edit = GTK_EDIT(editable)->editor;
    edit_cursor_move (edit, position - edit->curs1);
    edit_move_to_prev_col (edit, 0);
    edit->force |= REDRAW_PAGE;
    edit->search_start = 0;
    edit_update_curs_row (edit);
}


/* returns 1 on error */
gint gtk_edit_load_file_from_filename (GtkWidget * edit, const gchar * filename)
{
    return edit_load_file_from_filename (GTK_EDIT (edit)->editor, (char *) filename);
}

void gtk_edit_set_top_line (GtkWidget * e, int line)
{
    WEdit *edit;
    edit = GTK_EDIT (e)->editor;
    edit_move_display (edit, line - edit->num_widget_lines / 2 - 1);
    edit->force |= REDRAW_COMPLETELY;
}

void gtk_edit_set_cursor_line (GtkWidget * e, int line)
{
    WEdit *edit;
    edit = GTK_EDIT (e)->editor;
    edit_move_to_line (edit, line - 1);
    edit->force |= REDRAW_COMPLETELY;
}

void about_cb (GtkWidget * widget, void *data)
{
    gtk_dialog_cauldron ("About", GTK_CAULDRON_TOPLEVEL | GTK_CAULDRON_GRAB, " [ (Mcedit - an editor for the midnight commander\n\
ported from Cooledit - a user friendly text editor for the X Window System.)xf ]xf / ( %Bgqxf )f ", GNOME_STOCK_BUTTON_OK);
    return;
}

void gtk_edit_command (GtkEdit * edit, gint command)
{
    int r;
    gtk_widget_grab_focus (GTK_WIDGET (edit));
    r = edit_execute_key_command (edit->editor, command, -1);
    if (r)
	edit_update_screen (edit->editor);
}

void gtk_edit_quit (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Exit); }

void gtk_edit_load_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Load); }
void gtk_edit_new_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_New); }
void gtk_edit_save_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Save); }
void gtk_edit_save_as_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Save_As); }
void gtk_edit_insert_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Insert_File); }
void gtk_edit_copy_to_file (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Save_Block); }

void gtk_edit_clip_cut (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_XCut); }
void gtk_edit_clip_copy (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_XStore); }
void gtk_edit_clip_paste (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_XPaste); }

void gtk_edit_toggle_mark (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Mark); }
void gtk_edit_search (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Find); }
void gtk_edit_search_again (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Find_Again); }
void gtk_edit_replace (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Replace); }
void gtk_edit_copy (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Copy); }
void gtk_edit_move (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Move); }
void gtk_edit_delete (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Remove); }
void gtk_edit_undo (GtkEdit * widget, void *data) { GtkEdit *edit = (GtkEdit *) data ; gtk_edit_command (edit, CK_Undo); }

#if 0
struct _GnomeUIInfo {
    GnomeUIInfoType type;
    gchar *label;
    gchar *hint;		/* For toolbar items, the tooltip. For menu items, the status bar message */

    /* For an item, toggleitem, or radioitem, procedure to call when activated.    
       For a subtree, point to the GnomeUIInfo array for that subtree.
       For a radioitem lead entry, point to the GnomeUIInfo array for
       the radio item group.  For the radioitem array, procedure to
       call when activated. For a help item, specifies the help node to load
       (or NULL for main prog's name) 
       For builder data, point to the GnomeUIBuilderData structure for the following items */
    gpointer moreinfo;
    gpointer user_data;
    gpointer unused_data;
    GnomeUIPixmapType pixmap_type;
    /* Either 
     * a pointer to the char for the pixmap (GNOME_APP_PIXMAP_DATA),
     * a char* for the filename (GNOME_APP_PIXMAP_FILENAME),
     * or a char* for the stock pixmap name (GNOME_APP_PIXMAP_STOCK).
     */
    gpointer pixmap_info;
    guint accelerator_key;	/* Accelerator key... Set to 0 to ignore */
    GdkModifierType ac_mods;	/* An OR of the masks for the accelerator */
    GtkWidget *widget;		/* Filled in by gnome_app_create* */
};

#endif

typedef struct _TbItems TbItems;
struct _TbItems {
	char *key, *text, *tooltip, *icon;
	void (*cb) (GtkEdit *, void *);
	GtkWidget *widget; /* will be filled in */
};

#define TB_PROP 7

static TbItems tb_items[] =
{
    {"F1", "Help", "Interactive help browser", GNOME_STOCK_MENU_BLANK, 0, NULL},
    {"F2", "Save", "Save to current file name", GNOME_STOCK_MENU_SAVE, gtk_edit_save_file, NULL},
    {"F3", "Mark", "Toggle In/Off invisible marker to highlight text", GNOME_STOCK_MENU_BLANK, gtk_edit_toggle_mark, NULL},
    {"F4", "Replc", "Find and replace strings/regular expressions", GNOME_STOCK_MENU_SRCHRPL, gtk_edit_replace, NULL},
    {"F5", "Copy", "Copy highlighted block to cursor postition", GNOME_STOCK_MENU_COPY, gtk_edit_copy, NULL},

    {"F6", "Move", "Copy highlighted block to cursor postition", GNOME_STOCK_MENU_BLANK, gtk_edit_move, NULL},
    {"F7", "Find", "Find strings/regular expressions", GNOME_STOCK_MENU_SEARCH, gtk_edit_search, NULL},
    {"F8", "Dlete", "Delete highlighted text", GNOME_STOCK_MENU_BLANK, gtk_edit_delete, NULL},
    {"F9", "Menu", "Pull down menu", GNOME_STOCK_MENU_BLANK, /* gtk_edit_menu*/ 0, NULL},
    {"F10", "Quit", "Exit editor", GNOME_STOCK_MENU_QUIT, gtk_edit_quit, NULL},
    {0, 0, 0, 0, 0, 0}
};

static GtkWidget *create_toolbar (GtkWidget * window, GtkEdit * edit)
{
    GtkWidget *toolbar;
    TbItems *t;
    toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
    for (t = &tb_items[0]; t->text; t++) {
	t->widget = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
					     t->text,
					     t->tooltip,
					     0,
			     gnome_stock_pixmap_widget (window, t->icon),
					     t->cb,
					     t->cb ? edit : 0);
    }
    return toolbar;
}

/* returns 1 on error */
int edit (const char *file, int line)
{
    GtkWidget *app;
    GtkWidget *edit, *statusbar;

    edit = gtk_edit_new (NULL, NULL);
    app = gnome_app_new ("mcedit", (char *) (file ? file : "Mcedit"));

    {
	GnomeUIInfo file_menu[] =
	{
	    {
		GNOME_APP_UI_ITEM, N_ ("Open/Load"), N_ ("Load a different/new file"), gtk_edit_load_file, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 'O', GDK_CONTROL_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("New"), N_ ("Clear the edit buffer"), gtk_edit_new_file, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'N', GDK_CONTROL_MASK, 0
	    },
	    GNOMEUIINFO_SEPARATOR,
	    {
		GNOME_APP_UI_ITEM, N_ ("Save"), N_ ("Save the current edit buffer to filename"), gtk_edit_save_file, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE, 'S', GDK_CONTROL_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Save As"), N_ ("Save the current edit buffer as filename"), gtk_edit_save_as_file, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE_AS, 'A', GDK_CONTROL_MASK, 0
	    },
	    GNOMEUIINFO_SEPARATOR,
	    {
		GNOME_APP_UI_ITEM, N_ ("Insert File"), N_ ("Insert text from a file"), gtk_edit_insert_file, edit, 0, 0, 0, 'I', GDK_CONTROL_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Copy to file"), N_ ("copy a block to a file"), gtk_edit_copy_to_file, edit, 0, 0, 0, 'C', GDK_CONTROL_MASK, 0
	    },
	    GNOMEUIINFO_SEPARATOR,
	    {
		GNOME_APP_UI_ITEM, N_ ("Exit"), N_ ("Quit editor"), gtk_edit_quit, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_QUIT, 'Q', GDK_CONTROL_MASK, NULL
	    },
	    GNOMEUIINFO_END
	};

	GnomeUIInfo edit_menu[] =
	{
	    {
		GNOME_APP_UI_ITEM, N_ ("Copy"), N_ ("Copy selection to clipboard"), gtk_edit_clip_copy, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_COPY, 'C', GDK_CONTROL_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Cut"), N_ ("Cut selection to clipboard"), gtk_edit_clip_cut, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CUT, 'X', GDK_CONTROL_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Paste"), N_ ("Paste clipboard"), gtk_edit_clip_paste, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 'V', GDK_CONTROL_MASK, 0
	    },
	    GNOMEUIINFO_SEPARATOR,
	    {
		GNOME_APP_UI_ITEM, N_ ("Undo"), N_ ("Go back in time one key press"), gtk_edit_undo, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE, 'U', GDK_CONTROL_MASK, 0
	    },
	    GNOMEUIINFO_END
	};

	GnomeUIInfo search_menu[] =
	{
	    {
		GNOME_APP_UI_ITEM, N_ ("Find"), N_ ("Find string/regular expression"), gtk_edit_search, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN, 'F', GDK_MOD1_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Find again"), N_ ("Repeat most recent search"), gtk_edit_search_again, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_NEW, 'A', GDK_MOD1_MASK, 0
	    },
	    {
		GNOME_APP_UI_ITEM, N_ ("Search/Replace"), N_ ("Find and replace text/regular expressions"), gtk_edit_replace, edit, 0, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SAVE, 'R', GDK_MOD1_MASK, 0
	    },
	    GNOMEUIINFO_END
	};

	GnomeUIInfo help_menu[] =
	{
	    {
		GNOME_APP_UI_ITEM,
		N_ ("About..."), N_ ("Info about Mcedit"),
		about_cb, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT,
		0, (GdkModifierType) 0, NULL
	    },
#if 0
	    GNOMEUIINFO_SEPARATOR,
	    GNOMEUIINFO_HELP ("hello"),
#endif
	    GNOMEUIINFO_END
	};

	GnomeUIInfo main_menu[] =
	{
	    GNOMEUIINFO_SUBTREE (N_ ("File"), file_menu),
	    GNOMEUIINFO_SUBTREE (N_ ("Edit"), edit_menu),
	    GNOMEUIINFO_SUBTREE (N_ ("Search/Replace"), search_menu),
	    GNOMEUIINFO_SUBTREE (N_ ("Help"), help_menu),
	    GNOMEUIINFO_END
	};

	gtk_widget_realize (app);
	statusbar = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (statusbar), 0);
	gtk_widget_set_usize (app, 400, 400);
	gnome_app_create_menus (GNOME_APP (app), main_menu);
	gnome_app_set_contents (GNOME_APP (app), edit);
	gnome_app_set_statusbar (GNOME_APP (app), GTK_WIDGET (statusbar));
	GTK_EDIT (edit)->menubar = GNOME_APP (app)->menubar;
	GTK_EDIT (edit)->status = statusbar;
	gnome_app_set_toolbar(GNOME_APP (app), GTK_TOOLBAR(create_toolbar(app, GTK_EDIT (edit))));
	GTK_EDIT(edit)->destroy_me = gtk_widget_destroy;
	GTK_EDIT(edit)->destroy_me_user_data = app;

	gtk_widget_show (edit);
	gtk_widget_realize (edit);
	if (file)
	    if (*file)
		if (gtk_edit_load_file_from_filename (edit, file)) {
		    gtk_widget_destroy (app);
		    return 1;
		}
	gtk_edit_set_cursor_line (edit, line);
	gtk_widget_show_all (app);
	gtk_widget_grab_focus (edit);
    }
    return 0;
}
