/*
 * (C) 1998 The Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 *
 * Clipped label: This label is differnt from the one on Gtk as it
 * does requests no space, but uses all that is given to it.
 * 
 * It does not queue an resize when changing the text.
 * */

#include <string.h>
#include "gcliplabel.h"


static void gtk_clip_label_class_init    (GtkClipLabelClass  *klass);
static void gtk_clip_label_size_request  (GtkWidget          *widget,
				          GtkRequisition     *requisition);
static void gtk_clip_label_size_allocate (GtkWidget	      *widget,
					  GtkAllocation      *allocation);

static GtkLabelClass *parent_class = NULL;

guint
gtk_clip_label_get_type ()
{
	static guint label_type = 0;
	
	if (!label_type)
	{
		GtkTypeInfo clip_label_info =
		{
			"GtkClipLabel",
			sizeof (GtkClipLabel),
			sizeof (GtkClipLabelClass),
			(GtkClassInitFunc) gtk_clip_label_class_init,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		label_type = gtk_type_unique (gtk_label_get_type (), &clip_label_info);
	}
	
	return label_type;
}

static void
gtk_clip_label_class_init (GtkClipLabelClass *class)
{
	GtkWidgetClass *widget_class;
	
	parent_class = gtk_type_class (gtk_label_get_type ());
	
	widget_class = (GtkWidgetClass*) class;
	widget_class->size_request = gtk_clip_label_size_request;
	widget_class->size_allocate = gtk_clip_label_size_allocate;
}

void
gtk_clip_label_set (GtkLabel *label,
		    const char *str)
{
	char* p;
	
	g_return_if_fail (label != NULL);
	g_return_if_fail (GTK_IS_LABEL (label));
	g_return_if_fail (str != NULL);
	
	if (label->label)
		g_free (label->label);
	label->label = g_strdup (str);
	
	if (label->row)
		g_slist_free (label->row);
	label->row = NULL;
	label->row = g_slist_append (label->row, label->label);
	p = label->label;
	while ((p = strchr(p, '\n')))
		label->row = g_slist_append (label->row, ++p);
	
	if (GTK_WIDGET_VISIBLE (label))
	{
		if (GTK_WIDGET_MAPPED (label))
			gdk_window_clear_area (GTK_WIDGET (label)->window,
					       GTK_WIDGET (label)->allocation.x,
					       GTK_WIDGET (label)->allocation.y,
					       GTK_WIDGET (label)->allocation.width,
					       GTK_WIDGET (label)->allocation.height);
		gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

GtkWidget*
gtk_clip_label_new (const char *str)
{
	GtkClipLabel *label;
	
	g_return_val_if_fail (str != NULL, NULL);

	label = gtk_type_new (gtk_clip_label_get_type ());
	
	gtk_label_set (GTK_LABEL (label), str);
	
	return GTK_WIDGET (label);
}

static void
gtk_clip_label_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
	GtkClipLabel *label;
	GSList *row;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_CLIP_LABEL (widget));
	g_return_if_fail (requisition != NULL);
	
	label = GTK_CLIP_LABEL (widget);
	
	((GtkWidgetClass *)parent_class)->size_request (widget, requisition);
	
	requisition->width = GTK_LABEL (label)->misc.xpad * 2;
}

static void
gtk_clip_label_size_allocate (GtkWidget     *widget,
			      GtkAllocation *alloc)
{
	((GtkWidgetClass *)parent_class)->size_allocate (widget, alloc);
	
	widget->requisition.width = alloc->width;
}




