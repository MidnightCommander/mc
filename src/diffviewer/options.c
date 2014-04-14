/*
   Search functions for diffviewer.

   Copyright (C) 2010-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.
   Andrew Borodin <aborodin@vmail.ru>, 2012

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/widget.h"

#include "src/history.h"

#include "internal.h"
#include "options.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_diffviewer_cmd_options_save (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    mc_config_set_bool (mc_main_config, "DiffView", "show_symbols",
                        dview->display_symbols != 0 ? TRUE : FALSE);
    mc_config_set_bool (mc_main_config, "DiffView", "show_numbers",
                        dview->display_numbers != 0 ? TRUE : FALSE);
    mc_config_set_int (mc_main_config, "DiffView", "tab_size", dview->tab_size);

    mc_config_set_int (mc_main_config, "DiffView", "diff_quality", dview->opt.quality);

    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_tws",
                        dview->opt.strip_trailing_cr);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_all_space",
                        dview->opt.ignore_all_space);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_space_change",
                        dview->opt.ignore_space_change);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_tab_expansion",
                        dview->opt.ignore_tab_expansion);
    mc_config_set_bool (mc_main_config, "DiffView", "diff_ignore_case", dview->opt.ignore_case);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_diffviewer_cmd_options_load (event_info_t * event_info, gpointer data, GError ** error)
{
    gboolean show_numbers, show_symbols;
    int tab_size;
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    show_symbols = mc_config_get_bool (mc_main_config, "DiffView", "show_symbols", FALSE);
    if (show_symbols)
        dview->display_symbols = 1;
    show_numbers = mc_config_get_bool (mc_main_config, "DiffView", "show_numbers", FALSE);
    if (show_numbers)
        dview->display_numbers = mc_diffviewer_calc_nwidth ((const GArray ** const) dview->a);
    tab_size = mc_config_get_int (mc_main_config, "DiffView", "tab_size", 8);
    if (tab_size > 0 && tab_size < 9)
        dview->tab_size = tab_size;
    else
        dview->tab_size = 8;

    dview->opt.quality = mc_config_get_int (mc_main_config, "DiffView", "diff_quality", 0);

    dview->opt.strip_trailing_cr =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_tws", FALSE);
    dview->opt.ignore_all_space =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_all_space", FALSE);
    dview->opt.ignore_space_change =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_space_change", FALSE);
    dview->opt.ignore_tab_expansion =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_tab_expansion", FALSE);
    dview->opt.ignore_case =
        mc_config_get_bool (mc_main_config, "DiffView", "diff_ignore_case", FALSE);

    dview->new_frame = 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_diffviewer_cmd_options_show_dialog (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    const char *quality_str[] = {
        N_("No&rmal"),
        N_("&Fastest (Assume large files)"),
        N_("&Minimal (Find a smaller set of change)")
    };

    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_START_GROUPBOX (N_("Diff algorithm")),
            QUICK_RADIO (3, (const char **) quality_str, (int *) &dview->opt.quality, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("Diff extra options")),
            QUICK_CHECKBOX (N_("&Ignore case"), &dview->opt.ignore_case, NULL),
            QUICK_CHECKBOX (N_("Ignore tab &expansion"), &dview->opt.ignore_tab_expansion, NULL),
            QUICK_CHECKBOX (N_("Ignore &space change"), &dview->opt.ignore_space_change, NULL),
            QUICK_CHECKBOX (N_("Ignore all &whitespace"), &dview->opt.ignore_all_space, NULL),
            QUICK_CHECKBOX (N_("Strip &trailing carriage return"), &dview->opt.strip_trailing_cr,
                            NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 56,
        N_("Diff Options"), "[Diff Options]",
        quick_widgets, NULL, NULL
    };

    (void) event_info;
    (void) error;

    if (quick_dialog (&qdlg) != B_CANCEL)
        mc_diffviewer_reread (dview);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
