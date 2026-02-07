/*
   src/usermenu - tests for usermenu

   Copyright (C) 2025
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/usermenu"

#include "tests/mctest.h"

#include "src/filemanager/panel.h"
#include "src/filemanager/filemanager.h"

#undef other_panel
static WPanel *other_panel;

#include "src/usermenu.c"

#include "execute__common.c"

#define CURRENT_DIR "/etc/mc"
#define OTHER_DIR   "/usr/lib"
#define CURRENT_PFX "current_"
#define OTHER_PFX   "other_"
#define TAGGED_PFX  "tagged_"
#define FNAME1_BASE "file_without_spaces"
#define FNAME1_EXT  "txt"
#define FNAME1      FNAME1_BASE "." FNAME1_EXT
#define FNAME2_BASE "file with spaces"
#define FNAME2_EXT  "sh"
#define FNAME2      FNAME2_BASE "." FNAME2_EXT
#define FNAME3      "file_utf8_local_chars_ä_ü_ö_ß_"
#define FNAME4      "file_utf8_nonlocal_chars_à_á_"

/* --------------------------------------------------------------------------------------------- */

static void
setup_mock_panels (void)
{
    file_entry_t *list;

    setup ();

    current_panel = g_new0 (WPanel, 1);

    current_panel->dir.size = 4;
    current_panel->dir.len = current_panel->dir.size;
    current_panel->dir.list = g_new0 (file_entry_t, current_panel->dir.size);
    current_panel->cwd_vpath = vfs_path_from_str (CURRENT_DIR);

    list = current_panel->dir.list;

    list[0].fname = g_string_new (CURRENT_PFX FNAME1);
    list[1].fname = g_string_new (CURRENT_PFX TAGGED_PFX FNAME2);
    list[1].f.marked = TRUE;
    list[2].fname = g_string_new (CURRENT_PFX FNAME3);
    list[3].fname = g_string_new (CURRENT_PFX TAGGED_PFX FNAME4);
    list[3].f.marked = TRUE;

    current_panel->current = 0;
    current_panel->marked = 2;

    other_panel = g_new0 (WPanel, 1);

    other_panel->dir.size = 4;
    other_panel->dir.len = other_panel->dir.size;
    other_panel->cwd_vpath = vfs_path_from_str (OTHER_DIR);
    other_panel->dir.list = g_new0 (file_entry_t, other_panel->dir.size);

    list = other_panel->dir.list;

    list[0].fname = g_string_new (OTHER_PFX TAGGED_PFX FNAME1);
    list[0].f.marked = TRUE;
    list[1].fname = g_string_new (OTHER_PFX FNAME2);
    list[2].fname = g_string_new (OTHER_PFX TAGGED_PFX FNAME3);
    list[2].f.marked = TRUE;
    list[3].fname = g_string_new (OTHER_PFX FNAME4);

    other_panel->current = 1;
    other_panel->marked = 2;
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown_mock_panels (void)
{
    dir_list_free_list (&current_panel->dir);
    vfs_path_free (current_panel->cwd_vpath, TRUE);
    MC_PTR_FREE (current_panel);
    dir_list_free_list (&other_panel->dir);
    vfs_path_free (other_panel->cwd_vpath, TRUE);
    MC_PTR_FREE (other_panel);
    teardown ();
}

/* --------------------------------------------------------------------------------------------- */

// clang-format off
#define MOCK_V1_FNAME                                                                              \
    CURRENT_DIR PATH_SEP_STR CURRENT_PFX TAGGED_PFX FNAME2 " "                                     \
    CURRENT_DIR PATH_SEP_STR CURRENT_PFX TAGGED_PFX FNAME4 " "

#define MOCK_V2_FNAME                                                                              \
    OTHER_DIR PATH_SEP_STR OTHER_PFX TAGGED_PFX FNAME1 " "                                         \
    OTHER_DIR PATH_SEP_STR OTHER_PFX TAGGED_PFX FNAME3 " "
// clang-format on

static const struct check_expand_format_ds
{
    const Widget *edit_widget;
    char macro;
    gboolean do_quote;
    int panel_marked;
    const char *expected;
} check_expand_format_ds[] = {
    // In a fully UTF-8 environment, only the files with real spaces should match
    { NULL, '%', FALSE, -1, "%" },
    { NULL, 'f', FALSE, -1, CURRENT_PFX FNAME1 },
    { NULL, 'p', FALSE, -1, CURRENT_PFX FNAME1 },
    { NULL, 'F', FALSE, -1, OTHER_PFX FNAME2 },
    { NULL, 'P', FALSE, -1, OTHER_PFX FNAME2 },
    { NULL, 'x', FALSE, -1, FNAME1_EXT },
    { NULL, 'X', FALSE, -1, FNAME2_EXT },
    { NULL, 'n', FALSE, -1, CURRENT_PFX FNAME1_BASE },
    { NULL, 'N', FALSE, -1, OTHER_PFX FNAME2_BASE },
    { NULL, 'd', FALSE, -1, CURRENT_DIR },
    { NULL, 'D', FALSE, -1, OTHER_DIR },
    { NULL, 's', FALSE, 0, CURRENT_PFX FNAME1 },
    { NULL, 'S', FALSE, 0, OTHER_PFX FNAME2 },
    { NULL, 's', FALSE, -1, CURRENT_PFX TAGGED_PFX FNAME2 " " CURRENT_PFX TAGGED_PFX FNAME4 " " },
    { NULL, 'S', FALSE, -1, OTHER_PFX TAGGED_PFX FNAME1 " " OTHER_PFX TAGGED_PFX FNAME3 " " },
    { NULL, 't', FALSE, -1, CURRENT_PFX TAGGED_PFX FNAME2 " " CURRENT_PFX TAGGED_PFX FNAME4 " " },
    { NULL, 'u', FALSE, -1, CURRENT_PFX TAGGED_PFX FNAME2 " " CURRENT_PFX TAGGED_PFX FNAME4 " " },
    { NULL, 'T', FALSE, -1, OTHER_PFX TAGGED_PFX FNAME1 " " OTHER_PFX TAGGED_PFX FNAME3 " " },
    { NULL, 'U', FALSE, -1, OTHER_PFX TAGGED_PFX FNAME1 " " OTHER_PFX TAGGED_PFX FNAME3 " " },
    { NULL, 'v', FALSE, -1, MOCK_V1_FNAME },
    { NULL, 'V', FALSE, -1, MOCK_V2_FNAME },

    /* TODO:
#ifdef USE_INTERNAL_EDIT
    { NULL, 'c', FALSE, -1, " "},
    { NULL, 'i', FALSE, -1, " "},
    { NULL, 'y', FALSE, -1, " "},
    { NULL, 'b', FALSE, -1, " "},
#endif
    { NULL, 'm', FALSE, -1, " "},
    */
};

/* --------------------------------------------------------------------------------------------- */

START_PARAMETRIZED_TEST (check_expand_format, check_expand_format_ds)
{
    const int current_marked = current_panel->marked;
    const int other_marked = other_panel->marked;
    char *result;

    if (data->panel_marked > -1)
    {
        if (g_ascii_islower (data->macro))
            current_panel->marked = data->panel_marked;
        else
            other_panel->marked = data->panel_marked;
    }

    result = expand_format (data->edit_widget, data->macro, data->do_quote);
    ck_assert_str_eq (data->expected, result);

    g_free (result);
    current_panel->marked = current_marked;
    other_panel->marked = other_marked;
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup_mock_panels, teardown_mock_panels);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, check_expand_format, check_expand_format_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
