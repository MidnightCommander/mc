/*
   libmc - checks for initialization and deinitialization of WGroup widget

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

#define TEST_SUITE_NAME "lib/widget/group"

#include <config.h>

#include <check.h>

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static int ref = 0;

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
widget_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        ref++;
        return widget_default_callback (w, NULL, MSG_INIT, 0, NULL);

    case MSG_DESTROY:
        ref--;
        return widget_default_callback (w, NULL, MSG_DESTROY, 0, NULL);

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
group_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        ref++;
        return group_default_callback (w, NULL, MSG_INIT, 0, NULL);

    case MSG_DESTROY:
        ref--;
        return group_default_callback (w, NULL, MSG_DESTROY, 0, NULL);

    default:
        return group_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_group_init_deinit)
/* *INDENT-ON* */
{
    WGroup *g, *g0;
    Widget *w0;

    g = g_new0 (WGroup, 1);
    group_init (g, 0, 0, 20, 20, group_callback, NULL);

    g0 = g_new0 (WGroup, 1);
    group_init (g0, 0, 0, 10, 10, group_callback, NULL);
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 0, 0, 5, 5, widget_callback, NULL);
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 5, 5, 5, 5, widget_callback, NULL);
    group_add_widget (g0, w0);

    g0 = g_new0 (WGroup, 1);
    group_init (g0, 10, 10, 10, 10, group_callback, NULL);
    group_add_widget (g, g0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 10, 10, 5, 5, widget_callback, NULL);
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 15, 15, 5, 5, widget_callback, NULL);
    group_add_widget (g0, w0);

    w0 = g_new0 (Widget, 1);
    widget_init (w0, 5, 5, 10, 10, widget_callback, NULL);
    group_add_widget (g, w0);

    fail_unless (w0->id == 7, "last id (%d) != 7", ref);

    send_message (g, NULL, MSG_INIT, 0, NULL);

    fail_unless (ref == 8, "ref (%d) != 8", ref);

    widget_destroy (WIDGET (g));

    fail_unless (ref == 0, "ref (%d) != 0", ref);
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
    tcase_add_test (tc_core, test_group_init_deinit);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "group_init_deinit.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
