/* GmcCharGrid Widget - Simple character grid for the gmc viewer
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GMC_CHARGRID_H
#define GMC_CHARGRID_H


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <libgnome/gnome-defs.h>


BEGIN_GNOME_DECLS


#define GMC_CHAR_GRID(obj)         GTK_CHECK_CAST (obj, gmc_char_grid_get_type (), GmcCharGrid)
#define GMC_CHAR_GRID_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gmc_char_grid_get_type (), GmcCharGridClass)
#define GMC_IS_CHAR_GRID(obj)      GTK_CHECK_TYPE (obj, gmc_char_grid_get_type ())


typedef struct _GmcCharGrid      GmcCharGrid;
typedef struct _GmcCharGridClass GmcCharGridClass;

struct _GmcCharGrid {
	GtkWidget widget;

	int width;
	int height;

	void *chars;
	void *attrs;

	int frozen : 1;

	GdkFont *font;
	GdkGC   *gc;

	int char_width;
	int char_height;
	int char_y;
};

struct _GmcCharGridClass {
	GtkWidgetClass parent_class;

	void (* size_changed) (GmcCharGrid *cgrid, guint width, guint height);
};


guint      gmc_char_grid_get_type   (void);
GtkWidget *gmc_char_grid_new        (void);

void       gmc_char_grid_clear      (GmcCharGrid *cgrid, int x, int y, int width, int height, GdkColor *bg);
void       gmc_char_grid_put_char   (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char ch);
void       gmc_char_grid_put_string (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char *str);
void       gmc_char_grid_put_text   (GmcCharGrid *cgrid, int x, int y, GdkColor *fg, GdkColor *bg, char *text, int length);

void       gmc_char_grid_set_font   (GmcCharGrid *cgrid, const char *font_name);
void       gmc_char_grid_set_size   (GmcCharGrid *cgrid, guint width, guint height);
void       gmc_char_grid_get_size   (GmcCharGrid *cgrid, guint *width, guint *height);

void       gmc_char_grid_freeze     (GmcCharGrid *cgrid);
void       gmc_char_grid_thaw       (GmcCharGrid *cgrid);


END_GNOME_DECLS

#endif
