
#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "edit-window.h"
#include "mime-data.h"
#include "mime-info.h"
#include "capplet-widget.h"


extern GtkWidget *capplet;
extern GHashTable *user_mime_types;

typedef struct {
	GtkWidget *window;
	GtkWidget *icon_entry;
	GtkWidget *mime_type;
/*  	GtkWidget *ext_tag_label; */
	GtkWidget *regexp1_tag_label;
	GtkWidget *regexp2_tag_label;
/*  	GtkWidget *ext_label; */
	GtkWidget *regexp1_label;
	GtkWidget *regexp2_label;
	GtkWidget *open_entry;
	GtkWidget *edit_entry;
	GtkWidget *view_entry;
        GtkWidget *ext_scroll;
        GtkWidget *ext_clist;
        GtkWidget *ext_entry;
        GtkWidget *ext_add_button;
        GtkWidget *ext_remove_button;
	MimeInfo *mi;
        MimeInfo *user_mi;
        GList *tmp_ext[2];
} edit_window;
static edit_window *main_win = NULL;
static gboolean changing = TRUE;
static void
destruction_handler (GtkWidget *widget, gpointer data)
{
	g_free (main_win);
	main_win = NULL;
}
static void
entry_changed (GtkWidget *widget, gpointer data)
{
	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
ext_clist_selected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        gboolean deletable;

	deletable = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (clist), row));
	if (deletable)
	        gtk_widget_set_sensitive (main_win->ext_remove_button, TRUE);
	else
	        gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
}
static void
ext_clist_deselected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        if (g_list_length (GTK_CLIST (clist)->selection) == 0)
	        gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
}
static void
ext_entry_changed (GtkWidget *entry, gpointer data)
{
        gchar *text;
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	gtk_widget_set_sensitive (main_win->ext_add_button, (strlen (text) >0));
}
static void
ext_add (GtkWidget *widget, gpointer data)
{
        gchar *row[1];
	gint rownumber;

	row[0] = g_strdup (gtk_entry_get_text (GTK_ENTRY (main_win->ext_entry)));
	rownumber = gtk_clist_append (GTK_CLIST (main_win->ext_clist), row);
	gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist), rownumber,
				GINT_TO_POINTER (TRUE));
	gtk_entry_set_text (GTK_ENTRY (main_win->ext_entry), "");

	main_win->tmp_ext[0] = g_list_prepend (main_win->tmp_ext[0], row[0]);
	if (changing == FALSE)
	        capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
ext_remove (GtkWidget *widget, gpointer data)
{
        gint row;
	gchar *text;
	gchar *store;
	GList *tmp;

	text = (gchar *)g_malloc (sizeof (gchar) * 1024);
	gtk_clist_freeze (GTK_CLIST (main_win->ext_clist));
	row = GPOINTER_TO_INT (GTK_CLIST (main_win->ext_clist)->selection->data);
	gtk_clist_get_text (GTK_CLIST (main_win->ext_clist), row, 0, &text);
	store = g_strdup (text);
	gtk_clist_remove (GTK_CLIST (main_win->ext_clist), row);

	gtk_clist_thaw (GTK_CLIST (main_win->ext_clist));

	for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
	        GList *found;

		if (strcmp (tmp->data, store) == 0) {
		        found = tmp;

			main_win->tmp_ext[0] = g_list_remove_link (main_win->tmp_ext[0], found);
			g_list_free_1 (found);
			break;
		}
	}

	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
apply_entry_change (GtkWidget *entry, gchar *key, MimeInfo *mi)
{
	const gchar *buf; 
	gchar *text;
	/* buf is the value that existed before when we 
	 * started the capplet */
	buf = local_mime_get_value (mi->mime_type, key);
	if (buf == NULL)
		buf = gnome_mime_get_value (mi->mime_type, key);
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (text && !*text)
		text = NULL;
	
	/* First we see if they've added something. */
	if (buf == NULL && text)
			set_mime_key_value (mi->mime_type, key, text);
	else {
		/* Has the value changed? */
		if (text && strcmp (text, buf))
			set_mime_key_value (mi->mime_type, key, text);
		else
			/* We _REALLY_ need a way to specify in
			 * user.keys not to use the system defaults.
			 * (ie. override the system default and
			 *  query it).
			 * If we could then we'd set it here. */
			;
	}
}
static GList*
copy_mi_extensions (GList *orig)
{
        GList *tmp;
	GList *list = NULL;

        for (tmp = orig; tmp; tmp = tmp->next) {
	       list = g_list_append (list, g_strdup (tmp->data));
	}
	return list;
}
static void
make_readable (MimeInfo *mi)
{
       GList *list;
       GString *extension;
       
       extension = g_string_new ("");
       for (list = ((MimeInfo *) mi)->user_ext[0]; list; list = list->next) {
	       g_string_append (extension, (gchar *) list->data);
	       if (list->next != NULL)
		       g_string_append (extension, ", ");
       }
       mi->ext_readable[0] = extension->str;
       g_string_free (extension, FALSE);

       extension = g_string_new ("");
       for (list = ((MimeInfo *) mi)->user_ext[1]; list; list = list->next) {
	       g_string_append (extension, (gchar *) list->data);
	       if (list->next != NULL)
		       g_string_append (extension, ", ");
       }
       mi->ext_readable[1] = extension->str;
       g_string_free (extension, FALSE);
}
static void
apply_changes (MimeInfo *mi)
{
        GList *tmp;
	int i;

	apply_entry_change (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (main_win->icon_entry)),
			    "icon-filename", mi);
	apply_entry_change (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->open_entry)),
			    "open", mi);
	apply_entry_change (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->view_entry)),
			    "view", mi);
	apply_entry_change (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->edit_entry)),
			    "edit", mi);

	if (!main_win->user_mi) {
	       add_to_key (mi->mime_type, "ext: tmp", user_mime_types, TRUE);
	       /* the tmp extension will be removed when we copy the tmp_ext 
		* stuff over the top of it.
		*/
	       main_win->user_mi = g_hash_table_lookup (user_mime_types,
							mi->mime_type);
	}

	for (i = 0; i < 2; i++) {
	       if (main_win->tmp_ext[i]) {
	               main_win->user_mi->user_ext[i] = copy_mi_extensions (main_win->tmp_ext[i]);
		       mi->user_ext[i] = copy_mi_extensions (main_win->tmp_ext[i]);
	       } else {
	               main_win->user_mi->user_ext[i] = NULL;
		       mi->user_ext[i] = NULL;
	       }
	}

	make_readable (main_win->user_mi);

	if (! (main_win->user_mi->ext[0] || main_win->user_mi->ext[1] ||
	       main_win->user_mi->user_ext[0] || main_win->user_mi->ext[1]))
	       g_hash_table_remove (user_mime_types, mi->mime_type);

	/* Free the 2 tmp lists */
	for (i = 0; i < 2; i++) {
	       if (main_win->tmp_ext[i])
	               for (tmp = main_win->tmp_ext[i]; tmp; tmp = tmp->next)
		              g_free (tmp->data);
	}
	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
browse_callback (GtkWidget *widget, gpointer data)
{
}
static void
initialize_main_win ()
{
	GtkWidget *align, *vbox, *hbox, *vbox2, *vbox3;
	GtkWidget *frame, *table, *label;
	GtkWidget *button;
	GString *extension;
	gchar *title[2] = {"Extensions"};

	main_win = g_new (edit_window, 1);
	main_win->window = gnome_dialog_new ("",
					     GNOME_STOCK_BUTTON_OK,
					     GNOME_STOCK_BUTTON_CANCEL,
					     NULL);
	gtk_signal_connect (GTK_OBJECT (main_win->window),
			    "destroy",
			    destruction_handler,
			    NULL);
	vbox = GNOME_DIALOG (main_win->window)->vbox;
	
	/* icon box */
	main_win->icon_entry = gnome_icon_entry_new ("mime_icon_entry", _("Select an icon..."));
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), main_win->icon_entry);
	gtk_signal_connect (GTK_OBJECT (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (main_win->icon_entry))),
			    "changed",
			    entry_changed,
			    NULL);
	gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Mime Type: ")), FALSE, FALSE, 0);
	main_win->mime_type = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), main_win->mime_type, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	/* extension/regexp */
	vbox2 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->ext_clist = gtk_clist_new_with_titles (1, title);
	gtk_clist_column_titles_passive (GTK_CLIST (main_win->ext_clist));
	gtk_clist_set_auto_sort (GTK_CLIST (main_win->ext_clist), TRUE);

	gtk_signal_connect (GTK_OBJECT (main_win->ext_clist), 
			    "select-row",
			    GTK_SIGNAL_FUNC (ext_clist_selected), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (main_win->ext_clist),
			    "unselect-row",
			    GTK_SIGNAL_FUNC (ext_clist_deselected),
			    NULL);
	main_win->ext_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (main_win->ext_scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (main_win->ext_scroll), 
			   main_win->ext_clist);

	vbox3 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->ext_add_button = gtk_button_new_with_label (_("Add"));
	gtk_signal_connect (GTK_OBJECT (main_win->ext_add_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (ext_add),
			    NULL);
	gtk_box_pack_start (GTK_BOX (vbox3), main_win->ext_add_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (main_win->ext_add_button, FALSE);

	main_win->ext_remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_signal_connect (GTK_OBJECT (main_win->ext_remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (ext_remove),
			    NULL);
	gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox3), main_win->ext_remove_button,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), main_win->ext_scroll, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox3, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

	main_win->ext_entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (main_win->ext_entry),
			    "changed",
			    ext_entry_changed,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (main_win->ext_entry),
			    "activate",
			    ext_add,
			    NULL);
	gtk_box_pack_start (GTK_BOX (vbox2), main_win->ext_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp1_label = gtk_label_new ("");
	main_win->regexp1_tag_label = gtk_label_new (_("First Regular Expression: "));
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp1_tag_label,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp1_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp2_label = gtk_label_new ("");
	main_win->regexp2_tag_label = gtk_label_new (_("Second Regular Expression: "));
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp2_tag_label,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp2_label, FALSE, FALSE, 0);

	/* Actions box */
	frame = gtk_frame_new (_("Mime Type Actions"));
	vbox2 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	label = gtk_label_new (_("Example: emacs %f"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
	label = gtk_label_new (_("Open"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 0, 1);
	main_win->open_entry = gnome_file_entry_new ("MIME_CAPPLET_OPEN", _("Select a file..."));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->open_entry))),
			    "changed",
			    entry_changed,
			    NULL);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->open_entry,
				   1, 2, 0, 1);
	label = gtk_label_new (_("View"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 1, 2);

	main_win->view_entry = gnome_file_entry_new ("MIME_CAPPLET_VIEW", _("Select a file..."));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->view_entry))),
			    "changed",
			    entry_changed,
			    NULL);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->view_entry,
				   1, 2, 1, 2);
	label = gtk_label_new (_("Edit"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 2, 3);
	main_win->edit_entry = gnome_file_entry_new ("MIME_CAPPLET_EDIT", _("Select a file..."));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (main_win->edit_entry))),
			    "changed",
			    entry_changed,
			    NULL);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->edit_entry,
				   1, 2, 2, 3);
}
static void
setup_entry (gchar *key, GtkWidget *g_entry, MimeInfo *mi)
{
	const gchar *buf;
	GtkWidget *entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (g_entry));
	buf = local_mime_get_value (mi->mime_type, key);
	if (buf == NULL)
		buf = gnome_mime_get_value (mi->mime_type, key);
	if (buf)
		gtk_entry_set_text (GTK_ENTRY (entry), buf);
	else
		gtk_entry_set_text (GTK_ENTRY (entry), "");
}
void
initialize_main_win_vals (void)
{
	MimeInfo *mi;
	gchar *title;
	gboolean showext = FALSE;
	if (main_win == NULL)
		return;
	mi = main_win->mi;
	if (mi == NULL)
		return;
	/* now we fill in the fields with the mi stuff. */

	changing = TRUE;
	gtk_label_set_text (GTK_LABEL (main_win->mime_type), mi->mime_type);
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY (main_win->icon_entry),
				   gnome_mime_get_value (mi->mime_type,
							 "icon-filename"));

	gtk_widget_show_all (GNOME_DIALOG (main_win->window)->vbox);
	/* we initialize everything */
	title = g_strdup_printf (_("Set actions for %s"), mi->mime_type);
	gtk_window_set_title (GTK_WINDOW (main_win->window), title);
	g_free (title);

	/* not sure why this is necessary */
	gtk_clist_clear (GTK_CLIST (main_win->ext_clist));
	if (mi->ext[0]) {
	        GList *tmp;
                gchar *extension[1];
		gint row;
		for (tmp = mi->ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (FALSE));
		}
		showext = TRUE;
	}
	if (mi->ext[1]) {
	        GList *tmp;
	        gchar *extension[1];
		gint row;
		for (tmp = mi->ext[1]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (FALSE));
		}
		showext = TRUE;
	}
	if (main_win->tmp_ext[0]) {
	        GList *tmp;
		gchar *extension[1];
		gint row;
		for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (TRUE));
		}
		showext = TRUE;
	}
	if (main_win->tmp_ext[1]) {
	        GList *tmp;
		gchar *extension[1];
		gint row;
		for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (TRUE));
		}
		showext = TRUE;
	}
	if (!showext) {
	        gtk_widget_hide (main_win->ext_clist);
	        gtk_widget_hide (main_win->ext_entry);
		gtk_widget_hide (main_win->ext_add_button);
		gtk_widget_hide (main_win->ext_remove_button);
		gtk_widget_hide (main_win->ext_scroll);
	}
	if (mi->regex_readable[0])
		gtk_label_set_text (GTK_LABEL (main_win->regexp1_label),
				    mi->regex_readable[0]);
	else {
		gtk_widget_hide (main_win->regexp1_label);
		gtk_widget_hide (main_win->regexp1_tag_label);
	}
	if (mi->regex_readable[1])
		gtk_label_set_text (GTK_LABEL (main_win->regexp2_label),
				    mi->regex_readable[1]);
	else {
		gtk_widget_hide (main_win->regexp2_label);
		gtk_widget_hide (main_win->regexp2_tag_label);
	}
	/* initialize the entries */
	setup_entry ("open", main_win->open_entry, mi);
	setup_entry ("view", main_win->view_entry, mi);
	setup_entry ("edit", main_win->edit_entry, mi);
	changing = FALSE;

}
void
launch_edit_window (MimeInfo *mi)
{
	gint size;

	if (main_win == NULL)
		initialize_main_win ();
	main_win->mi = mi;
	main_win->user_mi = g_hash_table_lookup (user_mime_types, mi->mime_type);
	main_win->tmp_ext[0] = NULL;
	main_win->tmp_ext[1] = NULL;
	if (main_win->user_mi) {
	        if (main_win->user_mi->user_ext[0])
	                main_win->tmp_ext[0] = copy_mi_extensions (main_win->user_mi->user_ext[0]);
		if (main_win->user_mi->user_ext[1])
		        main_win->tmp_ext[1] = copy_mi_extensions (main_win->user_mi->user_ext[1]);
	}
	initialize_main_win_vals ();

	switch(gnome_dialog_run (GNOME_DIALOG (main_win->window))) {
	case 0:
		apply_changes (mi);
	case 1:
		main_win->mi = NULL;
		gtk_widget_hide (main_win->window);
		break;
	}
}

void
hide_edit_window (void)
{
	if (main_win && main_win->mi && main_win->window)
		gtk_widget_hide (main_win->window);
}
void
show_edit_window (void)
{
	if (main_win && main_win->mi && main_win->window)
		gtk_widget_show (main_win->window);
}




