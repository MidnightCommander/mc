/*
   lib/search - tests for the search ignoring accents

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

#define TEST_SUITE_NAME "lib/search/ignore_accents"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/search.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_ignore_accents_ds") */
static const struct test_ignore_accents_ds
{
    const char *charset;
    gboolean utf8_display;
    gboolean case_sensitive;
    gboolean whole_words;
    const char *needle;
    const char *haystack;
    off_t expected_offset;  // -1: not found
    gsize expected_len;
} test_ignore_accents_ds[] = {
    // UTF-8: accents in the text, in the search string, or in both
    { "UTF-8", TRUE, FALSE, FALSE, "kazmer", "Kázmér", 0, 8 },
    { "UTF-8", TRUE, FALSE, FALSE, "Kázmér", "kazmer", 0, 6 },
    { "UTF-8", TRUE, FALSE, FALSE, "kázmer", "kazmér", 0, 7 },
    { "UTF-8", TRUE, FALSE, FALSE, "tukorfurogep", "árvíztűrő tükörfúrógép", 14, 17 },
    // letters without a decomposition
    { "UTF-8", TRUE, FALSE, FALSE, "lodz", "Łódź", 0, 7 },
    // decomposed text (NFD): the combining marks are part of the match
    { "UTF-8", TRUE, FALSE, FALSE, "kazmer", "Ka\xcc\x81zme\xcc\x81r", 0, 10 },
    { "UTF-8", TRUE, FALSE, FALSE, "Ka\xcc\x81zmer", "kazmer", 0, 6 },
    // case sensitive
    { "UTF-8", TRUE, TRUE, FALSE, "kazmer", "Kázmér", -1, 0 },
    { "UTF-8", TRUE, TRUE, FALSE, "Kazmer", "KÁZMÉR Kazmér", 9, 7 },
    { "UTF-8", TRUE, TRUE, FALSE, "Á", "a A", 2, 1 },
    // Greek
    { "UTF-8", TRUE, FALSE, FALSE, "αθηνα", "Αθήνα", 0, 10 },
    // regex special characters are matched literally
    { "UTF-8", TRUE, FALSE, FALSE, "a.(b)", "axb á.(b)", 4, 6 },
    // other letters don't match
    { "UTF-8", TRUE, FALSE, FALSE, "e", "áóú", -1, 0 },
    // whole words
    { "UTF-8", TRUE, FALSE, TRUE, "kazmer", "kázmérék kázmér", 12, 8 },
    // UTF-8 text on a non-UTF-8 display: the regex works on raw bytes
    { "UTF-8", FALSE, FALSE, FALSE, "kazmer", "KÁZMÉR", 0, 8 },
    { "UTF-8", FALSE, FALSE, FALSE, "kazmer", "Ka\xcc\x81zme\xcc\x81r", 0, 10 },
    { "UTF-8", FALSE, TRUE, FALSE, "kazmer", "Kázmér kázmér", 9, 8 },
    // 8-bit charsets: "\xc1rv\xedzt\xfbr\xf5" is "Árvíztűrő" in ISO-8859-2 and CP1250
    { "ISO8859-2", FALSE, FALSE, FALSE, "arvizturo", "\xc1rv\xedzt\xfbr\xf5", 0, 9 },
    { "ISO8859-2", FALSE, TRUE, FALSE, "arvizturo", "\xc1rv\xedzt\xfbr\xf5", -1, 0 },
    { "ISO8859-2", FALSE, TRUE, FALSE, "Arvizturo", "\xc1rv\xedzt\xfbr\xf5", 0, 9 },
    { "CP1250", FALSE, FALSE, FALSE, "t\xfbr\xf5", "ARVIZTURO", 5, 4 },
    { "CP1250", FALSE, FALSE, FALSE, "x", "\xc1rv\xedzt\xfbr\xf5", -1, 0 },
};

/* @Test(dataSource = "test_ignore_accents_ds") */
START_PARAMETRIZED_TEST (test_ignore_accents, test_ignore_accents_ds)
{
    mc_search_t *search;
    gsize found_len = 0;
    gboolean found;

    // given
    mc_global.utf8_display = data->utf8_display;
    search = mc_search_new (data->needle, data->charset);
    search->search_type = MC_SEARCH_T_NORMAL;
    search->is_case_sensitive = data->case_sensitive;
    search->whole_words = data->whole_words;
    search->ignore_accents = TRUE;

    // when
    found = mc_search_run (search, data->haystack, 0, (off_t) strlen (data->haystack), &found_len);

    // then
    mctest_assert_true (found == (data->expected_offset >= 0));
    if (found)
    {
        ck_assert_int_eq (search->normal_offset, data->expected_offset);
        ck_assert_int_eq (found_len, data->expected_len);
    }

    mc_search_free (search);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_ignore_accents, test_ignore_accents_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
