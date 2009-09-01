/*
   File highlight plugin.
   Reading and parce rules from ini-files

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
#include <string.h>

#include "../src/global.h"
#include "../src/main.h"
#include "../src/strescape.h"
#include "../src/tty/color.h"
#include "../src/filehighlight/fhl.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define mc_fhl_INI_FILE "filehighlight.ini"

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_fhl_parce_fill_color_info (mc_fhl_filter_t * mc_filter, mc_fhl_t * fhl, const gchar * group_name)
{
    gchar **colors;
    gsize colors_size;

    colors = mc_config_get_string_list (fhl->config, "filehighlight", group_name, &colors_size);

    if (colors == NULL)
        return;

    if (colors[0] == NULL) {
        g_strfreev (colors);
        return;
    }

    mc_filter->fgcolor = g_strdup (colors[0]);

    if (colors[1] == NULL)
        mc_filter->bgcolor = NULL;
    else
        mc_filter->bgcolor = g_strdup (colors[1]);

    g_strfreev (colors);

    mc_filter->color_pair_index = tty_try_alloc_color_pair (mc_filter->fgcolor, mc_filter->bgcolor);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_fhl_parce_get_file_type_id (mc_fhl_t * fhl, const gchar * group_name)
{
    mc_fhl_filter_t *mc_filter;

    const gchar *types[] = {
        "FILE", "FILE_EXE",
        "DIR", "LINK_DIR",
        "LINK", "HARDLINK", "SYMLINK",
        "STALE_LINK",
        "DEVICE", "DEVICE_BLOCK", "DEVICE_CHAR",
        "SPECIAL", "SPECIAL_SOCKET", "SPECIAL_FIFO", "SPECIAL_DOOR",
        NULL
    };
    int i;
    gchar *param_type = mc_config_get_string (fhl->config, group_name, "type", "");

    if (*param_type == '\0') {
        g_free (param_type);
        return FALSE;
    }

    for (i = 0; types[i] != NULL; i++) {
        if (strcmp (types[i], param_type) == 0)
            break;
    }
    g_free (param_type);
    if (types[i] == NULL)
        return FALSE;

    mc_filter = g_new0 (mc_fhl_filter_t, 1);
    mc_filter->type = MC_FLHGH_T_FTYPE;
    mc_filter->file_type = (mc_flhgh_ftype_type) i;
    mc_fhl_parce_fill_color_info (mc_filter, fhl, group_name);

    g_ptr_array_add (fhl->filters, (gpointer) mc_filter);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_fhl_parce_get_regexp (mc_fhl_t * fhl, const gchar * group_name)
{
    mc_fhl_filter_t *mc_filter;
    gchar *regexp = mc_config_get_string (fhl->config, group_name, "regexp", "");

    if (*regexp == '\0') {
        g_free (regexp);
        return FALSE;
    }

    mc_filter = g_new0 (mc_fhl_filter_t, 1);
    mc_filter->type = MC_FLHGH_T_FREGEXP;
    mc_filter->search_condition = mc_search_new (regexp, -1);
    mc_filter->search_condition->is_case_sentitive = TRUE;
    mc_filter->search_condition->search_type = MC_SEARCH_T_REGEX;

    mc_fhl_parce_fill_color_info (mc_filter, fhl, group_name);
    g_ptr_array_add (fhl->filters, (gpointer) mc_filter);
    g_free (regexp);
    return TRUE;

}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_fhl_parce_get_extensions (mc_fhl_t * fhl, const gchar * group_name)
{
    mc_fhl_filter_t *mc_filter;
    gchar **exts, **exts_orig;
    gchar *esc_ext;
    gsize exts_size;
    GString *buf = g_string_new ("");

    exts_orig = exts =
        mc_config_get_string_list (fhl->config, group_name, "extensions", &exts_size);

    if (exts_orig == NULL)
        return FALSE;

    if (exts_orig[0] == NULL) {
        g_strfreev (exts_orig);
        return FALSE;
    }

    while (*exts != NULL) {
        esc_ext = strutils_regex_escape (*exts);
        if (buf->len != 0)
            g_string_append_c (buf, '|');
        g_string_append (buf, esc_ext);
        g_free (esc_ext);
        exts++;
    }
    g_strfreev (exts_orig);
    esc_ext = g_string_free (buf, FALSE);
    buf = g_string_new (".*\\.(");
    g_string_append (buf, esc_ext);
    g_string_append (buf, ")$");
    g_free (esc_ext);

    mc_filter = g_new0 (mc_fhl_filter_t, 1);
    mc_filter->type = MC_FLHGH_T_FREGEXP;
    mc_filter->search_condition = mc_search_new (buf->str, -1);
    mc_filter->search_condition->is_case_sentitive = TRUE;
    mc_filter->search_condition->search_type = MC_SEARCH_T_REGEX;

    mc_fhl_parce_fill_color_info (mc_filter, fhl, group_name);
    g_ptr_array_add (fhl->filters, (gpointer) mc_filter);
    g_string_free (buf, TRUE);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */


gboolean
mc_fhl_read_ini_file (mc_fhl_t * fhl, const gchar * filename)
{
    if (fhl == NULL || filename == NULL || !exist_file (filename))
        return FALSE;

    if (fhl->config) {

        if (!mc_config_read_file (fhl->config, filename))
            return FALSE;

        return TRUE;
    }

    fhl->config = mc_config_init (filename);

    if (fhl->config == NULL)
        return FALSE;

    return TRUE;
}


/* --------------------------------------------------------------------------------------------- */

gboolean
mc_fhl_init_from_standart_files (mc_fhl_t * fhl)
{
    gchar *name;
    gchar *user_mc_dir;

    /* ${datadir}/mc/filehighlight.ini  */
    name = concat_dir_and_file (mc_home_alt, mc_fhl_INI_FILE);
    if (exist_file (name) && (!mc_fhl_read_ini_file (fhl, name))) {
        g_free (name);
        return FALSE;
    }
    g_free (name);

    /* ${sysconfdir}/mc/filehighlight.ini  */
    name = concat_dir_and_file (mc_home, mc_fhl_INI_FILE);
    if (exist_file (name) && (!mc_fhl_read_ini_file (fhl, name))) {
        g_free (name);
        return FALSE;
    }
    g_free (name);

    /* ~/.mc/filehighlight.ini */
    user_mc_dir = concat_dir_and_file (home_dir, MC_BASE);
    name = concat_dir_and_file (user_mc_dir, mc_fhl_INI_FILE);
    g_free (user_mc_dir);
    if (exist_file (name) && (!mc_fhl_read_ini_file (fhl, name))) {
        g_free (name);
        return FALSE;
    }
    g_free (name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_fhl_parce_ini_file (mc_fhl_t * fhl)
{
    gchar **ftype_names, **orig_ftype_names;
    gsize ftype_names_size;


    mc_fhl_array_free (fhl);
    fhl->filters = g_ptr_array_new ();

    if (!mc_config_has_group (fhl->config, "filehighlight"))
        return FALSE;

    orig_ftype_names = ftype_names =
        mc_config_get_keys (fhl->config, "filehighlight", &ftype_names_size);

    while (*ftype_names) {

        if (!mc_config_has_group (fhl->config, *ftype_names)) {
            ftype_names++;
            continue;
        }

        if (mc_config_has_param (fhl->config, *ftype_names, "type")) {
            /* parce filetype filter */
            mc_fhl_parce_get_file_type_id (fhl, *ftype_names);
        } else if (mc_config_has_param (fhl->config, *ftype_names, "regexp")) {
            /* parce regexp filter */
            mc_fhl_parce_get_regexp (fhl, *ftype_names);
        } else if (mc_config_has_param (fhl->config, *ftype_names, "extensions")) {
            /* parce extensions filter */
            mc_fhl_parce_get_extensions (fhl, *ftype_names);
        }

        ftype_names++;
    }

    g_strfreev (orig_ftype_names);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
