/* File operation contexts for the Midnight Commander

   Copyright (C) 1998 The Free Software Foundation

   Authors: Federico Mena <federico@nuclecu.unam.mx>
            Miguel de Icaza <miguel@nuclecu.unam.mx>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <unistd.h>

#include "global.h"
#include "fileopctx.h"
#include "../vfs/vfs.h"


/**
 * file_op_context_new:
 * 
 * Creates a new file operation context with the default values.  If you later want
 * to have a user interface for this, call #file_op_context_create_ui().
 * 
 * Return value: The newly-created context, filled with the default file mask values.
 **/
FileOpContext *
file_op_context_new (void)
{
    FileOpContext *ctx;

    ctx = g_new0 (FileOpContext, 1);
    ctx->eta_secs = 0.0;
    ctx->progress_bytes = 0.0;
    ctx->op_preserve = TRUE;
    ctx->do_reget = TRUE;
    ctx->stat_func = (mc_stat_fn) mc_lstat;
    ctx->preserve = TRUE;
    ctx->preserve_uidgid = (geteuid () == 0) ? TRUE : FALSE;
    ctx->umask_kill = 0777777;
    ctx->erase_at_end = TRUE;

    return ctx;
}


/**
 * file_op_context_destroy:
 * @ctx: The file operation context to destroy.
 * 
 * Destroys the specified file operation context and its associated UI data, if
 * it exists.
 **/
void
file_op_context_destroy (FileOpContext *ctx)
{
    g_return_if_fail (ctx != NULL);

    if (ctx->ui)
	file_op_context_destroy_ui (ctx);

    regfree (&ctx->rx);

    /* FIXME: do we need to free ctx->dest_mask? */

    g_free (ctx);
}
