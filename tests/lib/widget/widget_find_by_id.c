/*
   libmc - checks for search widget with requested ID

   Copyright (C) 2020-2024
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2020-2022

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

#include "tests/mctest.h"

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_widget_find_by_id)
/* *INDENT-ON* */
{
    WGroup *g, *g0;
    Widget *w0;
    WRect r;

    g = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, 20, 20);
    group_init (g, &r, NULL, NULL);     /* ID = 0 */

    g0 = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, 10, 10);
    group_init (g0, &r, NULL, NULL);    /* ID = 1 */
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    rect_init (&r, 0, 0, 5, 5);
    widget_init (w0, &r, widget_default_callback, NULL);        /* ID = 2 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    rect_init (&r, 5, 5, 5, 5);
    widget_init (w0, &r, widget_default_callback, NULL);        /* ID = 3 */
    group_add_widget (g0, w0);

    g0 = g_new0 (WGroup, 1);
    rect_init (&r, 10, 10, 10, 10);
    group_init (g0, &r, NULL, NULL);    /* ID = 4 */
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    rect_init (&r, 10, 10, 5, 5);
    widget_init (w0, &r, widget_default_callback, NULL);        /* ID = 5 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    rect_init (&r, 15, 15, 5, 5);
    widget_init (w0, &r, widget_default_callback, NULL);        /* ID = 6 */
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    rect_init (&r, 5, 5, 10, 10);
    widget_init (w0, &r, widget_default_callback, NULL);        /* ID = 7 */
    group_add_widget (g, w0);

    w0 = WIDGET (g);

    ck_assert_msg (widget_find_by_id (w0, 0) != NULL, "Not found ID=0");
    ck_assert_msg (widget_find_by_id (w0, 1) != NULL, "Not found ID=1");
    ck_assert_msg (widget_find_by_id (w0, 2) != NULL, "Not found ID=2");
    ck_assert_msg (widget_find_by_id (w0, 3) != NULL, "Not found ID=3");
    ck_assert_msg (widget_find_by_id (w0, 4) != NULL, "Not found ID=4");
    ck_assert_msg (widget_find_by_id (w0, 5) != NULL, "Not found ID=5");
    ck_assert_msg (widget_find_by_id (w0, 6) != NULL, "Not found ID=6");
    ck_assert_msg (widget_find_by_id (w0, 7) != NULL, "Not found ID=7");
    ck_assert_msg (widget_find_by_id (w0, 8) == NULL, "Found ID=8");

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
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_widget_find_by_id);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
