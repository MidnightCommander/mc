/*
   libmc - checks for search widget with requested ID

   Copyright (C) 2021-2024
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2021-2022

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
START_TEST (test_widget_make_global_local)
/* *INDENT-ON* */
{
    WRect r;
    WGroup *g0, *g1, *g2;
    Widget *w0, *w1, *w2;

    /* top level group */
    g0 = g_new0 (WGroup, 1);
    rect_init (&r, 20, 20, 40, 40);
    group_init (g0, &r, NULL, NULL);

    /* g0 child */
    w0 = g_new0 (Widget, 1);
    rect_init (&r, 1, 1, 5, 5);
    widget_init (w0, &r, widget_default_callback, NULL);
    group_add_widget (g0, w0);

    /* g0 child */
    g1 = g_new0 (WGroup, 1);
    rect_init (&r, 5, 5, 30, 30);
    group_init (g1, &r, NULL, NULL);

    /* g1 child */
    w1 = g_new0 (Widget, 1);
    rect_init (&r, 5, 5, 10, 10);
    widget_init (w1, &r, widget_default_callback, NULL);
    group_add_widget (g1, w1);

    /* g1 child */
    g2 = g_new0 (WGroup, 1);
    rect_init (&r, 15, 15, 20, 20);
    group_init (g2, &r, NULL, NULL);
    group_add_widget (g1, g2);

    /* g2 child */
    w2 = g_new0 (Widget, 1);
    rect_init (&r, 15, 15, 5, 5);
    widget_init (w2, &r, widget_default_callback, NULL);
    group_add_widget (g2, w2);

    /* g0 child */
    group_add_widget (g0, g1);

    /* test global coordinates */
    /* w0 is a member of g0 */
    ck_assert_int_eq (w0->rect.y, 21);
    ck_assert_int_eq (w0->rect.x, 21);
    /* g1 is a member of g0 */
    ck_assert_int_eq (WIDGET (g1)->rect.y, 25);
    ck_assert_int_eq (WIDGET (g1)->rect.x, 25);
    /* w1 is a member of g1 */
    ck_assert_int_eq (w1->rect.y, 30);
    ck_assert_int_eq (w1->rect.x, 30);
    /* g2 is a member of g1 */
    ck_assert_int_eq (WIDGET (g2)->rect.y, 40);
    ck_assert_int_eq (WIDGET (g2)->rect.x, 40);
    /* w2 is a member of g2 */
    ck_assert_int_eq (w2->rect.y, 55);
    ck_assert_int_eq (w2->rect.x, 55);

    group_remove_widget (w0);
    group_remove_widget (g1);

    /* test local coordinates */
    /* w0 is not a member of g0 */
    ck_assert_int_eq (w0->rect.y, 1);
    ck_assert_int_eq (w0->rect.x, 1);

    /* g1 is not a member of g0 */
    ck_assert_int_eq (WIDGET (g1)->rect.y, 5);
    ck_assert_int_eq (WIDGET (g1)->rect.x, 5);
    /* w1 is a member of g1 */
    ck_assert_int_eq (w1->rect.y, 10);
    ck_assert_int_eq (w1->rect.x, 10);
    /* g2 is not a member of g1 */
    ck_assert_int_eq (WIDGET (g2)->rect.y, 20);
    ck_assert_int_eq (WIDGET (g2)->rect.x, 20);
    /* w2 is a member of g2 */
    ck_assert_int_eq (w2->rect.y, 35);
    ck_assert_int_eq (w2->rect.x, 35);

    widget_destroy (w0);
    widget_destroy (WIDGET (g1));
    widget_destroy (WIDGET (g0));
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
    tcase_add_test (tc_core, test_widget_make_global_local);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
