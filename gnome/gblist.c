/*
 * gblist:  Implements a GtkCList derived widget that does not take any action
 *          on select and unselect calls.
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 * (C) 1998 the Free Software Foundation.
 */
#include <stdlib.h>
#include "config.h"
#include "gblist.h"

static void
blist_select_row (GtkCList *clist, gint row, gint column, GdkEvent *event)
{
	/* nothing */
}

static void
blist_unselect_row (GtkCList *clist, gint row, gint column, GdkEvent *event)
{
	/* nothing */
}

static void
gtk_blist_class_init (GtkBListClass *klass)
{
	GtkCListClass *clist_class = (GtkCListClass *) klass;

	clist_class->select_row   = blist_select_row;
	clist_class->unselect_row = blist_unselect_row;
}

GtkType
gtk_blist_get_type (void)
{
	static GtkType blist_type = 0;
	
	if (!blist_type){
		GtkTypeInfo blist_info =
		{
			"GtkBList",
			sizeof (GtkBList),
			sizeof (GtkBListClass),
			(GtkClassInitFunc) gtk_blist_class_init,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};
		
		blist_type = gtk_type_unique (gtk_clist_get_type (), &blist_info);
	}
	
	return blist_type;
}

GtkWidget *
gtk_blist_new_with_titles (gint columns, gchar * titles[])
{
	GtkWidget *widget;

	g_return_val_if_fail (titles != NULL, NULL);
	
	widget = gtk_type_new (gtk_blist_get_type ());
	
	gtk_clist_construct (GTK_CLIST (widget), columns, titles);
	
	return widget;
}
