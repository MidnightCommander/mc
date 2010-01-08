/*
   Internal file viewer for the Midnight Commander
   Function for hex view

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

#include <errno.h>
#include <fcntl.h>

#include "src/global.h"
#include "lib/tty/tty.h"
#include "lib/skin/skin.h"
#include "src/main.h"
#include "src/wtools.h"
#include "src/charsets.h"

#include "lib/vfs/mc-vfs/vfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum {
    MARK_NORMAL,
    MARK_SELECTED,
    MARK_CURSOR,
    MARK_CHANGED
} mark_t;

/*** file scope variables ************************************************************************/

static const char hex_char[] = "0123456789ABCDEF";

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_hex (mcview_t * view)
{
    const screen_dimen top = view->data_area.top;
    const screen_dimen left = view->data_area.left;
    const screen_dimen height = view->data_area.height;
    const screen_dimen width = view->data_area.width;
    const int ngroups = view->bytes_per_line / 4;
    const screen_dimen text_start = 8 + 13 * ngroups + ((width < 80) ? 0 : (ngroups - 1 + 1));
    /* 8 characters are used for the file offset, and every hex group
     * takes 13 characters. On ``big'' screens, the groups are separated
     * by an extra vertical line, and there is an extra space before the
     * text column.
     */

    screen_dimen row, col;
    off_t from;
    int c;
    mark_t boldflag = MARK_NORMAL;
    struct hexedit_change_node *curr = view->change_list;
    size_t i;
    int ch = 0;

    char hex_buff[10];          /* A temporary buffer for sprintf and mvwaddstr */
    int bytes;                  /* Number of bytes already printed on the line */

    mcview_display_clean (view);

    /* Find the first displayable changed byte */
    from = view->dpy_start;
    while (curr && (curr->offset < from)) {
        curr = curr->next;
    }

    for (row = 0; mcview_get_byte (view, from, NULL) == TRUE && row < height; row++) {
        col = 0;

        /* Print the hex offset */
        g_snprintf (hex_buff, sizeof (hex_buff), "%08" OFFSETTYPE_PRIX " ",
                    (long unsigned int) from);
        widget_move (view, top + row, left);
        tty_setcolor (MARKED_COLOR);
        for (i = 0; col < width && hex_buff[i] != '\0'; i++) {
            tty_print_char (hex_buff[i]);
/*		tty_print_char(hex_buff[i]);*/
            col += 1;
        }
        tty_setcolor (NORMAL_COLOR);

        for (bytes = 0; bytes < view->bytes_per_line; bytes++, from++) {

#ifdef HAVE_CHARSET
            if (view->utf8) {
                int cw = 1;
                gboolean read_res = TRUE;
                ch = mcview_get_utf (view, from, &cw, &read_res);
                if (!read_res)
                    break;
            }
#endif
            if (! mcview_get_byte (view, from, &c))
                break;

            /* Save the cursor position for mcview_place_cursor() */
            if (from == view->hex_cursor && !view->hexview_in_text) {
                view->cursor_row = row;
                view->cursor_col = col;
            }

            /* Determine the state of the current byte */
            boldflag =
                (from == view->hex_cursor) ? MARK_CURSOR
                : (curr != NULL && from == curr->offset) ? MARK_CHANGED
                : (view->search_start <= from &&
                   from < view->search_end) ? MARK_SELECTED : MARK_NORMAL;

            /* Determine the value of the current byte */
            if (curr != NULL && from == curr->offset) {
                c = curr->value;
                curr = curr->next;
            }

            /* Select the color for the hex number */
            tty_setcolor (boldflag == MARK_NORMAL ? NORMAL_COLOR :
                          boldflag == MARK_SELECTED ? MARKED_COLOR :
                          boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag == MARK_CURSOR */
                          view->hexview_in_text ? MARKED_SELECTED_COLOR : VIEW_UNDERLINED_COLOR);

            /* Print the hex number */
            widget_move (view, top + row, left + col);
            if (col < width) {
                tty_print_char (hex_char[c / 16]);
                col += 1;
            }
            if (col < width) {
                tty_print_char (hex_char[c % 16]);
                col += 1;
            }

            /* Print the separator */
            tty_setcolor (NORMAL_COLOR);
            if (bytes != view->bytes_per_line - 1) {
                if (col < width) {
                    tty_print_char (' ');
                    col += 1;
                }

                /* After every four bytes, print a group separator */
                if (bytes % 4 == 3) {
                    if (view->data_area.width >= 80 && col < width) {
                        tty_print_one_vline ();
                        col += 1;
                    }
                    if (col < width) {
                        tty_print_char (' ');
                        col += 1;
                    }
                }
            }

            /* Select the color for the character; this differs from the
             * hex color when boldflag == MARK_CURSOR */
            tty_setcolor (boldflag == MARK_NORMAL ? NORMAL_COLOR :
                          boldflag == MARK_SELECTED ? MARKED_COLOR :
                          boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag == MARK_CURSOR */
                          view->hexview_in_text ? VIEW_UNDERLINED_COLOR : MARKED_SELECTED_COLOR);

#ifdef HAVE_CHARSET
            if (utf8_display) {
                if (!view->utf8) {
                    ch = convert_from_8bit_to_utf_c ((unsigned char) ch, view->converter);
                }
                if (!g_unichar_isprint (ch))
                    ch = '.';
            } else {
                if (view->utf8) {
                    ch = convert_from_utf_to_current_c (ch, view->converter);
                } else {
#endif
                    ch = convert_to_display_c (ch);
#ifdef HAVE_CHARSET
                }
            }
#endif
            c = convert_to_display_c (c);
            if (!g_ascii_isprint (c))
                c = '.';

            /* Print corresponding character on the text side */
            if (text_start + bytes < width) {
                widget_move (view, top + row, left + text_start + bytes);
                if (!view->utf8) {
                    tty_print_char (c);
                } else {
                    tty_print_anychar (ch);
                }
            }

            /* Save the cursor position for mcview_place_cursor() */
            if (from == view->hex_cursor && view->hexview_in_text) {
                view->cursor_row = row;
                view->cursor_col = text_start + bytes;
            }
        }
    }

    /* Be polite to the other functions */
    tty_setcolor (NORMAL_COLOR);

    mcview_place_cursor (view);
    view->dpy_end = from;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_hexedit_save_changes (mcview_t * view)
{
    struct hexedit_change_node *curr, *next;
    int fp, answer;
    char *text, *error;

    if (view->change_list == NULL)
        return TRUE;

  retry_save:
    assert (view->filename != NULL);
    fp = mc_open (view->filename, O_WRONLY);
    if (fp == -1)
        goto save_error;

    for (curr = view->change_list; curr != NULL; curr = next) {
        next = curr->next;

        if (mc_lseek (fp, curr->offset, SEEK_SET) == -1 || mc_write (fp, &(curr->value), 1) != 1)
            goto save_error;

        /* delete the saved item from the change list */
        view->change_list = next;
        view->dirty++;
        mcview_set_byte (view, curr->offset, curr->value);
        g_free (curr);
    }

    if (mc_close (fp) == -1) {
        error = g_strdup (unix_error_string (errno));
        message (D_ERROR, _(" Save file "),
                 _(" Error while closing the file: \n %s \n"
                   " Data may have been written or not. "), error);
        g_free (error);
    }
    mcview_update (view);
    return TRUE;

  save_error:
    error = g_strdup (unix_error_string (errno));
    text = g_strdup_printf (_(" Cannot save file: \n %s "), error);
    g_free (error);
    (void) mc_close (fp);

    answer = query_dialog (_(" Save file "), text, D_ERROR, 2, _("&Retry"), _("&Cancel"));
    g_free (text);

    if (answer == 0)
        goto retry_save;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_hexedit_mode (mcview_t * view)
{
    view->hexedit_mode = !view->hexedit_mode;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_hexedit_free_change_list (mcview_t * view)
{
    struct hexedit_change_node *curr, *next;

    for (curr = view->change_list; curr != NULL; curr = next) {
        next = curr->next;
        g_free (curr);
    }
    view->change_list = NULL;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_enqueue_change (struct hexedit_change_node **head, struct hexedit_change_node *node)
{
    /* chnode always either points to the head of the list or
     * to one of the ->next fields in the list. The value at
     * this location will be overwritten with the new node.   */
    struct hexedit_change_node **chnode = head;

    while (*chnode != NULL && (*chnode)->offset < node->offset)
        chnode = &((*chnode)->next);

    node->next = *chnode;
    *chnode = node;
}

/* --------------------------------------------------------------------------------------------- */
