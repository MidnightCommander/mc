/*
   Internal file viewer for the Midnight Commander
   Functions for datasources

   Copyright (C) 1994-2019
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
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

/*
   The data source provides the viewer with data from either a file, a
   string or the output of a command. The mcview_get_byte() function can be
   used to get the value of a byte at a specific offset. If the offset
   is out of range, -1 is returned. The function mcview_get_byte_indexed(a,b)
   returns the byte at the offset a+b, or -1 if a+b is out of range.

   The mcview_set_byte() function has the effect that later calls to
   mcview_get_byte() will return the specified byte for this offset. This
   function is designed only for use by the hexedit component after
   saving its changes. Inspect the source before you want to use it for
   other purposes.

   The mcview_get_filesize() function returns the current size of the
   data source. If the growing buffer is used, this size may increase
   later on. Use the mcview_may_still_grow() function when you want to
   know if the size can change later.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/widget.h"         /* D_NORMAL, D_ERROR */

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_set_datasource_stdio_pipe (WView * view, mc_pipe_t * p)
{
    p->out.len = MC_PIPE_BUFSIZE;
    p->out.null_term = FALSE;
    p->err.len = MC_PIPE_BUFSIZE;
    p->err.null_term = TRUE;
    view->datasource = DS_STDIO_PIPE;
    view->ds_stdio_pipe = p;
    view->pipe_first_err_msg = TRUE;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_none (WView * view)
{
    view->datasource = DS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_get_filesize (WView * view)
{
    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        return mcview_growbuf_filesize (view);
    case DS_FILE:
        return view->ds_file_filesize;
    case DS_STRING:
        return view->ds_string_len;
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_update_filesize (WView * view)
{
    if (view->datasource == DS_FILE)
    {
        struct stat st;
        if (mc_fstat (view->ds_file_fd, &st) != -1)
            view->ds_file_filesize = st.st_size;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_file (WView * view, off_t byte_index)
{
    g_assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return (char *) (view->ds_file_data + (byte_index - view->ds_file_offset));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Invalid UTF-8 is reported as negative integers (one for each byte),
 * see ticket 3783. */
gboolean
mcview_get_utf (WView * view, off_t byte_index, int *ch, int *ch_len)
{
    gchar *str = NULL;
    int res;
    gchar utf8buf[UTF8_CHAR_LEN + 1];

    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        str = mcview_get_ptr_growing_buffer (view, byte_index);
        break;
    case DS_FILE:
        str = mcview_get_ptr_file (view, byte_index);
        break;
    case DS_STRING:
        str = mcview_get_ptr_string (view, byte_index);
        break;
    case DS_NONE:
    default:
        break;
    }

    *ch = 0;

    if (str == NULL)
        return FALSE;

    res = g_utf8_get_char_validated (str, -1);

    if (res < 0)
    {
        /* Retry with explicit bytes to make sure it's not a buffer boundary */
        int i;

        for (i = 0; i < UTF8_CHAR_LEN; i++)
        {
            if (mcview_get_byte (view, byte_index + i, &res))
                utf8buf[i] = res;
            else
            {
                utf8buf[i] = '\0';
                break;
            }
        }
        utf8buf[UTF8_CHAR_LEN] = '\0';
        str = utf8buf;
        res = g_utf8_get_char_validated (str, -1);
    }

    if (res < 0)
    {
        /* Implicit conversion from signed char to signed int keeps negative values. */
        *ch = *str;
        *ch_len = 1;
    }
    else
    {
        gchar *next_ch = NULL;

        *ch = res;
        /* Calculate UTF-8 char length */
        next_ch = g_utf8_next_char (str);
        *ch_len = next_ch - str;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_string (WView * view, off_t byte_index)
{
    g_assert (view->datasource == DS_STRING);

    if (byte_index >= 0 && byte_index < (off_t) view->ds_string_len)
        return (char *) (view->ds_string_data + byte_index);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_string (WView * view, off_t byte_index, int *retval)
{
    char *p;

    if (retval != NULL)
        *retval = -1;

    p = mcview_get_ptr_string (view, byte_index);
    if (p == NULL)
        return FALSE;

    if (retval != NULL)
        *retval = (unsigned char) (*p);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_none (WView * view, off_t byte_index, int *retval)
{
    (void) &view;
    (void) byte_index;

    g_assert (view->datasource == DS_NONE);

    if (retval != NULL)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_byte (WView * view, off_t offset, byte b)
{
    (void) &b;

    g_assert (offset < mcview_get_filesize (view));
    g_assert (view->datasource == DS_FILE);

    view->ds_file_datalen = 0;  /* just force reloading */
}

/* --------------------------------------------------------------------------------------------- */

/*static */
void
mcview_file_load_data (WView * view, off_t byte_index)
{
    off_t blockoffset;
    ssize_t res;
    size_t bytes_read;

    g_assert (view->datasource == DS_FILE);

    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return;

    if (byte_index >= view->ds_file_filesize)
        return;

    blockoffset = mcview_offset_rounddown (byte_index, view->ds_file_datasize);
    if (mc_lseek (view->ds_file_fd, blockoffset, SEEK_SET) == -1)
        goto error;

    bytes_read = 0;
    while (bytes_read < view->ds_file_datasize)
    {
        res =
            mc_read (view->ds_file_fd, view->ds_file_data + bytes_read,
                     view->ds_file_datasize - bytes_read);
        if (res == -1)
            goto error;
        if (res == 0)
            break;
        bytes_read += (size_t) res;
    }
    view->ds_file_offset = blockoffset;
    if ((off_t) bytes_read > view->ds_file_filesize - view->ds_file_offset)
    {
        /* the file has grown in the meantime -- stick to the old size */
        view->ds_file_datalen = view->ds_file_filesize - view->ds_file_offset;
    }
    else
    {
        view->ds_file_datalen = bytes_read;
    }
    return;

  error:
    view->ds_file_datalen = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_close_datasource (WView * view)
{
    switch (view->datasource)
    {
    case DS_NONE:
        break;
    case DS_STDIO_PIPE:
        if (view->ds_stdio_pipe != NULL)
        {
            mcview_growbuf_done (view);
            mcview_display (view);
        }
        mcview_growbuf_free (view);
        break;
    case DS_VFS_PIPE:
        if (view->ds_vfs_pipe != -1)
            mcview_growbuf_done (view);
        mcview_growbuf_free (view);
        break;
    case DS_FILE:
        (void) mc_close (view->ds_file_fd);
        view->ds_file_fd = -1;
        MC_PTR_FREE (view->ds_file_data);
        break;
    case DS_STRING:
        MC_PTR_FREE (view->ds_string_data);
    default:
        break;
    }
    view->datasource = DS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_file (WView * view, int fd, const struct stat *st)
{
    view->datasource = DS_FILE;
    view->ds_file_fd = fd;
    view->ds_file_filesize = st->st_size;
    view->ds_file_offset = 0;
    view->ds_file_data = g_malloc (4096);
    view->ds_file_datalen = 0;
    view->ds_file_datasize = 4096;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_load_command_output (WView * view, const char *command)
{
    mc_pipe_t *p;
    GError *error = NULL;

    mcview_close_datasource (view);

    p = mc_popen (command, &error);
    if (p == NULL)
    {
        mcview_display (view);
        mcview_show_error (view, error->message);
        g_error_free (error);
        return FALSE;
    }

    /* Check if filter produced any output */
    mcview_set_datasource_stdio_pipe (view, p);
    if (!mcview_get_byte (view, 0, NULL))
    {
        mcview_close_datasource (view);
        mcview_display (view);
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_vfs_pipe (WView * view, int fd)
{
    g_assert (fd != -1);

    view->datasource = DS_VFS_PIPE;
    view->ds_vfs_pipe = fd;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_string (WView * view, const char *s)
{
    view->datasource = DS_STRING;
    view->ds_string_len = strlen (s);
    view->ds_string_data = (byte *) g_strndup (s, view->ds_string_len);
}

/* --------------------------------------------------------------------------------------------- */
