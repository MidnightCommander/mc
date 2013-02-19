/*
   Editor text keep buffer.

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru> 2013

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
 *  \brief Source: editor text keep buffer.
 *  \author Andrew Borodin
 *  \date 2013
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/vfs/vfs.h"

#include "editbuffer.h"

/* --------------------------------------------------------------------------------------------- */
/*-
 *
 * here's a quick sketch of the layout: (don't run this through indent.)
 *
 * (b1 is buffers1 and b2 is buffers2)
 *
 *                                       |
 * \0\0\0\0\0m e _ f i l e . \nf i n . \n|T h i s _ i s _ s o\0\0\0\0\0\0\0\0\0
 * ______________________________________|______________________________________
 *                                       |
 * ...  |  b2[2]   |  b2[1]   |  b2[0]   |  b1[0]   |  b1[1]   |  b1[2]   | ...
 *      |->        |->        |->        |->        |->        |->        |
 *                                       |
 *           _<------------------------->|<----------------->_
 *                       curs2           |       curs1
 *           ^                           |                   ^
 *           |                          ^|^                  |
 *         cursor                       |||                cursor
 *                                      |||
 *                              file end|||file beginning
 *                                       |
 *                                       |
 *
 *           _
 * This_is_some_file
 * fin.
 *
 *
 * This is called a "gab buffer".
 * See also:
 * http://en.wikipedia.org/wiki/Gap_buffer
 * http://stackoverflow.com/questions/4199694/data-structure-for-text-editor
 */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
  * Get pointer to byte at specified index
  *
  * @param buf pointer to editor buffer
  * @param byte_index byte index
  *
  * @return NULL if byte_index is negative or larger than file size; pointer to byte otherwise.
  */

static char *
edit_buffer_get_byte_ptr (const edit_buffer_t * buf, off_t byte_index)
{
    unsigned char *b;

    if (byte_index >= (buf->curs1 + buf->curs2) || byte_index < 0)
        return NULL;

    if (byte_index >= buf->curs1)
    {
        off_t p;

        p = buf->curs1 + buf->curs2 - byte_index - 1;
        b = buf->buffers2[p >> S_EDIT_BUF_SIZE];
        return (char *) &b[EDIT_BUF_SIZE - 1 - (p & M_EDIT_BUF_SIZE)];
    }

    b = buf->buffers1[byte_index >> S_EDIT_BUF_SIZE];
    return (char *) &b[byte_index & M_EDIT_BUF_SIZE];
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Initialize editor buffers.
 *
 * @param buf pointer to editor buffer
 */

void
edit_buffer_init (edit_buffer_t * buf)
{
    memset (buf->buffers1, 0, sizeof (buf->buffers1));
    memset (buf->buffers2, 0, sizeof (buf->buffers2));
    buf->curs1 = 0;
    buf->curs2 = 0;
    buf->buffers2[0] = g_malloc0 (EDIT_BUF_SIZE);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Clean editor buffers.
 *
 * @param buf pointer to editor buffer
 */

void
edit_buffer_clean (edit_buffer_t * buf)
{
    size_t j;

    for (j = 0; j <= MAXBUFF; j++)
    {
        g_free (buf->buffers1[j]);
        g_free (buf->buffers2[j]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Get byte at specified index
  *
  * @param buf pointer to editor buffer
  * @param byte_index byte index
  *
  * @return '\n' if byte_index is negative or larger than file size; byte at byte_index otherwise.
  */

int
edit_buffer_get_byte (const edit_buffer_t * buf, off_t byte_index)
{
    char *p;

    p = edit_buffer_get_byte_ptr (buf, byte_index);

    return (p != NULL) ? *(unsigned char *) p : '\n';
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
/**
  * Get utf-8 symbol at specified index
  *
  * @param buf pointer to editor buffer
  * @param byte_index byte index
  * @param char_width width of returned symbol
  *
  * @return '\n' if byte_index is negative or larger than file size;
  *         0 if utf-8 symbol at specified index is invalid;
  *         utf-8 symbol otherwise
  */

int
edit_buffer_get_utf (const edit_buffer_t * buf, off_t byte_index, int *char_width)
{
    gchar *str = NULL;
    gunichar res;
    gunichar ch;
    gchar *next_ch = NULL;

    if (byte_index >= (buf->curs1 + buf->curs2) || byte_index < 0)
    {
        *char_width = 0;
        return '\n';
    }

    str = edit_buffer_get_byte_ptr (buf, byte_index);
    if (str == NULL)
    {
        *char_width = 0;
        return 0;
    }

    res = g_utf8_get_char_validated (str, -1);
    if (res == (gunichar) (-2) || res == (gunichar) (-1))
    {
        /* Retry with explicit bytes to make sure it's not a buffer boundary */
        size_t i;
        gchar utf8_buf[UTF8_CHAR_LEN + 1];

        for (i = 0; i < UTF8_CHAR_LEN; i++)
            utf8_buf[i] = edit_buffer_get_byte (buf, byte_index + i);
        utf8_buf[i] = '\0';
        res = g_utf8_get_char_validated (utf8_buf, -1);
    }

    if (res == (gunichar) (-2) || res == (gunichar) (-1))
    {
        ch = *str;
        *char_width = 0;
    }
    else
    {
        ch = res;
        /* Calculate UTF-8 char width */
        next_ch = g_utf8_next_char (str);
        if (next_ch != NULL)
            *char_width = next_ch - str;
        else
        {
            ch = 0;
            *char_width = 0;
        }
    }

    return (int) ch;
}

/* --------------------------------------------------------------------------------------------- */
/**
  * Get utf-8 symbol before specified index
  *
  * @param buf pointer to editor buffer
  * @param byte_index byte index
  * @param char_width width of returned symbol
  *
  * @return 0 if byte_index is negative or larger than file size;
  *         1-byte value before specified index if utf-8 symbol before specified index is invalid;
  *         utf-8 symbol otherwise
  */

int
edit_buffer_get_prev_utf (const edit_buffer_t * buf, off_t byte_index, int *char_width)
{
    size_t i;
    gchar utf8_buf[3 * UTF8_CHAR_LEN + 1];
    gchar *str;
    gchar *cursor_buf_ptr;
    gunichar res;

    if (byte_index > (buf->curs1 + buf->curs2) || byte_index <= 0)
    {
        *char_width = 0;
        return 0;
    }

    for (i = 0; i < (3 * UTF8_CHAR_LEN); i++)
        utf8_buf[i] = edit_buffer_get_byte (buf, byte_index + i - (2 * UTF8_CHAR_LEN));
    utf8_buf[i] = '\0';

    cursor_buf_ptr = utf8_buf + (2 * UTF8_CHAR_LEN);
    str = g_utf8_find_prev_char (utf8_buf, cursor_buf_ptr);

    if (str == NULL || g_utf8_next_char (str) != cursor_buf_ptr)
    {
        *char_width = 1;
        return *(cursor_buf_ptr - 1);
    }

    res = g_utf8_get_char_validated (str, -1);
    if (res == (gunichar) (-2) || res == (gunichar) (-1))
    {
        *char_width = 1;
        return *(cursor_buf_ptr - 1);
    }

    *char_width = cursor_buf_ptr - str;
    return (int) res;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/**
 * Load file into editor buffer
 *
 * @param buf pointer to editor buffer
 * @param fd file descriptor
 * @param size file size
 *
 * @return number of read bytes
 */

off_t
edit_buffer_read_file (edit_buffer_t * buf, int fd, off_t size)
{
    off_t ret;
    off_t i;
    off_t data_size;

    buf->curs2 = size;
    i = buf->curs2 >> S_EDIT_BUF_SIZE;

    if (buf->buffers2[i] == NULL)
        buf->buffers2[i] = g_malloc0 (EDIT_BUF_SIZE);

    /* fill last part of buffers2 */
    data_size = buf->curs2 & M_EDIT_BUF_SIZE;
    ret = mc_read (fd, (char *) buf->buffers2[i] + EDIT_BUF_SIZE - data_size, data_size);
    if (ret < 0 || ret != data_size)
        return ret;

    /* fullfill other parts of buffers2 from end to begin */
    data_size = EDIT_BUF_SIZE;
    for (--i; i >= 0; i--)
    {
        off_t sz;

        /* edit->buffers2[0] is already allocated */
        if (buf->buffers2[i] == NULL)
            buf->buffers2[i] = g_malloc0 (data_size);
        sz = mc_read (fd, (char *) buf->buffers2[i], data_size);
        if (sz >= 0)
            ret += sz;
        if (sz != data_size)
            break;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
