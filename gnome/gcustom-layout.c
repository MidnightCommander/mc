/* Custom layout preferences box for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Owen Taylor <otaylor@redhat.com>
 */

#include <config.h>
#include "x.h"
#include <ctype.h>
#include "panel.h"
#include "gcustom-layout.h"
#include "gscreen.h"

typedef struct _ColumnInfo ColumnInfo;

struct _ColumnInfo {
	gchar *name;
	gchar *value;
};

struct _GCustomLayout {
	GtkWidget *table;
	GtkWidget *srcList;
	GtkWidget *destList;
	GtkWidget *addButton;
	GtkWidget *delButton;
	GHashTable *hash;
	GnomePropertyBox *prop_box;
	WPanel *panel;
};

ColumnInfo possible_columns[] = {
	{ N_("Access Time"), 	      "atime" },
	{ N_("Creation Time"), 	      "ctime" },
	{ N_("Group"), 		      "group" },
	{ N_("Group ID"), 	      "ngid"  },
	{ N_("Inode Number"), 	      "inode" },
	{ N_("Mode"), 		      "mode"  },
	{ N_("Modification Time"),    "mtime" },
	{ N_("Name"), 		      "name"  },
	{ N_("Number of Hard Links"), "nlink" },
	{ N_("Owner"), 		      "owner" },
	{ N_("Permission"), 	      "perm"  },
	{ N_("Size"), 		      "size"  },
	{ N_("Size (short)"), 	      "bsize" },
	{ N_("Type"), 		      "type"  },
	{ N_("User ID"), 	      "nuid"  }
};
gint n_possible_columns = sizeof(possible_columns) / sizeof(possible_columns[0]);

static void
custom_layout_add_clicked (GtkWidget *widget, GCustomLayout *layout)
{
	gint row, new_row;
	ColumnInfo *info;
	gchar *tmp_name;

	if (GTK_CLIST (layout->srcList)->selection) {
		row = GPOINTER_TO_UINT (GTK_CLIST (layout->srcList)->selection->data);

		info = gtk_clist_get_row_data (GTK_CLIST (layout->srcList), row);

		tmp_name = gettext (info->name);
		new_row = gtk_clist_append (GTK_CLIST (layout->destList), &tmp_name);
		gtk_clist_set_row_data (GTK_CLIST (layout->destList),
					new_row,
					info);
		gtk_clist_select_row (GTK_CLIST (layout->destList), new_row, 0);

		gtk_widget_set_sensitive (layout->addButton, FALSE);
		gnome_property_box_changed (layout->prop_box);
	}
	if (GTK_CLIST (layout->destList)->rows >= 2)
		gtk_widget_set_sensitive (layout->delButton, TRUE);
}

static void
custom_layout_del_clicked (GtkWidget *widget, GCustomLayout *layout)
{
	gint row;

	if (GTK_CLIST (layout->destList)->selection) {
		row = GPOINTER_TO_UINT (GTK_CLIST (layout->destList)->selection->data);
		gtk_clist_remove (GTK_CLIST (layout->destList), row);
		gnome_property_box_changed (layout->prop_box);
	}
	if (GTK_CLIST (layout->destList)->rows <= 1)
		gtk_widget_set_sensitive (layout->delButton, FALSE);
}

static void
custom_layout_select_row (GtkWidget *widget, gint row, gint col, GdkEvent *event,
			GCustomLayout *layout)
{
        gint i;
	ColumnInfo *info, *tmp_info;

	info = gtk_clist_get_row_data (GTK_CLIST (layout->srcList), row);

	for (i=0; i<GTK_CLIST (layout->destList)->rows; i++) {
		tmp_info = gtk_clist_get_row_data (GTK_CLIST (layout->destList), i);
		if (tmp_info == info) {
		        gtk_widget_set_sensitive (layout->addButton, FALSE);
			return;
		}
	}

	gtk_widget_set_sensitive (layout->addButton, TRUE);
}

static void
custom_layout_row_move (GtkWidget *widget,
			gint source_row, gint dest_row,
			GCustomLayout *layout)
{
	gnome_property_box_changed (layout->prop_box);
}

static void
custom_layout_destroy (GtkWidget *widget, GCustomLayout *layout)
{
	g_hash_table_destroy (layout->hash);
	g_free (layout);
}

static void
custom_layout_create(GCustomLayout *layout, ColumnInfo *columns, gint ncolumns)
{
	GtkWidget *vbox2;
	GtkWidget *scrollwin;
	GtkWidget *label;
	GtkWidget *align;
	gint       i;
	gchar     *tmp_name;

	layout->table = gtk_table_new(3, 1, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (layout->table), GNOME_PAD_SMALL);

	/* make list of possible columns to add */
	vbox2 = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	label = gtk_label_new (_("Possible Columns"));
	gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox2), scrollwin, TRUE, TRUE, 0);

	layout->srcList = gtk_clist_new(1);
	gtk_container_add(GTK_CONTAINER(scrollwin), layout->srcList);
	gtk_widget_set_usize(layout->srcList, 150, 200);
	gtk_clist_set_selection_mode(GTK_CLIST(layout->srcList),
				     GTK_SELECTION_BROWSE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(layout->srcList), 0, 1);
	gtk_table_attach(GTK_TABLE(layout->table), vbox2, 0, 1, 0, 1,
			 GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			 GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* make list of currently displayed column types */
	vbox2 = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	label = gtk_label_new (_("Displayed Columns"));
	gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox2), scrollwin, TRUE, TRUE, 0);

	layout->destList = gtk_clist_new(1);
	gtk_container_add(GTK_CONTAINER(scrollwin), layout->destList);
	gtk_widget_set_usize(layout->destList, 150, 200);
	gtk_clist_set_selection_mode(GTK_CLIST(layout->destList),
				     GTK_SELECTION_BROWSE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(layout->destList), 0, 1);
	gtk_clist_set_reorderable (GTK_CLIST (layout->destList), TRUE);

	gtk_table_attach(GTK_TABLE(layout->table), vbox2, 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND | GTK_SHRINK,
			 GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

	/* add add/remove buttons in center */
	layout->addButton = gtk_button_new_with_label(_("Add"));
	layout->delButton = gtk_button_new_with_label(_("Remove"));

	align = gtk_alignment_new(0.0, 0.5, 0, 0);
	vbox2 = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), layout->addButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), layout->delButton, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align), vbox2);
	gtk_table_attach(GTK_TABLE(layout->table), align, 1, 2, 0, 1, 0, 0, 5, 5);

	layout->hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (i = 0; i < ncolumns; i++) {
	        tmp_name = gettext(columns[i].name);
		gtk_clist_append (GTK_CLIST (layout->srcList), &tmp_name);
		gtk_clist_set_row_data (GTK_CLIST (layout->srcList),
					i,
					&columns[i]);

		g_hash_table_insert (layout->hash,
				     columns[i].value,
				     &columns[i]);
	}

	gtk_signal_connect(GTK_OBJECT(layout->addButton), "clicked",
			   GTK_SIGNAL_FUNC(custom_layout_add_clicked),
			   layout);
	gtk_signal_connect(GTK_OBJECT(layout->delButton), "clicked",
			   GTK_SIGNAL_FUNC(custom_layout_del_clicked),
			   layout);
	gtk_signal_connect(GTK_OBJECT(layout->srcList), "select_row",
			   GTK_SIGNAL_FUNC(custom_layout_select_row),
			   layout);
	gtk_signal_connect(GTK_OBJECT(layout->destList), "row_move",
			   GTK_SIGNAL_FUNC(custom_layout_row_move),
			   layout);

	gtk_widget_show_all (layout->table);

	gtk_signal_connect(GTK_OBJECT(layout->table), "destroy",
			   GTK_SIGNAL_FUNC(custom_layout_destroy), layout);
}

static void
custom_layout_set (GCustomLayout *layout, gchar *string)
{
	gint new_row;
	gint i;
	gchar **strings;
	ColumnInfo *info;
	gchar *tmp_name;

	gtk_clist_clear (GTK_CLIST (layout->destList));

	/* skip over initial half or full */
	while (*string && !isspace(*string))
		string++;
	while (*string && isspace(*string))
		string++;

	strings = g_strsplit (string, ",", -1);

	for (i=0; strings[i]; i++) {
		info = g_hash_table_lookup (layout->hash, strings[i]);
		if (info) {
		        tmp_name = gettext (info->name);
			new_row = gtk_clist_append (GTK_CLIST (layout->destList), &tmp_name);
			gtk_clist_set_row_data (GTK_CLIST (layout->destList),
						new_row, info);
		}
	}

	g_strfreev (strings);

	/* Set the status of the "Add" button correctly */

	if (GTK_CLIST (layout->srcList)->selection) {
		gint row = GPOINTER_TO_UINT (GTK_CLIST (layout->srcList)->selection->data);
		custom_layout_select_row (NULL, row, 0, NULL, layout);
	}
	/* Set the status of the "Remove" button correctly */
	if (GTK_CLIST (layout->destList)->rows <= 1)
		gtk_widget_set_sensitive (layout->delButton, FALSE);

}

static gchar *
custom_layout_get (GCustomLayout *layout)
{
	gint i;
	GString *result;
	gchar *string;
	ColumnInfo *info;

	result = g_string_new ("full ");

	for (i=0; i<GTK_CLIST (layout->destList)->rows; i++) {
		if (i != 0)
			g_string_append_c (result, ',');
		info = gtk_clist_get_row_data (GTK_CLIST (layout->destList), i);
		g_string_append (result, info->value);
	}

	string = result->str;
	g_string_free (result, FALSE);

	return string;
}

GCustomLayout *
custom_layout_create_page (GnomePropertyBox *prop_box, WPanel *panel)
{
	GCustomLayout *layout;

	layout = g_new (GCustomLayout, 1);
	custom_layout_create (layout, possible_columns, n_possible_columns);
	layout->prop_box = prop_box;
	layout->panel = panel;

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (prop_box),
                                        layout->table,
                                        gtk_label_new (_("Custom View")));

	custom_layout_set (layout, panel->user_format);

	return layout;
}

void
custom_layout_apply (GCustomLayout    *layout)
{
	gchar *format;
	GList *tmp_list;
	PanelContainer *container;

	format = custom_layout_get (layout);

	tmp_list = containers;
	while (tmp_list) {
		container = tmp_list->data;

		g_free (container->panel->user_format);
		container->panel->user_format = g_strdup (format);

		g_free (default_user_format);
		default_user_format = g_strdup (format);

		set_panel_formats (container->panel);

		tmp_list = tmp_list->next;
	}

	g_free (format);
}

