/*
   src/subshell - tests for subshell_init_ready() and subshell_ensure_initialized()

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

#define TEST_SUITE_NAME "/src/subshell"

#include "tests/mctest.h"

#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "src/vfs/local/local.h"

#include "subshell__common.c"

/* --------------------------------------------------------------------------------------------- */

static int
delete_select_channel__call_count_for_fd (int fd)
{
    int n = 0;

    for (guint i = 0; i < delete_select_channel__calls->len; i++)
        if (g_array_index (delete_select_channel__calls, int, i) == fd)
            n++;

    return n;
}

static void
write_fake_cwd (const char *cwd)
{
    char line[MC_MAXPATHLEN + 2];
    const int n = g_snprintf (line, sizeof (line), "%s\n", cwd);

    write_all (subshell_pipe[WRITE], line, (size_t) n);
}

static gboolean
fd_readable (int fd)
{
    fd_set read_set;
    struct timeval no_wait = { 0 };

    FD_ZERO (&read_set);
    FD_SET (fd, &read_set);

    return select (fd + 1, &read_set, NULL, NULL, &no_wait) > 0;
}

/* --------------------------------------------------------------------------------------------- */

static int fake_pty[2] = { -1, -1 };
static pid_t fake_shell_pid = -1;
static mc_shell_t test_shell;

static void
kill_fake_shell (void)
{
    if (fake_shell_pid > 0)
    {
        kill (fake_shell_pid, SIGKILL);
        waitpid (fake_shell_pid, NULL, 0);
        fake_shell_pid = -1;
    }
}

/* Let the fake shell die and deliver its SIGCHLD to sigchld_handler() synchronously. */
static void
kill_fake_shell_and_report (void)
{
    sigset_t chld_set, old_set;
    siginfo_t info;

    sigemptyset (&chld_set);
    sigaddset (&chld_set, SIGCHLD);
    sigprocmask (SIG_BLOCK, &chld_set, &old_set);

    kill (fake_shell_pid, SIGKILL);
    waitid (P_PID, (id_t) fake_shell_pid, &info, WEXITED | WNOWAIT);
    test_sigchld_handler (SIGCHLD);
    fake_shell_pid = -1;

    sigprocmask (SIG_SETMASK, &old_set, NULL);
}

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    // Skip prompt/cmdline widgets
    mc_global.mc_run_mode = MC_RUN_VIEWER;

    test_shell.type = SHELL_SH;
    test_shell.name = "sh";
    test_shell.path = (char *) "/bin/sh";
    test_shell.real_path = (char *) "/bin/sh";
    mc_global.shell = &test_shell;

    mc_global.tty.use_subshell = TRUE;
    subshell_alive = TRUE;
    subshell_stopped = FALSE;
    subshell_initialized = FALSE;
    subshell_state = INACTIVE;
    use_persistent_buffer = FALSE;
    subshell_cwd[0] = '\0';

    subshell_pipe[READ] = -1;
    subshell_pipe[WRITE] = -1;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fake_pty) != 0)
    {
        fake_pty[0] = fake_pty[1] = -1;
        ck_abort_msg ("Cannot create test pty");
    }
    // SOCK_DGRAM keeps one CWD report per read(); a stream pipe can merge writes
    if (socketpair (AF_UNIX, SOCK_DGRAM, 0, subshell_pipe) != 0)
    {
        subshell_pipe[READ] = -1;
        subshell_pipe[WRITE] = -1;
        ck_abort_msg ("Cannot create test pipe");
    }
    mc_global.tty.subshell_pty = fake_pty[0];

    fake_shell_pid = fork ();
    if (fake_shell_pid == -1)
        ck_abort_msg ("Cannot fork fake shell child");
    if (fake_shell_pid == 0)
    {
        for (;;)
            raise (SIGSTOP);
    }

    {
        int status;

        // Reap the first SIGSTOP so synchronize() does not depend on catching SIGCHLD
        if (waitpid (fake_shell_pid, &status, WUNTRACED) == -1 || !WIFSTOPPED (status))
        {
            kill_fake_shell ();
            ck_abort_msg ("Fake shell child did not stop");
        }
    }
    subshell_pid = fake_shell_pid;
    subshell_stopped = TRUE;

    {
        struct sigaction sa = { 0 };

        sa.sa_handler = test_sigchld_handler;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction (SIGCHLD, &sa, NULL);
    }

    subshell_get_cwd__return_value = vfs_path_from_str ("/tmp");

    delete_select_channel__calls = g_array_new (FALSE, FALSE, sizeof (int));
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    signal (SIGCHLD, SIG_DFL);
    kill_fake_shell ();

    if (delete_select_channel__calls != NULL)
    {
        g_array_free (delete_select_channel__calls, TRUE);
        delete_select_channel__calls = NULL;
    }

    vfs_path_free (subshell_get_cwd__return_value, TRUE);
    subshell_get_cwd__return_value = NULL;

    if (fake_pty[0] >= 0)
        close (fake_pty[0]);
    if (fake_pty[1] >= 0)
        close (fake_pty[1]);
    fake_pty[0] = fake_pty[1] = -1;

    if (subshell_pipe[READ] >= 0)
        close (subshell_pipe[READ]);
    if (subshell_pipe[WRITE] >= 0)
        close (subshell_pipe[WRITE]);
    subshell_pipe[READ] = subshell_pipe[WRITE] = -1;

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (init_ready_finishes_handshake_when_cwd_available)
{
    // given
    write_fake_cwd ("/tmp");  // handshake
    write_fake_cwd ("/tmp");  // forced initial cd in subshell_finish_init()

    // when
    subshell_init_ready (subshell_pipe[READ], NULL);

    // then
    mctest_assert_true (subshell_initialized);
    mctest_assert_str_eq (subshell_cwd, "/tmp");
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (init_ready_bails_out_when_subshell_not_alive)
{
    // given
    subshell_alive = FALSE;

    // when
    subshell_init_ready (subshell_pipe[READ], NULL);

    // then
    mctest_assert_false (subshell_initialized);
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (ensure_initialized_finishes_pending_init)
{
    // given
    write_fake_cwd ("/tmp");  // handshake
    write_fake_cwd ("/tmp");  // forced initial cd in subshell_finish_init()

    mctest_assert_false (subshell_initialized);

    // when
    subshell_ensure_initialized ();

    // then
    mctest_assert_true (subshell_initialized);
    mctest_assert_str_eq (subshell_cwd, "/tmp");
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (ensure_initialized_is_a_noop_once_already_initialized)
{
    // given
    subshell_initialized = TRUE;

    // when
    subshell_ensure_initialized ();

    // then
    mctest_assert_true (subshell_initialized);
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (chdir_is_ignored_until_initialized)
{
    // when
    test_subshell_chdir (subshell_get_cwd__return_value);

    // then
    mctest_assert_false (fd_readable (fake_pty[1]));
    mctest_assert_str_eq (subshell_cwd, "");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (chdir_is_sent_once_initialized)
{
    // given
    subshell_initialized = TRUE;
    g_strlcpy (subshell_cwd, "/", sizeof (subshell_cwd));
    write_fake_cwd ("/tmp");  // response to the command line clearing
    write_fake_cwd ("/tmp");  // response to cd

    // when
    test_subshell_chdir (subshell_get_cwd__return_value);

    // then
    mctest_assert_true (fd_readable (fake_pty[1]));
    mctest_assert_str_eq (subshell_cwd, "/tmp");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (shell_death_before_handshake_disables_subshell)
{
    // when
    kill_fake_shell_and_report ();

    // then
    mctest_assert_false (subshell_alive);
    mctest_assert_false (mc_global.tty.use_subshell);
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (mc_global.tty.subshell_pty), 1);
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (shell_death_after_handshake_keeps_subshell_enabled)
{
    // given
    subshell_initialized = TRUE;

    // when
    kill_fake_shell_and_report ();

    // then
    mctest_assert_false (subshell_alive);
    mctest_assert_true (mc_global.tty.use_subshell);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (close_select_channels_removes_both_fds)
{
    // when
    subshell_close_select_channels ();

    // then
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (mc_global.tty.subshell_pty), 1);
    ck_assert_int_eq (delete_select_channel__call_count_for_fd (subshell_pipe[READ]), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, init_ready_finishes_handshake_when_cwd_available);
    tcase_add_test (tc_core, init_ready_bails_out_when_subshell_not_alive);
    tcase_add_test (tc_core, ensure_initialized_finishes_pending_init);
    tcase_add_test (tc_core, ensure_initialized_is_a_noop_once_already_initialized);
    tcase_add_test (tc_core, chdir_is_ignored_until_initialized);
    tcase_add_test (tc_core, chdir_is_sent_once_initialized);
    tcase_add_test (tc_core, shell_death_before_handshake_disables_subshell);
    tcase_add_test (tc_core, shell_death_after_handshake_keeps_subshell_enabled);
    tcase_add_test (tc_core, close_select_channels_removes_both_fds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
