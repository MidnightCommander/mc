#include "new-mime-window.h"
#include "capplet-widget.h"
#include <config.h>
static GtkWidget *add_dialog = NULL;
extern GtkWidget *capplet;

/*Public functions */
void
launch_new_mime_window (void)
{
        GtkWidget *mime_entry;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *ext_entry;
	GtkWidget *regex1_entry;
	GtkWidget *regex2_entry;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *table;
	
        add_dialog = gnome_dialog_new (_("Add Mime Type"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	label = gtk_label_new (_("Add a new Mime Type\nFor example:  image/tiff; text/x-scheme"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new (_("Mime Type:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	mime_entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_entry, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), hbox, FALSE, FALSE, 0);
	
	frame = gtk_frame_new (_("Extensions"));
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), frame, FALSE, FALSE, 0);
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	label = gtk_label_new (_("Type in the extensions for this mime-type.\nFor example:  .html, .htm"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Extension:")), FALSE, FALSE, 0);
	ext_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), ext_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	frame = gtk_frame_new (_("Regular Expressions"));
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (add_dialog)->vbox), frame, FALSE, FALSE, 0);
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	label = gtk_label_new (_("You can set up two regular expressions here to identify the Mime Type\nby.  These fields are optional."));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
/*	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);*/
	label = gtk_label_new (_("First Regular Expression: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);
	regex1_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table),
				   regex1_entry,
				   1, 2, 0, 1);
	label = gtk_label_new (_("Second Regular Expression: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);
	regex2_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table),
				   regex2_entry,
				   1, 2, 1, 2);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	
        gtk_widget_show_all (GNOME_DIALOG (add_dialog)->vbox);
        switch (gnome_dialog_run (GNOME_DIALOG (add_dialog))) {
        case 0:
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
                add_new_mime_type (gtk_entry_get_text (GTK_ENTRY (mime_entry)),
				   gtk_entry_get_text (GTK_ENTRY (ext_entry)),
				   gtk_entry_get_text (GTK_ENTRY (regex1_entry)),
				   gtk_entry_get_text (GTK_ENTRY (regex2_entry)));
        case 1:
                gtk_widget_destroy (add_dialog);
        default:;
        }
	add_dialog = NULL;
}
void
hide_new_mime_window (void)
{
	if (add_dialog != NULL)
		gtk_widget_hide (add_dialog);
}
void
show_new_mime_window (void)
{
	if (add_dialog != NULL)
		gtk_widget_show (add_dialog);
}
