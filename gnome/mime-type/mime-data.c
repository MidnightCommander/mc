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
#include "mime-data.h"
/* Prototypes */
static void mime_fill_from_file (const char *filename);
static void mime_load_from_dir (const char *mime_info_dir);
static void add_to_key (char *mime_type, char *def);
static char *get_priority (char *def, int *priority);


/* Global variables */
static char *current_lang;
                                /* Our original purpose for having
                                 * the hash table seems to have dissappeared.
                                 * Oh well... must-fix-later */
static GHashTable *mime_types = NULL;

/* Initialization functions */
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
add_to_key (char *mime_type, char *def)
{
	int priority = 1;
	char *s, *p, *ext;
	int used;
	MimeInfo *info;

	info = g_hash_table_lookup (mime_types, (const void *) mime_type);
	if (info == NULL) {
		info = g_malloc (sizeof (MimeInfo));
		info->mime_type = g_strdup (mime_type);
		info->regex[0] = NULL;
		info->regex[1] = NULL;
		info->ext[0] = NULL;
		info->ext[1] = NULL;
                info->keys = gnome_mime_get_keys (mime_type);
		g_hash_table_insert (mime_types, mime_type, info);
	}
	if (strncmp (def, "ext", 3) == 0){
		char *tokp;

		def += 3;
		def = get_priority (def, &priority);
		s = p = g_strdup (def);

		used = 0;
		
		while ((ext = strtok_r (s, " \t\n\r,", &tokp)) != NULL){
			info->ext[priority] = g_list_prepend (info->ext[priority], ext);
                        g_print ("inserting:%s:\n", ext);
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
		else
			info->regex[priority] = regex;
	}
}
static void
mime_fill_from_file (const char *filename)
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
				
				add_to_key (current_key, p);
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
mime_load_from_dir (const char *mime_info_dir)
{
	DIR *dir;
	struct dirent *dent;
	const int extlen = sizeof (".mime") - 1;
	
	dir = opendir (mime_info_dir);
	if (!dir)
		return;

	while ((dent = readdir (dir)) != NULL){
		char *filename;
		
		int len = strlen (dent->d_name);

		if (len <= extlen)
			continue;
		
		if (strcmp (dent->d_name + len - extlen, ".mime"))
			continue;

		filename = g_concat_dir_and_file (mime_info_dir, dent->d_name);
		mime_fill_from_file (filename);
		g_free (filename);
	}
	closedir (dir);
}
static void
add_mime_vals_to_clist (gchar *mime_type, gpointer mi, gpointer clist)
{
        static gchar *text[2];
        GList *list;
        GString *extension;
        gint row;
        
        extension = g_string_new ("");
        
        for (list = ((MimeInfo *) mi)->ext[0];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next != NULL)
                        g_string_append (extension, ", ");
                else if (((MimeInfo *) mi)->ext[1] != NULL)
                        g_string_append (extension, ", ");
        }
        for (list = ((MimeInfo *) mi)->ext[1];list; list=list->next) {
                g_string_append (extension, (gchar *) list->data);
                if (list->next)
                        g_string_append (extension, ", ");
        }
        text[0] = ((MimeInfo *) mi)->mime_type;
        text[1] = extension->str;
        row = gtk_clist_insert (GTK_CLIST (clist), 1, text);
        gtk_clist_set_row_data (GTK_CLIST (clist), row, mi);
        g_string_free (extension, TRUE);
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
}

/* public functions */
void
edit_clicked ()
{
        MimeInfo *mi;
        
        /*mi = (MimeInfo *) gtk_clist_get_row_data (GTK_CLIST (widget), row);*/
}

GtkWidget *
get_mime_clist ()
{
	GtkWidget *clist;
        GtkWidget *retval;
        gchar *titles[2];

        titles[0] = "Mime Type";
        titles[1] = "Extension";
        retval = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (retval),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	clist = gtk_clist_new_with_titles (2, titles);
        gtk_signal_connect (GTK_OBJECT (clist),
                            "select_row",
                            GTK_SIGNAL_FUNC (selected_row_callback),
                            NULL);
        gtk_clist_set_auto_sort (GTK_CLIST (clist), TRUE);
        g_hash_table_foreach (mime_types, (GHFunc) add_mime_vals_to_clist, clist);
        gtk_clist_columns_autosize (GTK_CLIST (clist));

        gtk_container_add (GTK_CONTAINER (retval), clist);
        return retval;
}
void
init_mime_type ()
{
	char *mime_info_dir;
	
	mime_types = g_hash_table_new (g_str_hash, g_str_equal);

	mime_info_dir = gnome_unconditional_datadir_file ("mime-info");
	mime_load_from_dir (mime_info_dir);
	g_free (mime_info_dir);

	mime_info_dir = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/mime-info");
	mime_load_from_dir (mime_info_dir);
	g_free (mime_info_dir);

}
