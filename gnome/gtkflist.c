/* File list widget for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include "gtkflist.h"


enum {
	ROW_POPUP_MENU,
	EMPTY_POPUP_MENU,
	OPEN_ROW,
	START_DRAG,
	LAST_SIGNAL
};


static void gtk_flist_class_init (GtkFListClass *class);
static void gtk_flist_init (GtkFList *flist);

static gint gtk_flist_button_press (GtkWidget *widget, GdkEventButton *event);
static gint gtk_flist_button_release (GtkWidget *widget, GdkEventButton *event);
static gint gtk_flist_motion (GtkWidget *widget, GdkEventMotion *event);
static gint gtk_flist_key (GtkWidget *widget, GdkEventKey *event);
static void gtk_flist_drag_begin (GtkWidget *widget, GdkDragContext *context);
static void gtk_flist_drag_end (GtkWidget *widget, GdkDragContext *context);
static void gtk_flist_drag_data_get (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *data, guint info, guint time);
static void gtk_flist_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time);
static gboolean gtk_flist_drag_motion (GtkWidget *widget, GdkDragContext *context,
				       gint x, gint y, guint time);
static gboolean gtk_flist_drag_drop (GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, guint time);
static void gtk_flist_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					  gint x, gint y, GtkSelectionData *data,
					  guint info, guint time);

static void gtk_flist_clear (GtkCList *clist);


static GtkCListClass *parent_class;

static guint flist_signals[LAST_SIGNAL];


/**
 * gtk_flist_get_type:
 * @void: 
 * 
 * Creates the GtkFList class and its type information
 * 
 * Return value: The type ID for GtkFListClass
 **/
GtkType
gtk_flist_get_type (void)
{
	static GtkType flist_type = 0;

	if (!flist_type) {
		GtkTypeInfo flist_info = {
			"GtkFList",
			sizeof (GtkFList),
			sizeof (GtkFListClass),
			(GtkClassInitFunc) gtk_flist_class_init,
			(GtkObjectInitFunc) gtk_flist_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		flist_type = gtk_type_unique (gtk_clist_get_type (), &flist_info);
	}

	return flist_type;
}

/* Standard class initialization function */
static void
gtk_flist_class_init (GtkFListClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	clist_class = (GtkCListClass *) class;

	parent_class = gtk_type_class (gtk_clist_get_type ());

	flist_signals[ROW_POPUP_MENU] =
		gtk_signal_new ("row_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, row_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
	flist_signals[EMPTY_POPUP_MENU] =
		gtk_signal_new ("empty_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, empty_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
	flist_signals[OPEN_ROW] =
		gtk_signal_new ("open_row",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, open_row),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	flist_signals[START_DRAG] =
		gtk_signal_new ("start_drag",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, start_drag),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_EVENT);

	gtk_object_class_add_signals (object_class, flist_signals, LAST_SIGNAL);

	clist_class->clear = gtk_flist_clear;

	widget_class->button_press_event = gtk_flist_button_press;
	widget_class->button_release_event = gtk_flist_button_release;
	widget_class->motion_notify_event = gtk_flist_motion;
	widget_class->key_press_event = gtk_flist_key;
	widget_class->key_release_event = gtk_flist_key;
	widget_class->drag_begin = gtk_flist_drag_begin;
	widget_class->drag_end = gtk_flist_drag_end;
	widget_class->drag_data_get = gtk_flist_drag_data_get;
	widget_class->drag_leave = gtk_flist_drag_leave;
	widget_class->drag_motion = gtk_flist_drag_motion;
	widget_class->drag_drop = gtk_flist_drag_drop;
	widget_class->drag_data_received = gtk_flist_drag_data_received;
}

/* Standard object initialization function */
static void
gtk_flist_init (GtkFList *flist)
{
	flist->anchor_row = -1;

	/* GtkCList does not specify pointer motion by default */
	gtk_widget_add_events (GTK_WIDGET (flist), GDK_POINTER_MOTION_MASK);
}

/* Selects the rows between the anchor to the specified row, inclusive.  */
static void
select_range (GtkFList *flist, int row)
{
	int min, max;
	int i;

	if (flist->anchor_row == -1)
		flist->anchor_row = row;

	if (row < flist->anchor_row) {
		min = row;
		max = flist->anchor_row;
	} else {
		min = flist->anchor_row;
		max = row;
	}

	for (i = min; i <= max; i++)
		gtk_clist_select_row (GTK_CLIST (flist), i, 0);
}

/* Handles row selection according to the specified modifier state */
static void
select_row (GtkFList *flist, int row, guint state)
{
	int range, additive;

	range = (state & GDK_SHIFT_MASK) != 0;
	additive = (state & GDK_CONTROL_MASK) != 0;

	if (!additive)
		gtk_clist_unselect_all (GTK_CLIST (flist));

	if (!range) {
		if (additive) {
			if (flist->panel->dir.list[row].f.marked)
				gtk_clist_unselect_row (GTK_CLIST (flist), row, 0);
			else
				gtk_clist_select_row (GTK_CLIST (flist), row, 0);
		} else if (!flist->panel->dir.list[row].f.marked)
			gtk_clist_select_row (GTK_CLIST (flist), row, 0);

		flist->anchor_row = row;
	} else
		select_range (flist, row);
}

/* Our handler for button_press events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkFList *flist;
	GtkCList *clist;
	int on_row;
	gint row, col;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1 || event->button == 2) {
			if (on_row) {
				file_entry *fe;

				fe = &flist->panel->dir.list[row];

				/* Save the mouse info for DnD */

				flist->dnd_press_button = event->button;
				flist->dnd_press_x = event->x;
				flist->dnd_press_y = event->y;

				/* Handle selection */

				if ((fe->f.marked
				     && !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				    || ((event->state & GDK_CONTROL_MASK)
					&& !(event->state & GDK_SHIFT_MASK))) {
					flist->dnd_select_pending = TRUE;
					flist->dnd_select_pending_state = event->state;
					flist->dnd_select_pending_row = row;
				} else
					select_row (flist, row, event->state);
			} else
				gtk_clist_unselect_all (clist);

			retval = TRUE;
		} else if (event->button == 3) {
			if (on_row) {
				file_entry *fe;

				fe = &flist->panel->dir.list[row];
				if (!fe->f.marked)
					select_row (flist, row, event->state);

				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[ROW_POPUP_MENU],
						 event);
			} else
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[EMPTY_POPUP_MENU],
						 event);

			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button != 1)
			break;

		flist->dnd_select_pending = FALSE;
		flist->dnd_select_pending_state = 0;

		if (on_row)
			gtk_signal_emit (GTK_OBJECT (flist), flist_signals[OPEN_ROW]);

		retval = TRUE;
		break;

	default:
		break;
	}

	return retval;
}

/* Our handler for button_release events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_button_release (GtkWidget *widget, GdkEventButton *event)
{
	GtkFList *flist;
	GtkCList *clist;
	int on_row;
	gint row, col;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->button_release_event) (widget, event);

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	if (!(event->button == 1 || event->button == 2))
		return FALSE;

	flist->dnd_press_button = 0;
	flist->dnd_press_x = 0;
	flist->dnd_press_y = 0;

	if (on_row) {
		if (flist->dnd_select_pending) {
			select_row (flist, row, flist->dnd_select_pending_state);
			flist->dnd_select_pending = FALSE;
			flist->dnd_select_pending_state = 0;
		}

		retval = TRUE;
	}

	return retval;
}

/* Our handler for motion_notify events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GtkFList *flist;
	GtkCList *clist;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	if (!((flist->dnd_press_button == 1 && (event->state & GDK_BUTTON1_MASK))
	      || (flist->dnd_press_button == 2 && (event->state & GDK_BUTTON2_MASK))))
		return FALSE;

	/* This is the same threshold value that is used in gtkdnd.c */

	if (MAX (abs (flist->dnd_press_x - event->x),
		 abs (flist->dnd_press_y - event->y)) <= 3)
		return FALSE;

	/* Handle any pending selections */

	if (flist->dnd_select_pending) {
		if (!flist->panel->dir.list[flist->dnd_select_pending_row].f.marked)
			select_row (flist,
				    flist->dnd_select_pending_row,
				    flist->dnd_select_pending_state);

		flist->dnd_select_pending = FALSE;
		flist->dnd_select_pending_state = 0;
	}

	gtk_signal_emit (GTK_OBJECT (flist),
			 flist_signals[START_DRAG],
			 flist->dnd_press_button,
			 event);
	return TRUE;
}

/* Our handler for key_press and key_release events.  We do nothing, and we do
 * this to avoid GtkCList's broken behavior.
 */
static gint
gtk_flist_key (GtkWidget *widget, GdkEventKey *event)
{
	return FALSE;
}

/* We override the drag_begin signal to do nothing */
static void
gtk_flist_drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_end signal to do nothing */
static void
gtk_flist_drag_end (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_data_get signal to do nothing */
static void
gtk_flist_drag_data_get (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *data, guint info, guint time)
{
	/* nothing */
}

/* We override the drag_leave signal to do nothing */
static void
gtk_flist_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	/* nothing */
}

/* We override the drag_motion signal to do nothing */
static gboolean
gtk_flist_drag_motion (GtkWidget *widget, GdkDragContext *context,
				   gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_drop signal to do nothing */
static gboolean
gtk_flist_drag_drop (GtkWidget *widget, GdkDragContext *context,
				 gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_data_received signal to do nothing */
static void
gtk_flist_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					  gint x, gint y, GtkSelectionData *data,
					  guint info, guint time)
{
	/* nothing */
}

/* Our handler for the clear signal of the clist.  We have to reset the anchor
 * to null.
 */
static void
gtk_flist_clear (GtkCList *clist)
{
	GtkFList *flist;

	g_return_if_fail (clist != NULL);
	g_return_if_fail (GTK_IS_FLIST (clist));

	flist = GTK_FLIST (clist);
	flist->anchor_row = -1;

	if (parent_class->clear)
		(* parent_class->clear) (clist);
}

/**
 * gtk_flist_new_with_titles:
 * @panel: A panel
 * @columns: The number of columns in the list
 * @titles: The titles for the columns
 * 
 * Creates a new file list associated to a panel.
 * 
 * Return value: The newly-created file list.
 **/
GtkWidget *
gtk_flist_new_with_titles (WPanel *panel, int columns, char **titles)
{
	GtkFList *flist;

	g_return_val_if_fail (panel != NULL, NULL);

	flist = gtk_type_new (gtk_flist_get_type ());
	gtk_clist_construct (GTK_CLIST (flist), columns, titles);

	flist->panel = panel;

	return GTK_WIDGET (flist);
}
