/*
   libmc - checks for delivery of mouse events to widgets of WGroup

   Copyright (C) 2026
   The Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "lib/widget/group"

#include "tests/mctest.h"

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

#define LINES_NUM 10
#define COLS_NUM  40

/* --------------------------------------------------------------------------------------------- */

/* The layout of the file manager with hidden menubar and keybar:
   the menubar is at the bottom of Z-order, the keybar is at the top of Z-order,
   and the panel occupies the whole group including the lines of hidden bars. */
static WGroup *g;
static Widget *menubar, *panel, *keybar;

// widget that got the last MSG_MOUSE_DOWN
static Widget *receiver;

/* --------------------------------------------------------------------------------------------- */

static void
bar_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    (void) event;

    if (msg == MSG_MOUSE_DOWN)
        receiver = w;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    if (msg == MSG_MOUSE_DOWN)
    {
        // like the file panel: no events on the 1st line, pass them on
        if (event->y == 0)
            event->result.abort = TRUE;
        else
            receiver = w;
    }
}

/* --------------------------------------------------------------------------------------------- */

static Widget *
widget_create (int y, int lines, widget_mouse_cb_fn mouse_callback)
{
    Widget *w;
    WRect r;

    w = g_new0 (Widget, 1);
    rect_init (&r, y, 0, lines, COLS_NUM);
    widget_init (w, &r, widget_default_callback, mouse_callback);
    group_add_widget (g, w);

    return w;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;

    g = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, LINES_NUM, COLS_NUM);
    group_init (g, &r, group_default_callback, NULL);

    menubar = widget_create (0, 1, bar_mouse_callback);
    widget_set_options (menubar, WOP_MOUSE_WHEN_HIDDEN, TRUE);
    panel = widget_create (0, LINES_NUM, panel_mouse_callback);
    keybar = widget_create (LINES_NUM - 1, 1, bar_mouse_callback);

    receiver = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    widget_destroy (WIDGET (g));
}

/* --------------------------------------------------------------------------------------------- */

static int
click (int y)
{
    // global coordinates are 1-based
    Gpm_Event event = { .buttons = GPM_B_LEFT, .x = COLS_NUM / 2, .y = y + 1, .type = GPM_DOWN };

    return group_handle_mouse_event (WIDGET (g), &event);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_group_mouse_ds") */
static const struct test_group_mouse_ds
{
    gboolean menubar_visible;
    gboolean keybar_visible;
    int y;
    const char *expected;  // "menubar", "panel" or "keybar"
} test_group_mouse_ds[] = {
    // visible bars get mouse events
    { TRUE, TRUE, 0, "menubar" },
    { TRUE, TRUE, LINES_NUM - 1, "keybar" },
    { TRUE, TRUE, LINES_NUM / 2, "panel" },
    // hidden keybar doesn't get mouse events: invisible buttons must not be pressed
    { TRUE, FALSE, LINES_NUM - 1, "panel" },
    // hidden menubar gets mouse events: a click on the top line shows it
    { FALSE, TRUE, 0, "menubar" },
    { FALSE, FALSE, 0, "menubar" },
};

/* @Test(dataSource = "test_group_mouse_ds") */
START_PARAMETRIZED_TEST (test_group_mouse, test_group_mouse_ds)
{
    Widget *expected;
    int ret;

    // given
    widget_set_visibility (menubar, data->menubar_visible);
    widget_set_visibility (keybar, data->keybar_visible);

    if (strcmp (data->expected, "menubar") == 0)
        expected = menubar;
    else if (strcmp (data->expected, "keybar") == 0)
        expected = keybar;
    else
        expected = panel;

    // when
    ret = click (data->y);

    // then
    mctest_assert_true (ret != MOU_UNHANDLED);
    mctest_assert_ptr_eq (receiver, expected);
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
    mctest_add_parameterized_test (tc_core, test_group_mouse, test_group_mouse_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
