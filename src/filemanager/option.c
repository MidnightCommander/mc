/*
   Configure box module for the Midnight Commander

   Copyright (C) 1994, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2009, 2010, 2011, 2012
   The Free Software Foundation, Inc.

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

/** \file option.c
 *  \brief Source: configure boxes module
 */

#include <config.h>

#include <stdlib.h>             /* atoi() */

#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"        /* str_term_width1() */
#include "lib/tty/key.h"        /* old_esc_mode_timeout */
#include "lib/widget.h"

#include "src/setup.h"          /* variables */
#include "src/history.h"        /* MC_HISTORY_ESC_TIMEOUT */
#include "src/execute.h"        /* pause_after_run */

#include "layout.h"             /* nice_rotating_dash */
#include "panel.h"              /* update_panels() */

#include "option.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

unsigned long configure_old_esc_mode_id, configure_time_out_id;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
configure_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_ACTION:
        /* message from "Single press" checkbutton */
        if (sender != NULL && sender->id == configure_old_esc_mode_id)
        {
            const gboolean not_single = !(((WCheck *) sender)->state & C_BOOL);
            Widget *ww;

            /* input line */
            ww = dlg_find_by_id (DIALOG (w), configure_time_out_id);
            widget_disable (ww, not_single);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
configure_box (void)
{
    const char *pause_options[] = {
        N_("&Never"),
        N_("On dum&b terminals"),
        N_("Alwa&ys")
    };

    int pause_options_num;


    pause_options_num = G_N_ELEMENTS (pause_options);

    {
        char time_out[BUF_TINY] = "";
        char *time_out_new;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("File operations")),
                    QUICK_CHECKBOX (N_("&Verbose operation"), &verbose, NULL),
                    QUICK_CHECKBOX (N_("Compute tota&ls"), &file_op_compute_totals, NULL),
                    QUICK_CHECKBOX (N_("Classic pro&gressbar"), &classic_progressbar, NULL),
                    QUICK_CHECKBOX (N_("Mkdi&r autoname"), &auto_fill_mkdir_name, NULL),
                    QUICK_CHECKBOX (N_("Preallocate &space"), &mc_global.vfs.preallocate_space,
                                    NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Esc key mode")),
                    QUICK_CHECKBOX (N_("S&ingle press"), &old_esc_mode, &configure_old_esc_mode_id),
                    QUICK_LABELED_INPUT (N_("Timeout:"), input_label_left,
                                         (const char *) time_out, 0, MC_HISTORY_ESC_TIMEOUT,
                                         &time_out_new, &configure_time_out_id),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Pause after run")),
                    QUICK_RADIO (pause_options_num, pause_options, &pause_after_run, NULL),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Other options")),
                    QUICK_CHECKBOX (N_("Use internal edi&t"), &use_internal_edit, NULL),
                    QUICK_CHECKBOX (N_("Use internal vie&w"), &use_internal_view, NULL),
                    QUICK_CHECKBOX (N_("Auto m&enus"), &auto_menu, NULL),
                    QUICK_CHECKBOX (N_("&Drop down menus"), &drop_menus, NULL),
                    QUICK_CHECKBOX (N_("Shell &patterns"), &easy_patterns, NULL),
                    QUICK_CHECKBOX (N_("Co&mplete: show all"),
                                    &mc_global.widget.show_all_if_ambiguous, NULL),
                    QUICK_CHECKBOX (N_("Rotating d&ash"), &nice_rotating_dash, NULL),
                    QUICK_CHECKBOX (N_("Cd follows lin&ks"), &mc_global.vfs.cd_symlinks, NULL),
                    QUICK_CHECKBOX (N_("Sa&fe delete"), &safe_delete, NULL),
                    QUICK_CHECKBOX (N_("A&uto save setup"), &auto_save_setup, NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Configure options"), "[Configuration]",
            quick_widgets, configure_callback, NULL
        };

        g_snprintf (time_out, sizeof (time_out), "%d", old_esc_mode_timeout);

        if (!old_esc_mode)
            quick_widgets[10].options = quick_widgets[11].options = W_DISABLED;

#ifndef HAVE_POSIX_FALLOCATE
        mc_global.vfs.preallocate_space = FALSE;
        quick_widgets[7].options = W_DISABLED;
#endif

        if (quick_dialog (&qdlg) == B_ENTER)
            old_esc_mode_timeout = atoi (time_out_new);

        g_free (time_out_new);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_options_box (void)
{
    const char *qsearch_options[] = {
        N_("Case &insensitive"),
        N_("Cas&e sensitive"),
        N_("Use panel sort mo&de")
    };

    int simple_swap;

    simple_swap = mc_config_get_bool (mc_main_config, CONFIG_PANELS_SECTION,
                                      "simple_swap", FALSE) ? 1 : 0;

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("Main options")),
                    QUICK_CHECKBOX (N_("Show mi&ni-status"),  &panels_options.show_mini_info, NULL),
                    QUICK_CHECKBOX (N_("Use SI si&ze units"), &panels_options.kilobyte_si, NULL),
                    QUICK_CHECKBOX (N_("Mi&x all files"), &panels_options.mix_all_files, NULL),
                    QUICK_CHECKBOX (N_("Show &backup files"), &panels_options.show_backups, NULL),
                    QUICK_CHECKBOX (N_("Show &hidden files"), &panels_options.show_dot_files, NULL),
                    QUICK_CHECKBOX (N_("&Fast dir reload"),  &panels_options.fast_reload, NULL),
                    QUICK_CHECKBOX (N_("Ma&rk moves down"),  &panels_options.mark_moves_down, NULL),
                    QUICK_CHECKBOX (N_("Re&verse files only"), &panels_options.reverse_files_only,
                                    NULL),
                    QUICK_CHECKBOX (N_("Simple s&wap"), &simple_swap, NULL),
                    QUICK_CHECKBOX (N_("A&uto save panels setup"), &panels_options.auto_save_setup,
                                    NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Navigation")),
                    QUICK_CHECKBOX (N_("L&ynx-like motion"), &panels_options.navigate_with_arrows,
                                    NULL),
                    QUICK_CHECKBOX (N_("Pa&ge scrolling"), &panels_options.scroll_pages, NULL),
                    QUICK_CHECKBOX (N_("&Mouse page scrolling"), &panels_options.mouse_move_pages,
                                    NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("File highlight")),
                    QUICK_CHECKBOX (N_("File &types"), &panels_options.filetype_mode, NULL),
                    QUICK_CHECKBOX (N_("&Permissions"), &panels_options.permission_mode, NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Quick search")),
                    QUICK_RADIO (QSEARCH_NUM, qsearch_options, (int *) &panels_options.qsearch_mode,
                                 NULL),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Panel options"), "[Panel options]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_ENTER)
            return;
    }

    mc_config_set_bool (mc_main_config, CONFIG_PANELS_SECTION,
                        "simple_swap", (gboolean) (simple_swap & C_BOOL));

    if (!panels_options.fast_reload_msg_shown && panels_options.fast_reload)
    {
        message (D_NORMAL, _("Information"),
                 _("Using the fast reload option may not reflect the exact\n"
                   "directory contents. In this case you'll need to do a\n"
                   "manual reload of the directory. See the man page for\n" "the details."));
        panels_options.fast_reload_msg_shown = TRUE;
    }

    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */
