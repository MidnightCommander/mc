/*
   lib - convert_char_to_display() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Yury V. Zaytsev <yury@shurup.com>, 2026

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

#include "lib/charsets.h"
#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

/* The same contract as mcview_get_byte(): -1 at end of buffer */
static int
test_get_byte (void *from, const off_t pos)
{
    const char *s = (const char *) from;

    return (pos < 0 || (size_t) pos >= strlen (s)) ? -1 : (unsigned char) s[pos];
}

/* --------------------------------------------------------------------------------------------- */

/* The same contract as mcview_get_utf(): negative value for invalid UTF-8 byte and at the end of
 * buffer */
static int
test_get_utf8 (void *from, const off_t pos, int *len)
{
    const char *s = (const char *) from;
    gunichar uni;

    if (pos < 0 || (size_t) pos >= strlen (s))
        return -1;

    uni = g_utf8_get_char_validated (s + pos, -1);
    if (uni == (gunichar) (-1) || uni == (gunichar) (-2))
    {
        *len = 1;
        return (signed char) s[pos];
    }

    *len = g_utf8_next_char (s + pos) - (s + pos);
    return (int) uni;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_convert_char_to_display_ds") */
static const struct test_convert_char_to_display_ds
{
    const char *display;
    const char *source;
    const char *text;
    const int ch;
    const int len;
    const gboolean printable;
    const gboolean wide;
} test_convert_char_to_display_ds[] = {
    // UTF-8 display
    { "UTF-8", "UTF-8", "\xd0\x96", 0x0416, 2, TRUE, FALSE },
    { "UTF-8", "UTF-8", "\xe4\xb8\xad", 0x4E2D, 3, TRUE, TRUE },
    { "UTF-8", "UTF-8", "\xc3Z", NON_PRINTABLE_CHAR, 1, FALSE, FALSE },
    { "UTF-8", "UTF-8", "\xff", NON_PRINTABLE_CHAR, 1, FALSE, FALSE },
    { "UTF-8", "UTF-8", "", -1, 0, FALSE, FALSE },
    // ticket #4668: 8-bit letters are printable on UTF-8 display
    { "UTF-8", "CP1251", "\xc0", 0x0410, 1, TRUE, FALSE },
    { "UTF-8", "CP1251", "\xcf", 0x041F, 1, TRUE, FALSE },
    { "UTF-8", "CP1251", "\n", '\n', 1, FALSE, FALSE },
    // 8-bit display
    { "KOI8-R", "UTF-8", "\xd0\x96", 0xF6, 2, TRUE, FALSE },
    { "KOI8-R", "UTF-8", "\xe4\xb8\xad", NON_PRINTABLE_CHAR, 3, TRUE, FALSE },
    { "KOI8-R", "UTF-8", "\xff", NON_PRINTABLE_CHAR, 1, FALSE, FALSE },
    { "KOI8-R", "CP1251", "\xc0", 0xE1, 1, TRUE, FALSE },
    { "KOI8-R", "KOI8-R", "\xc1", 0xC1, 1, TRUE, FALSE },
};

/* @Test(dataSource = "test_convert_char_to_display_ds") */
START_PARAMETRIZED_TEST (test_convert_char_to_display, test_convert_char_to_display_ds)
{
    // given
    charset_conv_t conv;

    str_init_strings (data->display);
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    mc_global.utf8_display = str_isutf8 (data->display);
    mc_global.display_codepage = get_codepage_index (data->display);
    mc_global.source_codepage = get_codepage_index (data->source);
    ck_assert_int_ge (mc_global.display_codepage, 0);
    ck_assert_int_ge (mc_global.source_codepage, 0);
    g_free (init_translation_table (mc_global.source_codepage, mc_global.display_codepage));

    mctest_assert_true (charset_conv_init (&conv, test_get_byte, test_get_utf8));

    // when
    convert_char_to_display (&conv, (void *) data->text, 0);

    // then
    ck_assert_int_eq (conv.ch, data->ch);
    ck_assert_int_eq (conv.len, data->len);
    ck_assert_int_eq (conv.printable, data->printable);
    ck_assert_int_eq (conv.wide, data->wide);

    str_close_conv (conv.conv);
    free_codepages_list ();
    str_uninit_strings ();
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_convert_char_to_display,
                                   test_convert_char_to_display_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
