/*
 * Preferences configuration page for the GNU Midnight Commander
 *
 * Author:
 *   Jonathan Blandford (jrb@redhat.com)
 */
#include <config.h>
#include "x.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dir.h"
#include "panel.h"
#include "gscreen.h"
#include "main.h"
#include "gmain.h"
#include "cmd.h"
#include "boxes.h"
#include "profile.h"
#include "setup.h"
#include "panelize.h"
#include "dialog.h"
#include "layout.h"
#include "gcustom-layout.h"
#include "gdesktop-prefs.h"
#include "../vfs/vfs.h"
#include "gprefs.h"

/* Orphan confirmation options */
/* Auto save setup */
/* use internal edit */ /* maybe have a mime-type capplet for these */
/* use internal view */
/* complete: show all */
/* Animation??? */
/* advanced chown */
/* cd follows links */
/* safe delete */

extern int vfs_timeout;
extern int ftpfs_always_use_proxy;
extern char* ftpfs_anonymous_passwd;
extern int file_op_compute_totals;
extern int we_can_afford_the_speed;
typedef enum
{
        PROPERTY_NONE,
        PROPERTY_BOOL,
        PROPERTY_STRING,
        PROPERTY_INT,
        PROPERTY_CUSTOM
} PropertyType_e;

typedef struct
{
        gchar *label;
        PropertyType_e type;
        gpointer property_variable;
        gpointer extra_data1;
        gpointer extra_data2;

        GtkWidget *widget;
} Property;

typedef struct
{
        gchar *title;
        Property *props;
} PrefsPage;

typedef struct
{
        WPanel *panel;
        GtkWidget *prop_box;
        PrefsPage *prefs_pages;

	GDesktopPrefs *desktop_prefs;
	gint desktop_prefs_page;

	GCustomLayout *custom_layout;
	gint custom_layout_page;
} PrefsDlg;


#define PROPERTIES_DONE { NULL, PROPERTY_NONE, NULL, NULL, NULL, NULL }
#define PREFSPAGES_DONE { NULL, NULL }

typedef GtkWidget* (*CustomCreateFunc) (PrefsDlg *dlg, Property *prop);
typedef void (*CustomApplyFunc) (PrefsDlg *dlg, Property *prop);

static Property file_display_props [] =
{
        {
                N_("Show backup files"), PROPERTY_BOOL,
                &show_backups, NULL, NULL, NULL
        },
        {
                N_("Show hidden files"), PROPERTY_BOOL,
                &show_dot_files, NULL, NULL, NULL
        },
        {
                N_("Mix files and directories"), PROPERTY_BOOL,
                &mix_all_files, NULL, NULL, NULL
        },
        {
                N_("Use shell patterns instead of regular expressions"), PROPERTY_BOOL,
                &easy_patterns, NULL, NULL, NULL
        },
        PROPERTIES_DONE
};

static Property confirmation_props [] =
{
        {
                N_("Confirm when deleting file"), PROPERTY_BOOL,
                &confirm_delete, NULL, NULL, NULL
        },
        {
                N_("Confirm when overwriting files"), PROPERTY_BOOL,
                &confirm_overwrite, NULL, NULL, NULL
        },
        {
                N_("Confirm when executing files"), PROPERTY_BOOL,
                &confirm_execute, NULL, NULL, NULL
        },
        {
                N_("Show progress while operations are being performed"), PROPERTY_BOOL,
                &verbose, NULL, NULL, NULL
        },
        PROPERTIES_DONE
};

static Property vfs_props [] =
{
        {
                N_("VFS Timeout:"), PROPERTY_INT,
                &vfs_timeout, N_("Seconds"), NULL, NULL
        },
        {
                N_("Anonymous FTP password:"), PROPERTY_STRING,
                &ftpfs_anonymous_passwd, NULL, NULL, NULL
        },
        {
                N_("Always use FTP proxy"), PROPERTY_BOOL,
                &ftpfs_always_use_proxy, NULL, NULL, NULL
        },
        PROPERTIES_DONE
};

static Property caching_and_optimization_props [] =
{
        {
                N_("Fast directory reload"), PROPERTY_BOOL,
                &fast_reload, NULL, NULL, NULL
        },
        {
                N_("Compute totals before copying files"), PROPERTY_BOOL,
                &file_op_compute_totals, NULL, NULL, NULL
        },
        {
                N_("FTP directory cache timeout :"), PROPERTY_INT,
                &ftpfs_directory_timeout, N_("Seconds"), NULL, NULL
        },
        {
                N_("Allow customization of icons in icon view"), PROPERTY_BOOL,
                &we_can_afford_the_speed, NULL, NULL, NULL
        },
        PROPERTIES_DONE
};

static PrefsPage prefs_pages [] =
{
        {
                N_("File display"),
                file_display_props
        },
        {
                N_("Confirmation"),
                confirmation_props
        },
        {
                N_("VFS"),
                vfs_props
        },
        {
                N_("Caching"),
                caching_and_optimization_props
        },
        PREFSPAGES_DONE
};

static void
apply_changes_bool (PrefsDlg *dlg, Property *cur_prop)
{
        GtkWidget *checkbox;

        checkbox = cur_prop->widget;

        if (GTK_TOGGLE_BUTTON (checkbox)->active)
                *( (int*) cur_prop->property_variable) = TRUE;
        else
                *( (int*) cur_prop->property_variable) = FALSE;
}

static void
apply_changes_string (PrefsDlg *dlg, Property *cur_prop)
{
        GtkWidget *entry;
        gchar *text;

        entry = cur_prop->widget;

        text = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))));

        *( (char**) cur_prop->property_variable) = g_strdup (text);
}

static void
apply_changes_int (PrefsDlg *dlg, Property *cur_prop)
{
        GtkWidget *entry;
        gdouble val;
	gchar *num;

        entry = cur_prop->widget;
        num = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))));
	sscanf(num,"%lg",&val);

        *( (int*) cur_prop->property_variable) = (gint) val;
}

static void
apply_changes_custom (PrefsDlg *dlg, Property *cur_prop)
{
        CustomApplyFunc apply = (CustomApplyFunc) cur_prop->extra_data2;

        apply (dlg, cur_prop);
}

static void
apply_page_changes (PrefsDlg *dlg, gint pagenum)
{
        Property *props;
        Property cur_prop;
        gint i;

        props = dlg->prefs_pages [pagenum].props;
        i = 0;
        cur_prop = props [i];
        while (cur_prop.label != NULL) {
                switch (cur_prop.type) {
                case PROPERTY_NONE :
                        g_warning ("Invalid case in gprefs.c:  apply_page_changes");
                        break;
                case PROPERTY_BOOL :
                        apply_changes_bool (dlg, &cur_prop);
                        break;
                case PROPERTY_STRING :
                        apply_changes_string (dlg, &cur_prop);
                        break;
                case PROPERTY_INT :
                        apply_changes_int (dlg, &cur_prop);
                        break;
                case PROPERTY_CUSTOM :
                        apply_changes_custom (dlg, &cur_prop);
                        break;
                }
		cur_prop = props[++i];
        }
}

static void
apply_callback (GtkWidget *prop_box, gint pagenum, PrefsDlg *dlg)
{
	if (pagenum == dlg->desktop_prefs_page)
		desktop_prefs_apply (dlg->desktop_prefs);
	else if (pagenum == dlg->custom_layout_page)
		custom_layout_apply (dlg->custom_layout);
	else if (pagenum != -1)
                apply_page_changes (dlg, pagenum);
        else {
		update_panels (UP_RELOAD, UP_KEEPSEL);
                save_setup ();
        }
}

static void
changed_callback (GtkWidget *widget, PrefsDlg *dlg)
{
        if (dlg->prop_box)
                gnome_property_box_changed (GNOME_PROPERTY_BOX (dlg->prop_box));
}

static GtkWidget*
create_prop_bool (PrefsDlg *dlg, Property *prop)
{
        GtkWidget *checkbox;

        checkbox = gtk_check_button_new_with_label (_(prop->label));

        if (*((int*) prop->property_variable)) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
                                              FALSE);
        }

        gtk_signal_connect (GTK_OBJECT (checkbox), "clicked",
                            changed_callback, (gpointer) dlg);

        prop->widget = checkbox;
	gtk_widget_show_all (checkbox);
        return checkbox;
}

static GtkWidget*
create_prop_string (PrefsDlg *dlg, Property *prop)
{
        GtkWidget *entry;
        GtkWidget *gtk_entry;
        GtkWidget *label;
        GtkWidget *hbox;
        gint max_length;

        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

        label = gtk_label_new (_(prop->label));
        gtk_box_pack_start (GTK_BOX (hbox), label,
                            FALSE, FALSE, 0);

        entry = gnome_entry_new (_(prop->label));
        gtk_entry = gnome_entry_gtk_entry (GNOME_ENTRY (entry));

        max_length = (int)prop->extra_data1;
        if (max_length != 0) {
                gtk_entry_set_max_length (GTK_ENTRY (gtk_entry),
                                          max_length);
        }

        gtk_entry_set_text (GTK_ENTRY (gtk_entry),
                            (gchar*) *( (gchar**) prop->property_variable));
        gtk_signal_connect_while_alive (GTK_OBJECT (gtk_entry),
                                        "changed",
                                        GTK_SIGNAL_FUNC (changed_callback),
                                        (gpointer) dlg,
                                        GTK_OBJECT (dlg->prop_box));

        gtk_box_pack_start (GTK_BOX (hbox), entry,
                            FALSE, FALSE, 0);

        prop->widget = entry;
	gtk_widget_show_all (hbox);
        return hbox;
}

static GtkWidget*
create_prop_int (PrefsDlg *dlg, Property *prop)
{
        GtkWidget *entry;
        GtkWidget *label;
        GtkWidget *hbox;
        gchar buffer [10];

        hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

        label = gtk_label_new (_(prop->label));
        gtk_box_pack_start (GTK_BOX (hbox), label,
                            FALSE, FALSE, 0);

        entry = gnome_entry_new (_(prop->label));

        snprintf (buffer, 9, "%d", *( (int*) prop->property_variable));

        gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))),
                            buffer);

        gtk_signal_connect_while_alive (GTK_OBJECT (gnome_entry_gtk_entry (GNOME_ENTRY (entry))),
                                        "changed",
                                        GTK_SIGNAL_FUNC (changed_callback),
                                        (gpointer) dlg,
                                        GTK_OBJECT (dlg->prop_box));

        gtk_box_pack_start (GTK_BOX (hbox), entry,
                            FALSE, FALSE, 0);
	if (prop->extra_data1) {
		label = gtk_label_new (_((gchar *)prop->extra_data1));
		gtk_box_pack_start (GTK_BOX (hbox), label,
				    FALSE, FALSE, 0);
	}

        prop->widget = entry;
	gtk_widget_show_all (hbox);
        return hbox;
}

static GtkWidget*
create_prop_custom (PrefsDlg *dlg, Property *prop)
{
        CustomCreateFunc create = (CustomCreateFunc) prop->extra_data1;

	if (!create)
		return create_prop_bool (dlg, prop);

        return create (dlg, prop);
}

static GtkWidget*
create_prop_widget (PrefsDlg *dlg, Property *prop)
{
        switch (prop->type) {
        case PROPERTY_NONE :
                g_warning ("Invalid case in gprefs.c:  create_prop_widget");
                break;
        case PROPERTY_BOOL :
                return create_prop_bool (dlg, prop);
        case PROPERTY_STRING :
                return create_prop_string (dlg, prop);
        case PROPERTY_INT :
                return create_prop_int (dlg, prop);
        case PROPERTY_CUSTOM :
                return create_prop_custom (dlg, prop);
                break;
        }
        return NULL;
}

static void
create_page (PrefsDlg *dlg, PrefsPage *page)
{
        GtkWidget *vbox;
        GtkWidget *prop_widget;
        Property *cur_prop;
        gint i;

        vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_widget_show (vbox);
        i = 0;
        cur_prop = &(page->props [i]);
        while (cur_prop->label != NULL) {
                cur_prop = &(page->props [i]);
                prop_widget = create_prop_widget (dlg, cur_prop);
                gtk_box_pack_start (GTK_BOX (vbox), prop_widget,
                                    FALSE, FALSE, 0);
                i++;
                cur_prop = &(page->props [i]);
        }

        gnome_property_box_append_page (GNOME_PROPERTY_BOX (dlg->prop_box),
                                        vbox,
                                        gtk_label_new (_(page->title)));
}

/* Help callback for the preferences dialog */
static void
help_callback (GnomePropertyBox *pb, gint page_num, gpointer data)
{
	static GnomeHelpMenuEntry entry = {
		"gmc",
		"gmcprefs.html"
	};

	gnome_help_display (NULL, &entry);
}

static void
create_prop_box (PrefsDlg *dlg)
{
        gint i;
        PrefsPage *cur_page;

	dlg->prop_box = gnome_property_box_new ();
	gnome_dialog_set_parent (GNOME_DIALOG (dlg->prop_box), GTK_WINDOW (dlg->panel->xwindow));
	gtk_window_set_modal (GTK_WINDOW (dlg->prop_box), TRUE);
	gtk_window_set_title (GTK_WINDOW (dlg->prop_box), _("Preferences"));

        i = 0;
        cur_page = &(dlg->prefs_pages [i]);
        while (cur_page->title != NULL) {
                create_page (dlg, cur_page);
                i++;
                cur_page = &(dlg->prefs_pages [i]);
        }

	dlg->desktop_prefs = desktop_prefs_new (GNOME_PROPERTY_BOX (dlg->prop_box));
	dlg->desktop_prefs_page = i++;

	dlg->custom_layout = custom_layout_create_page (GNOME_PROPERTY_BOX (dlg->prop_box),
							dlg->panel);
	dlg->custom_layout_page = i;

        gtk_signal_connect (GTK_OBJECT (dlg->prop_box), "apply",
                            GTK_SIGNAL_FUNC (apply_callback), dlg);
	gtk_signal_connect (GTK_OBJECT (dlg->prop_box), "help",
			    GTK_SIGNAL_FUNC (help_callback), dlg);
}

void
gnome_configure_box (GtkWidget *widget, WPanel *panel)
{
	static PrefsDlg dlg;

        dlg.panel = panel;
        dlg.prefs_pages = prefs_pages;

        create_prop_box (&dlg);
        gtk_widget_show (dlg.prop_box);
}
