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

void
x_focus_widget (Widget_Item *p)
{
	GtkWidget *w = GTK_WIDGET (p->widget->wdata);

	gtk_widget_grab_focus (GTK_WIDGET (p->widget->wdata));
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
		h->running = 0;
		h->ret_value = b->action;
		gtk_main_quit (); 
	}
}

int
x_create_button (Dlg_head *h, widget_data parent, WButton *b)
{
	GtkWidget *button;
	char *stock;
	int tag;

	if (strcasecmp (b->text, "ok") == 0)
		stock = GNOME_STOCK_BUTTON_OK;
	else if (strcasecmp (b->text, "cancel") == 0)
		stock = GNOME_STOCK_BUTTON_CANCEL;
	else if (strcasecmp (b->text, "help") == 0)
		stock = GNOME_STOCK_BUTTON_HELP;
	else if (strcasecmp (b->text, "yes") == 0)
		stock = GNOME_STOCK_BUTTON_YES;
	else if (strcasecmp (b->text, "no") == 0)
		stock = GNOME_STOCK_BUTTON_NO;
	else if (strcasecmp (b->text, "exit") == 0)
		stock = GNOME_STOCK_BUTTON_CLOSE;
	else
		stock = 0;

	if (stock){
		button = gnome_stock_button (stock);
	} else 
		button = gtk_button_new_with_label (b->text);
		
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

int
x_create_radio (Dlg_head *h, widget_data parent, WRadio *r)
{
	GtkWidget *w, *vbox;
	GSList *group;
	int i;

        vbox = gtk_vbox_new (0, 0);
	for (i = 0; i < r->count; i++){
		if (i == 0){
			w = gtk_radio_button_new_with_label (NULL, r->texts [i]);
			group = gtk_radio_button_group (GTK_RADIO_BUTTON (w));
		} else {
			w = gtk_radio_button_new_with_label (group, r->texts [i]);
		}
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w), (i == r->sel));
		gtk_signal_connect (GTK_OBJECT (w), "toggled", GTK_SIGNAL_FUNC (radio_toggle), r);
		gtk_object_set_data (GTK_OBJECT (w), "index", (void *) (i+1));
		gtk_box_pack_start_defaults (GTK_BOX (vbox), w);
	}
	gtk_widget_show_all (vbox);
			    
	r->widget.wdata = (widget_data) vbox;
	return 1;
}

/* Check buttons */
int
x_create_check (Dlg_head *h, widget_data parent, WCheck *c)
{
	GtkWidget *w;
	int i;

	w = gtk_check_button_new_with_label (c->text);
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
}

int
x_create_input (Dlg_head *h, widget_data parent, WInput *in)
{
	GtkWidget *entry;

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	in->widget.wdata = (widget_data) entry;
	gtk_entry_set_text (GTK_ENTRY (entry), in->buffer);
	gtk_entry_set_position (GTK_ENTRY (entry), in->point);
	
	gtk_signal_connect (GTK_OBJECT (entry), "button_press_event",
			    GTK_SIGNAL_FUNC (entry_click), in);

	gtk_signal_connect (GTK_OBJECT (entry), "button_release_event",
			    GTK_SIGNAL_FUNC (entry_release), in);
	return 1;
}

void
x_update_input (WInput *in)
{
	GtkEntry *entry = GTK_ENTRY (in->widget.wdata);

	/* If the widget has not been initialized yet (done by WIDGET_INIT) */
	if (!entry)
		return;
	
	gtk_entry_set_text (entry, in->buffer);
	printf ("POniendo el putno en %d\n", in->point);
	gtk_entry_set_position (entry, in->point);
	gtk_widget_draw (GTK_WIDGET (entry), NULL);
}

/* Listboxes */
void
x_listbox_select_nth (WListbox *l, int nth)
{
	static int inside;
	GtkCList *clist;
	
	if (inside)
		return;
	
	inside = 1;
	clist = GTK_CLIST (l->widget.wdata);
	
	gtk_clist_select_row (clist, nth, 0);
	if (!gtk_clist_row_is_visible (clist, nth))
		gtk_clist_moveto (clist, nth, 0, 0.5, 0.0);
	
	inside = 0;
}

void
x_listbox_delete_nth (WListbox *l, int nth)
{
	gtk_clist_remove (GTK_CLIST (l->widget.wdata), nth);
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
		printf ("Activando\n");
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
	if (event->type = GDK_BUTTON_PRESS){
		
	}
	inside = 0;
}

int
x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l)
{
	GtkWidget *listbox;
	GtkRequisition req;
	WLEntry *p;
	int i;
	
	listbox = gtk_clist_new (1);
	gtk_clist_set_selection_mode (GTK_CLIST (listbox), GTK_SELECTION_BROWSE);
	gtk_widget_size_request (listbox, &req);
	gtk_widget_set_usize (listbox, req.width, req.height + 20*8);
	gtk_signal_connect (GTK_OBJECT (listbox), "select_row",
			    GTK_SIGNAL_FUNC (listbox_select), l);
	gtk_widget_show (listbox);
	l->widget.wdata = (widget_data) listbox;

	for (p = l->list, i = 0; i < l->count; i++, p = p->next){
		char *text [1];

		text [0] = p->text;
		gtk_clist_append (GTK_CLIST (listbox), text);
	}
	
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
	gtk_clist_append (GTK_CLIST (l->widget.wdata), text);
}

/* Labels */
int
x_create_label (Dlg_head *g, widget_data parent, WLabel *l)
{
	GtkWidget *label;

	/* Tempo-hack */
	if (*l->text == 0)
		label = gtk_label_new (l->widget.tkname);
	else
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
int
x_create_buttonbar (Dlg_head *h, widget_data parent, WButtonBar *bb)
{
}

void
x_redefine_label (WButtonBar *bb, int idx)
{
}

int
x_find_buttonbar_check (WButtonBar *bb, Widget *paneletc)
{
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
