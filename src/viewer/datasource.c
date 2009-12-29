/*
   Internal file viewer for the Midnight Commander
   Functions for datasources

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

#include "../src/global.h"
#include "../src/wtools.h"
#include "../../vfs/vfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
mcview_set_datasource_stdio_pipe (mcview_t * view, FILE * fp)
{
    assert (fp != NULL);
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
    switch (view->datasource) {
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
        assert (!"Unknown datasource type");
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_file (mcview_t * view, off_t byte_index)
{
    assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return (char *) (view->ds_file_data + (byte_index - view->ds_file_offset));
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_string (mcview_t * view, off_t byte_index)
{
    assert (view->datasource == DS_STRING);
    if (byte_index < view->ds_string_len)
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
    int width = 0;

    *result = TRUE;

    switch (view->datasource) {
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

    if (str == NULL) {
        *result = FALSE;
        width = 0;
        return 0;
    }

    res = g_utf8_get_char_validated (str, -1);

    if (res < 0) {
        ch = *str;
        width = 0;
    } else {
        ch = res;
        /* Calculate UTF-8 char width */
        next_ch = g_utf8_next_char (str);
        if (next_ch) {
            width = next_ch - str;
        } else {
            ch = 0;
            width = 0;
        }
    }
    *char_width = width;
    return ch;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_string (mcview_t * view, off_t byte_index, int *retval)
{
    assert (view->datasource == DS_STRING);
    if (byte_index < view->ds_string_len) {
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
    assert (view->datasource == DS_NONE);
    (void) &view;
    (void) byte_index;
    if (retval)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_set_byte (mcview_t * view, off_t offset, byte b)
{
    (void) &b;
    assert (offset < mcview_get_filesize (view));
    assert (view->datasource == DS_FILE);
    view->ds_file_datalen = 0;  /* just force reloading */
}

/* --------------------------------------------------------------------------------------------- */

/*static*/
void
mcview_file_load_data (mcview_t * view, off_t byte_index)
{
    off_t blockoffset;
    ssize_t res;
    size_t bytes_read;

    assert (view->datasource == DS_FILE);

    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
        return;

    if (byte_index >= view->ds_file_filesize)
        return;

    blockoffset = mcview_offset_rounddown (byte_index, view->ds_file_datasize);
    if (mc_lseek (view->ds_file_fd, blockoffset, SEEK_SET) == -1)
        goto error;

    bytes_read = 0;
    while (bytes_read < view->ds_file_datasize) {
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
    if (bytes_read > view->ds_file_filesize - view->ds_file_offset) {
        /* the file has grown in the meantime -- stick to the old size */
        view->ds_file_datalen = view->ds_file_filesize - view->ds_file_offset;
    } else {
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
    switch (view->datasource) {
    case DS_NONE:
        break;
    case DS_STDIO_PIPE:
        if (view->ds_stdio_pipe != NULL) {
            (void) pclose (view->ds_stdio_pipe);
            mcview_display (view);
            close_error_pipe (D_NORMAL, NULL);
            view->ds_stdio_pipe = NULL;
        }
        mcview_growbuf_free (view);
        break;
    case DS_VFS_PIPE:
        if (view->ds_vfs_pipe != -1) {
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
        assert (!"Unknown datasource type");
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
    if ((fp = popen (command, "r")) == NULL) {
        /* Avoid two messages.  Message from stderr has priority.  */
        mcview_display (view);
        if (!close_error_pipe (mcview_is_in_panel (view) ? -1 : D_ERROR, NULL))
            mcview_show_error (view, _(" Cannot spawn child process "));
        return FALSE;
    }

    /* First, check if filter produced any output */
    mcview_set_datasource_stdio_pipe (view, fp);
    if (! mcview_get_byte (view, 0, NULL)) {
        mcview_close_datasource (view);

        /* Avoid two messages.  Message from stderr has priority.  */
        mcview_display (view);
        if (!close_error_pipe (mcview_is_in_panel (view) ? -1 : D_ERROR, NULL))
            mcview_show_error (view, _("Empty output from child filter"));
        return FALSE;
    } else {
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
    assert (fd != -1);
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
