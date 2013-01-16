/*
   File operation contexts for the Midnight Commander

   Copyright (C) 1999, 2001, 2002, 2003, 2004, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Federico Mena <federico@nuclecu.unam.mx>
   Miguel de Icaza <miguel@nuclecu.unam.mx>

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

/** \file fileopctx.c
 *  \brief Source: file operation contexts
 *  \date 1998-2007
 *  \author Federico Mena <federico@nuclecu.unam.mx>
 *  \author Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>

#include <unistd.h>

#include "lib/global.h"
#include "fileopctx.h"
#include "filegui.h"
#include "lib/search.h"
#include "lib/vfs/vfs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * \fn FileOpContext * file_op_context_new (FileOperation op)
 * \param op file operation struct
 * \return The newly-created context, filled with the default file mask values.
 *
 * Creates a new file operation context with the default values.  If you later want
 * to have a user interface for this, call file_op_context_create_ui().
 */

FileOpContext *
file_op_context_new (FileOperation op)
{
    FileOpContext *ctx;

    ctx = g_new0 (FileOpContext, 1);
    ctx->operation = op;
    ctx->eta_secs = 0.0;
    ctx->progress_bytes = 0;
    ctx->op_preserve = TRUE;
    ctx->do_reget = 1;
    ctx->stat_func = mc_lstat;
    ctx->preserve = TRUE;
    ctx->preserve_uidgid = (geteuid () == 0);
    ctx->umask_kill = 0777777;
    ctx->erase_at_end = TRUE;
    ctx->skip_all = FALSE;

    return ctx;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * \fn void file_op_context_destroy (FileOpContext *ctx)
 * \param ctx The file operation context to destroy.
 *
 * Destroys the specified file operation context and its associated UI data, if
 * it exists.
 */

void
file_op_context_destroy (FileOpContext * ctx)
{
    if (ctx != NULL)
    {
        file_op_context_destroy_ui (ctx);
        mc_search_free (ctx->search_handle);
        g_free (ctx);
    }
}

/* --------------------------------------------------------------------------------------------- */

FileOpTotalContext *
file_op_total_context_new (void)
{
    FileOpTotalContext *tctx;
    tctx = g_new0 (FileOpTotalContext, 1);
    tctx->ask_overwrite = TRUE;
    return tctx;
}

/* --------------------------------------------------------------------------------------------- */

void
file_op_total_context_destroy (FileOpTotalContext * tctx)
{
    g_free (tctx);
}

/* --------------------------------------------------------------------------------------------- */
