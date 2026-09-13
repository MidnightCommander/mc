/*
   src/subshell - tests for peek_subshell_switch_key()

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

#include "subshell__common.c"

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (raw_ctrl_o_is_recognized)
{
    const char buf[] = { XCTRL ('o') & 255 };

    mctest_assert_true (peek_subshell_switch_key (buf, 1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (other_byte_is_not_recognized)
{
    const char buf[] = { 'x' };

    mctest_assert_false (peek_subshell_switch_key (buf, 1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (empty_buffer_is_not_recognized)
{
    mctest_assert_false (peek_subshell_switch_key ("", 0));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (kitty_ctrl_o_is_recognized)
{
    const char buf[] = ESC_STR "[111;5u";

    mctest_assert_true (peek_subshell_switch_key (buf, sizeof (buf) - 1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (kitty_plain_o_is_not_recognized)
{
    const char buf[] = ESC_STR "[111;1u";

    mctest_assert_false (peek_subshell_switch_key (buf, sizeof (buf) - 1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, raw_ctrl_o_is_recognized);
    tcase_add_test (tc_core, other_byte_is_not_recognized);
    tcase_add_test (tc_core, empty_buffer_is_not_recognized);
    tcase_add_test (tc_core, kitty_ctrl_o_is_recognized);
    tcase_add_test (tc_core, kitty_plain_o_is_not_recognized);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
