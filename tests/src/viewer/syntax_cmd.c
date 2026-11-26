/*
   src/viewer - syntax highlighting command tests

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

#define TEST_SUITE_NAME "/src/viewer"

#include "tests/mctest.h"

#include "src/viewer/syntax.h"

/* --------------------------------------------------------------------------------------------- */
/* Test: extract binary from simple command */

START_TEST (test_syntax_extract_binary_simple)
{
    // given
    const char *cmd = "source-highlight --failsafe --out-format=esc -i %s";
    char *binary;

    // when
    binary = mcview_syntax_extract_binary (cmd);

    // then
    mctest_assert_str_eq (binary, "source-highlight");
    g_free (binary);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: extract binary from command with absolute path */

START_TEST (test_syntax_extract_binary_absolute_path)
{
    // given
    const char *cmd = "/usr/bin/source-highlight --failsafe -i %s";
    char *binary;

    // when
    binary = mcview_syntax_extract_binary (cmd);

    // then
    mctest_assert_str_eq (binary, "/usr/bin/source-highlight");
    g_free (binary);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: extract binary from command with only binary name */

START_TEST (test_syntax_extract_binary_name_only)
{
    // given
    const char *cmd = "pygmentize";
    char *binary;

    // when
    binary = mcview_syntax_extract_binary (cmd);

    // then
    mctest_assert_str_eq (binary, "pygmentize");
    g_free (binary);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: extract binary returns NULL for empty string */

START_TEST (test_syntax_extract_binary_empty)
{
    // given / when
    char *binary = mcview_syntax_extract_binary ("");

    // then
    ck_assert_ptr_eq (binary, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: extract binary returns NULL for NULL */

START_TEST (test_syntax_extract_binary_null)
{
    // given / when
    char *binary = mcview_syntax_extract_binary (NULL);

    // then
    ck_assert_ptr_eq (binary, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command with simple filename substitution */

START_TEST (test_syntax_build_command_simple)
{
    // given
    const char *tmpl = "source-highlight --failsafe --out-format=esc -i %s";
    const char *filename = "test.c";
    char *cmd;

    // when
    cmd = mcview_syntax_build_command (tmpl, filename);

    // then
    mctest_assert_str_eq (cmd, "source-highlight --failsafe --out-format=esc -i 'test.c'");
    g_free (cmd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command shell-escapes filename with spaces */

START_TEST (test_syntax_build_command_spaces_in_filename)
{
    // given
    const char *tmpl = "source-highlight -i %s";
    const char *filename = "my file.c";
    char *cmd;

    // when
    cmd = mcview_syntax_build_command (tmpl, filename);

    // then
    mctest_assert_str_eq (cmd, "source-highlight -i 'my file.c'");
    g_free (cmd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command shell-escapes filename with quotes */

START_TEST (test_syntax_build_command_quotes_in_filename)
{
    // given
    const char *tmpl = "highlight -O xterm256 %s";
    const char *filename = "it's a test.c";
    char *cmd;

    // when
    cmd = mcview_syntax_build_command (tmpl, filename);

    // then
    mctest_assert_str_eq (cmd, "highlight -O xterm256 'it'\\''s a test.c'");
    g_free (cmd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command returns NULL when template has no %s */

START_TEST (test_syntax_build_command_no_placeholder)
{
    // given
    const char *tmpl = "source-highlight --failsafe";
    const char *filename = "test.c";

    // when
    char *cmd = mcview_syntax_build_command (tmpl, filename);

    // then
    ck_assert_ptr_eq (cmd, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command returns NULL for NULL template */

START_TEST (test_syntax_build_command_null_template)
{
    // given / when
    char *cmd = mcview_syntax_build_command (NULL, "test.c");

    // then
    ck_assert_ptr_eq (cmd, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: build command returns NULL for NULL filename */

START_TEST (test_syntax_build_command_null_filename)
{
    // given / when
    char *cmd = mcview_syntax_build_command ("highlight %s", NULL);

    // then
    ck_assert_ptr_eq (cmd, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/* Test: build command returns NULL for empty filename */

START_TEST (test_syntax_build_command_empty_filename)
{
    // given / when
    char *cmd = mcview_syntax_build_command ("highlight %s", "");

    // then — empty filename is valid, produces empty quoted string
    ck_assert_ptr_ne (cmd, NULL);
    mctest_assert_str_eq (cmd, "highlight ''");
    g_free (cmd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    Suite *s;
    SRunner *sr;
    TCase *tc_extract;
    TCase *tc_build;
    int number_failed;

    tc_extract = tcase_create ("syntax-extract-binary");
    tcase_add_test (tc_extract, test_syntax_extract_binary_simple);
    tcase_add_test (tc_extract, test_syntax_extract_binary_absolute_path);
    tcase_add_test (tc_extract, test_syntax_extract_binary_name_only);
    tcase_add_test (tc_extract, test_syntax_extract_binary_empty);
    tcase_add_test (tc_extract, test_syntax_extract_binary_null);

    tc_build = tcase_create ("syntax-build-command");
    tcase_add_test (tc_build, test_syntax_build_command_simple);
    tcase_add_test (tc_build, test_syntax_build_command_spaces_in_filename);
    tcase_add_test (tc_build, test_syntax_build_command_quotes_in_filename);
    tcase_add_test (tc_build, test_syntax_build_command_no_placeholder);
    tcase_add_test (tc_build, test_syntax_build_command_null_template);
    tcase_add_test (tc_build, test_syntax_build_command_null_filename);
    tcase_add_test (tc_build, test_syntax_build_command_empty_filename);

    s = suite_create (TEST_SUITE_NAME);
    suite_add_tcase (s, tc_extract);
    suite_add_tcase (s, tc_build);
    sr = srunner_create (s);
    srunner_set_log (sr, "-");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
