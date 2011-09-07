/* GLIB - Library of useful routines for C programming
   Copyright (C) 2009
   Free Software Foundation, Inc.

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

/** \file glibcompat.c
 *  \brief Source: compatibility with older versions of glib
 *
 *  Following code was copied from glib to GNU Midnight Commander to
 *  provide compatibility with older versions of glib.
 */

#include <config.h>

#include "global.h"


/* *INDENT-OFF* */
#ifdef HAVE_SUBSHELL_SUPPORT
#  ifdef SUBSHELL_OPTIONAL
#    define SUBSHELL_USE FALSE
#  else /* SUBSHELL_OPTIONAL */
#    define SUBSHELL_USE TRUE
#  endif /* SUBSHELL_OPTIONAL */
#else /* !HAVE_SUBSHELL_SUPPORT */
#    define SUBSHELL_USE FALSE
#endif /* !HAVE_SUBSHELL_SUPPORT */
/* *INDENT-ON* */

/*** global variables ****************************************************************************/

/* *INDENT-OFF* */
mc_global_t mc_global = {
#ifdef WITH_BACKGROUND
    .we_are_background = 0,
#endif /* WITH_BACKGROUND */

    .message_visible = 1,
    .keybar_visible = 1,
    .mc_run_mode = MC_RUN_FULL,

#ifdef HAVE_CHARSET
    .source_codepage = -1,
    .display_codepage = -1,
#else
    .eight_bit_clean = 1,
    .full_eight_bits = 0,
#endif /* !HAVE_CHARSET */

    .utf8_display = 0,
    .sysconfig_dir = NULL,
    .share_data_dir = NULL,

    .is_right = FALSE,

    .args =
    {
        .disable_colors = FALSE,
        .skin = NULL,
        .ugly_line_drawing = FALSE,
        .old_mouse = FALSE,
    },

    .widget =
    {
        .midnight_shutdown = FALSE,
        .confirm_history_cleanup = TRUE,
        .show_all_if_ambiguous = FALSE
    },

    .tty =
    {
        .setup_color_string = NULL,
        .term_color_string = NULL,
        .color_terminal_string = NULL,
#ifndef LINUX_CONS_SAVER_C
        .console_flag = '\0',
#endif /* !LINUX_CONS_SAVER_C */

        .use_subshell = SUBSHELL_USE,

#ifdef HAVE_SUBSHELL_SUPPORT
        .subshell_pty = 0,
#endif /* !HAVE_SUBSHELL_SUPPORT */

        .winch_flag = FALSE,
        .command_line_colors = NULL,
        .xterm_flag = FALSE,
        .slow_terminal = FALSE,
    },

    .vfs =
    {
        .cd_symlinks = TRUE
    }

};
/* *INDENT-ON* */

#undef SUBSHELL_USE

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
