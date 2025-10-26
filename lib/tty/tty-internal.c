/*
   Internal stuff of the terminal controlling library.

   Copyright (C) 2019-2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019.

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

/** \file
 *  \brief Source: internal stuff of the terminal controlling library.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib/global.h"
#include "lib/strutil.h"

#include <glib-unix.h>

#include "tty.h"
#include "tty-internal.h"

/*** global variables ****************************************************************************/

/* pipe to handle SIGWINCH */
int sigwinch_pipe[2];

GHashTable *double_line_map = NULL;

/*** file scope macro definitions ****************************************************************/

/*** global variables ****************************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_create_winch_pipe (void)
{
    GError *mcerror = NULL;

    if (!g_unix_open_pipe (sigwinch_pipe, FD_CLOEXEC, &mcerror))
    {
        fprintf (stderr, _ ("\nCannot create pipe for SIGWINCH: %s (%d)\n"), mcerror->message,
                 mcerror->code);
        g_error_free (mcerror);
        exit (EXIT_FAILURE);
    }

    /* If we read from an empty pipe, then read(2) will block until data is available.
     * If we write to a full pipe, then write(2) blocks until sufficient data has been read
     * from the pipe to allow the write to complete..
     * Therefore, use nonblocking I/O.
     */
    if (!g_unix_set_fd_nonblocking (sigwinch_pipe[0], TRUE, &mcerror))
    {
        fprintf (stderr, _ ("\nCannot configure write end of SIGWINCH pipe: %s (%d)\n"),
                 mcerror->message, mcerror->code);
        g_error_free (mcerror);
        tty_destroy_winch_pipe ();
        exit (EXIT_FAILURE);
    }

    if (!g_unix_set_fd_nonblocking (sigwinch_pipe[1], TRUE, &mcerror))
    {
        fprintf (stderr, _ ("\nCannot configure read end of SIGWINCH pipe: %s (%d)\n"),
                 mcerror->message, mcerror->code);
        g_error_free (mcerror);
        tty_destroy_winch_pipe ();
        exit (EXIT_FAILURE);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_destroy_winch_pipe (void)
{
    (void) close (sigwinch_pipe[0]);
    (void) close (sigwinch_pipe[1]);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_init_double_line_map (void)
{
    const gunichar double_lines_unicode[] = {
        0x2550,  // ═
        0x2551,  // ║
        0x2554,  // ╔
        0x2557,  // ╗
        0x255A,  // ╚
        0x255D,  // ╝
        0x255F,  // ╟
        0x2562,  // ╢
        0x2564,  // ╤
        0x2567,  // ╧
    };
    gunichar double_lines_local[G_N_ELEMENTS (double_lines_unicode)];
    gboolean error = FALSE;
    GIConv conv;
    GString *buffer;
    gchar utf8[7];
    estr_t conv_res;

    if (mc_global.utf8_display)
        return;

    conv = str_crt_conv_from ("UTF-8");
    if (conv == INVALID_CONV)
        return;

    buffer = g_string_new ("");
    for (unsigned int i = 0; i < G_N_ELEMENTS (double_lines_unicode); i++)
    {
        const int utf8len = g_unichar_to_utf8 (double_lines_unicode[i], utf8);
        utf8[utf8len] = '\0';
        g_string_assign (buffer, "");
        conv_res = str_convert (conv, utf8, buffer);
        if (conv_res != ESTR_SUCCESS)
        {
            error = TRUE;
            break;
        }
        double_lines_local[i] = (unsigned char) buffer->str[0];
    }
    str_close_conv (conv);
    g_string_free (buffer, TRUE);

    // Some charsets, e.g. KOI8-U only contain a subset of the double line characters.
    // We don't want mixed, broken appearance. If any of them are missing from the local charset
    // then refuse to use all of them.
    if (error)
        return;

    // All these characters can be represented in the locale. Create and fill up the hashtable.
    double_line_map = g_hash_table_new (g_direct_hash, g_direct_equal);
    for (unsigned int i = 0; i < G_N_ELEMENTS (double_lines_unicode); i++)
    {
        g_hash_table_insert (double_line_map,
                             GINT_TO_POINTER (tty_unicode_to_mc_acs (double_lines_unicode[i])),
                             GINT_TO_POINTER (double_lines_local[i]));
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_destroy_double_line_map (void)
{
    if (!mc_global.utf8_display && double_line_map != NULL)
        g_hash_table_destroy (double_line_map);

    double_line_map = NULL;
}

/* --------------------------------------------------------------------------------------------- */

gunichar
tty_double_line_map_lookup (mc_tty_char_t double_line)
{
    if (double_line_map == NULL)
        return 0;

    void *direct_value = g_hash_table_lookup (double_line_map, GINT_TO_POINTER (double_line));
    return GPOINTER_TO_INT (direct_value);
}

/* --------------------------------------------------------------------------------------------- */
