#ifndef __MC_BLIST_H__
#define __MC_BLIST_H__

#include <gdk/gdk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkclist.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_BLIST(obj)          (GTK_CHECK_CAST ((obj), gtk_blist_get_type (), GtkBList))
#define GTK_BLIST_CLASS(klass)  (GTK_CHECK_CLASS_CAST ((klass), gtk_blist_get_type (), GtkBListClass))
#define GTK_IS_BLIST(obj)       (GTK_CHECK_TYPE ((obj), gtk_blist_get_type ()))

typedef struct 
{
	GtkCList clist;
} GtkBList;

typedef struct
{
  GtkCListClass parent_class;
}  GtkBListClass;

GtkType    gtk_blist_get_type (void);
GtkWidget *gtk_blist_new_with_titles (gint columns, gchar * titles[]);

#ifdef __cplusplus
}
#endif				/* __cplusplus */


#endif				/* __GTK_CLIST_H__ */
