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
	int tag;
		
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
		gtk_box_pack_start_defaults (GTK_BOX (vbox), w);
		gtk_widget_show (w);
	}
	gtk_widget_show (vbox);

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
int
x_create_input (Dlg_head *h, widget_data parent, WInput *in)
{
	GtkWidget *entry;

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	in->widget.wdata = (widget_data) entry;
	gtk_entry_set_text (GTK_ENTRY (entry), in->buffer);
	return 1;
}

void
x_update_input (WInput *in)
{
	GtkEntry *entry = GTK_ENTRY (in->widget.wdata);
	
	gtk_entry_set_text (entry, in->buffer);
	gtk_entry_set_position (entry, in->point);
}

/* Listboxes */
void
x_listbox_select_nth (WListbox *l, int nth)
{
}

void
x_listbox_delete_nth (WListbox *l, int nth)
{
}

int
x_create_listbox (Dlg_head *h, widget_data parent, WListbox *l)
{
	GtkWidget *listbox;

	listbox = gtk_clist_new (1);
	gtk_widget_show (listbox);
	l->widget.wdata = (widget_data) listbox;
	return 1;
}

void
x_list_insert (WListbox *l, WLEntry *p, WLEntry *e)
{
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
