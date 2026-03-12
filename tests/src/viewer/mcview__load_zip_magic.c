/*
   src/viewer - tests for mcview_load() with PK/ZIP magic bytes

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

#define TEST_SUITE_NAME "/src/viewer"

#include "tests/mctest.h"

#include <fcntl.h>

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"
#include "src/util.h"

#include "src/viewer/internal.h"

/* --------------------------------------------------------------------------------------------- */
/* Stubs for symbols pulled in transitively from editor/diffviewer/tty.
 * These are never actually called in our tests, but the linker needs them
 * because libinternal.a bundles all of mc's subsystems together. */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

extern int mc_skin_color__cache[64];
int mc_skin_color__cache[64];

extern void *mc_editor_plugin_list;
void *mc_editor_plugin_list = NULL;

extern int we_are_strstrstrbackground;
int we_are_strstrstrbackground = 0;

void tty_lowlevel_setcolor (int color) { (void) color; }
void mc_editor_plugin_add (void *p) { (void) p; }
void mc_editor_plugins_load (void) {}
int button_get_width (void *b) { (void) b; return 0; }

#pragma GCC diagnostic pop

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static int mcview_show_error__call_count;

/* mock: mcview_compute_areas -- no-op in test (depends on widget geometry) */
void
mcview_compute_areas (WView *view)
{
    (void) view;
}

/* mock: mcview_update_bytes_per_line -- no-op in test (depends on display areas) */
void
mcview_update_bytes_per_line (WView *view)
{
    (void) view;
}

/* mock: mcview_set_codeset -- no-op in test (depends on charset subsystem) */
void
mcview_set_codeset (WView *view)
{
    (void) view;
}

/* mock: mcview_display -- no-op in test (depends on tty subsystem) */
void
mcview_display (WView *view)
{
    (void) view;
}

/* mock: mcview_show_error -- capture call count instead of showing dialog */
void
mcview_show_error (WView *view, const char *format, const char *filename)
{
    (void) view;
    (void) format;
    (void) filename;
    mcview_show_error__call_count++;
}

/* mock: file_error_message -- no-op in test */
void
file_error_message (const char *format, const char *filename)
{
    (void) format;
    (void) filename;
}

/* mock: load_file_position -- no-op in test */
void
load_file_position (const vfs_path_t *filename_vpath, long *line, long *column, off_t *offset,
                    GArray **bookmarks)
{
    (void) filename_vpath;
    (void) line;
    (void) column;
    (void) offset;
    (void) bookmarks;
}

/* mock: message -- no-op in test */
void
message (int flags, const char *title, const char *text, ...)
{
    (void) flags;
    (void) title;
    (void) text;
}

/* --------------------------------------------------------------------------------------------- */

static char *
create_test_file (const unsigned char *data, size_t size)
{
    char *tmp_path = NULL;
    GError *error = NULL;
    int fd;

    fd = g_file_open_tmp ("mc-test-viewer-XXXXXX", &tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        return NULL;
    }

    if (size > 0)
    {
        ssize_t written = write (fd, data, size);
        (void) written;
    }

    close (fd);
    return tmp_path;
}

/* --------------------------------------------------------------------------------------------- */

static WView test_view;

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    memset (&test_view, 0, sizeof (test_view));
    mcview_init (&test_view);
    test_view.mode_flags.magic = TRUE;

    mcview_show_error__call_count = 0;
}

/* @After */
static void
teardown (void)
{
    mcview_close_datasource (&test_view);
    vfs_path_free (test_view.filename_vpath, TRUE);
    test_view.filename_vpath = NULL;
    vfs_path_free (test_view.workdir_vpath, TRUE);
    test_view.workdir_vpath = NULL;
    g_free (test_view.command);
    test_view.command = NULL;

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test: file with PK/ZIP magic bytes should load successfully as DS_FILE */
START_TEST (test_zip_magic_file_loads_as_ds_file)
{
    // given -- a file that starts with PK\x03\x04 (ZIP local file header)
    const unsigned char zip_header[] = {
        'P', 'K', 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    char *tmp_path = create_test_file (zip_header, sizeof (zip_header));
    ck_assert_msg (tmp_path != NULL, "Failed to create temp file");

    // when
    gboolean result = mcview_load (&test_view, NULL, tmp_path, 0, 0, 0);

    // then -- should succeed and set DS_FILE datasource
    ck_assert_msg (result == TRUE, "mcview_load should return TRUE for ZIP-magic file");
    ck_assert_int_eq (test_view.datasource, DS_FILE);
    ck_assert_int_eq (mcview_show_error__call_count, 0);

    // cleanup
    unlink (tmp_path);
    g_free (tmp_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test: reloading ZIP-magic file should not crash (regression for segfault on second F3) */
START_TEST (test_zip_magic_file_reload_no_crash)
{
    // given -- a file with ZIP magic
    const unsigned char zip_header[] = {
        'P', 'K', 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    char *tmp_path = create_test_file (zip_header, sizeof (zip_header));
    ck_assert_msg (tmp_path != NULL, "Failed to create temp file");

    // when -- load, cleanup, reinit, load again (simulates two F3 presses)
    gboolean result1 = mcview_load (&test_view, NULL, tmp_path, 0, 0, 0);
    ck_assert_msg (result1 == TRUE, "First mcview_load should return TRUE");
    ck_assert_int_eq (test_view.datasource, DS_FILE);

    // simulate viewer close + reinit (like mcview_done + mcview_init)
    mcview_close_datasource (&test_view);
    vfs_path_free (test_view.filename_vpath, TRUE);
    test_view.filename_vpath = NULL;
    vfs_path_free (test_view.workdir_vpath, TRUE);
    test_view.workdir_vpath = NULL;
    g_free (test_view.command);
    test_view.command = NULL;
    mcview_init (&test_view);
    test_view.mode_flags.magic = TRUE;

    // second load -- should not crash
    gboolean result2 = mcview_load (&test_view, NULL, tmp_path, 0, 0, 0);

    // then
    ck_assert_msg (result2 == TRUE, "Second mcview_load should return TRUE");
    ck_assert_int_eq (test_view.datasource, DS_FILE);
    ck_assert_int_eq (mcview_show_error__call_count, 0);

    // cleanup
    unlink (tmp_path);
    g_free (tmp_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test: normal file (no magic) should load as DS_FILE */
START_TEST (test_normal_file_loads_as_ds_file)
{
    // given -- a plain text file
    const unsigned char text_data[] = "Hello, World!\nThis is a test file.\n";
    char *tmp_path = create_test_file (text_data, sizeof (text_data) - 1);
    ck_assert_msg (tmp_path != NULL, "Failed to create temp file");

    // when
    gboolean result = mcview_load (&test_view, NULL, tmp_path, 0, 0, 0);

    // then
    ck_assert_msg (result == TRUE, "mcview_load should return TRUE for normal file");
    ck_assert_int_eq (test_view.datasource, DS_FILE);
    ck_assert_int_eq (mcview_show_error__call_count, 0);

    // cleanup
    unlink (tmp_path);
    g_free (tmp_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test: nonexistent file should fail to load */
START_TEST (test_nonexistent_file_fails)
{
    // when
    gboolean result = mcview_load (&test_view, NULL, "/nonexistent/file/path", 0, 0, 0);

    // then
    ck_assert_msg (result == FALSE, "mcview_load should return FALSE for nonexistent file");
    ck_assert_int_eq (test_view.datasource, DS_NONE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test: gzip magic should also load successfully when VFS decompression fails */
START_TEST (test_gzip_magic_file_loads_as_ds_file)
{
    // given -- a file that starts with gzip magic (\x1f\x8b) but is not actually valid gzip
    const unsigned char gzip_header[] = {
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    char *tmp_path = create_test_file (gzip_header, sizeof (gzip_header));
    ck_assert_msg (tmp_path != NULL, "Failed to create temp file");

    // when
    gboolean result = mcview_load (&test_view, NULL, tmp_path, 0, 0, 0);

    // then
    ck_assert_msg (result == TRUE, "mcview_load should return TRUE for gzip-magic file");
    ck_assert_int_eq (test_view.datasource, DS_FILE);
    ck_assert_int_eq (mcview_show_error__call_count, 0);

    // cleanup
    unlink (tmp_path);
    g_free (tmp_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_zip_magic_file_loads_as_ds_file);
    tcase_add_test (tc_core, test_zip_magic_file_reload_no_crash);
    tcase_add_test (tc_core, test_normal_file_loads_as_ds_file);
    tcase_add_test (tc_core, test_nonexistent_file_fails);
    tcase_add_test (tc_core, test_gzip_magic_file_loads_as_ds_file);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
