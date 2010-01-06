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
#include "../src/strutil.h"

/*** global variables **************************************************/

extern int utf8_display;

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

/*** file scope functions **********************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static gchar *
mc_config_normalize_before_save(const gchar * value)
{
    GIConv conv;
    GString *buffer;

    if (utf8_display)
    {
        buffer = g_string_new(value);
    }
    else
    {
        conv = str_crt_conv_to ("UTF-8");
        if (conv == INVALID_CONV)
            return g_strdup(value);

        buffer = g_string_new ("");

        if (str_convert (conv, value, buffer) == ESTR_FAILURE)
        {
            g_string_free(buffer, TRUE);
            buffer = g_string_new(value);
        }

        str_close_conv (conv);
    }

    return g_string_free(buffer, FALSE);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*** public functions **************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_direct_set_string (mc_config_t * mc_config, const gchar * group,
		      const gchar * param, const gchar * value)
{
    gchar *buffer;

    if (!mc_config || !group || !param || !value)
	return;

    buffer = mc_config_normalize_before_save(value);

    g_key_file_set_value (mc_config->handle, group, param, buffer);

    g_free(buffer);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_set_string (mc_config_t * mc_config, const gchar * group,
		      const gchar * param, const gchar * value)
{
    gchar *buffer;

    if (!mc_config || !group || !param || !value)
	return;

    buffer = mc_config_normalize_before_save(value);

    g_key_file_set_string (mc_config->handle, group, param, buffer);

    g_free(buffer);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_set_bool (mc_config_t * mc_config, const gchar * group,
		    const gchar * param, gboolean value)
{
    if (!mc_config || !group || !param )
	return;

    g_key_file_set_boolean (mc_config->handle, group, param, value);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_set_int (mc_config_t * mc_config, const gchar * group,
		   const gchar * param, int value)
{
    if (!mc_config || !group || !param )
	return;

    g_key_file_set_integer (mc_config->handle, group, param, value);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_set_string_list (mc_config_t * mc_config, const gchar * group,
			   const gchar * param, const gchar * const value[],
			   gsize length)
{
    if (!mc_config || !group || !param || !value || length == 0)
	return;

    g_key_file_set_string_list (mc_config->handle, group, param, value,
				length);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


void
mc_config_set_bool_list (mc_config_t * mc_config, const gchar * group,
			 const gchar * param, gboolean value[], gsize length)
{
    if (!mc_config || !group || !param || !value || length == 0)
	return;

    g_key_file_set_boolean_list (mc_config->handle, group, param, value,
				 length);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void
mc_config_set_int_list (mc_config_t * mc_config, const gchar * group,
			const gchar * param, int value[], gsize length)
{
    if (!mc_config || !group || !param || !value || length == 0)
	return;

    g_key_file_set_integer_list (mc_config->handle, group, param, value,
				 length);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
