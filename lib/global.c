/*
   Global structure for some library-related variables

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file glibcompat.c
 *  \brief Source: global structure for some library-related variables
 *
 */

#include <config.h>

#include "mc-version.h"

#include "global.h"

#ifdef ENABLE_SUBSHELL
#ifdef SUBSHELL_OPTIONAL
#define SUBSHELL_USE FALSE
#else
#define SUBSHELL_USE TRUE
#endif
#else
#define SUBSHELL_USE FALSE
#endif

/*** global variables ****************************************************************************/

mc_global_t mc_global =
{
    .mc_version = MC_CURRENT_VERSION,

    .mc_run_mode = MC_RUN_FULL,
    .run_from_parent_mc = FALSE,
    .midnight_shutdown = FALSE,

    .sysconfig_dir = NULL,
    .share_data_dir = NULL,

    .profile_name = NULL,

    .source_codepage = -1,
    .display_codepage = -1,
    .utf8_display = FALSE,

    .message_visible = TRUE,
    .keybar_visible = TRUE,

#ifdef ENABLE_BACKGROUND
    .we_are_background = FALSE,
#endif

    .widget =
    {
        .confirm_history_cleanup = TRUE,
        .show_all_if_ambiguous = FALSE,
        .is_right = FALSE
    },

    .shell = NULL,

    .tty =
    {
        .skin = NULL,
        .shadows = TRUE,
        .setup_color_string = NULL,
        .term_color_string = NULL,
        .color_terminal_string = NULL,
        .command_line_colors = NULL,
#ifndef LINUX_CONS_SAVER_C
        .console_flag = '\0',
#endif

        .use_subshell = SUBSHELL_USE,

#ifdef ENABLE_SUBSHELL
        .subshell_pty = 0,
#endif

        .xterm_flag = FALSE,
        .disable_x11 = FALSE,
        .slow_terminal = FALSE,
        .disable_colors = FALSE,
        .ugly_line_drawing = FALSE,
        .old_mouse = FALSE,
        .alternate_plus_minus = FALSE
    },

    .vfs =
    {
        .cd_symlinks = TRUE,
        .preallocate_space = FALSE,
    }

};

const char PACKAGE_COPYRIGHT[] = N_ ("Copyright (C) 1996-2025 the Free Software Foundation");

#undef SUBSHELL_USE

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
