/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#include <config.h>
#include "capplet-widget.h"
#include "gnome.h"
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>
#include "edit-window.h"
#include "mime-data.h"
#include "mime-info.h"
#include "new-mime-window.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


/* Prototypes */
static void mime_fill_from_file (const char *filename, gboolean init_user);
static void mime_load_from_dir (const char *mime_info_dir, gboolean system_dir);
void add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user);
static char *get_priority (char *def, int *priority);


/* Global variables */
static char *current_lang;
static GHashTable *mime_types = NULL;
static GHashTable *initial_user_mime_types = NULL;
GHashTable *user_mime_types = NULL;
static GtkWidget *clist = NULL;
extern GtkWidget *delete_button;
extern GtkWidget *capplet;
/* Initialization functions */
static void
run_error (gchar *message)
{
	GtkWidget *error_box;

	error_box = gnome_message_box_new (
		message,
		GNOME_MESSAGE_BOX_ERROR,
		GNOME_STOCK_BUTTON_OK,
		NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (error_box));
}
static char *
get_priority (char *def, int *priority)
{
	*priority = 0;
	
	if (*def == ','){
		def++;
		if (*def == '1'){
			*priority = 0;
			def++;
		} else if (*def == '2'){
			*priority = 1;
			def++;
		}
	}

	while (*def && *def == ':')
		def++;

	return def;
}
static void
free_mime_info (MimeInfo *mi)
{

}
void
add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user)
{
	int priority = 1;
	char *s, *p, *ext;
	int used;
	MimeInfo *info;

	info = g_hash_table_lookup (table, (const void *) mime_type);
	if (info == NULL) {
		info = g_malloc (sizeof (MimeInfo));
		info->mime_type = g_strdup (mime_type);
		info->regex[0] = NULL;
		info->regex[1] = NULL;
		info->ext[0] = NULL;
		info->ext[1] = NULL;
                info->user_ext[0] = NULL;
                info->user_ext[1] = NULL;
                info->regex_readable[0] = NULL;
                info->regex_readable[1] = NULL;
                info->ext_readable[0] = NULL;
                info->ext_readable[1] = NULL;
                info->keys = gnome_mime_get_keys (mime_type);
		g_hash_table_insert (table, info->mime_type, info);
	}
	if (strncmp (def, "ext", 3) == 0){
		char *tokp;

		def += 3;
		def = get_priority (def, &priority);
		s = p = g_strdup (def);

		used = 0;
		
		while ((ext = strtok_r (s, " \t\n\r,", &tokp)) != NULL){
                        /* FIXME: We really need to check for duplicates before entering this. */
                        if (!init_user) {
                                info->ext[priority] = g_list_prepend (info->ext[priority], ext);
                        } else {
                                info->user_ext[priority] = g_list_prepend (info->user_ext[priority], ext);
                        }
			used = 1;
			s = NULL;
		}
		if (!used)
			g_free (p);
	}

	if (strncmp (def, "regex", 5) == 0){
		regex_t *regex;

		regex = g_new (regex_t, 1);
		def += 5;
		def = get_priority (def, &priority);

		while (*def && isspace (*def))
			def++;

		if (!*def)
			return;
		if (regcomp (regex, def, REG_EXTENDED | REG_NOSUB))
			g_free (regex);
		else {
			info->regex[priority] = regex;
                        g_free (info->regex_readable[priority]);
                        info->regex_readable[priority] = g_strdup (def);
                }
	}
}
static void
mime_fill_from_file (const char *filename, gboolean init_user)
{
	FILE *f;
	char buf [1024];
	char *current_key;
	gboolean used;
	
	g_assert (filename != NULL);

	f = fopen (filename, "r");
	if (!f)
		return;

	current_key = NULL;
	used = FALSE;
	while (fgets (buf, sizeof (buf), f)){
		char *p;

		if (buf [0] == '#')
			continue;

		/* Trim trailing spaces */
		for (p = buf + strlen (buf) - 1; p >= buf; p--){
			if (isspace (*p) || *p == '\n')
				*p = 0;
			else
				break;
		}
		
		if (!buf [0])
			continue;
		
		if (buf [0] == '\t' || buf [0] == ' '){
			if (current_key){
				char *p = buf;

				while (*p && isspace (*p))
					p++;

				if (*p == 0)
					continue;
				add_to_key (current_key, p, mime_types, init_user);
				if (init_user) {
                                        add_to_key (current_key, p, 
                                                    initial_user_mime_types,
                                                    TRUE);
                                        add_to_key (current_key, p, 
                                                    user_mime_types, TRUE);
                                }
				used = TRUE;
			}
		} else {
			if (!used && current_key)
				g_free (current_key);
			current_key = g_strdup (buf);
			if (current_key [strlen (current_key)-1] == ':')
				current_key [strlen (current_key)-1] = 0;
			
			used = FALSE;
		}
	}
	fclose (f);
}

static void
mime_load_from_dir (const char *mime_info_dir, gboolean system_dir)
{
	DIR *dir;
	struct dirent *dent;
	const int extlen = sizeof (".mime") - 1;
	char *filename;
	
	dir = opendir (mime_info_dir);
	if (!dir)
		return;
	if (system_dir) {
		filename = g_concat_dir_and_file (mime_info_dir, "gnome.mime");
		mime_fill_from_file (filename, FALSE);
		g_free (filename);
	}
	while ((dent = readdir (dir)) != NULL){
		
		int len = strlen (dent->d_name);

		if (len <= extlen)
			continue;
		
		if (strcmp (dent->d_name + len - extlen, ".mime"))
			continue;
		if (system_dir && !strcmp (dent->d_name, "gnome.mime"))
			continue;
		if (!system_dir && !strcmp (dent->d_name, "user.mime"))
			continue;
		
		filename = g_concat_dir_and_file (mime_info_dir, dent->d_name);
		mime_fill_from_file (filename, FALSE);
		g_free (filename);
	}
	if (!system_dir) {
		filename = g_concat_dir_and_file (mime_info_dir, "user.mime");
		mime_fill_from_file (filename, TRUE);
		g_free (filename);
	}
	closedir (dir);
}
static int
add_mime_vals_to_clist (gchar *mime_type, gpointer mi, gpointer cl)
{
        /* we also finalize the MimeInfo structure here, now that we're done
         * loading it */
        static gchar *text[2];
        GList *list;
        GString *extension;
        gint row;
        
        extension = g_string_new ("");
        for (list = ((MimeInfo *) mi)->ext[0];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        if (strcmp (extension->str, "") != 0 && ((MimeInfo *)mi)->user_ext[0])
                g_string_append (extension, ", ");
        for (list = ((MimeInfo *) mi)->user_ext[0]; list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        ((MimeInfo *) mi)->ext_readable[0] = extension->str;
        g_string_free (extension, FALSE);
        
        extension = g_string_new ("");
        for (list = ((MimeInfo *) mi)->ext[1];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next)
                        g_string_append (extension, ", ");
        }
        if (strcmp (extension->str, "") != 0 && ((MimeInfo *)mi)->user_ext[1])
                g_string_append (extension, ", ");
        for (list = ((MimeInfo *) mi)->user_ext[1]; list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        ((MimeInfo *) mi)->ext_readable[1] = extension->str;
        g_string_free (extension, FALSE);

        if (((MimeInfo *) mi)->ext[0] || ((MimeInfo *) mi)->user_ext[0]) {
                extension = g_string_new ((((MimeInfo *) mi)->ext_readable[0]));
                if (((MimeInfo *) mi)->ext[1] || ((MimeInfo *) mi)->user_ext[1]) {
                        g_string_append (extension, ", ");
                        g_string_append (extension, (((MimeInfo *) mi)->ext_readable[1]));
                }
        } else if (((MimeInfo *) mi)->ext[1] || ((MimeInfo *) mi)->user_ext[1])
                extension = g_string_new ((((MimeInfo *) mi)->ext_readable[1]));
        else
                extension = g_string_new ("");

        text[0] = ((MimeInfo *) mi)->mime_type;
        text[1] = extension->str;

        row = gtk_clist_insert (GTK_CLIST (cl), 1, text);
        gtk_clist_set_row_data (GTK_CLIST (cl), row, mi);
        g_string_free (extension, TRUE);
        return row;
}
static void
selected_row_callback (GtkWidget *widget, gint row, gint column, GdkEvent *event, gpointer data)
{
        MimeInfo *mi;
        if (column < 0)
                return;
        
        mi = (MimeInfo *) gtk_clist_get_row_data (GTK_CLIST (widget),row);
        
        if (event && event->type == GDK_2BUTTON_PRESS)
                launch_edit_window (mi);

        if (g_hash_table_lookup (user_mime_types, mi->mime_type)) {
                gtk_widget_set_sensitive (delete_button, TRUE);
        } else
                gtk_widget_set_sensitive (delete_button, FALSE);
}

/* public functions */
void
delete_clicked (GtkWidget *widget, gpointer data)
{
        MimeInfo *mi;
        gint row = 0;

        if (GTK_CLIST (clist)->selection)
                row = GPOINTER_TO_INT ((GTK_CLIST (clist)->selection)->data);
        else
                return;
        mi = (MimeInfo *) gtk_clist_get_row_data (GTK_CLIST (clist), row);

        gtk_clist_remove (GTK_CLIST (clist), row);
        g_hash_table_remove (user_mime_types, mi->mime_type);
        remove_mime_info (mi->mime_type);
        free_mime_info (mi);
        capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
                                      TRUE);
}

void
edit_clicked (GtkWidget *widget, gpointer data)
{
        MimeInfo *mi;
        gint row = 0;

        if (GTK_CLIST (clist)->selection)
                row = GPOINTER_TO_INT ((GTK_CLIST (clist)->selection)->data);
        else
                return;
        mi = (MimeInfo *) gtk_clist_get_row_data (GTK_CLIST (clist), row);
        if (mi)
                launch_edit_window (mi);
        gtk_clist_remove (GTK_CLIST (clist), row);
        row = add_mime_vals_to_clist (mi->mime_type, mi, clist);
        gtk_clist_select_row (GTK_CLIST (clist), row, 0);
}
void
add_clicked (GtkWidget *widget, gpointer data)
{
        launch_new_mime_window ();
}

GtkWidget *
get_mime_clist (void)
{
        GtkWidget *retval;
        gchar *titles[2];

        titles[0] = _("Mime Type");
        titles[1] = _("Extension");
        retval = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (retval),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	clist = gtk_clist_new_with_titles (2, titles);
        gtk_signal_connect (GTK_OBJECT (clist),
                            "select_row",
                            GTK_SIGNAL_FUNC (selected_row_callback),
                            NULL);
        gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
        gtk_clist_set_auto_sort (GTK_CLIST (clist), TRUE);
        if (clist)
                g_hash_table_foreach (mime_types, (GHFunc) add_mime_vals_to_clist, clist);
        gtk_clist_columns_autosize (GTK_CLIST (clist));
        gtk_clist_select_row (GTK_CLIST (clist), 0, 0);
        gtk_container_add (GTK_CONTAINER (retval), clist);
        return retval;
}

static void
finalize_mime_type_foreach (gpointer mime_type, gpointer info, gpointer data)
{
        MimeInfo *mi = (MimeInfo *)info;
        GList *list;
        GString *extension;
        
        extension = g_string_new ("");
        for (list = ((MimeInfo *) mi)->ext[0];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        if (strcmp (extension->str, "") != 0 && mi->user_ext[0])
                g_string_append (extension, ", ");
        for (list = ((MimeInfo *) mi)->user_ext[0]; list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        ((MimeInfo *) mi)->ext_readable[0] = extension->str;
        g_string_free (extension, FALSE);
        
        extension = g_string_new ("");
        for (list = ((MimeInfo *) mi)->ext[1];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next)
                        g_string_append (extension, ", ");
        }
        if (strcmp (extension->str, "") != 0 && mi->user_ext[1]) 
                g_string_append (extension, ", ");
        for (list = ((MimeInfo *) mi)->user_ext[1]; list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
        }
        ((MimeInfo *) mi)->ext_readable[1] = extension->str;
        g_string_free (extension, FALSE);

        if (((MimeInfo *) mi)->ext[0] || ((MimeInfo *) mi)->user_ext[0]) {
                extension = g_string_new ((((MimeInfo *) mi)->ext_readable[0]));
                if (((MimeInfo *) mi)->ext[1] || ((MimeInfo *) mi)->user_ext[1]) {
                        g_string_append (extension, ", ");
                        g_string_append (extension, (((MimeInfo *) mi)->ext_readable[1]));
                }
        } else if (((MimeInfo *) mi)->ext[1] || ((MimeInfo *) mi)->user_ext[1])
                extension = g_string_new ((((MimeInfo *) mi)->ext_readable[1]));
        else
                extension = g_string_new ("");
        g_string_free (extension, TRUE);
}
static void
finalize_user_mime ()
{
        g_hash_table_foreach (user_mime_types, finalize_mime_type_foreach, NULL);
        g_hash_table_foreach (initial_user_mime_types, finalize_mime_type_foreach, NULL);
}
void
init_mime_type (void)
{
	char *mime_info_dir;
	
	mime_types = g_hash_table_new (g_str_hash, g_str_equal);
	initial_user_mime_types = g_hash_table_new (g_str_hash, g_str_equal);
	user_mime_types = g_hash_table_new (g_str_hash, g_str_equal);

	mime_info_dir = gnome_unconditional_datadir_file ("mime-info");
	mime_load_from_dir (mime_info_dir, TRUE);
	g_free (mime_info_dir);

	mime_info_dir = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/mime-info");
	mime_load_from_dir (mime_info_dir, FALSE);
	g_free (mime_info_dir);
        finalize_user_mime ();
        init_mime_info ();
}
void
add_new_mime_type (gchar *mime_type, gchar *raw_ext, gchar *regexp1, gchar *regexp2)
{
        gchar *temp;
        MimeInfo *mi = NULL;
        gint row;
        gchar *ext = NULL;
        gchar *ptr, *ptr2;
        
        /* first we make sure that the information is good */
        if (mime_type == NULL || *mime_type == '\000') {
                run_error (_("You must enter a mime-type"));
                return;
        } else if ((raw_ext == NULL || *raw_ext == '\000') &&
                   (regexp1 == NULL || *regexp1 == '\000') &&
                   (regexp2 == NULL || *regexp2 == '\000')){
                run_error (_("You must add either a regular-expression or\na file-name extension"));
                return;
        }
        if (strchr (mime_type, '/') == NULL) {
                run_error (_("Please put your mime-type in the format:\nCATEGORY/TYPE\n\nFor Example:\nimage/png"));
                return;
        }
        if (g_hash_table_lookup (user_mime_types, mime_type) ||
            g_hash_table_lookup (mime_types, mime_type)) {
                run_error (_("This mime-type already exists"));
                return;
        }
        if (raw_ext || *raw_ext) {
                ptr2 = ext = g_malloc (sizeof (raw_ext));
                for (ptr = raw_ext;*ptr; ptr++) {
                        if (*ptr != '.' && *ptr != ',') {
                                *ptr2 = *ptr;
                                ptr2 += 1;
                        }
                }
                *ptr2 = '\000';
        }
        /* passed check, now we add it. */
        if (ext) {
                temp = g_strconcat ("ext: ", ext, NULL);
                add_to_key (mime_type, temp, user_mime_types, TRUE);
                mi = (MimeInfo *) g_hash_table_lookup (user_mime_types, mime_type);
                g_free (temp);
        }
        if (regexp1) {
                temp = g_strconcat ("regex: ", regexp1, NULL);
                add_to_key (mime_type, temp, user_mime_types, TRUE);
                g_free (temp);
        }
        if (regexp2) {
                temp = g_strconcat ("regex,2: ", regexp2, NULL);
                add_to_key (mime_type, temp, user_mime_types, TRUE);
                g_free (temp);
        }
        /* Finally add it to the clist */
        if (mi) {
                row = add_mime_vals_to_clist (mime_type, mi, clist);
                gtk_clist_select_row (GTK_CLIST (clist), row, 0);
                gtk_clist_moveto (GTK_CLIST (clist), row, 0, 0.5, 0.0);
        }
        g_free (ext);
}
static void
write_mime_foreach (gpointer mime_type, gpointer info, gpointer data)
{
	gchar *buf;
	MimeInfo *mi = (MimeInfo *) info;
	fwrite ((char *) mi->mime_type, 1, strlen ((char *) mi->mime_type), (FILE *) data);
	fwrite ("\n", 1, 1, (FILE *) data);
        if (mi->ext_readable[0]) {
                fwrite ("\text: ", 1, strlen ("\text: "), (FILE *) data);
                fwrite (mi->ext_readable[0], 1,
                        strlen (mi->ext_readable[0]),
                        (FILE *) data);
                fwrite ("\n", 1, 1, (FILE *) data);
        }
        if (mi->regex_readable[0]) {
                fwrite ("\tregex: ", 1, strlen ("\tregex: "), (FILE *) data);
                fwrite (mi->regex_readable[0], 1,
                        strlen (mi->regex_readable[0]),
                        (FILE *) data);
                fwrite ("\n", 1, 1, (FILE *) data);
        }
        if (mi->regex_readable[1]) {
                fwrite ("\tregex,2: ", 1, strlen ("\tregex,2: "), (FILE *) data);
                fwrite (mi->regex_readable[1], 1,
                        strlen (mi->regex_readable[1]),
                        (FILE *) data);
                fwrite ("\n", 1, 1, (FILE *) data);
        }
        fwrite ("\n", 1, 1, (FILE *) data);
}

static void
write_mime (GHashTable *hash)
{
	struct stat s;
	gchar *dirname, *filename;
	FILE *file;
	GtkWidget *error_box;

	dirname = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/mime-info");
	if ((stat (dirname, &s) < 0) || !(S_ISDIR (s.st_mode))){
		if (errno == ENOENT) {
			if (mkdir (dirname, S_IRWXU) < 0) {
				run_error (_("We are unable to create the directory\n"
					   "~/.gnome/mime-info\n\n"
					   "We will not be able to save the state."));
				return;
			}
		} else {
			run_error (_("We are unable to access the directory\n"
				   "~/.gnome/mime-info\n\n"
				   "We will not be able to save the state."));
			return;
		}
	}
	filename = g_concat_dir_and_file (dirname, "user.mime");
        
        remove (filename);
	file = fopen (filename, "w");
	if (file == NULL) {
		run_error (_("Cannot create the file\n~/.gnome/mime-info/user.mime\n\n"
			     "We will not be able to save the state"));
		return;
	}
	g_hash_table_foreach (hash, write_mime_foreach, file);
	fclose (file);
}

void
write_user_mime (void)
{
        write_mime (user_mime_types);
}

void
write_initial_mime (void)
{
        write_mime (initial_user_mime_types);
}

void
reread_list ()
{
        gtk_clist_freeze (GTK_CLIST (clist));
        gtk_clist_clear (GTK_CLIST (clist));
        g_hash_table_foreach (mime_types, (GHFunc) add_mime_vals_to_clist, clist);
        gtk_clist_thaw (GTK_CLIST (clist));
}
static void
clean_mime_type (gpointer mime_type, gpointer mime_info, gpointer data)
{
        /* we should write this )-: */
}
void
discard_mime_info ()
{
        gchar *filename;
	g_hash_table_foreach (user_mime_types, clean_mime_type, NULL);
	g_hash_table_destroy (user_mime_types);
	g_hash_table_foreach (initial_user_mime_types, clean_mime_type, NULL);
	g_hash_table_destroy (initial_user_mime_types);
	user_mime_types = g_hash_table_new (g_str_hash, g_str_equal);
        initial_user_mime_types = g_hash_table_new (g_str_hash, g_str_equal);
        
	filename = g_concat_dir_and_file (gnome_util_user_home (), "/.gnome/mime-info/user.keys");
	mime_fill_from_file (filename, TRUE);
        finalize_user_mime ();
        reread_list ();
	g_free (filename);
}







