/*
   lib - tty function testing

   Copyright (C) 2011-2025
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

#include <stdio.h>

#include "lib/strutil.h"
#include "lib/util.h"

#include "lib/tty/tty.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

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
    setenv ("TERM", "", 1);

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
    setenv ("TERM", "gnome-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 0);
    ck_assert_int_eq (actual_result_force_true, 1);
}
END_TEST
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tty_check_term_xterm_like)
{
    // given
    setenv ("TERM", "alacritty-terminal", 1);

    // when
    const gboolean actual_result_force_false = tty_check_xterm_compat (FALSE);
    const gboolean actual_result_force_true = tty_check_xterm_compat (TRUE);

    // then
    ck_assert_int_eq (my_exit__status__captured, 0);
    ck_assert_int_eq (actual_result_force_false, 1);
    ck_assert_int_eq (actual_result_force_true, 1);
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
    tcase_add_test (tc_core, test_tty_check_term_unset);
    tcase_add_test (tc_core, test_tty_check_term_non_xterm);
    tcase_add_test (tc_core, test_tty_check_term_xterm_like);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
