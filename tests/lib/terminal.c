/*
   lib/terminal - tests for terminal emulation functions

   Copyright (C) 2013-2025
   Free Software Foundation, Inc.

   Written by:
   Johannes Altmanninger, 2025

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

#define TEST_SUITE_NAME "/lib/terminal"

#include "tests/mctest.h"

#include <string.h>

#include "lib/global.h"  // include <glib.h>
#include "lib/terminal.h"
#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings (NULL);
}

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_strip_ctrl_codes)
{
    char *s = strdup (
        "\033]0;~\a\033[30m\033(B\033[m\033]133;A;special_key=1\a$ "
        "\033[K\033[?2004h\033[>4;1m\033[=5u\033=\033[?2004l\033[>4;0m\033[=0u\033>\033[?2004h"
        "\033[>4;1m\033[=5u\033=\033[?2004l\033[>4;0m\033[=0u\033>\033[?2004h\033[>4;1m\033[=5u"
        "\033=");
    char *actual = strip_ctrl_codes (s);
    const char *expected = "$ ";
    ck_assert_str_eq (actual, expected);
    free (s);
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
    tcase_add_test (tc_core, test_strip_ctrl_codes);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
