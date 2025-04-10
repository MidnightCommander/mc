/*
   lib - tests lib/utilinux:my_system() function

   Copyright (C) 2013-2025
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/utilunix"

#include "tests/mctest.h"

#include "lib/util.h"

#include "utilunix__my_system-common.c"

/* --------------------------------------------------------------------------------------------- */

START_TEST (fork_child_as_shell)
{
    int actual_value;
    // given
    fork__return_value = 0;

    // when
    actual_value = my_system (EXECUTE_AS_SHELL, "/bin/shell", "some command");

    // then
    ck_assert_int_eq (actual_value, 0);

    VERIFY_SIGACTION_CALLS ();
    VERIFY_SIGNAL_CALLS ();

    mctest_assert_str_eq (execvp__file__captured, "/bin/shell");
    ck_assert_int_eq (execvp__args__captured->len, 3);

    mctest_assert_str_eq (g_ptr_array_index (execvp__args__captured, 0), "/bin/shell");
    mctest_assert_str_eq (g_ptr_array_index (execvp__args__captured, 1), "-c");
    mctest_assert_str_eq (g_ptr_array_index (execvp__args__captured, 2), "some command");

    // All exec* calls is mocked, so call to _exit() function with 127 status code it's a normal
    // situation
    ck_assert_int_eq (my_exit__status__captured, 127);
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
    tcase_add_test (tc_core, fork_child_as_shell);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
