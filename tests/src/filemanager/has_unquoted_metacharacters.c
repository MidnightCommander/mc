/*
   src/filemanager - tests for command_has_unquoted_metacharacters()

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Marek Libra <marek.libra@gmail.com>, 2026

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

/* --------------------------------------------------------------------------------------------- */

MC_TESTABLE gboolean command_has_unquoted_metacharacters (const char *cmd);

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_metacharacters_ds") */
static const struct test_metacharacters_ds
{
    const char *input_value;
    gboolean expected_result;
} test_metacharacters_ds[] = {
    /* simple commands - no metacharacters */
    { "cd", FALSE },
    { "cd /tmp", FALSE },
    { "cd ..", FALSE },
    { "cd ~/projects", FALSE },
    { "cd $HOME", FALSE },
    { "cd -", FALSE },
    { "cd /path/to/dir", FALSE },
    { "cd foo bar", FALSE },

    /* semicolons */
    { "cd ..; make", TRUE },
    { "cd ..; make; cd -", TRUE },
    { "cd /tmp; ls", TRUE },
    { "cd ..;ls", TRUE },
    { "cd /tmp;ls", TRUE },

    /* && */
    { "cd .. && make", TRUE },
    { "cd /tmp && ls && pwd", TRUE },
    { "cd ..&&ls", TRUE },

    /* || */
    { "cd .. || echo fail", TRUE },
    { "cd ..||ls", TRUE },

    /* pipe */
    { "cd foo | bar", TRUE },
    { "cd ..|ls", TRUE },

    /* background / single & */
    { "cd &", TRUE },
    { "cd a&b", TRUE },

    /* quoted paths - metacharacters inside quotes are literal */
    { "cd 'foo bar'", FALSE },
    { "cd \"foo bar\"", FALSE },
    { "cd 'foo;bar'", FALSE },
    { "cd 'foo|bar'", FALSE },
    { "cd 'foo&bar'", FALSE },
    { "cd \"foo;bar\"", FALSE },
    { "cd \"foo|bar\"", FALSE },
    { "cd \"foo&bar\"", FALSE },

    /* quoted path followed by a real separator */
    { "cd 'foo'; ls", TRUE },

    /* backslash-escaped metacharacters are literal */
    { "cd foo\\ bar", FALSE },
    { "cd foo\\;bar", FALSE },
    { "cd foo\\|bar", FALSE },
    { "cd foo\\&bar", FALSE },
    { "cd path\\;with\\;semicolons", FALSE },

    /* escaped metacharacter plus a real separator */
    { "cd foo\\;bar; make", TRUE },

    /* double-backslash before semicolon: \\; at runtime - first \ escapes second, the ; is real */
    { "cd foo\\\\;ls", TRUE },
    /* triple-backslash before semicolon: \\\; at runtime - \\ + \; (escaped semicolon) */
    { "cd foo\\\\\\;ls", FALSE },

    /* command substitution $(...) - internal cd cannot expand */
    { "cd $(echo /tmp)", TRUE },
    { "cd $(pwd)", TRUE },
    { "cd $(echo foo;echo bar)", TRUE },

    /* command substitution with backticks */
    { "cd `echo /tmp`", TRUE },
    { "cd `pwd`", TRUE },

    /* single-quoted command substitution - literal, not metacharacters */
    { "cd '$(echo /tmp)'", FALSE },
    { "cd '`echo /tmp`'", FALSE },

    /* double-quoted command substitution - shell still expands these */
    { "cd \"$(echo /tmp)\"", TRUE },
    { "cd \"`echo /tmp`\"", TRUE },

    /* escaped $ and backtick */
    { "cd \\$(echo /tmp)", FALSE },
    { "cd \\`echo /tmp\\`", FALSE },

    /* empty and whitespace-only strings */
    { "", FALSE },
    { "   ", FALSE },

    /* parameter expansion - not command substitution */
    { "cd ${HOME}", FALSE },

    /* arithmetic expansion - triggers $( detection */
    { "cd $((1+2))", TRUE },

    /* unterminated quotes - no metachar, shell would error anyway */
    { "cd 'foo", FALSE },
    { "cd \"foo", FALSE },
};

/* @Test(dataSource = "test_metacharacters_ds") */
START_PARAMETRIZED_TEST (test_metacharacters, test_metacharacters_ds)
{
    gboolean actual_result;

    actual_result = command_has_unquoted_metacharacters (data->input_value);

    ck_assert_int_eq (actual_result, data->expected_result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_metacharacters, test_metacharacters_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
