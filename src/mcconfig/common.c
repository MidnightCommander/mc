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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"

#include "../../vfs/vfs.h"	/* mc_stat */

#include "mcconfig.h"

/*** global variables **************************************************/

mc_config_t *mc_main_config;
mc_config_t *mc_panels_config;

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/*** file scope functions **********************************************/

/*** public functions **************************************************/

mc_config_t *
mc_config_init (const gchar * ini_path)
{
    mc_config_t *mc_config;
    struct stat st;

    mc_config = g_try_malloc0 (sizeof (mc_config_t));

    if (mc_config == NULL)
	return NULL;

    mc_config->handle = g_key_file_new ();
    if (mc_config->handle == NULL)
    {
	g_free (mc_config);
	return NULL;
    }
    if (!ini_path || !exist_file (ini_path))
    {
	return mc_config;
    }

    if (!mc_stat (ini_path, &st) && st.st_size)
    {
	/* file present and not empty */
	g_key_file_load_from_file
	    (mc_config->handle, ini_path, G_KEY_FILE_KEEP_COMMENTS, NULL);
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
#if GLIB_CHECK_VERSION (2, 15, 0)
    return g_key_file_remove_key (mc_config->handle, group, param, NULL);
#else
    g_key_file_remove_key (mc_config->handle, group, param, NULL);
    return TRUE;
#endif
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_del_group (mc_config_t * mc_config, const char *group)
{
    if (!mc_config || !group)
	return FALSE;

#if GLIB_CHECK_VERSION (2, 15, 0)
    return g_key_file_remove_group (mc_config->handle, group, NULL);
#else
    g_key_file_remove_group (mc_config->handle, group, NULL);
    return TRUE;
#endif
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_read_file (mc_config_t * mc_config, const gchar * ini_path)
{
    mc_config_t *tmp_config;
    gchar **groups, **curr_grp;
    gchar **keys, **curr_key;
    gchar *value;

    if (mc_config == NULL){
	return FALSE;
    }

    tmp_config = mc_config_init(ini_path);
    if (tmp_config == NULL)
        return FALSE;

    groups = mc_config_get_groups (tmp_config, NULL);

    for (curr_grp = groups; *curr_grp != NULL; curr_grp++)
    {
        keys = mc_config_get_keys (tmp_config, *curr_grp ,NULL);
        for (curr_key = keys; *curr_key != NULL; curr_key++)
        {
            value = g_key_file_get_value (tmp_config->handle, *curr_grp, *curr_key, NULL);
            if (value == NULL)
                continue;

            g_key_file_set_value (mc_config->handle, *curr_grp, *curr_key, value);
            g_free(value);
        }
        g_strfreev(keys);
    }
    g_strfreev(groups);
    mc_config_deinit(tmp_config);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

gboolean
mc_config_save_file (mc_config_t * mc_config)
{
    gchar *data;
    gsize len;
    gboolean ret;

    if (mc_config == NULL || mc_config->ini_path == NULL){
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

gboolean
mc_config_save_to_file (mc_config_t * mc_config, const gchar * ini_path)
{
    gchar *data;
    gsize len;
    gboolean ret;

    if (mc_config == NULL){
	return FALSE;
    }
    data = g_key_file_to_data (mc_config->handle,&len,NULL);
    if (exist_file(ini_path))
    {
	mc_unlink (ini_path);
    }
    ret = g_file_set_contents(ini_path,data,len,NULL);

    g_free(data);
    return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
