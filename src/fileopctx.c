/* File operation contexts for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <config.h>
#include <unistd.h>
#include <glib.h>
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
	ctx->eta_secs        = 0.0;
	ctx->progress_bytes  = 0.0;
	ctx->op_preserve     = TRUE;
	ctx->do_reget        = TRUE;
	ctx->stat_func       = mc_lstat;
	ctx->preserve        = TRUE;
	ctx->preserve_uidgid = (geteuid () == 0) ? TRUE : FALSE;
	ctx->umask_kill      = 0777777;
	ctx->erase_at_end    = TRUE;

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

	/* FIXME: do we need to free ctx->dest_mask? */

	g_free (ctx);
}
