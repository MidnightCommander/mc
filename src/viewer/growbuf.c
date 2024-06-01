/*
   Internal file viewer for the Midnight Commander
   Function for work with growing buffers

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
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2014
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

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/widget.h"         /* D_NORMAL */

#include "internal.h"

/* Block size for reading files in parts */
#define VIEW_PAGE_SIZE ((size_t) 8192)

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_growbuf_init (WView *view)
{
    view->growbuf_in_use = TRUE;
    view->growbuf_blockptr = g_ptr_array_new_with_free_func (g_free);
    view->growbuf_lastindex = VIEW_PAGE_SIZE;
    view->growbuf_finished = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_growbuf_done (WView *view)
{
    view->growbuf_finished = TRUE;

    if (view->datasource == DS_STDIO_PIPE)
    {
        mc_pclose (view->ds_stdio_pipe, NULL);
        view->ds_stdio_pipe = NULL;
    }
    else                        /* view->datasource == DS_VFS_PIPE */
    {
        (void) mc_close (view->ds_vfs_pipe);
        view->ds_vfs_pipe = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_growbuf_free (WView *view)
{
    g_assert (view->growbuf_in_use);

    g_ptr_array_free (view->growbuf_blockptr, TRUE);
    view->growbuf_blockptr = NULL;
    view->growbuf_in_use = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_growbuf_filesize (WView *view)
{
    g_assert (view->growbuf_in_use);

    if (view->growbuf_blockptr->len == 0)
        return 0;
    else
        return ((off_t) view->growbuf_blockptr->len - 1) * VIEW_PAGE_SIZE + view->growbuf_lastindex;
}

/* --------------------------------------------------------------------------------------------- */
/** Copies the output from the pipe to the growing buffer, until either
 * the end-of-pipe is reached or the interval [0..ofs) of the growing
 * buffer is completely filled.
 */

void
mcview_growbuf_read_until (WView *view, off_t ofs)
{
    gboolean short_read = FALSE;

    g_assert (view->growbuf_in_use);

    if (view->growbuf_finished)
        return;

    while (mcview_growbuf_filesize (view) < ofs || short_read)
    {
        ssize_t nread = 0;
        byte *p;
        size_t bytesfree;

        if (view->growbuf_lastindex == VIEW_PAGE_SIZE)
        {
            /* Append a new block to the growing buffer */
            byte *newblock = g_try_malloc (VIEW_PAGE_SIZE);
            if (newblock == NULL)
                return;

            g_ptr_array_add (view->growbuf_blockptr, newblock);
            view->growbuf_lastindex = 0;
        }

        p = (byte *) g_ptr_array_index (view->growbuf_blockptr,
                                        view->growbuf_blockptr->len - 1) + view->growbuf_lastindex;

        bytesfree = VIEW_PAGE_SIZE - view->growbuf_lastindex;

        if (view->datasource == DS_STDIO_PIPE)
        {
            mc_pipe_t *sp = view->ds_stdio_pipe;
            GError *error = NULL;

            if (bytesfree > MC_PIPE_BUFSIZE)
                bytesfree = MC_PIPE_BUFSIZE;

            sp->out.len = bytesfree;
            sp->err.len = MC_PIPE_BUFSIZE;

            mc_pread (sp, &error);

            if (error != NULL)
            {
                mcview_show_error (view, error->message);
                g_error_free (error);
                mcview_growbuf_done (view);
                return;
            }

            if (view->pipe_first_err_msg && sp->err.len > 0)
            {
                /* ignore possible following errors */
                /* reset this flag before call of mcview_show_error() to break
                 * endless recursion: mcview_growbuf_read_until() -> mcview_show_error() ->
                 * MSG_DRAW -> mcview_display() -> mcview_get_byte() -> mcview_growbuf_read_until()
                 */
                view->pipe_first_err_msg = FALSE;

                mcview_show_error (view, sp->err.buf);

                /* when switch from parse to raw mode and back,
                 * do not close the already closed pipe (see call to mcview_growbuf_done below).
                 * return from here since (sp == view->ds_stdio_pipe) would now be invalid.
                 * NOTE: this check was removed by ticket #4103 but the above call to
                 *       mcview_show_error triggers the stdio pipe handle to be closed:
                 *       mcview_close_datasource -> mcview_growbuf_done
                 */
                if (view->ds_stdio_pipe == NULL)
                    return;
            }

            if (sp->out.len > 0)
            {
                memmove (p, sp->out.buf, sp->out.len);
                nread = sp->out.len;
            }
            else if (sp->out.len == MC_PIPE_STREAM_EOF || sp->out.len == MC_PIPE_ERROR_READ)
            {
                if (sp->out.len == MC_PIPE_ERROR_READ)
                {
                    char *err_msg;

                    err_msg = g_strdup_printf (_("Failed to read data from child stdout:\n%s"),
                                               unix_error_string (sp->out.error));
                    mcview_show_error (view, err_msg);
                    g_free (err_msg);
                }

                /* when switch from parse to raw mode and back,
                 * do not close the already closed pipe after following loop:
                 * mcview_growbuf_read_until() -> mcview_show_error() ->
                 * MSG_DRAW -> mcview_display() -> mcview_get_byte() -> mcview_growbuf_read_until()
                 */
                mcview_growbuf_done (view);

                mcview_display (view);
                return;
            }
        }
        else
        {
            g_assert (view->datasource == DS_VFS_PIPE);
            do
            {
                nread = mc_read (view->ds_vfs_pipe, p, bytesfree);
            }
            while (nread == -1 && errno == EINTR);

            if (nread <= 0)
            {
                mcview_growbuf_done (view);
                return;
            }
        }
        short_read = ((size_t) nread < bytesfree);
        view->growbuf_lastindex += nread;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_growing_buffer (WView *view, off_t byte_index, int *retval)
{
    char *p;

    g_assert (view->growbuf_in_use);

    if (retval != NULL)
        *retval = -1;

    if (byte_index < 0)
        return FALSE;

    p = mcview_get_ptr_growing_buffer (view, byte_index);
    if (p == NULL)
        return FALSE;

    if (retval != NULL)
        *retval = (unsigned char) (*p);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_growing_buffer (WView *view, off_t byte_index)
{
    off_t pageno, pageindex;

    g_assert (view->growbuf_in_use);

    if (byte_index < 0)
        return NULL;

    pageno = byte_index / VIEW_PAGE_SIZE;
    pageindex = byte_index % VIEW_PAGE_SIZE;

    mcview_growbuf_read_until (view, byte_index + 1);
    if (view->growbuf_blockptr->len == 0)
        return NULL;
    if (pageno < (off_t) view->growbuf_blockptr->len - 1)
        return ((char *) g_ptr_array_index (view->growbuf_blockptr, pageno) + pageindex);
    if (pageno == (off_t) view->growbuf_blockptr->len - 1
        && pageindex < (off_t) view->growbuf_lastindex)
        return ((char *) g_ptr_array_index (view->growbuf_blockptr, pageno) + pageindex);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
