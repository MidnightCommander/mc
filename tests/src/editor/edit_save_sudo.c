/*
   src/editor - tests for sudo save fallback helpers

   Copyright (C) 2026
   Free Software Foundation, Inc.
*/

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <string.h>

#include "src/editor/edit-impl.h"
#include "src/editor/editwidget.h"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_should_try_sudo_only_on_eacces_failure)
{
    mctest_assert_true (edit_save_should_try_sudo (0, EACCES));
    mctest_assert_false (edit_save_should_try_sudo (1, EACCES));
    mctest_assert_false (edit_save_should_try_sudo (0, ENOENT));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_handle_sudo_success_updates_editor_state)
{
    WEdit edit;

    memset (&edit, 0, sizeof (edit));
    edit.modified = 1;
    edit.delete_file = 1;
    edit.force = 0;

    ck_assert_int_eq (edit_save_handle_sudo_result (&edit, 1), 1);
    ck_assert_int_eq (edit.modified, 0);
    ck_assert_int_eq (edit.delete_file, 0);
    ck_assert_int_ne ((edit.force & REDRAW_COMPLETELY), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_handle_sudo_cancel_sets_redraw_only)
{
    WEdit edit;

    memset (&edit, 0, sizeof (edit));
    edit.modified = 1;
    edit.delete_file = 1;
    edit.force = 0;

    ck_assert_int_eq (edit_save_handle_sudo_result (&edit, -1), -1);
    ck_assert_int_eq (edit.modified, 1);
    ck_assert_int_eq (edit.delete_file, 1);
    ck_assert_int_ne ((edit.force & REDRAW_COMPLETELY), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_handle_sudo_not_handled_keeps_state)
{
    WEdit edit;

    memset (&edit, 0, sizeof (edit));
    edit.modified = 1;
    edit.delete_file = 1;
    edit.force = 0;

    ck_assert_int_eq (edit_save_handle_sudo_result (&edit, 0), 0);
    ck_assert_int_eq (edit.modified, 1);
    ck_assert_int_eq (edit.delete_file, 1);
    ck_assert_int_eq (edit.force, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_should_try_sudo_only_on_eacces_failure);
    tcase_add_test (tc_core, test_handle_sudo_success_updates_editor_state);
    tcase_add_test (tc_core, test_handle_sudo_cancel_sets_redraw_only);
    tcase_add_test (tc_core, test_handle_sudo_not_handled_keeps_state);

    return mctest_run_all (tc_core);
}
