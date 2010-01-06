/*
   File highlight plugin.
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


#include "../src/global.h"
#include "../lib/filehighlight/fhl.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_fhl_array_free (mc_fhl_t * fhl)
{
    guint i;
    mc_fhl_filter_t *mc_filter;

    if (fhl->filters == NULL)
        return;

    for (i = 0; i < fhl->filters->len; i++) {
        mc_filter = (mc_fhl_filter_t *) g_ptr_array_index (fhl->filters, i);

        g_free (mc_filter->fgcolor);
        g_free (mc_filter->bgcolor);

        if (mc_filter->search_condition != NULL)
            mc_search_free (mc_filter->search_condition);

        g_free (mc_filter);
    }
    g_ptr_array_free (fhl->filters, TRUE);
    fhl->filters = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_fhl_t *
mc_fhl_new (gboolean need_auto_fill)
{

    mc_fhl_t *fhl;

    fhl = g_try_new0 (mc_fhl_t, 1);
    if (fhl == NULL)
        return NULL;

    if (!need_auto_fill)
        return fhl;

    if (!mc_fhl_init_from_standart_files (fhl)) {
        g_free (fhl);
        return NULL;
    }

    if (!mc_fhl_parse_ini_file (fhl)) {
        mc_fhl_free (&fhl);
        return NULL;
    }

    return fhl;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_fhl_free (mc_fhl_t ** fhl)
{
    if (fhl == NULL || *fhl == NULL)
        return;

    mc_fhl_clear (*fhl);

    g_free (*fhl);
    *fhl = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_fhl_clear (mc_fhl_t * fhl)
{
    if (fhl == NULL)
        return;

    if (fhl->config)
        mc_config_deinit (fhl->config);

    mc_fhl_array_free (fhl);
}

/* --------------------------------------------------------------------------------------------- */
