/*
 * Widgets for the GNOME edition of the Midnight Commander
 *
 * Copyright (C) 1997 The Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@gnu.org)
 *
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "x.h"
#include "gwidget.h"
#include "dlg.h"

GtkWidget *
get_gtk_widget (Widget_Item *p)
{
	GtkWidget *w = GTK_WIDGET (p->widget->wdata);

	if (GNOME_IS_ENTRY (w))
		return (gnome_entry_gtk_entry ((GnomeEntry *)(w)));
	else 
		return (GTK_WIDGET (p->widget->wdata));
}

void
x_dialog_stop (Dlg_head *h)
{
	if (h->grided & DLG_GNOME_APP)
		return;
	gtk_main_quit ();
}

void
x_focus_widget (Widget_Item *p)
{
	GtkWidget *w = get_gtk_widget (p);

	gtk_widget_grab_focus (w);
}

void
x_unfocus_widget (Widget_Item *p)
{
	GtkWidget *w = get_gtk_widget (p);
	GtkWidget *toplevel = gtk_widget_get_toplevel (w);

	/* Only happens if the widget is not yet added to its container */
	/* I am not yet sure why this happens */
	if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_focus (GTK_WINDOW (gtk_widget_get_toplevel (w)), NULL);
}

void
x_destroy_cmd (void *w)
{
	Widget *widget = (Widget *) w;

	if (!widget->wdata)
		return;
	gtk_widget_destroy (GTK_WIDGET(widget->wdata));
}

/* Buttons */
static void
gbutton_callback (GtkWidget *w, void *data)
{
	WButton *b = data;
	Dlg_head *h = (Dlg_head *) b->widget.parent;
	int stop = 0;

	if (b->callback)
		stop = (*b->callback)(b->action, b->callback_data);
	
	if (!b->callback || stop){
		h->ret_value = b->action;
		dlg_stop (h);
	}
}

int
x_create_button (Dlg_head *h, widget_data parent, WButton *b)
{
	GtkWidget *button;
	char *stock;
	int tag;

	if (strcasecmp (b->text, _("ok")) == 0)
		stock = GNOME_STOCK_BUTTON_OK;
	else if (strcasecmp (b->text, _("cancel")) == 0)
		stock = GNOME_STOCK_BUTTON_CANCEL;
	else if (strcasecmp (b->text, _("help")) == 0)
		stock = GNOME_STOCK_BUTTON_HELP;
	else if (strcasecmp (b->text, _("yes")) == 0)
		stock = GNOME_STOCK_BUTTON_YES;
	else if (strcasecmp (b->text, _("no")) == 0)
		stock = GNOME_STOCK_BUTTON_NO;
	else if (strcasecmp (b->text, _("exit")) == 0)
		stock = GNOME_STOCK_BUTTON_CLOSE;
	else if (strcasecmp (b->text, _("abort")) == 0)
		stock = GNOME_STOCK_BUTTON_CANCEL;
	else
		stock = 0;

	if (stock){
		button = gnome_stock_button (stock);
	} else 
		button = gtk_button_new_with_label (b->text);

	if (b->flags == DEFPUSH_BUTTON){
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_grab_default (button);
	}
	gtk_widget_show (button);
	tag = gtk_signal_connect (GTK_OBJECT(button), "clicked", (GtkSignalFunc) gbutton_callback, b);
	gtk_object_set_data (GTK_OBJECT (button), "click-signal-tag", (void *) tag);
	b->widget.wdata = (widget_data) button;
		
	return 1;
}

static void
x_set_text (GtkWidget *w, gpointer data)
{
	if (!GTK_IS_LABEL (w))
		return;
	gtk_label_set (GTK_LABEL(w), data);
}

void
x_button_set (WButton *b, char *text)
{
	GtkWidget *button = GTK_WIDGET (b->widget.wdata);

	gtk_container_foreach (GTK_CONTAINER (button), x_set_text, text);
}

/* Radio buttons */

void
x_radio_focus_item (WRadio *radio)
{
	GList  *children = GTK_BOX (radio->widget.wdata)->children;
	int    i;
	
	for (i = 0; i < radio->count; i++){
		if (i == radio->pos){
			GtkBoxChild *bc = (GtkBoxChild *) children->data;
			
			gtk_widget_grab_focus (GTK_WIDGET (bc->widget));
			break;
		}
		children = children->next;
	}
}

void
x_radio_toggle (WRadio *radio)
{
	GList  *children = GTK_BOX (radio->widget.wdata)->children;
	int    i;
	
	for (i = 0; i < radio->count; i++){
		GtkBoxChild *bc = (GtkBoxChild *) children->data;

		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (bc->widget), (i == radio->sel) ? 1 : 0);
		children = children->next;
	}
}

static void
radio_toggle (GtkObject *object, WRadio *r)
{
	int idx = (int) gtk_object_get_data (object, "index");

	if (!GTK_TOGGLE_BUTTON (object)->active)
		return;

	g_return_if_fail (idx != 0);
	idx--;
	r->sel = idx;
}

static char *
remove_hotkey (char *text)
{
	char *t = g_strdup (text);
	char *p = strchr (t,'&');

	if (p)
		strcpy (p, p+1);

	return t;
}

int
x_create_radio (Dlg_head *h, widget_data parent, WRadio *r)
{
	GtkWidget *w, *vbox;
	int i;

        vbox = gtk_vbox_new (0, 0);
	for (i = 0; i < r->count; i++){
		char *text = remove_hotkey (_(r->texts [i]));
		
		if (i == 0){
			w = gtk_radio_button_new_with_label (NULL, text);
			r->first_gtk_radio = w;
		} else
			w = gtk_radio_button_new_with_label_from_widget (r->first_gtk_radio, text);

		g_free (text);
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w), (i == r->sel));
		gtk_signal_connect (GTK_OBJECT (w), "toggled", GTK_SIGNAL_FUNC (radio_toggle), r);
		gtk_object_set_data (GTK_OBJECT (w), "index", (void *) (i+1));
		gtk_box_pack_start_defaults (GTK_BOX (vbox), w);
	}
	gtk_widget_show_all (vbox);
			    
	r->widget.wdata = (widget_data) vbox;

	return 1;
}

static void
x_check_changed (GtkToggleButton *t, WCheck *c)
{
	c->state ^= C_BOOL;
	c->state ^= C_CHANGE;
}

/* Check buttons */
int
x_create_check (Dlg_head *h, widget_data parent, WCheck *c)
{
	GtkWidget *w;

	w = gtk_check_button_new_with_label (c->text);
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w), (c->state & C_BOOL));
	gtk_signal_connect (GTK_OBJECT (w), "toggled", GTK_SIGNAL_FUNC (x_check_changed), c);
	gtk_widget_show (w);
	c->widget.wdata = (widget_data) w;
	return 1;
}

/* Input lines */
static void
entry_click (GtkWidget *widget, GdkEvent *event, WInput *in)
{
	dlg_select_widget (in->widget.parent, in);
}

static void
entry_release (GtkEditable *entry, GdkEvent *event, WInput *in)
{
	in->point = entry->current_pos;
	in->mark  = (entry->current_pos == entry->selection_start_pos) ?
		entry->selection_end_pos : entry->selection_start_pos;
	if (in->point != in->mark)
		in->first = 1;
}

int
x_create_input (Dlg_head *h, widget_data parent, WInput *in)
{
	GtkWidget *gnome_entry;
	GtkEntry *entry;

	/* The widget might have been initialized manually.
	 * Look in gscreen.c for an example
	 */
	if (in->widget.wdata)
		return 1;

#ifdef USE_GNOME_ENTRY
	gnome_entry = gnome_entry_new (in->widget.tkname);
#else
	entry = GTK_ENTRY (gnome_entry = gtk_entry_new ());
	gtk_entry_set_visibility (entry, !in->is_password);
#endif
	gtk_widget_show (gnome_entry);
	in->widget.wdata = (widget_data) gnome_entry;

#ifdef USE_GNOME_ENTRY
	entry = GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (gnome_entry)));
#endif
	gtk_entry_set_text (entry, in->buffer);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	gtk_entry_set_position (entry, in->point);
	
	gtk_signal_connect (GTK_OBJECT (entry), "button_press_event",
			    GTK_SIGNAL_FUNC (entry_click), in);

	gtk_signal_connect (GTK_OBJECT (entry), "button_release_event",
			    GTK_SIGNAL_FUNC (entry_release), in);
	return 1;
}

void
x_update_input (WInput *in)
{
	GnomeEntry *gnome_entry;
	GtkEntry   *entry;
	char       *text;
	int        draw = 0;
	
	/* If the widget has not been initialized yet (done by WIDGET_INIT) */
	if (!in->widget.wdata)
		return;

#ifdef USE_GNOME_ENTRY
	gnome_entry = GNOME_ENTRY (in->widget.wdata);
	entry = GTK_ENTRY (gnome_entry_gtk_entry (gnome_entry));
#else
	entry = GTK_ENTRY (in->widget.wdata);
#endif

	if (in->first == -1){
		gtk_editable_select_region (GTK_EDITABLE (entry), 0, 0);
		in->first = 0;
	}

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	
	if (text && strcmp (text, in->buffer)){
		gtk_entry_set_text (entry, in->buffer);
		draw = 1;
	}      

	if (GTK_EDITABLE (entry)->current_pos != in->point){
		gtk_entry_set_position (entry, in->point);
		draw = 1;
	}
	
	if (draw){
#ifdef USE_GNOME_ENTRY
		gtk_widget_draw (GTK_WIDGET (gnome_entry), NULL);
#else
		gtk_widget_draw (GTK_WIDGET (entry), NULL);
#endif
		gtk_editable_changed (GTK_EDITABLE (entry));
		gtk_widget_queue_draw (GTK_WIDGET (entry));
	}
}

/* Listboxes */
static GtkWidget *
listbox_pull (widget_data data)
{
	return GTK_BIN (data)->child;
}

void
x_listbox_select_nth (WListbox *l, int nth)
{
	static int inside;
	GtkCList *clist;
	
	if (inside)
		return;

	if (!l->widget.wdata)
		return;
	
	inside = 1;
	clist = GTK_CLIST (listbox_pull (l->widget.wdata));
	
	gtk_clist_select_row (clist, nth, 0);
	if (gtk_clist_row_is_visible (clist, nth) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto (clist, nth, 0, 0.5, 0.0);
	
	inside = 0;
}

void
x_listbox_delete_nth (WListbox *l, int nth)
{
	gtk_clist_remove (GTK_CLIST (listbox_pull (l->widget.wdata)), nth);
}

static void
listbox_select (GtkWidget *widget, int row, int column, GdkEvent *event, WListbox *l)
{
	Dlg_head *h = l->widget.parent;
	static int inside;

	if (inside)
		return;
	inside = 1;

	listbox_select_by_number (l, row);
	
	if (!event){
		inside = 0;
		return;
	}

	
	if (event->type == GDK_2BUTTON_PRESS){
		switch (l->action){
		case listbox_nothing:
			break;
			
		case listbox_finish:
			h->running   = 0;
			h->ret_value = B_ENTER;
			gtk_main_quit ();
			return;
			
		case listbox_cback:
			if ((*l->cback)(l) == listbox_finish){
				gtk_main_quit ();
				return;
			}
		}
	}

	/* Send an artificial DLG_POST_KEY */
	if (event->type == GDK_BUTTON_PRESS)
		(*l->widget.parent->callback)(l->widget.parent, 0, DLG_POST_KEY);

	inside = 0;
}

int
x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l)
{
	GtkWidget *listbox, *sw;
	GtkRequisition req;
	WLEntry *p;
	int i;
	
	listbox = gtk_clist_new (1);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (sw), listbox);
	
	gtk_clist_set_selection_mode (GTK_CLIST (listbox), GTK_SELECTION_BROWSE);
	gtk_widget_size_request (listbox, &req);
	gtk_widget_set_usize (listbox, req.width, req.height + 20*8);
	gtk_signal_connect (GTK_OBJECT (listbox), "select_row",
			    GTK_SIGNAL_FUNC (listbox_select), l);
	l->widget.wdata = (widget_data) sw;
	gtk_widget_show (listbox);
	
	for (p = l->list, i = 0; i < l->count; i++, p = p->next){
		char *text [1];

		text [0] = p->text;
		gtk_clist_append (GTK_CLIST (listbox), text);
	}
	x_listbox_select_nth (l, l->pos);
	return 1;
}

void
x_list_insert (WListbox *l, WLEntry *p, WLEntry *e)
{
	int pos = 0, i;
	char *text [1];

	if (!l->widget.wdata)
		return;

	for (i = 0; i < l->count; i++){
		if (p == e)
			break;
		p = p->next;
		pos++;
	} 

	if (p != e){
		printf ("x_list_insert: should not happen!\n");
		return;
	}
	text [0] = e->text;
	gtk_clist_append (GTK_CLIST (listbox_pull (l->widget.wdata)), text);
}

/* Labels */
int
x_create_label (Dlg_head *g, widget_data parent, WLabel *l)
{
	GtkWidget *label;

	/* Tempo-hack */
	if (*l->text == 0){
		if (0)
			label = gtk_label_new (l->widget.tkname);
		else
			label = gtk_label_new ("");
	} else
		label = gtk_label_new (l->text);
	gtk_widget_show (label);
	l->widget.wdata = (widget_data) label;

	return 1;
}

void
x_label_set_text (WLabel *label, char *text)
{
	if (label->widget.wdata)
		gtk_label_set (GTK_LABEL (label->widget.wdata), text);
}

/* Buttonbar */
static void
buttonbar_clicked (GtkWidget *widget, WButtonBar *bb)
{
	GtkBox *box = GTK_BOX (widget->parent);
	GList *children = box->children;
	int i;
	
	/* Find out which button we are (number) */
	for (i = 0; children; children = children->next, i++){
		if (((GtkBoxChild *)children->data)->widget == widget){
			if (bb->labels [i].function)
				(*bb->labels [i].function)(bb->labels [i].data);
			return;
		}
	}
	printf ("Mhm, should not happen or The Cow is out\n");
}

int
x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb)
{
	GtkWidget *hbox;
	int i;
	
	hbox = gtk_hbox_new (0, 0);
#if 0
	for (i = 0; i < 10; i++){
		char buffer [40];
		GtkButton *b;

		sprintf (buffer, "F%d %s", i+1, bb->labels [i].text ? bb->labels [i].text : "      ");
		b = (GtkButton *) gtk_button_new_with_label (buffer);
		gtk_signal_connect (GTK_OBJECT (b), "clicked",
				    GTK_SIGNAL_FUNC (buttonbar_clicked), bb);
		gtk_widget_show (GTK_WIDGET (b));
		gtk_box_pack_start_defaults (GTK_BOX (hbox), (GtkWidget *)b);
	}
	gtk_widget_show (hbox);
#endif
	bb->widget.wdata = (widget_data) hbox;
	return 1;
}

void
x_redefine_label (WButtonBar *bb, int idx)
{
#if 0
	GtkBox *box = GTK_BOX (bb->widget.wdata);
	GtkBoxChild *bc = (GtkBoxChild *)g_list_nth (box->children, idx)->data;
	GtkButton *button = GTK_BUTTON (bc->widget);
	GtkWidget *label = gtk_label_new (bb->labels [idx].text);

	gtk_widget_show (label);
	gtk_container_remove (GTK_CONTAINER (button), button->child);
	gtk_container_add (GTK_CONTAINER (button), label);
#endif
}

/* Gauges */
int
x_create_gauge (Dlg_head *h, widget_data parent, WGauge *g)
{
	GtkWidget *pbar;

	pbar = gtk_progress_bar_new ();
	gtk_widget_show (pbar);
	g->widget.wdata = (widget_data) pbar;

	return 1;
}

void
x_gauge_show (WGauge *g)
{
	gtk_widget_show (GTK_WIDGET (g->widget.wdata));
}

void
x_gauge_set_value (WGauge *g, int max, int current)
{
	gtk_progress_bar_update (GTK_PROGRESS_BAR (g->widget.wdata), ((gfloat) current / (gfloat) max));
}
