#include "edit-window.h"
#include "capplet-widget.h"


extern GtkWidget *capplet;

typedef struct {
	GtkWidget *window;
	GtkWidget *icon_entry;
	GtkWidget *mime_type;
	GtkWidget *ext_label;
	GtkWidget *regexp1_label;
	GtkWidget *regexp2_label;
	GtkWidget *open_entry;
	GtkWidget *edit_entry;
	GtkWidget *view_entry;
	GtkWidget *open_cbox;
	GtkWidget *edit_cbox;
	GtkWidget *view_cbox;
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
apply_entry_change (GtkWidget *entry, gchar *key, MimeInfo *mi)
{
	gchar *buf; 
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
static void
apply_changes (MimeInfo *mi)
{
	apply_entry_change (main_win->open_entry, "open", mi);
	apply_entry_change (main_win->view_entry, "view", mi);
	apply_entry_change (main_win->edit_entry, "edit", mi);
}
static void
initialize_main_win ()
{
	GtkWidget *align, *vbox, *hbox, *vbox2;
	GtkWidget *frame, *table, *label;
	GtkWidget *button;
	GString *extension;

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
	gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Mime Type: ")), FALSE, FALSE, 0);
	main_win->mime_type = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), main_win->mime_type, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);

	/* extension/regexp */
	vbox2 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->ext_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Extension: ")),
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->ext_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp1_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("First Regular Expresion: ")),
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp1_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp2_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Second Regular Expresion: ")),
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp2_label, FALSE, FALSE, 0);

	/* Actions box */
	frame = gtk_frame_new (_("Mime-Type Actions"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	table = gtk_table_new (8, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL/2);
	gtk_container_add (GTK_CONTAINER (frame), table);
	label = gtk_label_new (_("Open"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 0, 1);
	main_win->open_entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (main_win->open_entry),
			    "changed",
			    entry_changed,
			    NULL);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->open_entry,
				   1, 2, 0, 1);
	button = gtk_button_new_with_label ("Browse...");
	gtk_table_attach_defaults (GTK_TABLE (table),
				   button,
				   2, 3, 0, 1);
	
	main_win->open_cbox = gtk_check_button_new_with_label (_("Use default Open action"));
/*	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->open_cbox,
				   0, 2, 1, 2);*/

	label = gtk_label_new (_("View"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 3, 4);
	main_win->view_entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (main_win->view_entry),
			    "changed",
			    entry_changed,
			    NULL);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->view_entry,
				   1, 2, 3, 4);
		button = gtk_button_new_with_label ("Browse...");
		gtk_table_attach_defaults (GTK_TABLE (table),
				   button,
				   2, 3, 3, 4);

	main_win->open_cbox = gtk_check_button_new_with_label (_("Use default View action"));
/*	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->open_cbox,
				   0, 2, 4, 5);*/
	label = gtk_label_new (_("Edit"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   label,
				   0, 1, 6, 7);
	main_win->edit_entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (main_win->edit_entry),
			    "changed",
			    entry_changed,
			    NULL);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->edit_entry,
				   1, 2, 6, 7);
	button = gtk_button_new_with_label ("Browse...");
	gtk_table_attach_defaults (GTK_TABLE (table),
				   button,
				   2, 3, 6, 7);

	main_win->edit_cbox = gtk_check_button_new_with_label (_("Use default Edit action"));
/*	gtk_table_attach_defaults (GTK_TABLE (table),
				   main_win->edit_cbox,
				   0, 2, 7, 8);*/
}

static void
setup_entry (gchar *key, GtkWidget *entry, MimeInfo *mi)
{
	gchar *buf;
	buf = local_mime_get_value (mi->mime_type, key);
	if (buf == NULL)
		buf = gnome_mime_get_value (mi->mime_type, key);
	if (buf)
		gtk_entry_set_text (GTK_ENTRY (entry), buf);
	else
		gtk_entry_set_text (GTK_ENTRY (entry), "");
}
void
launch_edit_window (MimeInfo *mi)
{
	gint size;

	changing = TRUE;
	if (main_win == NULL)
		initialize_main_win ();
	/* now we fill in the fields with the mi stuff. */
	gtk_label_set_text (GTK_LABEL (main_win->mime_type), mi->mime_type);
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY (main_win->icon_entry),
				   gnome_mime_get_value (mi->mime_type,
							 "icon-filename"));

	/* we initialize everything */
	if (mi->ext[0]) {
		GString *extension;
                extension = g_string_new (mi->ext_readable[0]);
                if (mi->ext[1]) {
                        g_string_append (extension, ", ");
                        g_string_append (extension, (mi->ext_readable[1]));
                }
		gtk_label_set_text (GTK_LABEL (main_win->ext_label),
				    extension->str);
		g_string_free (extension, TRUE);
        } else if (mi->ext[1])
                gtk_label_set_text (GTK_LABEL (main_win->ext_label),
				    mi->ext_readable[1]);
	else
                gtk_label_set_text (GTK_LABEL (main_win->ext_label),
				    _("<No extention>"));
	if (mi->regex_readable[0])
		gtk_label_set_text (GTK_LABEL (main_win->regexp1_label),
				    mi->regex_readable[0]);
	else
		gtk_label_set_text (GTK_LABEL (main_win->regexp1_label),
				    _("<No Regular Expression>"));
	if (mi->regex_readable[1])
		gtk_label_set_text (GTK_LABEL (main_win->regexp2_label),
				    mi->regex_readable[1]);
	else
		gtk_label_set_text (GTK_LABEL (main_win->regexp2_label),
				    _("<No Regular Expression>"));
	/* initialize the entries */
	setup_entry ("open", main_win->open_entry, mi);
	setup_entry ("view", main_win->view_entry, mi);
	setup_entry ("edit", main_win->edit_entry, mi);

	changing = FALSE;

	gtk_widget_show_all (GNOME_DIALOG (main_win->window)->vbox);
	if  ((gnome_dialog_run (GNOME_DIALOG (main_win->window))) == 0) {
		apply_changes (mi);
	}
	gtk_widget_hide (main_win->window);
}

