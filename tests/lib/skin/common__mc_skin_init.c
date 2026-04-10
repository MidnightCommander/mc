/*
   lib/skin - mc_skin_init() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Manuel Einfalt <einfalt1@proton.me>, 2026

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

#define TEST_SUITE_NAME "/lib/skin"

#include "tests/mctest.h"

#include "lib/util.h"
#include "lib/skin.h"

#include "lib/strutil.h"          // str_init_strings
#include "src/vfs/local/local.h"  // vfs_init_localfs

/* --------------------------------------------------------------------------------------------- */

static const struct skin_tests  // xterm, xterm-256color, xterm-direct
{
    const char *term;
    const gboolean nocolor;
    const gchar *skin_name;
    const gboolean has_error;
    const gboolean ret;
} skin_tests[] = {
    { "xterm", FALSE, "dark", FALSE, TRUE },
    { "xterm", FALSE, "julia256", TRUE, FALSE },
    { "xterm", FALSE, "seasons-summer16M", TRUE, FALSE },
    { "xterm", TRUE, "dark", FALSE, TRUE },
    { "xterm", TRUE, "julia256", TRUE, FALSE },
    { "xterm", TRUE, "seasons-summer16M", TRUE, FALSE },

    { "xterm-256color", FALSE, "dark", FALSE, TRUE },
    { "xterm-256color", FALSE, "julia256", FALSE, TRUE },
    { "xterm-256color", FALSE, "seasons-summer16M", TRUE, FALSE },
    { "xterm-256color", TRUE, "dark", FALSE, TRUE },
    { "xterm-256color", TRUE, "julia256", FALSE, TRUE },
    { "xterm-256color", TRUE, "seasons-summer16M", TRUE, FALSE },

    { "xterm-256direct", FALSE, "dark", FALSE, TRUE },
    { "xterm-256direct", FALSE, "julia256", FALSE, TRUE },
    { "xterm-256direct", FALSE, "seasons-summer16M", FALSE, TRUE },
    { "xterm-256direct", TRUE, "dark", FALSE, TRUE },
    { "xterm-256direct", TRUE, "julia256", FALSE, TRUE },
    { "xterm-256direct", TRUE, "seasons-summer16M", FALSE, TRUE },
};

extern GHashTable *mc_tty_color__hashtable;

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    mc_global.share_data_dir = (char *) TEST_SHARE_DIR;
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;

    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    tty_color_role_to_pair = g_new (int, COLOR_MAP_SIZE);
    mc_tty_color__hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

// Mock
gboolean
tty_use_truecolors (GError **error)
{
    const char *termname = getenv ("TERM");

    if (termname == NULL)
    {
        mc_propagate_error (error, 0, _ ("foobar"));
        return FALSE;
    }

    if (strcmp (termname, "xterm-256direct") == 0)
        return TRUE;

    mc_propagate_error (error, 0, _ ("foobar"));
    return FALSE;
}

// Mock
gboolean
tty_use_256colors (GError **error)
{
    const char *termname = getenv ("TERM");

    if (termname == NULL)
    {
        mc_propagate_error (error, 0, _ ("foobar"));
        return FALSE;
    }

    if (strcmp (termname, "xterm-256color") == 0 || tty_use_truecolors (NULL))
        return TRUE;

    mc_propagate_error (error, 0, _ ("foobar"));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

START_PARAMETRIZED_TEST (skin_test, skin_tests)
{
    GError *mcerror = NULL;

    mc_global.tty.disable_colors = data->nocolor;
    setenv ("TERM", data->term, 1);

    if (data->ret)
    {
        mctest_assert_true (mc_skin_init (data->skin_name, &mcerror));
    }
    else
    {
        mctest_assert_false (mc_skin_init (data->skin_name, &mcerror));
    }

    if (data->has_error)
    {
        mctest_assert_not_null (mcerror);
        g_error_free (mcerror);
    }
    else
        mctest_assert_null (mcerror);
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
    mctest_add_parameterized_test (tc_core, skin_test, skin_tests);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
