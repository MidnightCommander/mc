/*
   Editor key translation.

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2011, 2012
   The Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1996, 1997
   Andrew Borodin <aborodin@vmail.ru> 2012

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

/** \file
 *  \brief Source: editor key translation
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"        /* keys */
#include "lib/tty/key.h"        /* KEY_M_SHIFT */
#include "lib/strutil.h"        /* str_isutf8 () */
#include "lib/util.h"           /* is_printable() */
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "edit-impl.h"
#include "editwidget.h"         /* WEdit */
#include "editcmd_dialogs.h"

#include "src/keybind-defaults.h"       /* keybind_lookup_keymap_command() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/*
 * Translate the keycode into either 'command' or 'char_for_insertion'.
 * 'command' is one of the editor commands from cmddef.h.
 */
gboolean
edit_translate_key (WEdit * edit, long x_key, int *cmd, int *ch)
{
    unsigned long command = (unsigned long) CK_InsertChar;
    int char_for_insertion = -1;

    /* an ordinary insertable character */
    if (!edit->extmod && x_key < 256)
    {
#ifndef HAVE_CHARSET
        if (is_printable (x_key))
        {
            char_for_insertion = x_key;
            goto fin;
        }
#else
        int c;

        if (edit->charpoint >= 4)
        {
            edit->charpoint = 0;
            edit->charbuf[edit->charpoint] = '\0';
        }
        if (edit->charpoint < 4)
        {
            edit->charbuf[edit->charpoint++] = x_key;
            edit->charbuf[edit->charpoint] = '\0';
        }

        /* input from 8-bit locale */
        if (!mc_global.utf8_display)
        {
            /* source in 8-bit codeset */
            c = convert_from_input_c (x_key);

            if (is_printable (c))
            {
                if (!edit->utf8)
                    char_for_insertion = c;
                else
                    char_for_insertion = convert_from_8bit_to_utf_c2 ((unsigned char) x_key);
                goto fin;
            }
        }
        else
        {
            /* UTF-8 locale */
            int res;

            res = str_is_valid_char (edit->charbuf, edit->charpoint);
            if (res < 0 && res != -2)
            {
                edit->charpoint = 0;    /* broken multibyte char, skip */
                goto fin;
            }

            if (edit->utf8)
            {
                /* source in UTF-8 codeset */
                if (res < 0)
                {
                    char_for_insertion = x_key;
                    goto fin;
                }

                edit->charbuf[edit->charpoint] = '\0';
                edit->charpoint = 0;
                if (g_unichar_isprint (g_utf8_get_char (edit->charbuf)))
                {
                    char_for_insertion = x_key;
                    goto fin;
                }
            }
            else
            {
                /* 8-bit source */
                if (res < 0)
                {
                    /* not finised multibyte input (in meddle multibyte utf-8 char) */
                    goto fin;
                }

                if (g_unichar_isprint (g_utf8_get_char (edit->charbuf)))
                {
                    c = convert_from_utf_to_current (edit->charbuf);
                    edit->charbuf[0] = '\0';
                    edit->charpoint = 0;
                    char_for_insertion = c;
                    goto fin;
                }

                /* unprinteble utf input, skip it */
                edit->charbuf[0] = '\0';
                edit->charpoint = 0;
            }
        }
#endif /* HAVE_CHARSET */
    }

    /* Commands specific to the key emulation */
    if (edit->extmod)
    {
        edit->extmod = FALSE;
        command = keybind_lookup_keymap_command (editor_x_map, x_key);
    }
    else
        command = keybind_lookup_keymap_command (editor_map, x_key);

    if (command == CK_IgnoreKey)
        command = CK_InsertChar;

  fin:
    *cmd = (int) command;       /* FIXME */
    *ch = char_for_insertion;

    return !(command == (unsigned long) CK_InsertChar && char_for_insertion == -1);
}

/* --------------------------------------------------------------------------------------------- */
