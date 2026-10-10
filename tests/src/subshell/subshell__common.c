/*
   Common code for testing functions in src/subshell/common.c file.

   Copyright (C) 2026
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Rename symbols that collide with libinternal.la / libmc.la. */
#define init_subshell                   test_init_subshell
#define invoke_subshell                 test_invoke_subshell
#define flush_subshell                  test_flush_subshell
#define read_subshell_prompt            test_read_subshell_prompt
#define do_update_prompt                test_do_update_prompt
#define exit_subshell                   test_exit_subshell
#define subshell_chdir                  test_subshell_chdir
#define subshell_get_console_attributes test_subshell_get_console_attributes
#define sigchld_handler                 test_sigchld_handler
#define subshell_state                  test_subshell_state
#define subshell_prompt                 test_subshell_prompt
#define update_subshell_prompt          test_update_subshell_prompt
#define should_read_new_subshell_prompt test_should_read_new_subshell_prompt
#define add_select_channel              test_add_select_channel
#define delete_select_channel           test_delete_select_channel
#define subshell_get_cwd                test_subshell_get_cwd
#define subshell_handle_cons_saver      test_subshell_handle_cons_saver
#define subshell_get_mainloop_quit      test_subshell_get_mainloop_quit
#define subshell_set_mainloop_quit      test_subshell_set_mainloop_quit
#define cmdline                         test_cmdline
#define setup_cmdline                   test_setup_cmdline

#include "src/subshell/common.c"

/* --------------------------------------------------------------------------------------------- */

/* @ThenReturnValue */
static vfs_path_t *subshell_get_cwd__return_value;

/* @Mock */
const vfs_path_t *
test_subshell_get_cwd (void)
{
    return subshell_get_cwd__return_value;
}

/* @Mock */
void
test_subshell_handle_cons_saver (void)
{
}

WInput *test_cmdline = NULL;

/* @Mock */
void
test_setup_cmdline (void)
{
}

/* @Mock */
int
test_subshell_get_mainloop_quit (void)
{
    return 0;
}

/* @Mock */
void
test_subshell_set_mainloop_quit (const int param_quit)
{
    (void) param_quit;
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GArray *delete_select_channel__calls;

/* @Mock */
void
test_add_select_channel (int fd, select_fn callback, void *info)
{
    (void) fd;
    (void) callback;
    (void) info;
}

/* @Mock */
void
test_delete_select_channel (int fd)
{
    g_array_append_val (delete_select_channel__calls, fd);
}

/* --------------------------------------------------------------------------------------------- */
