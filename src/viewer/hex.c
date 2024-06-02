/*
   Internal file viewer for the Midnight Commander
   Function for hex view

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
   Ilia Maslakov <il.smind@gmail.com>, 2009

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

#include <errno.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/lock.h"           /* lock_file() and unlock_file() */
#include "lib/util.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum
{
    MARK_NORMAL,
    MARK_SELECTED,
    MARK_CURSOR,
    MARK_CHANGED
} mark_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const char hex_char[] = "0123456789ABCDEF";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/** Determine the state of the current byte.
 *
 * @param view viewer object
 * @param from offset
 * @param curr current node
 */

static mark_t
mcview_hex_calculate_boldflag (WView *view, off_t from, struct hexedit_change_node *curr,
                               gboolean force_changed)
{
    return (from == view->hex_cursor) ? MARK_CURSOR
        : ((curr != NULL && from == curr->offset) || force_changed) ? MARK_CHANGED
        : (view->search_start <= from && from < view->search_end) ? MARK_SELECTED : MARK_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_display_hex (WView *view)
{
    const WRect *r = &view->data_area;
    int ngroups = view->bytes_per_line / 4;
    /* 8 characters are used for the file offset, and every hex group
     * takes 13 characters. Starting at width of 80 columns, the groups
     * are separated by an extra vertical line. Starting at width of 81,
     * there is an extra space before the text column. There is always a
     * mostly empty column on the right, to allow overflowing CJKs.
     */
    int text_start;

    int row = 0;
    off_t from;
    mark_t boldflag_byte = MARK_NORMAL;
    mark_t boldflag_char = MARK_NORMAL;
    struct hexedit_change_node *curr = view->change_list;
#ifdef HAVE_CHARSET
    int cont_bytes = 0;         /* number of continuation bytes remanining from current UTF-8 */
    gboolean cjk_right = FALSE; /* whether the second byte of a CJK is to be processed */
#endif /* HAVE_CHARSET */
    gboolean utf8_changed = FALSE;      /* whether any of the bytes in the UTF-8 were changed */

    char hex_buff[10];          /* A temporary buffer for sprintf and mvwaddstr */

    text_start = 8 + 13 * ngroups +
        ((r->cols < 80) ? 0 : (r->cols == 80) ? (ngroups - 1) : (ngroups - 1 + 1));

    mcview_display_clean (view);

    /* Find the first displayable changed byte */
    /* In UTF-8 mode, go back by 1 or maybe 2 lines to handle continuation bytes properly. */
    from = view->dpy_start;
#ifdef HAVE_CHARSET
    if (view->utf8)
    {
        if (from >= view->bytes_per_line)
        {
            row--;
            from -= view->bytes_per_line;
        }
        if (view->bytes_per_line == 4 && from >= view->bytes_per_line)
        {
            row--;
            from -= view->bytes_per_line;
        }
    }
#endif /* HAVE_CHARSET */
    while (curr && (curr->offset < from))
    {
        curr = curr->next;
    }

    for (; mcview_get_byte (view, from, NULL) && row < r->lines; row++)
    {
        int col = 0;
        int bytes;              /* Number of bytes already printed on the line */

        /* Print the hex offset */
        if (row >= 0)
        {
            int i;

            g_snprintf (hex_buff, sizeof (hex_buff), "%08" PRIXMAX " ", (uintmax_t) from);
            widget_gotoyx (view, r->y + row, r->x);
            tty_setcolor (VIEW_BOLD_COLOR);
            for (i = 0; col < r->cols && hex_buff[i] != '\0'; col++, i++)
                tty_print_char (hex_buff[i]);
            tty_setcolor (VIEW_NORMAL_COLOR);
        }

        for (bytes = 0; bytes < view->bytes_per_line; bytes++, from++)
        {
            int c;
#ifdef HAVE_CHARSET
            int ch = 0;

            if (view->utf8)
            {
                struct hexedit_change_node *corr = curr;

                if (cont_bytes != 0)
                {
                    /* UTF-8 continuation bytes, print a space (with proper attributes)... */
                    cont_bytes--;
                    ch = ' ';
                    if (cjk_right)
                    {
                        /* ... except when it'd wipe out the right half of a CJK, then print nothing */
                        cjk_right = FALSE;
                        ch = -1;
                    }
                }
                else
                {
                    int j;
                    gchar utf8buf[UTF8_CHAR_LEN + 1];
                    int res;
                    int first_changed = -1;

                    for (j = 0; j < UTF8_CHAR_LEN; j++)
                    {
                        if (mcview_get_byte (view, from + j, &res))
                            utf8buf[j] = res;
                        else
                        {
                            utf8buf[j] = '\0';
                            break;
                        }
                        if (curr != NULL && from + j == curr->offset)
                        {
                            utf8buf[j] = curr->value;
                            if (first_changed == -1)
                                first_changed = j;
                        }
                        if (curr != NULL && from + j >= curr->offset)
                            curr = curr->next;
                    }
                    utf8buf[UTF8_CHAR_LEN] = '\0';

                    /* Determine the state of the current multibyte char */
                    ch = g_utf8_get_char_validated (utf8buf, -1);
                    if (ch == -1 || ch == -2)
                    {
                        ch = '.';
                    }
                    else
                    {
                        gchar *next_ch;

                        next_ch = g_utf8_next_char (utf8buf);
                        cont_bytes = next_ch - utf8buf - 1;
                        if (g_unichar_iswide (ch))
                            cjk_right = TRUE;
                    }

                    utf8_changed = (first_changed >= 0 && first_changed <= cont_bytes);
                    curr = corr;
                }
            }
#endif /* HAVE_CHARSET */

            /* For negative rows, the only thing we care about is overflowing
             * UTF-8 continuation bytes which were handled above. */
            if (row < 0)
            {
                if (curr != NULL && from == curr->offset)
                    curr = curr->next;
                continue;
            }

            if (!mcview_get_byte (view, from, &c))
                break;

            /* Save the cursor position for mcview_place_cursor() */
            if (from == view->hex_cursor && !view->hexview_in_text)
            {
                view->cursor_row = row;
                view->cursor_col = col;
            }

            /* Determine the state of the current byte */
            boldflag_byte = mcview_hex_calculate_boldflag (view, from, curr, FALSE);
            boldflag_char = mcview_hex_calculate_boldflag (view, from, curr, utf8_changed);

            /* Determine the value of the current byte */
            if (curr != NULL && from == curr->offset)
            {
                c = curr->value;
                curr = curr->next;
            }

            /* Select the color for the hex number */
            tty_setcolor (boldflag_byte == MARK_NORMAL ? VIEW_NORMAL_COLOR :
                          boldflag_byte == MARK_SELECTED ? VIEW_BOLD_COLOR :
                          boldflag_byte == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag_byte == MARK_CURSOR */
                          view->hexview_in_text ? VIEW_SELECTED_COLOR : VIEW_UNDERLINED_COLOR);

            /* Print the hex number */
            widget_gotoyx (view, r->y + row, r->x + col);
            if (col < r->cols)
            {
                tty_print_char (hex_char[c / 16]);
                col += 1;
            }
            if (col < r->cols)
            {
                tty_print_char (hex_char[c % 16]);
                col += 1;
            }

            /* Print the separator */
            tty_setcolor (VIEW_NORMAL_COLOR);
            if (bytes != view->bytes_per_line - 1)
            {
                if (col < r->cols)
                {
                    tty_print_char (' ');
                    col += 1;
                }

                /* After every four bytes, print a group separator */
                if (bytes % 4 == 3)
                {
                    if (view->data_area.cols >= 80 && col < r->cols)
                    {
                        tty_print_one_vline (TRUE);
                        col += 1;
                    }
                    if (col < r->cols)
                    {
                        tty_print_char (' ');
                        col += 1;
                    }
                }
            }

            /* Select the color for the character; this differs from the
             * hex color when boldflag == MARK_CURSOR */
            tty_setcolor (boldflag_char == MARK_NORMAL ? VIEW_NORMAL_COLOR :
                          boldflag_char == MARK_SELECTED ? VIEW_BOLD_COLOR :
                          boldflag_char == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag_char == MARK_CURSOR */
                          view->hexview_in_text ? VIEW_SELECTED_COLOR : MARKED_SELECTED_COLOR);


#ifdef HAVE_CHARSET
            if (mc_global.utf8_display)
            {
                if (!view->utf8)
                {
                    c = convert_from_8bit_to_utf_c ((unsigned char) c, view->converter);
                }
                if (!g_unichar_isprint (c))
                    c = '.';
            }
            else if (view->utf8)
                ch = convert_from_utf_to_current_c (ch, view->converter);
            else
#endif
            {
#ifdef HAVE_CHARSET
                c = convert_to_display_c (c);
#endif

                if (!is_printable (c))
                    c = '.';
            }

            /* Print corresponding character on the text side */
            if (text_start + bytes < r->cols)
            {
                widget_gotoyx (view, r->y + row, r->x + text_start + bytes);
#ifdef HAVE_CHARSET
                if (view->utf8)
                    tty_print_anychar (ch);
                else
#endif
                    tty_print_char (c);
            }

            /* Save the cursor position for mcview_place_cursor() */
            if (from == view->hex_cursor && view->hexview_in_text)
            {
                view->cursor_row = row;
                view->cursor_col = text_start + bytes;
            }
        }
    }

    /* Be polite to the other functions */
    tty_setcolor (VIEW_NORMAL_COLOR);

    mcview_place_cursor (view);
    view->dpy_end = from;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_hexedit_save_changes (WView *view)
{
    int answer = 0;

    if (view->change_list == NULL)
        return TRUE;

    while (answer == 0)
    {
        int fp;
        char *text;
        struct hexedit_change_node *curr, *next;

        g_assert (view->filename_vpath != NULL);

        fp = mc_open (view->filename_vpath, O_WRONLY);
        if (fp != -1)
        {
            for (curr = view->change_list; curr != NULL; curr = next)
            {
                next = curr->next;

                if (mc_lseek (fp, curr->offset, SEEK_SET) == -1
                    || mc_write (fp, &(curr->value), 1) != 1)
                    goto save_error;

                /* delete the saved item from the change list */
                view->change_list = next;
                view->dirty++;
                mcview_set_byte (view, curr->offset, curr->value);
                g_free (curr);
            }

            view->change_list = NULL;

            if (view->locked)
                view->locked = unlock_file (view->filename_vpath);

            if (mc_close (fp) == -1)
                message (D_ERROR, _("Save file"),
                         _("Error while closing the file:\n%s\n"
                           "Data may have been written or not"), unix_error_string (errno));

            view->dirty++;
            return TRUE;
        }

      save_error:
        text = g_strdup_printf (_("Cannot save file:\n%s"), unix_error_string (errno));
        (void) mc_close (fp);

        answer = query_dialog (_("Save file"), text, D_ERROR, 2, _("&Retry"), _("&Cancel"));
        g_free (text);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_toggle_hexedit_mode (WView *view)
{
    view->hexedit_mode = !view->hexedit_mode;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_hexedit_free_change_list (WView *view)
{
    struct hexedit_change_node *curr, *next;

    for (curr = view->change_list; curr != NULL; curr = next)
    {
        next = curr->next;
        g_free (curr);
    }
    view->change_list = NULL;

    if (view->locked)
        view->locked = unlock_file (view->filename_vpath);

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
