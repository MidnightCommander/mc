/*
   Internal file viewer for the Midnight Commander
   Function for work with growing bufers

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

#include "../src/global.h"
#include "../src/wtools.h"
#include "internal.h"

/* Block size for reading files in parts */
#define VIEW_PAGE_SIZE		((size_t) 8192)

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_growbuf_init (mcview_t * view)
{
    view->growbuf_in_use = TRUE;
    view->growbuf_blockptr = NULL;
    view->growbuf_blocks = 0;
    view->growbuf_lastindex = VIEW_PAGE_SIZE;
    view->growbuf_finished = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_growbuf_free (mcview_t * view)
{
    size_t i;

    assert (view->growbuf_in_use);

    for (i = 0; i < view->growbuf_blocks; i++)
        g_free (view->growbuf_blockptr[i]);
    g_free (view->growbuf_blockptr);
    view->growbuf_blockptr = NULL;
    view->growbuf_in_use = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

off_t
mcview_growbuf_filesize (mcview_t * view)
{
    assert (view->growbuf_in_use);

    if (view->growbuf_blocks == 0)
        return 0;
    else
        return ((off_t) view->growbuf_blocks - 1) * VIEW_PAGE_SIZE + view->growbuf_lastindex;
}

/* --------------------------------------------------------------------------------------------- */

/* Copies the output from the pipe to the growing buffer, until either
 * the end-of-pipe is reached or the interval [0..ofs) of the growing
 * buffer is completely filled. */
void
mcview_growbuf_read_until (mcview_t * view, off_t ofs)
{
    ssize_t nread;
    byte *p;
    size_t bytesfree;
    gboolean short_read;

    assert (view->growbuf_in_use);

    if (view->growbuf_finished)
        return;

    short_read = FALSE;
    while (mcview_growbuf_filesize (view) < ofs || short_read) {
        if (view->growbuf_lastindex == VIEW_PAGE_SIZE) {
            /* Append a new block to the growing buffer */
            byte *newblock = g_try_malloc (VIEW_PAGE_SIZE);
            byte **newblocks = g_try_malloc (sizeof (*newblocks) * (view->growbuf_blocks + 1));
            if (!newblock || !newblocks) {
                g_free (newblock);
                g_free (newblocks);
                return;
            }
            memcpy (newblocks, view->growbuf_blockptr, sizeof (*newblocks) * view->growbuf_blocks);
            g_free (view->growbuf_blockptr);
            view->growbuf_blockptr = newblocks;
            view->growbuf_blockptr[view->growbuf_blocks++] = newblock;
            view->growbuf_lastindex = 0;
        }
        p = view->growbuf_blockptr[view->growbuf_blocks - 1] + view->growbuf_lastindex;
        bytesfree = VIEW_PAGE_SIZE - view->growbuf_lastindex;

        if (view->datasource == DS_STDIO_PIPE) {
            nread = fread (p, 1, bytesfree, view->ds_stdio_pipe);
            if (nread == 0) {
                view->growbuf_finished = TRUE;
                (void) pclose (view->ds_stdio_pipe);
                mcview_display (view);
                close_error_pipe (D_NORMAL, NULL);
                view->ds_stdio_pipe = NULL;
                return;
            }
        } else {
            assert (view->datasource == DS_VFS_PIPE);
            do {
                nread = mc_read (view->ds_vfs_pipe, p, bytesfree);
            } while (nread == -1 && errno == EINTR);
            if (nread == -1 || nread == 0) {
                view->growbuf_finished = TRUE;
                (void) mc_close (view->ds_vfs_pipe);
                view->ds_vfs_pipe = -1;
                return;
            }
        }
        short_read = ((size_t) nread < bytesfree);
        view->growbuf_lastindex += nread;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_get_byte_growing_buffer (mcview_t * view, off_t byte_index, int *retval)
{
    if (retval)
        *retval = -1;
    off_t pageno = byte_index / VIEW_PAGE_SIZE;
    off_t pageindex = byte_index % VIEW_PAGE_SIZE;

    assert (view->growbuf_in_use);

    if ((size_t) pageno != pageno)
        return FALSE;

    mcview_growbuf_read_until (view, byte_index + 1);
    if (view->growbuf_blocks == 0)
        return FALSE;
    if (pageno < view->growbuf_blocks - 1) {
        if (retval)
            *retval = view->growbuf_blockptr[pageno][pageindex];
        return TRUE;
    }
    if (pageno == view->growbuf_blocks - 1 && pageindex < view->growbuf_lastindex) {
        if (retval)
            *retval = view->growbuf_blockptr[pageno][pageindex];
        return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcview_get_ptr_growing_buffer (mcview_t * view, off_t byte_index)
{
    off_t pageno = byte_index / VIEW_PAGE_SIZE;
    off_t pageindex = byte_index % VIEW_PAGE_SIZE;

    assert (view->growbuf_in_use);

    if ((size_t) pageno != pageno)
        return NULL;

    mcview_growbuf_read_until (view, byte_index + 1);
    if (view->growbuf_blocks == 0)
        return NULL;
    if (pageno < view->growbuf_blocks - 1)
        return (char *) (view->growbuf_blockptr[pageno] + pageindex);
    if (pageno == view->growbuf_blocks - 1 && pageindex < view->growbuf_lastindex)
        return (char *) (view->growbuf_blockptr[pageno] + pageindex);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
