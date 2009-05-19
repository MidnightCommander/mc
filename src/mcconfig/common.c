/* Configure module for the Midnight Commander
   Copyright (C) 1994, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2009 Free Software Foundation, Inc. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  
 */

#include <config.h>

#include "global.h"

#include "mcconfig.h"

/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/*** file scope functions **********************************************/

/*** public functions **************************************************/

mc_config_t *
mc_config_init (const gchar * ini_path)
{
    mc_config_t *mc_config;

    if (!ini_path || !exist_file (ini_path))
    {
	return NULL;
    }
    mc_config = g_try_malloc0 (sizeof (mc_config_t));

    if (mc_config == NULL)
	return NULL;

    mc_config->handle = g_key_file_new ();
    if (mc_config->handle == NULL)
    {
	g_free (mc_config);
	return NULL;
    }
    if (!g_key_file_load_from_file
	(mc_config->handle, ini_path, G_KEY_FILE_KEEP_COMMENTS, NULL))
    {
	g_key_file_free (mc_config->handle);
	g_free (mc_config);
	return NULL;
    }
    mc_config->ini_path = g_strdup (ini_path);
    return mc_config;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_deinit (mc_config_t * mc_config)
{
    if (!mc_config)
	return;

    g_free (mc_config->ini_path);
    g_key_file_free (mc_config->handle);
    g_free (mc_config);

}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_has_param (mc_config_t * mc_config, const char *group,
		     const gchar * param)
{
    if (!mc_config || !group || !param)
	return FALSE;

    return g_key_file_has_key (mc_config->handle, group, param, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_has_group (mc_config_t * mc_config, const char *group)
{
    if (!mc_config || !group)
	return FALSE;

    return g_key_file_has_group (mc_config->handle, group);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


gboolean
mc_config_del_param (mc_config_t * mc_config, const char *group,
		     const gchar * param)
{
    if (!mc_config || !group || !param)
	return FALSE;

    return g_key_file_remove_key (mc_config->handle, group, param, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_del_group (mc_config_t * mc_config, const char *group)
{
    if (!mc_config || !group)
	return FALSE;

    return g_key_file_remove_group (mc_config->handle, group, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_save_file (mc_config_t * mc_config)
{
    gchar *data;
    gsize len;
    gboolean ret;

    if (mc_config == NULL){
	return FALSE;
    }
    data = g_key_file_to_data (mc_config->handle,&len,NULL);
    if (exist_file(mc_config->ini_path))
    {
	mc_unlink (mc_config->ini_path);
    }

    ret = g_file_set_contents(mc_config->ini_path,data,len,NULL);
    g_free(data);
    return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
