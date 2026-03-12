/*
   lib/terminal - tests for terminal emulation functions

   Copyright (C) 2025-2026
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

static char *
encode_controls_simple (const char *s)
{
    return encode_controls (s, -1);
}

static char *
decode_controls_simple (const char *s)
{
    GString *ret;

    ret = decode_controls (s);
    return g_string_free (ret, FALSE);
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
    char *s;
    char *actual;

    // clang-format off
    s = g_strdup (ESC_STR "]0;~\a"                   ESC_STR "[30m" ESC_STR "(B" ESC_STR "[m"
                  ESC_STR "]133;A;special_key=1\a$ " ESC_STR "[K"   ESC_STR "[?2004h"
                  ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "="    ESC_STR "[?2004l"
                  ESC_STR "[>4;0m" ESC_STR "[=0u"    ESC_STR ">"    ESC_STR "[?2004h"
                  ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "="    ESC_STR "[?2004l"
                  ESC_STR "[>4;0m" ESC_STR "[=0u"    ESC_STR ">"    ESC_STR "[?2004h"
                  ESC_STR "[>4;1m" ESC_STR "[=5u"    ESC_STR "=");
    // clang-format on

    actual = strip_ctrl_codes (s);

    const char *expected = "$ ";

    ck_assert_str_eq (actual, expected);
    g_free (s);
}
END_TEST

// Test the handling of inner and final incomplete UTF-8, also make sure there's no overrun.
// Ticket #4801. Invalid UTF-8 fragments are left in the string as-is.
START_TEST (test_strip_ctrl_codes2)
{
    char *s;
    char *actual;
    // U+2764 heart in UTF-8, followed by " ábcdéfghíjklnmó\000pqrst" in Latin-1
    const char s_orig[] = "\342\235\244 \341bcd\351fgh\355jklm\363\000pqrst";

    ck_assert_int_eq (sizeof (s_orig), 25);

    // copy the entire string, with embedded '\0'
    s = g_malloc (sizeof (s_orig));
    memcpy (s, s_orig, sizeof (s_orig));

    actual = strip_ctrl_codes (s);

    ck_assert_str_eq (actual, s_orig);
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_encode_controls)
{
    mctest_assert_str_eq (encode_controls_simple ("\001"), "^A");
    mctest_assert_str_eq (encode_controls_simple ("\032"), "^Z");
    mctest_assert_str_eq (encode_controls_simple ("\033"), "\\e");
    mctest_assert_str_eq (encode_controls_simple ("\034"), "^\\");
    mctest_assert_str_eq (encode_controls_simple ("\035"), "^]");
    mctest_assert_str_eq (encode_controls_simple ("\036"), "^^");
    mctest_assert_str_eq (encode_controls_simple ("\037"), "^_");
    mctest_assert_str_eq (encode_controls_simple ("\177"), "^?");
    mctest_assert_str_eq (encode_controls_simple (" "), "\\s");
    mctest_assert_str_eq (encode_controls_simple ("\\"), "\\\\");
    mctest_assert_str_eq (encode_controls_simple ("^"), "\\^");

    mctest_assert_str_eq (encode_controls ("\000", 1), "^@");
    mctest_assert_str_eq (encode_controls ("ab\000\000cd\000\000ef", 7), "ab^@^@cd^@");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_decode_controls)
{
    GString *ret;

    mctest_assert_str_eq (decode_controls_simple ("^A"), "\001");
    mctest_assert_str_eq (decode_controls_simple ("^Z"), "\032");
    mctest_assert_str_eq (decode_controls_simple ("^["), "\033");
    mctest_assert_str_eq (decode_controls_simple ("\\e"), "\033");
    mctest_assert_str_eq (decode_controls_simple ("\\E"), "\033");
    mctest_assert_str_eq (decode_controls_simple ("^\\"), "\034");
    mctest_assert_str_eq (decode_controls_simple ("^]"), "\035");
    mctest_assert_str_eq (decode_controls_simple ("^^"), "\036");
    mctest_assert_str_eq (decode_controls_simple ("^_"), "\037");
    mctest_assert_str_eq (decode_controls_simple ("^?"), "\177");
    mctest_assert_str_eq (decode_controls_simple ("\\s"), " ");
    mctest_assert_str_eq (decode_controls_simple ("\\\\"), "\\");
    mctest_assert_str_eq (decode_controls_simple ("\\^"), "^");
    mctest_assert_str_eq (decode_controls_simple ("\\^"), "^");
    mctest_assert_str_eq (decode_controls_simple ("^A^[\\e\\E^\\^^^?"),
                          "\001\033\033\033\034\036\177");
    mctest_assert_str_eq (decode_controls_simple ("\\\\\\\\\\\\"), "\\\\\\");
    mctest_assert_str_eq (decode_controls_simple ("^^^^^^"), "\036\036\036");
    mctest_assert_str_eq (decode_controls_simple ("^^^^^\\^\\\\\\\\\\\\^\\^^^^^"),
                          "\036\036\034\034\\\\^^\036\036");
    mctest_assert_str_eq (decode_controls_simple ("trailing^"), "trailing^");

    ret = decode_controls ("^@");
    mctest_assert_mem_eq (ret->str, ret->len, "\000", 1);
    g_string_free (ret, TRUE);

    ret = decode_controls ("ab^@^@cd^@");
    mctest_assert_mem_eq (ret->str, ret->len, "ab\000\000cd\000", 7);
    g_string_free (ret, TRUE);
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
    tcase_add_test (tc_core, test_encode_controls);
    tcase_add_test (tc_core, test_decode_controls);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
