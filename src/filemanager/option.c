/*
   Configure box module for the Midnight Commander

   Copyright (C) 1994, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2009, 2010, 2011
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

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* don't use max() macro to avoid double call of str_term_width1() in widget width calculation */
#undef max

static int
max (int a, int b)
{
    return a > b ? a : b;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
configure_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_ACTION:
        /* message from "Single press" checkbutton */
        if (sender != NULL && sender->id == 17)
        {
            const gboolean not_single = !(((WCheck *) sender)->state & C_BOOL);
            Widget *w;

            /* label */
            w = dlg_find_by_id (h, sender->id - 1);
            widget_disable (w, not_single);
            /* input */
            w = dlg_find_by_id (h, sender->id - 2);
            widget_disable (w, not_single);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
configure_box (void)
{
    int dlg_width = 60;
    int dlg_height = 21;

    char time_out[BUF_TINY] = "";
    char *time_out_new;

    const char *pause_options[] = {
        N_("&Never"),
        N_("On dum&b terminals"),
        N_("Alwa&ys")
    };

    int pause_options_num = G_N_ELEMENTS (pause_options);

    QuickWidget quick_widgets[] = {
        /* buttons */
        QUICK_BUTTON (38, dlg_width, dlg_height - 3, dlg_height, N_("&Cancel"), B_CANCEL, NULL),
        QUICK_BUTTON (14, dlg_width, dlg_height - 3, dlg_height, N_("&OK"), B_ENTER, NULL),
        /* other options */
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 12, dlg_height, N_("A&uto save setup"),
                        &auto_save_setup),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 11, dlg_height, N_("Sa&fe delete"),
                        &safe_delete),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 10, dlg_height, N_("Cd follows lin&ks"),
                        &mc_global.vfs.cd_symlinks),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 9, dlg_height, N_("Rotating d&ash"),
                        &nice_rotating_dash),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 8, dlg_height, N_("Co&mplete: show all"),
                        &mc_global.widget.show_all_if_ambiguous),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 7, dlg_height, N_("Shell &patterns"),
                        &easy_patterns),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 6, dlg_height, N_("&Drop down menus"),
                        &drop_menus),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 5, dlg_height, N_("Auto m&enus"), &auto_menu),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 4, dlg_height, N_("Use internal vie&w"),
                        &use_internal_view),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 3, dlg_height, N_("Use internal edi&t"),
                        &use_internal_edit),
        QUICK_GROUPBOX (dlg_width / 2, dlg_width, 2, dlg_height, dlg_width / 2 - 4, 16,
                        N_("Other options")),
        /* pause options */
        QUICK_RADIO (5, dlg_width, 14, dlg_height, pause_options_num, pause_options,
                     &pause_after_run),
        QUICK_GROUPBOX (3, dlg_width, 13, dlg_height, dlg_width / 2 - 4, 5, N_("Pause after run")),

        /* Esc key mode */
        QUICK_INPUT (10, dlg_width, 11, dlg_height, (const char *) time_out, 8, 0,
                     MC_HISTORY_ESC_TIMEOUT, &time_out_new),
        QUICK_LABEL (5, dlg_width, 11, dlg_height, N_("Timeout:")),
        QUICK_CHECKBOX (5, dlg_width, 10, dlg_height, N_("S&ingle press"), &old_esc_mode),
        QUICK_GROUPBOX (3, dlg_width, 9, dlg_height, dlg_width / 2 - 4, 4, N_("Esc key mode")),

        /* file operation options */
        QUICK_CHECKBOX (5, dlg_width, 7, dlg_height, N_("Preallocate &space"),
                        &mc_global.vfs.preallocate_space),
        QUICK_CHECKBOX (5, dlg_width, 6, dlg_height, N_("Mkdi&r autoname"), &auto_fill_mkdir_name),
        QUICK_CHECKBOX (5, dlg_width, 5, dlg_height, N_("Classic pro&gressbar"),
                        &classic_progressbar),
        QUICK_CHECKBOX (5, dlg_width, 4, dlg_height, N_("Compute tota&ls"),
                        &file_op_compute_totals),
        QUICK_CHECKBOX (5, dlg_width, 3, dlg_height, N_("&Verbose operation"), &verbose),
        QUICK_GROUPBOX (3, dlg_width, 2, dlg_height, dlg_width / 2 - 4, 7,
                        N_("File operation options")),
        QUICK_END
    };

    const size_t qw_num = G_N_ELEMENTS (quick_widgets) - 1;

    QuickDialog Quick_input = {
        dlg_width, dlg_height, -1, -1,
        N_("Configure options"), "[Configuration]",
        quick_widgets, configure_callback, NULL, TRUE
    };

    int b0_len, b1_len;
    int b_len, c_len, g_len, l_len;
    size_t i;

#ifdef ENABLE_NLS
    for (i = 0; i < qw_num; i++)
        switch (i)
        {
        case 0:
        case 1:
            /* buttons */
            quick_widgets[i].u.button.text = _(quick_widgets[i].u.button.text);
            break;
        case 12:
        case 14:
        case 18:
        case 24:
            /* groupboxes */
            quick_widgets[i].u.groupbox.title = _(quick_widgets[i].u.groupbox.title);
            break;
        case 13:
            {
                /* radio button */
                size_t j;
                for (j = 0; j < (size_t) pause_options_num; j++)
                    pause_options[j] = _(pause_options[j]);
            }
            break;
        case 15:
            /* input line */
            break;
        case 16:
            /* label */
            quick_widgets[i].u.label.text = _(quick_widgets[i].u.label.text);
            break;
        default:
            /* checkboxes */
            quick_widgets[i].u.checkbox.text = _(quick_widgets[i].u.checkbox.text);
            break;
        }

    Quick_input.title = _(Quick_input.title);
#endif /* ENABLE_NLS */

    /* calculate widget and dialog widths */
    /* dialog title */
    dlg_width = max (dlg_width, str_term_width1 (Quick_input.title) + 4);
    /* buttons */
    b0_len = str_term_width1 (quick_widgets[0].u.button.text) + 3;
    b1_len = str_term_width1 (quick_widgets[1].u.button.text) + 5;
    b_len = b0_len + b1_len + 1;

    /* checkboxes within groupboxes */
    c_len = 0;
    for (i = 2; i < 24; i++)
        if ((i < 12) || (i == 17) || (i > 18))
            c_len = max (c_len, str_term_width1 (quick_widgets[i].u.checkbox.text) + 3);
    /* radiobuttons */
    for (i = 0; i < (size_t) pause_options_num; i++)
        c_len = max (c_len, str_term_width1 (pause_options[i]) + 3);
    /* label + input */
    l_len = str_term_width1 (quick_widgets[16].u.label.text);
    c_len = max (c_len, l_len + 1 + 8);
    /* groupboxes */
    g_len = max (c_len + 2, str_term_width1 (quick_widgets[24].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[18].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[14].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[12].u.groupbox.title) + 4);
    /* dialog width */
    Quick_input.xlen = max (dlg_width, g_len * 2 + 9);
    Quick_input.xlen = max (Quick_input.xlen, b_len + 2);
    if ((Quick_input.xlen & 1) != 0)
        Quick_input.xlen++;

    /* fix widget parameters */
    for (i = 0; i < qw_num; i++)
        quick_widgets[i].x_divisions = Quick_input.xlen;

    /* groupboxes */
    quick_widgets[14].u.groupbox.width =
        quick_widgets[18].u.groupbox.width =
        quick_widgets[24].u.groupbox.width = Quick_input.xlen / 2 - 4;
    quick_widgets[12].u.groupbox.width = Quick_input.xlen / 2 - 3;

    /* input */
    quick_widgets[15].relative_x = quick_widgets[16].relative_x + l_len + 1;
    quick_widgets[15].u.input.len = quick_widgets[18].u.groupbox.width - l_len - 4;

    /* right column */
    quick_widgets[12].relative_x = Quick_input.xlen / 2;
    for (i = 2; i < 12; i++)
        quick_widgets[i].relative_x = quick_widgets[12].relative_x + 2;

    /* buttons */
    quick_widgets[1].relative_x = (Quick_input.xlen - b_len) / 3;
    quick_widgets[0].relative_x = 2 * quick_widgets[1].relative_x + b1_len + 1;

    g_snprintf (time_out, sizeof (time_out), "%d", old_esc_mode_timeout);

    if (!old_esc_mode)
        quick_widgets[15].options = quick_widgets[16].options = W_DISABLED;

#ifndef HAVE_POSIX_FALLOCATE
    mc_global.vfs.preallocate_space = FALSE;
    quick_widgets[19].options = W_DISABLED;
#endif

    if (quick_dialog (&Quick_input) == B_ENTER)
        old_esc_mode_timeout = atoi (time_out_new);

    g_free (time_out_new);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_options_box (void)
{
    int dlg_width = 60;
    int dlg_height = 19;

    int simple_swap = mc_config_get_bool (mc_main_config, CONFIG_PANELS_SECTION,
                                          "simple_swap", FALSE) ? 1 : 0;

    const char *qsearch_options[] = {
        N_("Case &insensitive"),
        N_("Case s&ensitive"),
        N_("Use panel sort mo&de")
    };

    QuickWidget quick_widgets[] = {
        /* buttons */
        QUICK_BUTTON (38, dlg_width, dlg_height - 3, dlg_height, N_("&Cancel"), B_CANCEL, NULL),
        QUICK_BUTTON (14, dlg_width, dlg_height - 3, dlg_height, N_("&OK"), B_ENTER, NULL),
        /* quick search */
        QUICK_RADIO (dlg_width / 2 + 2, dlg_width, 12, dlg_height, QSEARCH_NUM, qsearch_options,
                     (int *) &panels_options.qsearch_mode),
        QUICK_GROUPBOX (dlg_width / 2, dlg_width, 11, dlg_height, dlg_width / 2 - 4,
                        QSEARCH_NUM + 2,
                        N_("Quick search")),
        /* file highlighting */
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 9, dlg_height, N_("&Permissions"),
                        &panels_options.permission_mode),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 8, dlg_height, N_("File &types"),
                        &panels_options.filetype_mode),
        QUICK_GROUPBOX (dlg_width / 2, dlg_width, 7, dlg_height, dlg_width / 2 - 4, 4,
                        N_("File highlight")),
        /* navigation */
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 5, dlg_height, N_("&Mouse page scrolling"),
                        &panels_options.mouse_move_pages),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 4, dlg_height, N_("Pa&ge scrolling"),
                        &panels_options.scroll_pages),
        QUICK_CHECKBOX (dlg_width / 2 + 2, dlg_width, 3, dlg_height, N_("L&ynx-like motion"),
                        &panels_options.navigate_with_arrows),
        QUICK_GROUPBOX (dlg_width / 2, dlg_width, 2, dlg_height, dlg_width / 2 - 4, 5,
                        N_("Navigation")),
        /* main panel options */
        QUICK_CHECKBOX (5, dlg_width, 12, dlg_height, N_("A&uto save panels setup"),
                        &panels_options.auto_save_setup),
        QUICK_CHECKBOX (5, dlg_width, 11, dlg_height, N_("Simple s&wap"),
                        &simple_swap),
        QUICK_CHECKBOX (5, dlg_width, 10, dlg_height, N_("Re&verse files only"),
                        &panels_options.reverse_files_only),
        QUICK_CHECKBOX (5, dlg_width, 9, dlg_height, N_("Ma&rk moves down"),
                        &panels_options.mark_moves_down),
        QUICK_CHECKBOX (5, dlg_width, 8, dlg_height, N_("&Fast dir reload"),
                        &panels_options.fast_reload),
        QUICK_CHECKBOX (5, dlg_width, 7, dlg_height, N_("Show &hidden files"),
                        &panels_options.show_dot_files),
        QUICK_CHECKBOX (5, dlg_width, 6, dlg_height, N_("Show &backup files"),
                        &panels_options.show_backups),
        QUICK_CHECKBOX (5, dlg_width, 5, dlg_height, N_("Mi&x all files"),
                        &panels_options.mix_all_files),
        QUICK_CHECKBOX (5, dlg_width, 4, dlg_height, N_("Use SI si&ze units"),
                        &panels_options.kilobyte_si),
        QUICK_CHECKBOX (5, dlg_width, 3, dlg_height, N_("Show mi&ni-status"),
                        &panels_options.show_mini_info),
        QUICK_GROUPBOX (3, dlg_width, 2, dlg_height, dlg_width / 2 - 4, 14,
                        N_("Main options")),
        QUICK_END
    };

    const size_t qw_num = G_N_ELEMENTS (quick_widgets) - 1;

    QuickDialog Quick_input = {
        dlg_width, dlg_height, -1, -1,
        N_("Panel options"), "[Panel options]",
        quick_widgets, NULL, NULL, TRUE
    };

    int b0_len, b1_len;
    int b_len, c_len, g_len;
    size_t i;

#ifdef ENABLE_NLS
    for (i = 0; i < qw_num; i++)
        switch (i)
        {
        case 0:
        case 1:
            /* buttons */
            quick_widgets[i].u.button.text = _(quick_widgets[i].u.button.text);
            break;
        case 2:
            {
                /* radio button */
                size_t j;
                for (j = 0; j < QSEARCH_NUM; j++)
                    qsearch_options[j] = _(qsearch_options[j]);
            }
            break;
        case 3:
        case 6:
        case 10:
        case 21:
            /* groupboxes */
            quick_widgets[i].u.groupbox.title = _(quick_widgets[i].u.groupbox.title);
            break;
        default:
            /* checkboxes */
            quick_widgets[i].u.checkbox.text = _(quick_widgets[i].u.checkbox.text);
            break;
        }

    Quick_input.title = _(Quick_input.title);
#endif /* ENABLE_NLS */

    /* calculate widget and dialog widths */
    /* dialog title */
    dlg_width = max (dlg_width, str_term_width1 (Quick_input.title) + 4);
    /* buttons */
    b0_len = str_term_width1 (quick_widgets[0].u.button.text) + 3;
    b1_len = str_term_width1 (quick_widgets[1].u.button.text) + 5;
    b_len = b0_len + b1_len + 1;

    /* checkboxes within groupboxes */
    c_len = 0;
    for (i = 4; i < 21; i++)
        if ((i != 6) && (i != 10))
            c_len = max (c_len, str_term_width1 (quick_widgets[i].u.checkbox.text) + 4);

    /* radiobuttons */
    for (i = 0; i < QSEARCH_NUM; i++)
        c_len = max (c_len, str_term_width1 (qsearch_options[i]) + 3);
    /* groupboxes */
    g_len = max (c_len + 2, str_term_width1 (quick_widgets[3].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[6].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[10].u.groupbox.title) + 4);
    g_len = max (g_len, str_term_width1 (quick_widgets[21].u.groupbox.title) + 4);
    /* dialog width */
    Quick_input.xlen = max (dlg_width, g_len * 2 + 9);
    Quick_input.xlen = max (Quick_input.xlen, b_len + 2);
    if ((Quick_input.xlen & 1) != 0)
        Quick_input.xlen++;

    /* fix widget parameters */
    for (i = 0; i < qw_num; i++)
        quick_widgets[i].x_divisions = Quick_input.xlen;

    /* groupboxes */
    quick_widgets[3].u.groupbox.width =
        quick_widgets[6].u.groupbox.width =
        quick_widgets[10].u.groupbox.width = Quick_input.xlen / 2 - 3;
    quick_widgets[21].u.groupbox.width = Quick_input.xlen / 2 - 4;

    /* right column */
    quick_widgets[3].relative_x =
        quick_widgets[6].relative_x = quick_widgets[10].relative_x = Quick_input.xlen / 2;
    for (i = 2; i < 10; i++)
        if ((i != 3) && (i != 6))
            quick_widgets[i].relative_x = quick_widgets[3].relative_x + 2;

    /* buttons */
    quick_widgets[1].relative_x = (Quick_input.xlen - b_len) / 3;
    quick_widgets[0].relative_x = 2 * quick_widgets[1].relative_x + b1_len + 1;

    if (quick_dialog (&Quick_input) != B_ENTER)
        return;

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
