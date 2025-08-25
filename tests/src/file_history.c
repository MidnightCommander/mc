/*
   src/file_history - tests for file_history

   Copyright (C) 2025
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

#define TEST_SUITE_NAME "/src/file_history"

#include "tests/mctest.h"

#include "src/file_history.c"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_file_history_parse)
{
    GList *file_list = NULL;

    const char *mock_entries[] = { "/home/codespace/tmp/foo\n", "\n", "bar 2;0;5\n", "" };
    const size_t entries_count = sizeof (mock_entries) / sizeof (mock_entries[0]);

    for (size_t i = 0; i < entries_count; i++)
        file_history_parse_entry (mock_entries[i], &file_list);

    ck_assert_uint_eq (g_list_length (file_list), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_file_history_parse);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
