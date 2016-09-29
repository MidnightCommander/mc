/*
   libmc - checks for search widget with requested ID

   Copyright (C) 2020
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2020

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

#define TEST_SUITE_NAME "lib/widget"

#include <config.h>

#include <check.h>

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_widget_find_by_id)
/* *INDENT-ON* */
{
    WGroup *g, *g0;
    Widget *w0;

    g = g_new0 (WGroup, 1);
    group_init (g, 0, 0, 20, 20, NULL, NULL);   /* ID = 0 */

    g0 = g_new0 (WGroup, 1);
    group_init (g0, 0, 0, 10, 10, NULL, NULL);  /* ID = 1 */
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 0, 0, 5, 5, widget_default_callback, NULL);        /* ID = 2 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 5, 5, 5, 5, widget_default_callback, NULL);        /* ID = 3 */
    group_add_widget (g0, w0);

    g0 = g_new0 (WGroup, 1);
    group_init (g0, 10, 10, 10, 10, NULL, NULL);        /* ID = 4 */
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 10, 10, 5, 5, widget_default_callback, NULL);      /* ID = 5 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 15, 15, 5, 5, widget_default_callback, NULL);      /* ID = 6 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 5, 5, 10, 10, widget_default_callback, NULL);      /* ID = 7 */
    group_add_widget (g, w0);

    w0 = WIDGET (g);

    fail_unless (widget_find_by_id (w0, 0) != NULL, "Not found ID=0");
    fail_unless (widget_find_by_id (w0, 1) != NULL, "Not found ID=1");
    fail_unless (widget_find_by_id (w0, 2) != NULL, "Not found ID=2");
    fail_unless (widget_find_by_id (w0, 3) != NULL, "Not found ID=3");
    fail_unless (widget_find_by_id (w0, 4) != NULL, "Not found ID=4");
    fail_unless (widget_find_by_id (w0, 5) != NULL, "Not found ID=5");
    fail_unless (widget_find_by_id (w0, 6) != NULL, "Not found ID=6");
    fail_unless (widget_find_by_id (w0, 7) != NULL, "Not found ID=7");
    fail_unless (widget_find_by_id (w0, 8) == NULL, "Found ID=8");

    send_message (g, NULL, MSG_INIT, 0, NULL);
    widget_destroy (w0);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_widget_find_by_id);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "widget_find_by_id.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
