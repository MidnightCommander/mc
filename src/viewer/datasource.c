/*
   Internal file viewer for the Midnight Commander
   Functions for datasources

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

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

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_set_datasource_stdio_pipe (mcview_t * view, FILE * fp)
{
#ifdef HAVE_ASSERT_H
    assert (fp != NULL);
#endif
    view->datasource = DS_STDIO_PIPE;
    view->ds_stdio_pipe = fp;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_none (mcview_t * view)
{
    view->datasource = DS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_get_filesize (mcview_t * view)
{
    switch (view->datasource)
    {
    case DS_NONE:
        return 0;
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        return mcview_growbuf_filesize (view);
    case DS_FILE:
        return view->ds_file_filesize;
    case DS_STRING:
        return view->ds_string_len;
    default:
#ifdef HAVE_ASSERT_H
        assert (!"Unknown datasource type");
#endif
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_update_filesize (mcview_t * view)
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
mcview_get_ptr_file (mcview_t * view, off_t byte_index)
{
#ifdef HAVE_ASSERT_H
    assert (view->datasource == DS_FILE);
#endif

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return (char *) (view->ds_file_data + (byte_index - view->ds_file_offset));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_string (mcview_t * view, off_t byte_index)
{
#ifdef HAVE_ASSERT_H
    assert (view->datasource == DS_STRING);
#endif
    if (byte_index < (off_t) view->ds_string_len)
        return (char *) (view->ds_string_data + byte_index);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_get_utf (mcview_t * view, off_t byte_index, int *char_width, gboolean * result)
{
    gchar *str = NULL;
    int res = -1;
    gunichar ch;
    gchar *next_ch = NULL;
    gchar utf8buf[UTF8_CHAR_LEN + 1];

    *char_width = 0;
    *result = FALSE;

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
        break;
    }

    if (str == NULL)
        return 0;

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
        ch = *str;
        *char_width = 1;
    }
    else
    {
        ch = res;
        /* Calculate UTF-8 char width */
        next_ch = g_utf8_next_char (str);
        if (next_ch)
            *char_width = next_ch - str;
        else
            return 0;
    }
    *result = TRUE;
    return ch;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_string (mcview_t * view, off_t byte_index, int *retval)
{
#ifdef HAVE_ASSERT_H
    assert (view->datasource == DS_STRING);
#endif
    if (byte_index < (off_t) view->ds_string_len)
    {
        if (retval)
            *retval = view->ds_string_data[byte_index];
        return TRUE;
    }
    if (retval)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_none (mcview_t * view, off_t byte_index, int *retval)
{
    (void) &view;
    (void) byte_index;

#ifdef HAVE_ASSERT_H
    assert (view->datasource == DS_NONE);
#endif

    if (retval != NULL)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_byte (mcview_t * view, off_t offset, byte b)
{
    (void) &b;
#ifndef HAVE_ASSERT_H
    (void) offset;
#else
    assert (offset < mcview_get_filesize (view));
    assert (view->datasource == DS_FILE);

#endif
    view->ds_file_datalen = 0;  /* just force reloading */
}

/* --------------------------------------------------------------------------------------------- */

/*static */
void
mcview_file_load_data (mcview_t * view, off_t byte_index)
{
    off_t blockoffset;
    ssize_t res;
    size_t bytes_read;

#ifdef HAVE_ASSERT_H
    assert (view->datasource == DS_FILE);
#endif

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
mcview_close_datasource (mcview_t * view)
{
    switch (view->datasource)
    {
    case DS_NONE:
        break;
    case DS_STDIO_PIPE:
        if (view->ds_stdio_pipe != NULL)
        {
            (void) pclose (view->ds_stdio_pipe);
            mcview_display (view);
            close_error_pipe (D_NORMAL, NULL);
            view->ds_stdio_pipe = NULL;
        }
        mcview_growbuf_free (view);
        break;
    case DS_VFS_PIPE:
        if (view->ds_vfs_pipe != -1)
        {
            (void) mc_close (view->ds_vfs_pipe);
            view->ds_vfs_pipe = -1;
        }
        mcview_growbuf_free (view);
        break;
    case DS_FILE:
        (void) mc_close (view->ds_file_fd);
        view->ds_file_fd = -1;
        g_free (view->ds_file_data);
        view->ds_file_data = NULL;
        break;
    case DS_STRING:
        g_free (view->ds_string_data);
        view->ds_string_data = NULL;
        break;
    default:
#ifdef HAVE_ASSERT_H
        assert (!"Unknown datasource type")
#endif
            ;
    }
    view->datasource = DS_NONE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_file (mcview_t * view, int fd, const struct stat *st)
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
mcview_load_command_output (mcview_t * view, const char *command)
{
    FILE *fp;

    mcview_close_datasource (view);

    open_error_pipe ();
    fp = popen (command, "r");
    if (fp == NULL)
    {
        /* Avoid two messages.  Message from stderr has priority.  */
        mcview_display (view);
        if (!close_error_pipe (mcview_is_in_panel (view) ? -1 : D_ERROR, NULL))
            mcview_show_error (view, _("Cannot spawn child process"));
        return FALSE;
    }

    /* First, check if filter produced any output */
    mcview_set_datasource_stdio_pipe (view, fp);
    if (!mcview_get_byte (view, 0, NULL))
    {
        mcview_close_datasource (view);

        /* Avoid two messages.  Message from stderr has priority.  */
        mcview_display (view);
        if (!close_error_pipe (mcview_is_in_panel (view) ? -1 : D_ERROR, NULL))
            mcview_show_error (view, _("Empty output from child filter"));
        return FALSE;
    }
    else
    {
        /*
         * At least something was read correctly. Close stderr and let
         * program die if it will try to write something there.
         *
         * Ideally stderr should be read asynchronously to prevent programs
         * from blocking (poll/select multiplexor).
         */
        close_error_pipe (D_NORMAL, NULL);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_vfs_pipe (mcview_t * view, int fd)
{
#ifdef HAVE_ASSERT_H
    assert (fd != -1);
#endif
    view->datasource = DS_VFS_PIPE;
    view->ds_vfs_pipe = fd;

    mcview_growbuf_init (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_datasource_string (mcview_t * view, const char *s)
{
    view->datasource = DS_STRING;
    view->ds_string_data = (byte *) g_strdup (s);
    view->ds_string_len = strlen (s);
}

/* --------------------------------------------------------------------------------------------- */
