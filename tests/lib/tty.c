/*
   lib - tty function testing

   Copyright (C) 2011-2026
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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <termios.h>

#include "lib/strutil.h"
#include "lib/util.h"

#include "lib/tty/tty.h"

/* --------------------------------------------------------------------------------------------- */
/* @CapturedValue */
static int my_exit__status__captured;

/* @Mock */
void
my_exit (int status)
{
    my_exit__status__captured = status;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_unset)
{
    // given
    g_unsetenv ("TERM");

    // when
    tty_check_xterm_compat (FALSE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_set_empty)
{
    // given
    g_setenv ("TERM", "", TRUE);

    // when
    tty_check_xterm_compat (FALSE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_non_xterm)
{
    // given
    g_setenv ("TERM", "gnome-terminal", TRUE);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    mctest_assert_false (actual_result_force_false);
    mctest_assert_true (actual_result_force_true);
}
END_TEST
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_xterm_like)
{
    // given
    g_setenv ("TERM", "alacritty-terminal", TRUE);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    mctest_assert_true (actual_result_force_false);
    mctest_assert_true (actual_result_force_true);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Open a new pseudo-terminal and return the descriptor of its slave side. If @controlling is
   TRUE, the calling process must be a session leader, and the slave becomes its controlling
   terminal. The master descriptor is stored in @master. */
static int
open_pty_slave (int *master, gboolean controlling)
{
    int slave;

    *master = posix_openpt (O_RDWR | O_NOCTTY);
    if (*master == -1 || grantpt (*master) != 0 || unlockpt (*master) != 0)
        return -1;

    slave = open (ptsname (*master), O_RDWR | (controlling ? 0 : O_NOCTTY));
#ifdef TIOCSCTTY
    if (slave != -1 && controlling && ioctl (slave, TIOCSCTTY, 0) == -1)
        return -1;
#endif

    return slave;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
same_file (int fd1, int fd2)
{
    struct stat st1, st2;

    return fstat (fd1, &st1) == 0 && fstat (fd2, &st2) == 0 && st1.st_dev == st2.st_dev
        && st1.st_ino == st2.st_ino;
}

/* --------------------------------------------------------------------------------------------- */

/* Check that a key typed on the terminal with the @master side can be read from stdin */
static gboolean
key_reaches_stdin (int master)
{
    char buf[8] = "";

    alarm (5);  // don't hang the test if stdin is not that terminal
    return write (master, "k\n", 2) == 2 && read (STDIN_FILENO, buf, sizeof (buf)) == 2
        && buf[0] == 'k';
}

/* --------------------------------------------------------------------------------------------- */

/* Session setups for tty_stdin_to_terminal() tests */
typedef enum
{
    STDIN_IS_TTY,                         // stdin is a terminal, no controlling terminal
    STDIN_REDIRECTED,                     // stdin is /dev/null, there is a controlling terminal
    STDIN_REDIRECTED_NO_CTTY_STDERR_TTY,  // stdin is /dev/null, no ctty, stderr is a terminal
    NO_TERMINAL_AT_ALL,                   // stdin and stderr are /dev/null, no controlling terminal
} stdin_setup_t;

/* Run tty_stdin_to_terminal() in a new session prepared according to @setup and return
   0 if the result is as expected, a positive code of the failed check otherwise. */
static int
run_stdin_to_terminal (stdin_setup_t setup)
{
    pid_t pid;
    int status;

    pid = fork ();
    if (pid == 0)
    {
        int master, slave, devnull, saved_stdin;
        gboolean ret;

        if (setsid () == -1)
            _exit (10);

        slave = open_pty_slave (&master, setup == STDIN_REDIRECTED);
        devnull = open ("/dev/null", O_RDWR);
        if (slave == -1 || devnull == -1)
            _exit (11);

        dup2 (setup == STDIN_IS_TTY ? slave : devnull, STDIN_FILENO);
        dup2 (setup == STDIN_REDIRECTED_NO_CTTY_STDERR_TTY ? slave : devnull, STDERR_FILENO);
        saved_stdin = dup (STDIN_FILENO);

        ret = tty_stdin_to_terminal ();

        if (setup == NO_TERMINAL_AT_ALL)
            // must fail and leave stdin untouched
            _exit (ret ? 1 : (same_file (STDIN_FILENO, saved_stdin) ? 0 : 2));

        if (!ret)
            _exit (3);
        if (!isatty (STDIN_FILENO))
            _exit (4);
        // stdin must be the pty: the original one, /dev/tty or stderr
        if (!key_reaches_stdin (master))
            _exit (5);
        _exit (0);
    }

    if (pid == -1 || waitpid (pid, &status, 0) != pid || !WIFEXITED (status))
        return 100;

    return WEXITSTATUS (status);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_stdin_to_terminal_stdin_is_tty)
{
    // given: stdin is a terminal

    // when
    const int actual_result = run_stdin_to_terminal (STDIN_IS_TTY);

    // then: stdin is left as is
    ck_assert_int_eq (actual_result, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_stdin_to_terminal_redirected)
{
    // given: "mc < /dev/null" run from a terminal

    // when
    const int actual_result = run_stdin_to_terminal (STDIN_REDIRECTED);

    // then: stdin is the controlling terminal
    ck_assert_int_eq (actual_result, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_stdin_to_terminal_stderr_fallback)
{
    // given: stdin is redirected, no controlling terminal, stderr is a terminal

    // when
    const int actual_result = run_stdin_to_terminal (STDIN_REDIRECTED_NO_CTTY_STDERR_TTY);

    // then: stdin is the terminal of stderr
    ck_assert_int_eq (actual_result, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_stdin_to_terminal_no_terminal)
{
    // given: no terminal at all

    // when
    const int actual_result = run_stdin_to_terminal (NO_TERMINAL_AT_ALL);

    // then: failure is reported and stdin is left as is
    ck_assert_int_eq (actual_result, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_tty_check_term_unset);
    tcase_add_test (tc_core, test_tty_check_term_set_empty);
    tcase_add_test (tc_core, test_tty_check_term_non_xterm);
    tcase_add_test (tc_core, test_tty_check_term_xterm_like);
    tcase_add_test (tc_core, test_tty_stdin_to_terminal_stdin_is_tty);
    tcase_add_test (tc_core, test_tty_stdin_to_terminal_redirected);
    tcase_add_test (tc_core, test_tty_stdin_to_terminal_stderr_fallback);
    tcase_add_test (tc_core, test_tty_stdin_to_terminal_no_terminal);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
