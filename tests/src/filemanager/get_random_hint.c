/*
   src/filemanager - filemanager functions.
   Tests for getting random hints.

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/util.h"

#include "src/filemanager/filemanager.h"


/* --------------------------------------------------------------------------------------------- */
/* mocked functions */

/* @Mock */
char *
guess_message_value (void)
{
    return g_strdup ("not_exists");
}

/* --------------------------------------------------------------------------------------------- */

/* @ThenReturnValue */
static gboolean rand__return_value = 0;

/* @Mock */
int
rand (void)
{
    return rand__return_value;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    mc_global.share_data_dir = (char *) TEST_SHARE_DIR;
    str_init_strings (NULL);
}

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_not_force)
/* *INDENT-ON* */
{
    // given
    char *first_hint_for_ignore;
    char *actual_hint1;
    char *actual_hint2;
    char *actual_hint3;

    // when
    first_hint_for_ignore = get_random_hint (FALSE);
    actual_hint1 = get_random_hint (FALSE);
    actual_hint2 = get_random_hint (FALSE);
    actual_hint3 = get_random_hint (FALSE);

    // then
    mctest_assert_ptr_ne (first_hint_for_ignore, NULL);
    mctest_assert_str_eq (actual_hint1, "");
    mctest_assert_str_eq (actual_hint2, "");
    mctest_assert_str_eq (actual_hint3, "");

    g_free (actual_hint3);
    g_free (actual_hint2);
    g_free (actual_hint1);
    g_free (first_hint_for_ignore);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#define MC_HINT_FILE_SIZE 58
/* @DataSource("get_random_ds") */
/* *INDENT-OFF* */
static const struct get_random_ds
{
    int input_random_value;

    const char *expected_value;
} get_random_ds[] =
{
    { /* 0. */
        MC_HINT_FILE_SIZE + 2,
        "Para_1",
    },
    { /* 1. */
        MC_HINT_FILE_SIZE + 10,
        "Para_2_line_1 Para_2_line_2",
    },
    { /* 2. */
        MC_HINT_FILE_SIZE + 25,
        "Para_2_line_1 Para_2_line_2",
    },
    { /* 3. */
        MC_HINT_FILE_SIZE + 40,
        "Para_3",
    },
    { /* 4. */
        MC_HINT_FILE_SIZE + 50,
        "P A R A _ 4 ", /* the trailing space it's a bug, but not critical and may be omitted */
    },
};
/* *INDENT-ON* */
    /* @Test(dataSource = "get_random_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (get_random, get_random_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_value;

    rand__return_value = data->input_random_value;

    /* when */
    actual_value = get_random_hint (TRUE);

    /* then */
    mctest_assert_str_eq (actual_value, data->expected_value);
    g_free (actual_value);
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

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_not_force);
    mctest_add_parameterized_test (tc_core, get_random, get_random_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
