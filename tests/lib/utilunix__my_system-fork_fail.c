/*
   lib - tests lib/utilinux:my_system() function

   Copyright (C) 2013-2024
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/utilunix"

#include "tests/mctest.h"

#include <signal.h>
#include <unistd.h>

#include "lib/util.h"

#include "utilunix__my_system-common.c"

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (fork_fail)
/* *INDENT-ON* */
{
    int actual_value;

    /* given */
    fork__return_value = -1;

    /* when */
    actual_value = my_system (0, NULL, NULL);

    /* then */
    ck_assert_int_eq (actual_value, -1);

    VERIFY_SIGACTION_CALLS ();

    ck_assert_int_eq (signal_signum__captured->len, 0);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, fork_fail);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
