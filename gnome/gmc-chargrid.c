/* GmcCharGrid Widget - Simple character grid for the gmc viewer
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <gtk/gtksignal.h>
#include <string.h>
#include "gmc-chargrid.h"


#define DEFAULT_WIDTH  80
#define DEFAULT_HEIGHT 25
#define DEFAULT_FONT   "fixed"


#define CHARS(cgrid) ((char *) cgrid->chars)
#define ATTRS(cgrid) ((struct attr *) cgrid->attrs)

struct attr {
	gulong fg;
	gulong bg;
	int fg_set : 1;
	int bg_set : 1;
};


enum {
	SIZE_CHANGED,
	LAST_SIGNAL
};

typedef void (* GmcCharGridSignal1) (GtkObject *object,
				     guint      arg1,
				     guint      arg2,
				     gpointer   data);

static void gmc_char_grid_marshal_signal_1 (GtkObject     *object,
					    GtkSignalFunc  func,
					    gpointer       func_data,
					    GtkArg        *args);


static void gmc_char_grid_class_init        (GmcCharGridClass *class);
static void gmc_char_grid_init              (GmcCharGrid      *cgrid);
static void gmc_char_grid_destroy           (GtkObject        *object);
static void gmc_char_grid_realize           (GtkWidget        *widget);
static void gmc_char_grid_size_request      (GtkWidget        *widget,
					     GtkRequisition   *requisition);
static void gmc_char_grid_size_allocate     (GtkWidget        *widget,
					     GtkAllocation    *allocation);
static void gmc_char_grid_draw              (GtkWidget        *widget,
					     GdkRectangle     *area);
static gint gmc_char_grid_expose            (GtkWidget        *widget,
					     GdkEventExpose   *event);
static void gmc_char_grid_real_size_changed (GmcCharGrid      *cgrid,
					     guint             width,
					     guint             height);


static GtkWidgetClass *parent_class;

static guint cgrid_signals[LAST_SIGNAL] = { 0 };


guint
gmc_char_grid_get_type (void)
{
	static guint cgrid_type = 0;

	if (!cgrid_type) {
		GtkTypeInfo cgrid_info = {
			"GmcCharGrid",
			sizeof (GmcCharGrid),
			sizeof (GmcCharGridClass),
			(GtkClassInitFunc) gmc_char_grid_class_init,
			(GtkObjectInitFunc) gmc_char_grid_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		cgrid_type = gtk_type_unique (gtk_widget_get_type (), &cgrid_info);
	}

	return cgrid_type;
}

static void
gmc_char_grid_class_init (GmcCharGridClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	cgrid_signals[SIZE_CHANGED] =
		gtk_signal_new ("size_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GmcCharGridClass, size_changed),
				gmc_char_grid_marshal_signal_1,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);

	gtk_object_class_add_signals (object_class, cgrid_signals, LAST_SIGNAL);

	object_class->destroy = gmc_char_grid_destroy;

	widget_class->realize = gmc_char_grid_realize;
	widget_class->size_request = gmc_char_grid_size_request;
	widget_class->size_allocate = gmc_char_grid_size_allocate;
	widget_class->draw = gmc_char_grid_draw;
	widget_class->expose_event = gmc_char_grid_expose;

	class->size_changed = gmc_char_grid_real_size_changed;
}

static void
gmc_char_grid_init (GmcCharGrid *cgrid)
{
	cgrid->width         = 0;
	cgrid->height        = 0;
	cgrid->chars         = NULL;
	cgrid->attrs         = NULL;
	cgrid->frozen        = FALSE;
	cgrid->font          = NULL;
	cgrid->gc            = NULL;
	cgrid->char_width    = 0;
	cgrid->char_height   = 0;
	cgrid->char_y        = 0;
}

GtkWidget *
gmc_char_grid_new (void)
{
	GmcCharGrid *cgrid;

	cgrid = gtk_type_new (gmc_char_grid_get_type ());

	gmc_char_grid_set_font (cgrid, DEFAULT_FONT);
	gmc_char_grid_set_size (cgrid, DEFAULT_WIDTH, DEFAULT_HEIGHT);

	return GTK_WIDGET (cgrid);
}

static void
gmc_char_grid_destroy (GtkObject *object)
{
	GmcCharGrid *cgrid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (object));

	cgrid = GMC_CHAR_GRID (object);

	if (cgrid->chars)
		g_free (cgrid->chars);

	if (cgrid->attrs)
		g_free (cgrid->attrs);

	if (cgrid->font)
		gdk_font_unref (cgrid->font);

	if (cgrid->gc)
		gdk_gc_destroy (cgrid->gc);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gmc_char_grid_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GmcCharGrid *cgrid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (widget));

	cgrid = GMC_CHAR_GRID (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.x = widget->allocation.x + (widget->allocation.width - cgrid->char_width * cgrid->width) / 2;
	attributes.y = widget->allocation.y + (widget->allocation.height - cgrid->char_height * cgrid->height) / 2;
	attributes.width = cgrid->width * cgrid->char_width;
	attributes.height = cgrid->height * cgrid->char_height;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = (gtk_widget_get_events (widget)
				 | GDK_EXPOSURE_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	cgrid->gc = gdk_gc_new (cgrid->widget.window);
	
	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gmc_char_grid_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GmcCharGrid *cgrid;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (widget));
	g_return_if_fail (requisition != NULL);

	cgrid = GMC_CHAR_GRID (widget);

	requisition->width = cgrid->width * cgrid->char_width;
	requisition->height = cgrid->height * cgrid->char_height;
}

static void
gmc_char_grid_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GmcCharGrid *cgrid;
	int w, h;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;

	cgrid = GMC_CHAR_GRID (widget);

	w = allocation->width / cgrid->char_width;
	h = allocation->height / cgrid->char_height;

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window,
					allocation->x + (allocation->width - cgrid->char_width * cgrid->width) / 2,
					allocation->y + (allocation->height - cgrid->char_height * cgrid->height) / 2,
					cgrid->width * cgrid->char_width,
					cgrid->height * cgrid->char_height);

	gmc_char_grid_set_size (cgrid, MAX (w, 1), MAX (h, 1));
}

static void
update_strip (GmcCharGrid *cgrid, int x, int y, int width)
{
	int i;
	char *chars;
	struct attr *attrs;
	int first;
	gulong color;
	gulong ocolor;
	GdkColor gcolor;

	chars = CHARS (cgrid) + (cgrid->width * y + x);
	attrs = ATTRS (cgrid) + (cgrid->width * y + x);

	/* First paint the background and then paint the text.  We do it in two passes
	 * so that we can paint runs of same-background and foreground more efficiently.
	 */

	i = 0;

	while (i < width) {
		first = i;
		ocolor = attrs[i].bg_set ? attrs[i].bg : GTK_WIDGET (cgrid)->style->bg[GTK_STATE_NORMAL].pixel;
		color = ocolor;

		do {
			i++;
		} while ((i < width) && (color == ocolor));

		gcolor.pixel = color;
		gdk_gc_set_foreground (cgrid->gc, &gcolor);

		gdk_draw_rectangle (cgrid->widget.window,
				    cgrid->gc,
				    TRUE,
				    (first + x) * cgrid->char_width,
				    y * cgrid->char_height,
				    cgrid->char_width * (i - first),
				    cgrid->char_height);
	}
	
	/* Now paint the text */
	
	i = 0;

	while (i < width) {
		first = i;
		ocolor = attrs[i].fg_set ? attrs[i].fg : GTK_WIDGET (cgrid)->style->fg[GTK_STATE_NORMAL].pixel;
		color = ocolor;

		do {
			i++;
		} while ((i < width) && (color == ocolor));

		gcolor.pixel = color;
		gdk_gc_set_foreground (cgrid->gc, &gcolor);

		gdk_draw_text (cgrid->widget.window,
			       cgrid->font,
			       cgrid->gc,
			       (first + x) * cgrid->char_width,
			       y * cgrid->char_height + cgrid->char_y,
			       &chars[first],
			       i - first);
	}
}

static void
update_region (GmcCharGrid *cgrid, int x, int y, int width, int height)
{
	int i;

	if ((width == 0) || (height == 0))
		return;

	x = MAX (0, x);
	y = MAX (0, y);
	width = MIN (width, cgrid->width - x);
	height = MIN (height, cgrid->height - y);

	for (i = 0; i < height; i++)
		update_strip (cgrid, x, y + i, width);
}

static void
paint (GmcCharGrid *cgrid, GdkRectangle *area)
{
	int x1, y1, x2, y2;

	/* This logic is shamelessly ripped from gtkterm :-)  - Federico */

	if (area->width > 1)
		area->width--;
	else if (area->x > 0)
		area->x--;

	if (area->height > 1)
		area->height--;
	else if (area->y > 0)
		area->y--;

	x1 = area->x / cgrid->char_width;
	y1 = area->y / cgrid->char_height;

	x2 = (area->x + area->width) / cgrid->char_width;
	y2 = (area->y + area->height) / cgrid->char_height;

	update_region (cgrid, x1, y1, (x2 - x1) + 1, (y2 - y1) + 1);
}

static void
gmc_char_grid_draw (GtkWidget *widget, GdkRectangle *area)
{
	GmcCharGrid *cgrid;
	GdkRectangle w_area;
	GdkRectangle p_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (widget));
	g_return_if_fail (area != NULL);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		cgrid = GMC_CHAR_GRID (widget);

		/* Offset the area because the window does not fill thea allocation */

		area->x -= (widget->allocation.width - cgrid->char_width * cgrid->width) / 2;
		area->y -= (widget->allocation.height - cgrid->char_height * cgrid->height) / 2;

		w_area.x = 0;
		w_area.y = 0;
		w_area.width = cgrid->char_width * cgrid->width;
		w_area.height = cgrid->char_height * cgrid->height;

		if (gdk_rectangle_intersect (area, &w_area, &p_area))
			paint (cgrid, &p_area);
	}
}

static gint
gmc_char_grid_expose (GtkWidget *widget, GdkEventExpose *event)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GMC_IS_CHAR_GRID (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_WIDGET_DRAWABLE (widget))
		paint (GMC_CHAR_GRID (widget), &event->area);

	return FALSE;
}

void
gmc_char_grid_clear (GmcCharGrid *cgrid, int x, int y, int width, int height, GdkColor *bg)
{
	int x1, y1, x2, y2;
	int xx, yy;
	char *ch;
	struct attr *attrs;

	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	x1 = MAX (x, 0);
	y1 = MAX (y, 0);
	x2 = MIN (x + width, cgrid->width);
	y2 = MIN (y + height, cgrid->height);

	ch = CHARS (cgrid) + (y1 * cgrid->width);
	attrs = ATTRS (cgrid) + (y1 * cgrid->width);

	for (yy = y1; yy < y2; yy++) {
		for (xx = x1; xx < x2; xx++) {
			ch[xx] = ' ';

			attrs[xx].bg = bg ? bg->pixel : 0;
			attrs[xx].bg_set = bg ? TRUE : FALSE;
		}

		ch += cgrid->width;
		attrs += cgrid->width;
	}

	if (GTK_WIDGET_DRAWABLE (cgrid) && !cgrid->frozen)
		update_region (cgrid, x, y, width, height);
}

void
gmc_char_grid_put_char (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char ch)
{
	char *chars;
	struct attr *attrs;
		
	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	if ((x < 0) || (x >= cgrid->width)
	    || (y < 0) || (y >= cgrid->height))
		return;

	chars = CHARS (cgrid) + (y * cgrid->width + x);
	attrs = ATTRS (cgrid) + (y * cgrid->width + x);

	*chars = ch;

	attrs->fg = fg ? fg->pixel : 0;
	attrs->fg_set = fg ? TRUE : FALSE;

	attrs->bg = bg ? bg->pixel : 0;
	attrs->bg_set = bg ? TRUE : FALSE;

	if (GTK_WIDGET_DRAWABLE (cgrid) && !cgrid->frozen)
		update_region (cgrid, x, y, 1, 1);
}

void
gmc_char_grid_put_string (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char *str)
{
	g_return_if_fail (str != NULL);

	gmc_char_grid_put_text (cgrid, x, y, fg, bg, str, strlen (str));
}

void
gmc_char_grid_put_text (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char *text, int length)
{
	char *chars;
	struct attr *attrs;
	int i, pos;

	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));
	g_return_if_fail (text != NULL);

	if ((x < 0) || (x >= cgrid->width)
	    || (y < 0) || (y >= cgrid->height))
		return;

	chars = CHARS (cgrid) + (cgrid->width * y + x);
	attrs = ATTRS (cgrid) + (cgrid->width * y + x);

	for (i = 0, pos = x; (i < length) && (pos < cgrid->width); i++, pos++) {
		*chars++ = *text++;

		attrs->fg = fg ? fg->pixel : 0;
		attrs->fg_set = fg ? TRUE : FALSE;

		attrs->bg = bg ? bg->pixel : 0;
		attrs->bg_set = bg ? TRUE : FALSE;

		attrs++;
	}

	if (GTK_WIDGET_DRAWABLE (cgrid) && !cgrid->frozen)
		update_region (cgrid, x, y, i, 1);
}

void
gmc_char_grid_set_font (GmcCharGrid *cgrid, const char *font_name)
{
	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	if (cgrid->font)
		gdk_font_unref (cgrid->font);

	cgrid->font = gdk_font_load (font_name);

	if (!cgrid->font)
		cgrid->font = gdk_font_load (DEFAULT_FONT);

	cgrid->char_width = gdk_char_width (cgrid->font, ' '); /* assume monospaced font! */
	cgrid->char_height = cgrid->font->ascent + cgrid->font->descent;
	cgrid->char_y = cgrid->font->ascent;

	gtk_widget_queue_resize (GTK_WIDGET (cgrid));
}

void
gmc_char_grid_set_size (GmcCharGrid *cgrid, guint width, guint height)
{
	gtk_signal_emit (GTK_OBJECT (cgrid), cgrid_signals[SIZE_CHANGED], width, height);
}

void
gmc_char_grid_get_size (GmcCharGrid *cgrid, guint *width, guint *height)
{
	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	if (width)
		*width = cgrid->width;

	if (height)
		*height = cgrid->height;
}

void
gmc_char_grid_freeze (GmcCharGrid *cgrid)
{
	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	cgrid->frozen = TRUE;
}

void
gmc_char_grid_thaw (GmcCharGrid *cgrid)
{
	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));

	cgrid->frozen = FALSE;

	if (GTK_WIDGET_DRAWABLE (cgrid))
		gtk_widget_queue_draw (GTK_WIDGET (cgrid));
}

static void
gmc_char_grid_marshal_signal_1 (GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args)
{
	GmcCharGridSignal1 rfunc;

	rfunc = (GmcCharGridSignal1) func;

	(*rfunc) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_UINT (args[1]), func_data);
}

static void
gmc_char_grid_real_size_changed (GmcCharGrid *cgrid, guint width, guint height)
{
	int i;
	char *chars;
	struct attr *attrs;

	g_return_if_fail (cgrid != NULL);
	g_return_if_fail (GMC_IS_CHAR_GRID (cgrid));
	g_return_if_fail ((width > 0) && (height > 0));

	if ((width == cgrid->width) && (height == cgrid->height))
		return;

	if (cgrid->chars)
		g_free (cgrid->chars);

	if (cgrid->attrs)
		g_free (cgrid->attrs);

	cgrid->width = width;
	cgrid->height = height;

	chars = g_new (char, width * height);
	cgrid->chars = chars;

	attrs = g_new (struct attr, width * height);
	cgrid->attrs = attrs;

	for (i = 0; i < (width * height); i++) {
		chars[i] = ' ';
		attrs[i].fg = 0;
		attrs[i].bg = 0;
		attrs[i].fg_set = FALSE;
		attrs[i].bg_set = FALSE;
	}

	gtk_widget_queue_resize (GTK_WIDGET (cgrid));
}
