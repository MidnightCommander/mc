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
#include "mime-info.h"
#include "mime-data.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#if !defined getc_unlocked && !defined HAVE_GETC_UNLOCKED
# define getc_unlocked(fp) getc (fp)
#endif

typedef struct {
	char       *mime_type;
	GHashTable *keys;
} GnomeMimeContext;

typedef enum {
	STATE_NONE,
	STATE_LANG,
	STATE_LOOKING_FOR_KEY,
	STATE_ON_MIME_TYPE,
	STATE_ON_KEY,
	STATE_ON_VALUE
} ParserState;

static char *current_lang = NULL;

/*
 * A hash table containing all of the Mime records for specific
 * mime types (full description, like image/png)
 */
static GHashTable *specific_types;
static GHashTable *initial_specific_types;

/*
 * A hash table containing all of the Mime records for non-specific
 * mime types (like image/\*)
 */
static GHashTable *generic_types;
static GHashTable *initial_generic_types;

#define SWITCH_TO_MIME_TYPE() { 
static GnomeMimeContext *
context_new (GString *str, gboolean is_default_context)
{
	GnomeMimeContext *context;
	GHashTable *table;
	char *mime_type, *p;
	
	mime_type = g_strdup (str->str);
	
	if (is_default_context) {
		if ((p = strstr (mime_type, "/*")) == NULL){
			table = initial_specific_types;
		} else {
			*(p+1) = 0;
			table = initial_generic_types;
		}
	} else {
		if ((p = strstr (mime_type, "/*")) == NULL){
			table = specific_types;
		} else {
			*(p+1) = 0;
			table = generic_types;
		}
	}
	context = g_hash_table_lookup (table, mime_type);

	if (context)
		return context;
	
	context = g_new (GnomeMimeContext, 1);
	context->mime_type = mime_type;
	context->keys = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (table, context->mime_type, context);
	return context;
}

static gboolean
release_key_and_value (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);

	return TRUE;
}


static gboolean
remove_this_key (gpointer key, gpointer value, gpointer user_data)
{
	if (strcmp (key, user_data) == 0){
		g_free (key);
		g_free (value);
		return TRUE;
	}

	return FALSE;
}
static void
context_add_key (GnomeMimeContext *context, char *key, char *value)
{
	char *v;

	v = g_hash_table_lookup (context->keys, key);
	if (v)
		g_hash_table_foreach_remove (context->keys, remove_this_key, key);

	g_hash_table_insert (context->keys, g_strdup (key), g_strdup (value));
}
static void
context_destroy (GnomeMimeContext *context)
{
	/*
	 * Remove the context from our hash tables, we dont know
	 * where it is: so just remove it from both (it can
	 * only be in one).
	 */
	if (context->mime_type) {
		g_hash_table_remove (specific_types, context->mime_type);
		g_hash_table_remove (generic_types, context->mime_type);
	}
	/*
	 * Destroy it
	 */
	if (context->keys) {
		g_hash_table_foreach_remove (context->keys, release_key_and_value, NULL);
		g_hash_table_destroy (context->keys);
	}
	g_free (context->mime_type);
	g_free (context);
}

static void
load_mime_type_info_from (char *filename)
{
	FILE *mime_file;
	gboolean in_comment, context_used;
	GString *line;
	int column, c;
	ParserState state;
	GnomeMimeContext *context, *default_context;
	char *key;

	mime_file = fopen (filename, "r");
	if (mime_file == NULL)
		return;

	in_comment = FALSE;
	context_used = FALSE;
	column = 0;
	context = NULL;
	default_context = NULL;
	key = NULL;
	line = g_string_sized_new (120);
	state = STATE_NONE;
	
	while ((c = getc_unlocked (mime_file)) != EOF){
		column++;
		if (c == '\r')
			continue;

		if (c == '#' && column == 0){
			in_comment = TRUE;
			continue;
		}
		
		if (c == '\n'){
			in_comment = FALSE;
			column = 0;
			if (state == STATE_ON_MIME_TYPE){
				context = context_new (line, FALSE);
				default_context = context_new (line, TRUE);
				context_used = FALSE;
				g_string_assign (line, "");
				state = STATE_LOOKING_FOR_KEY;
				continue;
			}
			if (state == STATE_ON_VALUE){
				context_used = TRUE;
				context_add_key (context, key, line->str);
				context_add_key (default_context, key, line->str);
				g_string_assign (line, "");
				g_free (key);
				key = NULL;
				state = STATE_LOOKING_FOR_KEY;
				continue;
			}
			continue;
		}

		if (in_comment)
			continue;

		switch (state){
		case STATE_NONE:
			if (c != ' ' && c != '\t')
				state = STATE_ON_MIME_TYPE;
			else
				break;
			/* fall down */
			
		case STATE_ON_MIME_TYPE:
			if (c == ':'){
				in_comment = TRUE;
				break;
			}
			g_string_append_c (line, c);
			break;

		case STATE_LOOKING_FOR_KEY:
			if (c == '\t' || c == ' ')
				break;

			if (c == '['){
				state = STATE_LANG;
				break;
			}

			if (column == 1){
				state = STATE_ON_MIME_TYPE;
				g_string_append_c (line, c);
				break;
			}
			state = STATE_ON_KEY;
			/* falldown */

		case STATE_ON_KEY:
			if (c == '\\'){
				c = getc (mime_file);
				if (c == EOF)
					break;
			}
			if (c == '='){
				key = g_strdup (line->str);
				g_string_assign (line, "");
				state = STATE_ON_VALUE;
				break;
			}
			g_string_append_c (line, c);
			break;

		case STATE_ON_VALUE:
			g_string_append_c (line, c);
			break;
			
		case STATE_LANG:
			if (c == ']'){
				state = STATE_ON_KEY;      
				if (current_lang && line->str [0]){
					if (strcmp (current_lang, line->str) != 0){
						in_comment = TRUE;
						state = STATE_LOOKING_FOR_KEY;
					}
				} else {
					in_comment = TRUE;
					state = STATE_LOOKING_FOR_KEY;
				}
				g_string_assign (line, "");
				break;
			}
			g_string_append_c (line, c);
			break;
		}
	}

	if (context){
		if (key && line->str [0]) {
			context_add_key (context, key, line->str);
			context_add_key (default_context, key, line->str);
		} else 
			if (!context_used) {
				context_destroy (context);
				context_destroy (default_context);
			}
		
	}

	g_string_free (line, TRUE);
	if (key)
		g_free (key);

	fclose (mime_file);
}
void
set_mime_key_value (gchar *mime_type, gchar *key, gchar *value)
{
	GnomeMimeContext *context;

	/* Assume no generic context's for now. */
	context = g_hash_table_lookup (specific_types, mime_type);
	if (context == NULL) {
		GString *str = g_string_new (mime_type);
		context = context_new (str, FALSE);
		g_string_free (str, TRUE);
		g_hash_table_insert (specific_types, mime_type, context);
	}
	context_add_key (context, key, value);
	
}
void
init_mime_info (void)
{
	gchar *filename;
	
	current_lang = getenv ("LANG");
	specific_types = g_hash_table_new (g_str_hash, g_str_equal);
	generic_types  = g_hash_table_new (g_str_hash, g_str_equal);
	initial_specific_types = g_hash_table_new (g_str_hash, g_str_equal);
	initial_generic_types  = g_hash_table_new (g_str_hash, g_str_equal);

	filename = g_concat_dir_and_file (gnome_util_user_home (), "/.gnome/mime-info/user.keys");
	load_mime_type_info_from (filename);
	g_free (filename);

}

const char *
local_mime_get_value (const char *mime_type, char *key)
{
	char *value, *generic_type, *p;
	GnomeMimeContext *context;
	
	g_return_val_if_fail (mime_type != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);
	context = g_hash_table_lookup (specific_types, mime_type);
	if (context){
		value = g_hash_table_lookup (context->keys, key);

		if (value)
			return value;
	}

	generic_type = g_strdup (mime_type);
	p = strchr (generic_type, '/');
	if (p)
		*(p+1) = 0;
	
	context = g_hash_table_lookup (generic_types, generic_type);
	g_free (generic_type);
	
	if (context){
		value = g_hash_table_lookup (context->keys, key);
		if (value)
			return value;
	}
	return NULL;
}
static void
clean_mime_foreach (gpointer mime_type, gpointer gmc, gpointer data)
{
	context_destroy ((GnomeMimeContext *) gmc);
}
static void
write_mime_keys_foreach (gpointer key_name, gpointer value, gpointer data)
{
	gchar *buf;
	if (current_lang && strcmp (current_lang, "C"))
		buf = g_strconcat ("\t[",
				   current_lang,
				   "]",
				   (gchar *) key_name,
				   "=",
				   (gchar *) value,
				   "\n", NULL);
	else
		buf = g_strconcat ("\t",
				   (gchar *) key_name,
				   "=",
				   (gchar *) value,
				   "\n", NULL);
	fwrite (buf, 1, strlen (buf), (FILE *) data);
	g_free (buf);
}
static void
write_mime_foreach (gpointer mime_type, gpointer gmc, gpointer data)
{
	gchar *buf;
	GnomeMimeContext *context = (GnomeMimeContext *) gmc;

	buf = g_strconcat ((gchar *) mime_type, ":\n", NULL);
	fwrite (buf, 1, strlen (buf), (FILE *) data);
	g_free (buf);
	g_hash_table_foreach (context->keys, write_mime_keys_foreach, data);
	fwrite ("\n", 1, strlen ("\n"), (FILE *) data);
}

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
static void
write_keys (GHashTable *spec_hash, GHashTable *generic_hash)
{
	struct stat s;
	gchar *dirname, *filename;
	FILE *file;
	GtkWidget *error_box;

	dirname = g_concat_dir_and_file (gnome_util_user_home (), ".gnome/mime-info");
	if ((stat (dirname, &s) < 0) || !(S_ISDIR (s.st_mode))){
		if (errno == ENOENT) {
			if (mkdir (dirname, S_IRWXU) < 0) {
				run_error ("We are unable to create the directory\n"
					   "~/.gnome/mime-info\n\n"
					   "We will not be able to save the state.");
				return;
			}
		} else {
			run_error ("We are unable to access the directory\n"
				   "~/.gnome/mime-info\n\n"
				   "We will not be able to save the state.");
			return;
		}
	}
	filename = g_concat_dir_and_file (dirname, "user.keys");
        
        remove (filename);
	file = fopen (filename, "w");
	if (file == NULL) {
		run_error (_("Cannot create the file\n~/.gnome/mime-info/user.keys\n\n"
			     "We will not be able to save the state"));
		return;
	}
	g_hash_table_foreach (spec_hash, write_mime_foreach, file);
	g_hash_table_foreach (generic_hash, write_mime_foreach, file);
	fclose (file);
}
void
write_initial_keys (void)
{
	write_keys (initial_generic_types, initial_specific_types);
}
void
write_user_keys (void)
{
	write_keys (generic_types, specific_types);
}
void
discard_mime_info (void)
{
	gchar *filename;
	
	current_lang = getenv ("LANG");
	g_hash_table_foreach (generic_types, clean_mime_foreach, NULL);
	g_hash_table_foreach (specific_types, clean_mime_foreach, NULL);
	g_hash_table_destroy (generic_types);
	g_hash_table_destroy (specific_types);
	specific_types = g_hash_table_new (g_str_hash, g_str_equal);
	generic_types  = g_hash_table_new (g_str_hash, g_str_equal);
	initial_specific_types = g_hash_table_new (g_str_hash, g_str_equal);
	initial_generic_types  = g_hash_table_new (g_str_hash, g_str_equal);

	filename = g_concat_dir_and_file (gnome_util_user_home (), "/.gnome/mime-info/user.keys");
	load_mime_type_info_from (filename);
	g_free (filename);

}
