#ifndef _GNOME_TREE_H
#define _GNOME_TREE_H

#include <gtk/gtkctree.h>

#define GTK_TYPE_DTREE            (gtk_dtree_get_type ())
#define GTK_DTREE(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_DTREE, GtkDTree))
#define GTK_DTREE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_DTREE, GtkDTreeClass))
#define GTK_IS_DTREE(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_DTREE))
#define GTK_IS_DTREE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DTREE))

typedef struct {
	GtkCTree ctree;

	char         *current_path;
	GtkCTreeNode *root_node;	/* root node */
	GtkCTreeNode *last_node;	/* last visited node */

	/* Pixmaps for showing directories */
	GdkPixmap *pixmap_open;	
	GdkPixmap *pixmap_close;

	/* Masks */
	GdkBitmap *bitmap_open;
	GdkBitmap *bitmap_close;
} GtkDTree;

typedef struct {
	GtkCTreeClass parent_class;
	
	void (* directory_changed) (GtkDTree *dtree, char *directory);
} GtkDTreeClass;

guint      gtk_dtree_get_type           (void);
GtkWidget *gtk_dtree_new                ();
void       gtk_dtree_select_parent      (GtkDTree *dtree,
					 char *directory);
void       gtk_dtree_select_child       (GtkDTree *dtree);
void       gtk_dtree_remove_dir_by_name (GtkDTree *dtree,
					 char *directory);
gboolean   gtk_dtree_select_dir         (GtkDTree *dtree,
					 char *directory);
#endif
