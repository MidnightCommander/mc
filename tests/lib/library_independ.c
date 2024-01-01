/*
   libmc - check if library is independent to $(topsrc)/src directory

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011, 2013

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

#define TEST_SUITE_NAME "lib/library_independ"

#include "tests/mctest.h"

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_library_independ)
/* *INDENT-ON* */
{
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

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_library_independ);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
