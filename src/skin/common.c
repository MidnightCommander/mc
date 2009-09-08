/*
   Skins engine.
   Interface functions

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include <stdlib.h>

#include "../src/global.h"
#include "../src/args.h"
#include "skin.h"
#include "internal.h"

/*** global variables ****************************************************************************/

mc_skin_t mc_skin__default;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean mc_skin_is_init = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_hash_destroy_key (gpointer data)
{
    g_free(data);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_hash_destroy_value (gpointer data)
{
    mc_skin_color_t *mc_skin_color = (mc_skin_color_t *) data;
    g_free (mc_skin_color->fgcolor);
    g_free (mc_skin_color->bgcolor);
    g_free (mc_skin_color);
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_skin_get_default_name(void)
{
    char *tmp_str;

    /* from command line */
    if (mc_args__skin != NULL)
	return g_strdup(mc_args__skin);

    /* from envirovement variable */
    tmp_str = getenv ("MC_SKIN");
    if (tmp_str != NULL)
	return g_strdup(tmp_str);

    /*  from config. Or 'default' if no present in config */
    return mc_config_get_string(mc_main_config, CONFIG_APP_SECTION, "skin" , "default");
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_reinit(void)
{
    mc_skin_deinit();
    mc_skin__default.name = mc_skin_get_default_name();
    mc_skin__default.colors = g_hash_table_new_full (g_str_hash, g_str_equal,
			       mc_skin_hash_destroy_key,
			       mc_skin_hash_destroy_value);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_init(void)
{
    mc_skin__default.name = mc_skin_get_default_name();
    mc_skin__default.colors = g_hash_table_new_full (g_str_hash, g_str_equal,
			       mc_skin_hash_destroy_key,
			       mc_skin_hash_destroy_value);

    if (! mc_skin_ini_file_load(&mc_skin__default))
    {
	mc_skin_reinit();
	mc_skin_set_hardcoded_skin(&mc_skin__default);
    }

    if (! mc_skin_ini_file_parce(&mc_skin__default))
    {
	mc_skin_reinit();
	mc_skin_set_hardcoded_skin(&mc_skin__default);
	(void) mc_skin_ini_file_parce(&mc_skin__default);
    }
    mc_skin_is_init = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_deinit(void)
{

    g_free(mc_skin__default.name);
    mc_skin__default.name = NULL;
    g_hash_table_destroy (mc_skin__default.colors);
    mc_skin__default.colors = NULL;

    g_free(mc_skin__default.description);
    mc_skin__default.description = NULL;

    if (mc_skin__default.config)
    {
        mc_config_deinit (mc_skin__default.config);
        mc_skin__default.config = NULL;
    }

    mc_skin_is_init = FALSE;
}

/* --------------------------------------------------------------------------------------------- */
