/*
   src/learn - tests for modifier-aware key names

   Copyright (C) 2026
   Free Software Foundation, Inc.
*/

#define TEST_SUITE_NAME "/src/learn"

#include "tests/mctest.h"

#include "lib/tty/key.h"

#include "src/learn.c"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_learn_build_key_name_without_modifiers)
{
    char *name;

    name = learn_build_key_name ("up", 0);
    mctest_assert_str_eq (name, "up");
    g_free (name);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_learn_build_key_name_single_modifiers)
{
    char *name;

    name = learn_build_key_name ("f9", KEY_M_CTRL);
    mctest_assert_str_eq (name, "ctrl-f9");
    g_free (name);

    name = learn_build_key_name ("right", KEY_M_ALT);
    mctest_assert_str_eq (name, "meta-right");
    g_free (name);

    name = learn_build_key_name ("left", KEY_M_SHIFT);
    mctest_assert_str_eq (name, "shift-left");
    g_free (name);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_learn_build_key_name_combined_modifiers)
{
    char *name;

    name = learn_build_key_name ("up", KEY_M_CTRL | KEY_M_SHIFT);
    mctest_assert_str_eq (name, "ctrl-shift-up");
    g_free (name);

    name = learn_build_key_name ("pgdn", KEY_M_CTRL | KEY_M_ALT | KEY_M_SHIFT);
    mctest_assert_str_eq (name, "ctrl-meta-shift-pgdn");
    g_free (name);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_learn_build_key_name_without_modifiers);
    tcase_add_test (tc_core, test_learn_build_key_name_single_modifiers);
    tcase_add_test (tc_core, test_learn_build_key_name_combined_modifiers);

    return mctest_run_all (tc_core);
}
