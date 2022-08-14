/*
   libmc - checks search functions

   Copyright (C) 2022
   Free Software Foundation, Inc.

   Written by:
   Steef Boerrigter <sxmboer@gmail.com>, 2022

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "lib/search/search"

#include "tests/mctest.h"

#include "search.c"             /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_regex_multiline_str_ds") */
/* *INDENT-OFF* */
static const struct test_regex_multiline_str_ds
{
    const mc_search_type_t type;
    const char *buffer;
    const char *search_str;
    const gboolean retval;
} test_regex_multiline_str_ds[] =
{
    { /* 1. */
        MC_SEARCH_T_NORMAL,
        "abcdefgh",
        "nope",
        FALSE
    },
    { /* 2. */
        MC_SEARCH_T_NORMAL,
        "abcdefgh",
        "def",
        TRUE
    },
    { /* 3. */
        MC_SEARCH_T_NORMAL,
        "abcdefgh",
        "z",
        FALSE
    },
    { /* 4. */
        MC_SEARCH_T_NORMAL,
        "abcd	\tefgh",
        "\t	",
        TRUE
    },
    { /* 5. multiline */
        MC_SEARCH_T_NORMAL,
        "abcd\ne\nfgh",
        "\ne\nf",
        TRUE
    },
    { /* 6. */
        MC_SEARCH_T_NORMAL,
        "abcd\nefgh",
        "d\ne",
        TRUE
    },
    { /* 7. mc-4.8.28 fails this edge condition */
        MC_SEARCH_T_NORMAL,
        "abcdefgh\n",
        "\n",
        TRUE
    },
    { /* 8. mc-4.8.28 fails this edge condition */
        MC_SEARCH_T_NORMAL,
        "abcdefgh\n",
        "\n\n",
        FALSE
    },
    { /* 9. regex including newline */
        MC_SEARCH_T_REGEX,
        "abcd\nefgh",
        "abc[c-e]\nef",
        TRUE
    },
    { /* 10. regex's newline support */
        MC_SEARCH_T_REGEX,
        "abcd\nefgh",
        "abc[c-e]\\nef",
        TRUE
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_regex_multiline_str_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_regex_multiline_str, test_regex_multiline_str_ds)
/* *INDENT-ON* */
{
    /* given */
    mc_search_t *s;
    gboolean retval;

    s = mc_search_new (data->buffer, NULL);
    s->is_case_sensitive = FALSE;
    s->search_type = data->type;

    /* when */
    retval = mc_search (data->search_str, DEFAULT_CHARSET, data->buffer, s->search_type);

    /* then */
    if (data->retval)
    {
        mctest_assert_true (retval);
    }
    else
    {
        mctest_assert_false (retval);
    }

    mc_search_free (s);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_regex_multiline_str, test_regex_multiline_str_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
