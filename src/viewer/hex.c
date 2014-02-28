/*
   Internal file viewer for the Midnight Commander
   Function for hex view

   Copyright (C) 1994-2014
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
   Andrew Borodin <aborodin@vmail.ru>, 2009
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
#include <fcntl.h>
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

/*** file scope variables ************************************************************************/

static const char hex_char[] = "0123456789ABCDEF";

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
static int
utf8_to_int (char *str, int *char_width, gboolean * result)
{
    int res = -1;
    gunichar ch;
    gchar *next_ch = NULL;
    int width = 0;

    *result = TRUE;

    if (str == NULL)
    {
        *result = FALSE;
        return 0;
    }

    res = g_utf8_get_char_validated (str, -1);

    if (res < 0)
        ch = *str;
    else
    {
        ch = res;
        /* Calculate UTF-8 char width */
        next_ch = g_utf8_next_char (str);
        if (next_ch)
            width = next_ch - str;
        else
            ch = 0;
    }
    *char_width = width;
    return ch;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/** Determine the state of the current byte.
 *
 * @param view viewer object
 * @param from offset
 * @param curr current node
 */

static mark_t
mcview_hex_calculate_boldflag (mcview_t * view, off_t from, struct hexedit_change_node *curr)
{
    return (from == view->hex_cursor) ? MARK_CURSOR
        : (curr != NULL && from == curr->offset) ? MARK_CHANGED
        : (view->search_start <= from && from < view->search_end) ? MARK_SELECTED : MARK_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
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
     * takes 13 characters. On "big" screens, the groups are separated
     * by an extra vertical line, and there is an extra space before the
     * text column.
     */

    screen_dimen row;
    off_t from;
    int c;
    mark_t boldflag = MARK_NORMAL;
    struct hexedit_change_node *curr = view->change_list;
#ifdef HAVE_CHARSET
    int ch = 0;
#endif /* HAVE_CHARSET */

    char hex_buff[10];          /* A temporary buffer for sprintf and mvwaddstr */
    int bytes;                  /* Number of bytes already printed on the line */

    mcview_display_clean (view);

    /* Find the first displayable changed byte */
    from = view->dpy_start;
    while (curr && (curr->offset < from))
    {
        curr = curr->next;
    }

    for (row = 0; mcview_get_byte (view, from, NULL) == TRUE && row < height; row++)
    {
        screen_dimen col = 0;
        size_t i;

        col = 0;

        /* Print the hex offset */
        g_snprintf (hex_buff, sizeof (hex_buff), "%08" PRIXMAX " ", (uintmax_t) from);
        widget_move (view, top + row, left);
        tty_setcolor (VIEW_BOLD_COLOR);
        for (i = 0; col < width && hex_buff[i] != '\0'; i++)
        {
            tty_print_char (hex_buff[i]);
            /*              tty_print_char(hex_buff[i]); */
            col += 1;
        }
        tty_setcolor (NORMAL_COLOR);

        for (bytes = 0; bytes < view->bytes_per_line; bytes++, from++)
        {

#ifdef HAVE_CHARSET
            if (view->utf8)
            {
                int cw = 1;
                gboolean read_res = TRUE;

                ch = mcview_get_utf (view, from, &cw, &read_res);
                if (!read_res)
                    break;
                /* char width is greater 0 bytes */
                if (cw != 0)
                {
                    int cnt;
                    char corr_buf[UTF8_CHAR_LEN + 1];
                    struct hexedit_change_node *corr = curr;
                    int res;

                    res = g_unichar_to_utf8 (ch, (char *) corr_buf);

                    for (cnt = 0; cnt < cw; cnt++)
                    {
                        if (curr != NULL && from + cnt == curr->offset)
                        {
                            /* replace only changed bytes in array of multibyte char */
                            corr_buf[cnt] = curr->value;
                            curr = curr->next;
                        }
                    }
                    corr_buf[res] = '\0';
                    /* Determine the state of the current multibyte char */
                    ch = utf8_to_int ((char *) corr_buf, &cw, &read_res);
                    curr = corr;
                }
            }
#endif /* HAVE_CHARSET */
            if (!mcview_get_byte (view, from, &c))
                break;

            /* Save the cursor position for mcview_place_cursor() */
            if (from == view->hex_cursor && !view->hexview_in_text)
            {
                view->cursor_row = row;
                view->cursor_col = col;
            }

            /* Determine the state of the current byte */
            boldflag = mcview_hex_calculate_boldflag (view, from, curr);

            /* Determine the value of the current byte */
            if (curr != NULL && from == curr->offset)
            {
                c = curr->value;
                curr = curr->next;
            }

            /* Select the color for the hex number */
            tty_setcolor (boldflag == MARK_NORMAL ? NORMAL_COLOR :
                          boldflag == MARK_SELECTED ? VIEW_BOLD_COLOR :
                          boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag == MARK_CURSOR */
                          view->hexview_in_text ? VIEW_SELECTED_COLOR : VIEW_UNDERLINED_COLOR);

            /* Print the hex number */
            widget_move (view, top + row, left + col);
            if (col < width)
            {
                tty_print_char (hex_char[c / 16]);
                col += 1;
            }
            if (col < width)
            {
                tty_print_char (hex_char[c % 16]);
                col += 1;
            }

            /* Print the separator */
            tty_setcolor (NORMAL_COLOR);
            if (bytes != view->bytes_per_line - 1)
            {
                if (col < width)
                {
                    tty_print_char (' ');
                    col += 1;
                }

                /* After every four bytes, print a group separator */
                if (bytes % 4 == 3)
                {
                    if (view->data_area.width >= 80 && col < width)
                    {
                        tty_print_one_vline (TRUE);
                        col += 1;
                    }
                    if (col < width)
                    {
                        tty_print_char (' ');
                        col += 1;
                    }
                }
            }

            /* Select the color for the character; this differs from the
             * hex color when boldflag == MARK_CURSOR */
            tty_setcolor (boldflag == MARK_NORMAL ? NORMAL_COLOR :
                          boldflag == MARK_SELECTED ? VIEW_BOLD_COLOR :
                          boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
                          /* boldflag == MARK_CURSOR */
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
            if (text_start + bytes < width)
            {
                widget_move (view, top + row, left + text_start + bytes);
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
    tty_setcolor (NORMAL_COLOR);

    mcview_place_cursor (view);
    view->dpy_end = from;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_hexedit_save_changes (mcview_t * view)
{
    int answer = 0;

    if (view->change_list == NULL)
        return TRUE;

    while (answer == 0)
    {
        int fp;
        char *text;
        struct hexedit_change_node *curr, *next;

#ifdef HAVE_ASSERT_H
        assert (view->filename_vpath != NULL);
#endif

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
