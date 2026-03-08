/*
   lib - keycode_to_cntrl() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Manuel Einfalt <einfalt1@proton.me>, 2026

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
#include "lib/util.h"
#include "lib/tty/key.h"

/* --------------------------------------------------------------------------------------------- */

static const struct test_keycodes
{
    int keycode;
    int expected_return;
} test_keycodes[] = { { '@', 0 },
                      { '`', -1 },
                      { KEY_M_CTRL | 0, 0 },
                      { 'A', 1 },
                      { 'a', 1 },
                      { KEY_M_CTRL | 1, 1 },
                      { 'B', 2 },
                      { 'b', 2 },
                      { KEY_M_CTRL | 2, 2 },
                      { 'C', 3 },
                      { 'c', 3 },
                      { KEY_M_CTRL | 3, 3 },
                      { 'D', 4 },
                      { 'd', 4 },
                      { KEY_M_CTRL | 4, 4 },
                      { 'E', 5 },
                      { 'e', 5 },
                      { KEY_M_CTRL | 5, 5 },
                      { 'F', 6 },
                      { 'f', 6 },
                      { KEY_M_CTRL | 6, 6 },
                      { 'G', 7 },
                      { 'g', 7 },
                      { KEY_M_CTRL | 7, 7 },
                      { 'H', 8 },
                      { 'h', 8 },
                      { KEY_M_CTRL | 8, 8 },
                      { 'I', 9 },
                      { 'i', 9 },
                      { KEY_M_CTRL | 9, 9 },
                      { 'J', 10 },
                      { 'j', 10 },
                      { KEY_M_CTRL | 10, 10 },
                      { 'K', 11 },
                      { 'k', 11 },
                      { KEY_M_CTRL | 11, 11 },
                      { 'L', 12 },
                      { 'l', 12 },
                      { KEY_M_CTRL | 12, 12 },
                      { 'M', 13 },
                      { 'm', 13 },
                      { KEY_M_CTRL | 13, 13 },
                      { 'N', 14 },
                      { 'n', 14 },
                      { KEY_M_CTRL | 14, 14 },
                      { 'O', 15 },
                      { 'o', 15 },
                      { KEY_M_CTRL | 15, 15 },
                      { 'P', 16 },
                      { 'p', 16 },
                      { KEY_M_CTRL | 16, 16 },
                      { 'Q', 17 },
                      { 'q', 17 },
                      { KEY_M_CTRL | 17, 17 },
                      { 'R', 18 },
                      { 'r', 18 },
                      { KEY_M_CTRL | 18, 18 },
                      { 'S', 19 },
                      { 's', 19 },
                      { KEY_M_CTRL | 19, 19 },
                      { 'T', 20 },
                      { 't', 20 },
                      { KEY_M_CTRL | 20, 20 },
                      { 'U', 21 },
                      { 'u', 21 },
                      { KEY_M_CTRL | 21, 21 },
                      { 'V', 22 },
                      { 'v', 22 },
                      { KEY_M_CTRL | 22, 22 },
                      { 'W', 23 },
                      { 'w', 23 },
                      { KEY_M_CTRL | 23, 23 },
                      { 'X', 24 },
                      { 'x', 24 },
                      { KEY_M_CTRL | 24, 24 },
                      { 'Y', 25 },
                      { 'y', 25 },
                      { KEY_M_CTRL | 25, 25 },
                      { 'Z', 26 },
                      { 'z', 26 },
                      { KEY_M_CTRL | 26, 26 },
                      { '[', 27 },
                      { KEY_M_CTRL | 27, 27 },
                      { '\\', 28 },
                      { KEY_M_CTRL | 28, 28 },
                      { ']', 29 },
                      { KEY_M_CTRL | 29, 29 },
                      { '^', 30 },
                      { KEY_M_CTRL | 30, 30 },
                      { '_', 31 },
                      { KEY_M_CTRL | 31, 31 },

                      // Invalid ASCII-inputs:
                      { '~', -1 },
                      { '}', -1 },
                      { '|', -1 },
                      { '{', -1 },

                      /*
                       * Invalid inputs are non-ASCII or other special keys.
                       * These have bits set outside of 0x7f and KEY_M_CTRL.
                       */
                      { 0x5000, -1 },
                      { 128, -1 },
                      { 0x0507, -1 } };

START_PARAMETRIZED_TEST (keycode_test, test_keycodes)
{
    const int actual_result = keycode_to_cntrl (data->keycode);
    mctest_assert_true (actual_result == data->expected_return);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, keycode_test, test_keycodes);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
