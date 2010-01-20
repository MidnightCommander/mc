/*
   Internal file viewer for the Midnight Commander
   Common finctions (used from some other mcviewer functions)

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995, 1998 Miguel de Icaza
	       1994, 1995 Janne Kukonlehto
	       1995 Jakub Jelinek
	       1996 Joseph M. Hinkle
	       1997 Norbert Warmuth
	       1998 Pavel Machek
	       2004 Roland Illig <roland.illig@gmx.de>
	       2005 Roland Illig <roland.illig@gmx.de>
	       2009 Slava Zanko <slavazanko@google.com>
	       2009 Andrew Borodin <aborodin@vmail.ru>
	       2009 Ilia Maslakov <il.smind@gmail.com>

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

#include <limits.h>

#include "lib/global.h"
#include "src/wtools.h"
#include "src/strutil.h"
#include "src/main.h"
#include "src/charsets.h"
#include "src/selcodepage.h"
#include "lib/vfs/mc-vfs/vfs.h"

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

#define OFF_T_BITWIDTH (unsigned int) (sizeof (off_t) * CHAR_BIT - 1)
const off_t INVALID_OFFSET = (off_t) - 1;
const off_t OFFSETTYPE_MAX = ((off_t) 1 << (OFF_T_BITWIDTH - 1)) - 1;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_magic_mode (mcview_t * view)
{
    char *filename, *command;

    mcview_altered_magic_flag = 1;
    view->magic_mode = !view->magic_mode;
    filename = g_strdup (view->filename);
    command = g_strdup (view->command);

    mcview_done (view);
    mcview_load (view, command, filename, 0);
    g_free (filename);
    g_free (command);
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_wrap_mode (mcview_t * view)
{
    view->text_wrap_mode = !view->text_wrap_mode;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_nroff_mode (mcview_t * view)
{
    view->text_nroff_mode = !view->text_nroff_mode;
    mcview_altered_nroff_flag = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_hex_mode (mcview_t * view)
{
    view->hex_mode = !view->hex_mode;

    if (view->hex_mode) {
        view->hex_cursor = view->dpy_start;
        view->dpy_start = mcview_offset_rounddown (view->dpy_start, view->bytes_per_line);
        view->widget.options |= W_WANT_CURSOR;
    } else {
        view->dpy_start = view->hex_cursor;
        mcview_moveto_bol (view);
        view->widget.options &= ~W_WANT_CURSOR;
    }
    mcview_altered_hex_mode = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_ok_to_quit (mcview_t * view)
{
    int r;

    if (view->change_list == NULL)
        return TRUE;

    r = query_dialog (_("Quit"),
                      _(" File was modified, Save with exit? "), D_NORMAL, 3,
                      _("&Cancel quit"), _("&Yes"), _("&No"));

    switch (r) {
    case 1:
        return mcview_hexedit_save_changes (view);
    case 2:
        mcview_hexedit_free_change_list (view);
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_done (mcview_t * view)
{
    /* Save current file position */
    if (mcview_remember_file_position && view->filename != NULL) {
        char *canon_fname;
        canon_fname = vfs_canon (view->filename);
        save_file_position (canon_fname, -1, 0, view->dpy_start);
        g_free (canon_fname);
    }

    /* Write back the global viewer mode */
    mcview_default_hex_mode = view->hex_mode;
    mcview_default_nroff_flag = view->text_nroff_mode;
    mcview_default_magic_flag = view->magic_mode;
    mcview_global_wrap_mode = view->text_wrap_mode;

    /* Free memory used by the viewer */

    /* view->widget needs no destructor */

    g_free (view->filename), view->filename = NULL;
    g_free (view->command), view->command = NULL;

    mcview_close_datasource (view);
    /* the growing buffer is freed with the datasource */

    coord_cache_free (view->coord_cache), view->coord_cache = NULL;

    if (!(view->converter == INVALID_CONV || view->converter != str_cnv_from_term)) {
        str_close_conv (view->converter);
        view->converter = str_cnv_from_term;
    }

    mcview_hexedit_free_change_list (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_codeset (mcview_t * view)
{
#ifdef HAVE_CHARSET
    const char *cp_id = NULL;

    view->utf8 = TRUE;
    cp_id = get_codepage_id (source_codepage >= 0 ? source_codepage : display_codepage);
    if (cp_id != NULL) {
        GIConv conv;
        conv = str_crt_conv_from (cp_id);
        if (conv != INVALID_CONV) {
            if (view->converter != str_cnv_from_term)
                str_close_conv (view->converter);
            view->converter = conv;
        }
        view->utf8 = (gboolean) str_isutf8 (cp_id);
    }
#else
    (void) view;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_select_encoding (mcview_t * view)
{
#ifdef HAVE_CHARSET
    if (do_select_codepage ()) {
        mcview_set_codeset (view);
    }
#else
    (void) view;
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_show_error (mcview_t * view, const char *msg)
{
    mcview_close_datasource (view);
    if (mcview_is_in_panel (view)) {
        mcview_set_datasource_string (view, msg);
    } else {
        message (D_ERROR, MSG_ERROR, "%s", msg);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* returns index of the first char in the line */
/* it is constant for all line characters */
off_t
mcview_bol (mcview_t * view, off_t current)
{
    int c;
    off_t filesize;
    filesize = mcview_get_filesize (view);
    if (current <= 0)
        return 0;
    if (current > filesize)
        return filesize;
    if (!mcview_get_byte (view, current, &c))
        return current;
    if (c == '\n') {
        if (!mcview_get_byte (view, current - 1, &c))
            return current;
        if (c == '\r')
            current--;
    }
    while (current > 0) {
        if (!mcview_get_byte (view, current - 1, &c))
            break;
        if (c == '\r' || c == '\n')
            break;
        current--;
    }
    return current;
}

/* returns index of last char on line + width EOL */
/* mcview_eol of the current line == mcview_bol next line */
off_t
mcview_eol (mcview_t * view, off_t current)
{
    int c, prev_ch = 0;
    off_t filesize;
    filesize = mcview_get_filesize (view);
    if (current < 0)
        return 0;
    if (current >= filesize)
        return filesize;
    while (current < filesize) {
        if (!mcview_get_byte (view, current, &c))
            break;
        if (c == '\n') {
            current++;
            break;
        } else if (prev_ch == '\r') {
            break;
        }
        current++;
        prev_ch = c;
    }
    return current;
}
