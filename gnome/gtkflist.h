/* File list widget for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GTKFLIST_H
#define GTKFLIST_H

#include "panel.h"
#include <gtk/gtkclist.h>


/* It is sad that we have to do this.  GtkCList's behavior is so broken that we
 * have to override all the event handlers and implement our own selection
 * behavior.  Sigh.
 */

#define TYPE_GTK_FLIST (gtk_flist_get_type ())
#define GTK_FLIST(obj)            (GTK_CHECK_CAST ((obj), TYPE_GTK_FLIST, GtkFList))
#define GTK_FLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_GTK_FLIST, GtkFListClass))
#define GTK_IS_FLIST(obj)         (GTK_CHECK_TYPE ((obj), TYPE_GTK_FLIST))
#define GTK_IS_FLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_GTK_FLIST))


typedef struct _GtkFList GtkFList;
typedef struct _GtkFListClass GtkFListClass;

struct _GtkFList {
	GtkCList clist;

	/* The panel we are associated to */
	WPanel *panel;

	/* The anchor row for range selections */
	int anchor_row;

	/* Mouse button and position saved on button press */
	int dnd_press_button;
	int dnd_press_x, dnd_press_y;

	/* Delayed selection information */
	int dnd_select_pending;
	guint dnd_select_pending_state;
	int dnd_select_pending_row;
};

struct _GtkFListClass {
	GtkCListClass parent_class;

	/* Signal: invoke the popup menu for rows */
	void (* row_popup_menu) (GtkFList *flist, GdkEventButton *event);

	/* Signal: invoke the popup menu for empty areas */
	void (* empty_popup_menu) (GtkFList *flist, GdkEventButton *event);

	/* Signal: open the file in the selected row */
	void (* open_row) (GtkFList *flist);

	/* Signal: initiate a drag and drop operation */
	void (* start_drag) (GtkFList *flist, gint button, GdkEvent *event);
};


GtkType gtk_flist_get_type (void);
GtkWidget *gtk_flist_new_with_titles (WPanel *panel, int columns, char **titles);


#endif
