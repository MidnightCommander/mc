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
    str_init_strings ("UTF-8");
}

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_csi)
{
    const char *s = &"\x1b[=5uRest"[2];
    const char *end = s + strlen (s);
    const gboolean ok = parse_csi (NULL, &s, end);

    ck_assert_msg (ok, "failed to parse CSI");
    ck_assert_str_eq (s, "Rest");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_strip_ctrl_codes)
{
    // clang-format off
    char *s = g_strdup (ESC_STR "]0;~\a"                   ESC_STR "[30m" ESC_STR "(B" ESC_STR "[m"
                        ESC_STR "]133;A;special_key=1\a$ " ESC_STR "[K"   ESC_STR "[?2004h"
                        ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "="    ESC_STR "[?2004l"
                        ESC_STR "[>4;0m" ESC_STR "[=0u"    ESC_STR ">"    ESC_STR "[?2004h"
                        ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "="    ESC_STR "[?2004l"
                        ESC_STR "[>4;0m" ESC_STR "[=0u"    ESC_STR ">"    ESC_STR "[?2004h"
                        ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "=");
    // clang-format on
    char *actual = strip_ctrl_codes (s);
    const char *expected = "$ ";

    ck_assert_str_eq (actual, expected);
    g_free (s);
}
END_TEST

// Test the handling of inner and final incomplete UTF-8, also make sure there's no overrun.
// Ticket #4801. Invalid UTF-8 fragments are left in the string as-is.
START_TEST (test_strip_ctrl_codes2)
{
    // U+2764 heart in UTF-8, followed by " ábcdéfghíjklnmó\000pqrst" in Latin-1
    const char s_orig[] = "\342\235\244 \341bcd\351fgh\355jklm\363\000pqrst";
    ck_assert_int_eq (sizeof (s_orig), 25);

    // copy the entire string, with embedded '\0'
    char *s = g_malloc (sizeof (s_orig));
    memcpy (s, s_orig, sizeof (s_orig));

    char *actual = strip_ctrl_codes (s);

    ck_assert_str_eq (actual, s_orig);
    g_free (s);
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
    tcase_add_test (tc_core, test_parse_csi);
    tcase_add_test (tc_core, test_strip_ctrl_codes);
    tcase_add_test (tc_core, test_strip_ctrl_codes2);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
