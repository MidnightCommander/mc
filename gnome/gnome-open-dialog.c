/* gnome-open-dialog.c
 * Copyright (C) 1999  Red Hat Software. Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include "util.h"
#include <gnome.h>
#include "gnome-open-dialog.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

static void gnome_open_dialog_init		   (GnomeOpenDialog *open_dialog);
static void gnome_open_dialog_class_init           (GnomeOpenDialogClass *klass);
static void gnome_open_dialog_destroy              (GtkObject *object);
static void gnome_open_dialog_generate_tree_helper (GtkCTree *ctree, GtkCTreeNode *parent, GNode *node);
static GSList *get_presorted_from(char *dir);
static void gnome_open_dialog_destroy_func         (GtkCTree *ctree, GtkCTreeNode *node, gpointer data);

static GnomeDialogClass *parent_class = NULL;
#define SMALL_ICON_SIZE 20


GtkType
gnome_open_dialog_get_type (void)
{
	static GtkType open_dialog_type = 0;

	if (!open_dialog_type)
	{
		static const GtkTypeInfo open_dialog_info =
		{
			"GnomeOpenDialog",
			sizeof (GnomeOpenDialog),
			sizeof (GnomeOpenDialogClass),
			(GtkClassInitFunc) gnome_open_dialog_class_init,
			(GtkObjectInitFunc) gnome_open_dialog_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		open_dialog_type = gtk_type_unique (gnome_dialog_get_type (), &open_dialog_info);
	}

	return open_dialog_type;
}

static void
gnome_open_dialog_class_init (GnomeOpenDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gnome_dialog_get_type ());
	object_class->destroy = gnome_open_dialog_destroy;

}
static void
gnome_open_dialog_destroy_func (GtkCTree     *ctree,
				 GtkCTreeNode *node,
				 gpointer      data)
{
	GnomeDesktopEntry *gde;
	if (ctree == NULL || node == NULL)
		return;

	gde = (GnomeDesktopEntry *)gtk_ctree_node_get_row_data (GTK_CTREE (ctree),node);
	if (gde)
		gnome_desktop_entry_free (gde);
}
static void
gnome_open_dialog_destroy (GtkObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_OPEN_DIALOG (object));
	g_free (GNOME_OPEN_DIALOG (object)->file_name);
	gtk_ctree_post_recursive (GTK_CTREE (GNOME_OPEN_DIALOG (object)->ctree),
				  NULL,
				  gnome_open_dialog_destroy_func,
				  NULL);
	gtk_widget_unref (GNOME_OPEN_DIALOG (object)->ctree);
	gtk_widget_unref (GNOME_OPEN_DIALOG (object)->entry);
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
static void
gnome_open_dialog_init (GnomeOpenDialog *open_dialog)
{
  
}
/* Private internal functions */
/* this will help order our tree */
static gchar *
strip_name_from_gde (GnomeDesktopEntry *gde,
		     gboolean fname)
{
	gchar *retval = NULL;
	gchar *ptr;

	if (gde == NULL)
		return NULL;
	if (!fname)
		return (g_strdup (gde->name));

	if (strcmp (x_basename (gde->location), ".directory")) {
		return g_strdup (x_basename(gde->location));
	}

	ptr = strrchr (gde->location, '/');
	if (ptr)
		ptr--;
	while (ptr && ptr != gde->location && *ptr != '/') {
		ptr--;
	}
	if (ptr) {
		retval = g_strdup (ptr +1);
		ptr = strrchr (retval, '/');
		if (ptr)
			ptr[0] = '\000';
	}
	return retval;
}
static void
insert_node_custom (GSList *order,
		    GNode *sorted_list,
		    GNode *node,
		    gchar *name)
{
	/* This little insertion sort is pretty ugly, but I guess
	 * it works.  It'd be good to move this information into
	 * Gnome-libs, somewhere...
	 */
	GNode *temp_node;
	gchar *fname;
	gchar *fname2;

	g_return_if_fail (sorted_list != NULL);

	temp_node = sorted_list->children;
	while (order) {
		if (!strcmp ((gchar *) order->data, x_basename (name))) {
			if (temp_node)
				g_node_insert_before (sorted_list, temp_node, node);
			else
				g_node_append (sorted_list, node);
			return;
		}
		if (temp_node) {
			fname = strip_name_from_gde ((GnomeDesktopEntry *) temp_node->data, TRUE);
			if (fname && !strcmp (fname, (gchar *) order->data)) {
				temp_node = temp_node->next;
			}
			g_free (fname);
		}
		order = order->next;
	}
	fname2 = strip_name_from_gde (node->data, FALSE);
	for (;temp_node;temp_node = temp_node->next) {
		fname = strip_name_from_gde (temp_node->data, FALSE);
		if (fname && fname2 && strcmp (fname2, fname) < 0) {
			g_node_insert_before (sorted_list, temp_node, node);
			return;
		}
		g_free (fname);
	}
	g_free (fname2);
	g_node_append (sorted_list, node);
}
/* Stolen from gnomecc:tree.c */
static GNode *
read_directory (gchar *directory)
{
        DIR *parent_dir;
        struct dirent *child_dir;
        struct stat filedata;
	GSList *order;
        GNode *retval = g_node_new(NULL);
	GNode *new_node;
	
        parent_dir = opendir (directory);
        if (parent_dir == NULL)
                return NULL;
	order = get_presorted_from (directory);
        while ((child_dir = readdir (parent_dir)) != NULL) {
		new_node = NULL;
                if (child_dir->d_name[0] != '.') {

                        /* we check to see if it is interesting. */
                        GString *name = g_string_new (directory);
                        g_string_append (name, "/");
                        g_string_append (name, child_dir->d_name);
                        
                        if (stat (name->str, &filedata) != -1) {
                                gchar* test;
                                if (S_ISDIR (filedata.st_mode)) {
                                        /* it might be interesting... */
                                        new_node = read_directory (name->str);
                                        if (new_node) 
                                                /* it is interesting!!! */
                                                insert_node_custom (order, retval, new_node, name->str);
/*						g_node_prepend (retval, new_node);*/
                                }
                                test = strrchr(child_dir->d_name, '.');
                                if (test && !strcmp (".desktop", test)) {
                                        /* it's a .desktop file -- it's interesting for sure! */
                                        new_node = g_node_new (gnome_desktop_entry_load (name->str));
					insert_node_custom (order, retval, new_node, name->str);
/*					g_node_prepend (retval, new_node);*/
                                }
                        }
                        g_string_free (name, TRUE);
                } else if (!strcmp (child_dir->d_name, ".directory")) {
                        GString *name = g_string_new (directory);
                        g_string_append (name, "/.directory");
                        retval->data = gnome_desktop_entry_load (name->str);
                        g_string_free (name, TRUE);
                }
        }
	closedir (parent_dir);
        if (retval->children == NULL) {
                if (retval->data)
                        gnome_desktop_entry_free (retval->data);
		g_node_destroy (retval);
                return NULL;
        }
        return retval;
}
static void
gnome_open_dialog_generate_tree_helper (GtkCTree *ctree, GtkCTreeNode *parent, GNode *node)
{
        GNode *i;
        GtkCTreeNode *child = NULL;
        static char *text[2];
	GnomePixmap *icon_gpixmap;
        gchar *icon;
        GdkPixmap *icon_pixmap, *icon_mask;


	text[0] = NULL;
        text[1] = NULL;

        for (i = node;i;i = i->next) {
		icon_pixmap=NULL;
                icon_mask=NULL;
                icon=NULL;
                icon_gpixmap = NULL;

                if (i->data && ((GnomeDesktopEntry *)i->data)->name) {
                        text[0] = ((GnomeDesktopEntry *)i->data)->name;
		} else
                        text[0] = NULL;
		if ((i->data) && ((GnomeDesktopEntry *)i->data)->icon)
                        icon = ((GnomeDesktopEntry *)i->data)->icon;
                if (icon && g_file_exists (icon)) 
                        icon_gpixmap = (GnomePixmap *)gnome_pixmap_new_from_file_at_size(icon,
                                                                                         SMALL_ICON_SIZE,
                                                                                         SMALL_ICON_SIZE);
                if (icon_gpixmap) {
                        icon_pixmap = icon_gpixmap->pixmap;
                        icon_mask   = icon_gpixmap->mask;
                }

                if (i->data && text[0]) {
			if (((GnomeDesktopEntry *)i->data)->type && !strcmp(((GnomeDesktopEntry *)i->data)->type,"Directory"))
				child = gtk_ctree_insert_node (ctree,parent,NULL, text, 3, icon_pixmap, icon_mask, icon_pixmap, icon_mask,FALSE,FALSE);
			else 
				child = gtk_ctree_insert_node (ctree,parent,NULL, text, 3, icon_pixmap, icon_mask, icon_pixmap, icon_mask,TRUE,FALSE);
			gtk_ctree_node_set_row_data (ctree, child, i->data);
		} else 
			gnome_desktop_entry_free (i->data);
                if (i->children)
                        gnome_open_dialog_generate_tree_helper (ctree, child, i->children);
/*
  if (parent == NULL)
                        gtk_ctree_expand_recursive (ctree, child);
*/
	}
}

/* Stolen from gnome-core/panel/menu.c */
static GSList *
get_presorted_from(char *dir)
{
	char buf[PATH_MAX+1];
	GSList *list = NULL;
	char *fname = g_concat_dir_and_file(dir,".order");
	FILE *fp = fopen(fname,"r");
	
	if(!fp) {
		g_free(fname);
		return NULL;
	}
	while(fgets(buf,PATH_MAX+1,fp)!=NULL) {
		char *p = strchr(buf,'\n');
		if(p) *p = '\0';
		list = g_slist_prepend(list,g_strdup(buf));
	}
	fclose(fp);
	g_free(fname);
	return g_slist_reverse(list);
}
static void
gnome_open_dialog_selected_row_callback (GtkWidget *widget, GtkCTreeNode *node, gint column, gpointer *data)
{
	GnomeDesktopEntry *gde = NULL;
	GnomeOpenDialog *dialog = NULL;
	GtkWidget *entry;
	
	if (column < 0)
                return;

	dialog = GNOME_OPEN_DIALOG (data);
	gde = (GnomeDesktopEntry *)gtk_ctree_node_get_row_data (GTK_CTREE (widget),node);

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (gde != NULL);
	
	if (gde->exec) {
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (dialog->entry));
		gtk_entry_set_text (GTK_ENTRY (entry), gde->exec[0]);
	}
}
GtkWidget *
gnome_open_dialog_get_tree (GnomeOpenDialog *dialog)
{
	GtkWidget *retval;
	gchar *prefix;
	GNode *node;

	/* widget stuff */
	retval = gtk_ctree_new (1, 0);
        gtk_clist_set_row_height(GTK_CLIST (retval),20);
        gtk_ctree_set_line_style (GTK_CTREE (retval), GTK_CTREE_LINES_DOTTED);
        gtk_ctree_set_expander_style (GTK_CTREE (retval), GTK_CTREE_EXPANDER_SQUARE);
        gtk_clist_set_column_width(GTK_CLIST (retval), 0, 150);
        gtk_signal_connect( GTK_OBJECT (retval),
			    "tree_select_row",
			    GTK_SIGNAL_FUNC (gnome_open_dialog_selected_row_callback),
			    dialog);

        gtk_ctree_set_indent (GTK_CTREE (retval), 15);
        gtk_clist_set_column_auto_resize (GTK_CLIST (retval), 0, TRUE);

	/* set up the apps */
	prefix = gnome_unconditional_datadir_file ("gnome/apps");
	node = read_directory (prefix);
	gnome_open_dialog_generate_tree_helper (GTK_CTREE (retval), NULL, node);
	gtk_ctree_expand_recursive (GTK_CTREE (retval), NULL);
	g_node_destroy (node);
	g_free (prefix);
	return retval;
}
/* Public functions */
GtkWidget *
gnome_open_dialog_new (gchar *file_name)
{
	GnomeOpenDialog *dialog;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *sw;
	gchar *label_string;

	g_return_val_if_fail (file_name != NULL, NULL);
	dialog = gtk_type_new (gnome_open_dialog_get_type ());

	/* the first label */
	label_string = g_strdup_printf (
	    _("Select an application to open \"%s\" with."),file_name);
	label = gtk_label_new (label_string);
	gtk_widget_set_usize (label, 300, -1);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	g_free (label_string);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	/* The entry */
	dialog->file_name = g_strdup (file_name);
	dialog->entry = gnome_file_entry_new ("GNOME_OPEN_DIALOG",
					      _("Select a file to run with"));
	gtk_widget_ref (dialog->entry);
	/* Watch me do something evil... (-: */
	label_string = 	gnome_unconditional_libdir_file ("");
	strcpy (label_string + strlen (label_string) - 4, "bin");
	
	gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (dialog->entry),
					   label_string);
	g_free (label_string);

	/* the file tree */
	frame = gtk_frame_new (_("Applications"));
	dialog->ctree = gnome_open_dialog_get_tree (dialog);
	gtk_widget_ref (dialog->ctree);
	sw = gtk_scrolled_window_new (GTK_CLIST (dialog->ctree)->hadjustment,
				      GTK_CLIST (dialog->ctree)->vadjustment);
	gtk_widget_set_usize (sw, 300, 170);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width (GTK_CONTAINER (sw), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (sw), dialog->ctree);
	gtk_container_add (GTK_CONTAINER (frame), sw);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    frame, FALSE, FALSE, 0);

	frame = gtk_frame_new (_("Program to run"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog->entry), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), dialog->entry);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    frame, FALSE, FALSE, 0);
	gnome_dialog_append_button (GNOME_DIALOG(dialog), 
				    GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button (GNOME_DIALOG(dialog), 
				    GNOME_STOCK_BUTTON_CANCEL);
	gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);
	return GTK_WIDGET (dialog);
}


