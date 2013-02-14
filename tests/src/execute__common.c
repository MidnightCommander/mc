/*
   Common code for testing functions in src/execute.c file.

   Copyright (C) 2013

   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#include "lib/widget.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

#include "src/execute.h"

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GPtrArray *vfs_file_is_local__vpath__captured;

/* @ThenReturnValue */
static gboolean vfs_file_is_local__return_value;

/* @Mock */
gboolean
vfs_file_is_local (const vfs_path_t * vpath)
{
    g_ptr_array_add (vfs_file_is_local__vpath__captured, vfs_path_clone (vpath));
    return vfs_file_is_local__return_value;
}

static void
vfs_file_is_local__init ()
{
    vfs_file_is_local__vpath__captured = g_ptr_array_new ();
}

static void
vfs_file_is_local__deinit ()
{
    g_ptr_array_foreach (vfs_file_is_local__vpath__captured, (GFunc) vfs_path_free, NULL);
    g_ptr_array_free (vfs_file_is_local__vpath__captured, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void do_execute (const char *lc_shell, const char *command, int flags);

/* @CapturedValue */
static char *do_execute__lc_shell__captured;
/* @CapturedValue */
static char *do_execute__command__captured;
/* @CapturedValue */
static int do_execute__flags__captured;

/* @Mock */
void
do_execute (const char *lc_shell, const char *command, int flags)
{
    do_execute__lc_shell__captured = g_strdup (lc_shell);
    do_execute__command__captured = g_strdup (command);
    do_execute__flags__captured = flags;
}

static void
do_execute__init ()
{
    do_execute__command__captured = NULL;
    do_execute__lc_shell__captured = NULL;
}

static void
do_execute__deinit ()
{
    g_free (do_execute__lc_shell__captured);
    g_free (do_execute__command__captured);
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static vfs_path_t *mc_getlocalcopy__pathname_vpath__captured;
/* @ThenReturnValue */
static vfs_path_t *mc_getlocalcopy__return_value;

/* @Mock */
vfs_path_t *
mc_getlocalcopy (const vfs_path_t * pathname_vpath)
{
    mc_getlocalcopy__pathname_vpath__captured = vfs_path_clone (pathname_vpath);
    return mc_getlocalcopy__return_value;
}

static void
mc_getlocalcopy__init (void)
{
    mc_getlocalcopy__pathname_vpath__captured = NULL;
    mc_getlocalcopy__return_value = NULL;
}

static void
mc_getlocalcopy__deinit (void)
{
    vfs_path_free (mc_getlocalcopy__pathname_vpath__captured);
}

/* --------------------------------------------------------------------------------------------- */


/* @CapturedValue */
static int message_flags__captured;

/* @CapturedValue */
static char *message_title__captured;

/* @CapturedValue */
static char *message_text__captured;

/* @Mock */
void
message (int flags, const char *title, const char *text, ...)
{
    va_list ap;

    message_flags__captured = flags;

    message_title__captured = (title == MSG_ERROR) ? g_strdup (_("Error")) : g_strdup (title);

    va_start (ap, text);
    message_text__captured = g_strdup_vprintf (text, ap);
    va_end (ap);

}

static void
message__init (void)
{
    message_flags__captured = 0;
    message_title__captured = NULL;
    message_text__captured = NULL;
}

static void
message__deinit (void)
{
    g_free (message_title__captured);
    g_free (message_text__captured);
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GPtrArray *mc_stat__vpath__captured = NULL;
/* @ThenReturnValue */
static int mc_stat__return_value = 0;

/* @Mock */
int
mc_stat (const vfs_path_t * vpath, struct stat *stat_ignored)
{
    (void) stat_ignored;
    if (mc_stat__vpath__captured != NULL)
        g_ptr_array_add (mc_stat__vpath__captured, vfs_path_clone (vpath));
    return mc_stat__return_value;
}


static void
mc_stat__init (void)
{
    mc_stat__vpath__captured = g_ptr_array_new ();
}

static void
mc_stat__deinit (void)
{
    g_ptr_array_foreach (mc_stat__vpath__captured, (GFunc) vfs_path_free, NULL);
    g_ptr_array_free (mc_stat__vpath__captured, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static vfs_path_t *mc_ungetlocalcopy__pathname_vpath__captured;
/* @CapturedValue */
static vfs_path_t *mc_ungetlocalcopy__local_vpath__captured;
/* @ThenReturnValue */
static int mc_ungetlocalcopy__return_value = 0;

/* @Mock */
int
mc_ungetlocalcopy (const vfs_path_t * pathname_vpath, const vfs_path_t * local_vpath,
                   gboolean has_changed_ignored)
{
    (void) has_changed_ignored;

    mc_ungetlocalcopy__pathname_vpath__captured = vfs_path_clone (pathname_vpath);
    mc_ungetlocalcopy__local_vpath__captured = vfs_path_clone (local_vpath);
    return mc_ungetlocalcopy__return_value;
}

static void
mc_ungetlocalcopy__init (void)
{
    mc_ungetlocalcopy__pathname_vpath__captured = NULL;
    mc_ungetlocalcopy__local_vpath__captured = NULL;
}

static void
mc_ungetlocalcopy__deinit (void)
{
    vfs_path_free (mc_ungetlocalcopy__pathname_vpath__captured);
    vfs_path_free (mc_ungetlocalcopy__local_vpath__captured);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    vfs_file_is_local__init ();
    do_execute__init ();
    mc_getlocalcopy__init ();
    message__init ();
    mc_stat__init ();
    mc_ungetlocalcopy__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    mc_ungetlocalcopy__deinit ();
    mc_stat__deinit ();
    message__deinit ();
    mc_getlocalcopy__deinit ();
    do_execute__deinit ();
    vfs_file_is_local__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
