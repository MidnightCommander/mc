/*
 * Simple Image cache for images
 * Copyright (C) 1998 the Free Software Foundation 
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */
#include <gnome.h>
#include <string.h>
#include "gcache.h"
static GHashTable *image_cache;

static void
destroy_image_callback (gpointer key, gpointer data, gpointer user_data)
{
	gdk_imlib_destroy_image (data);
	g_free (key);
}

void
image_cache_destroy ()
{
	g_hash_table_foreach (image_cache, destroy_image_callback, NULL);
	g_hash_table_destroy (image_cache);
}

GdkImlibImage *
image_cache_load_image (char *file)
{
	void *data;
	
	if (!image_cache)
		image_cache = g_hash_table_new (g_str_hash, g_str_equal);

	data = g_hash_table_lookup (image_cache, file);
	if (data)
		return data;

	data = gdk_imlib_load_image (file);
	if (data){
		g_hash_table_insert (image_cache, g_strdup (file), data);
	}
	return data;
}
