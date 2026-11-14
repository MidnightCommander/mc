/*
   src/viewer/image - tests for inline image preview in quick view

   Copyright (C) 2026
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

#include <glib/gstdio.h>  // g_unlink()
#include <string.h>
#include <unistd.h>

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

#include "src/viewer/image.c"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

// PNG signature and IHDR chunk header with 640x480 size
static const unsigned char png_header[] = { 0x89, 'P', 'N', 'G',  '\r', '\n', 0x1a, '\n',
                                            0,    0,   0,   13,   'I',  'H',  'D',  'R',
                                            0,    0,   2,   0x80, 0,    0,    1,    0xE0 };

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sniff_known_formats)
{
    const unsigned char jpeg[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0, 0x10 };
    const unsigned char gif87[] = "GIF87a\x01\x00";
    const unsigned char gif89[] = "GIF89a\x01\x00";
    const unsigned char webp[] = "RIFF\x00\x00\x00\x00WEBPVP8 ";
    const unsigned char tiff_le[] = { 'I', 'I', '*', 0, 8, 0, 0, 0 };
    const unsigned char tiff_be[] = { 'M', 'M', 0, '*', 0, 0, 0, 8 };
    unsigned char bmp[30] = { 'B', 'M' };

    ck_assert (mcview_image_sniff (png_header, sizeof (png_header)));
    ck_assert (mcview_image_sniff (jpeg, sizeof (jpeg)));
    ck_assert (mcview_image_sniff (gif87, 8));
    ck_assert (mcview_image_sniff (gif89, 8));
    ck_assert (mcview_image_sniff (webp, 16));
    ck_assert (mcview_image_sniff (tiff_le, sizeof (tiff_le)));
    ck_assert (mcview_image_sniff (tiff_be, sizeof (tiff_be)));
    ck_assert (mcview_image_sniff (bmp, sizeof (bmp)));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sniff_rejects_other_data)
{
    const unsigned char text[] = "#!/bin/sh\necho hello\n";
    const unsigned char riff_wav[] = "RIFF\x00\x00\x00\x00WAVEfmt ";
    unsigned char fake_bmp[30] = { 'B', 'M', 'a', 'b', 'c', 'd', 'e', 'f' };  // "BM" in a text file

    ck_assert (!mcview_image_sniff (text, sizeof (text) - 1));
    ck_assert (!mcview_image_sniff (riff_wav, 16));
    ck_assert (!mcview_image_sniff (fake_bmp, sizeof (fake_bmp)));
    ck_assert (!mcview_image_sniff ((const unsigned char *) "", 0));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sniff_truncated_data)
{
    // every proper prefix of a PNG header is not enough to decide
    for (size_t len = 0; len < 8; len++)
        ck_assert (!mcview_image_sniff (png_header, len));

    ck_assert (!mcview_image_sniff ((const unsigned char *) "GIF89", 5));
    ck_assert (!mcview_image_sniff ((const unsigned char *) "BM", 2));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const char *const term_vars[] = { "TERM", "TERM_PROGRAM", "LC_TERMINAL", "KITTY_WINDOW_ID",
                                         "TMUX", "STY" };

static void
save_and_clear_term_env (char *saved[])
{
    for (size_t i = 0; i < G_N_ELEMENTS (term_vars); i++)
    {
        saved[i] = g_strdup (g_getenv (term_vars[i]));
        g_unsetenv (term_vars[i]);
    }
}

static void
restore_term_env (char *saved[])
{
    for (size_t i = 0; i < G_N_ELEMENTS (term_vars); i++)
    {
        if (saved[i] != NULL)
            g_setenv (term_vars[i], saved[i], TRUE);
        else
            g_unsetenv (term_vars[i]);
        g_free (saved[i]);
    }
}

START_TEST (test_detect_protocol)
{
    char *saved[G_N_ELEMENTS (term_vars)];

    save_and_clear_term_env (saved);

    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);

    g_setenv ("TERM_PROGRAM", "iTerm.app", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);

    // inside tmux/screen the escape sequence would be swallowed
    g_setenv ("TMUX", "/tmp/tmux-1000/default,1,0", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);
    g_unsetenv ("TMUX");

    g_setenv ("STY", "1234.pts-0.host", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);
    g_unsetenv ("STY");

    g_setenv ("TERM_PROGRAM", "Apple_Terminal", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);

    g_setenv ("TERM_PROGRAM", "WezTerm", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);

    g_setenv ("TERM_PROGRAM", "ghostty", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_KITTY);

    // ssh case: only LC_TERMINAL is forwarded
    g_unsetenv ("TERM_PROGRAM");
    g_setenv ("LC_TERMINAL", "iTerm2", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);
    g_unsetenv ("LC_TERMINAL");

    g_setenv ("KITTY_WINDOW_ID", "1", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_KITTY);
    g_unsetenv ("KITTY_WINDOW_ID");

    g_setenv ("TERM", "xterm-kitty", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_KITTY);

    restore_term_env (saved);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_protocol_setting)
{
    char *saved[G_N_ELEMENTS (term_vars)];

    save_and_clear_term_env (saved);

    // unset setting means "auto"
    mcview_image_protocol = NULL;
    g_setenv ("TERM_PROGRAM", "iTerm.app", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);

    mcview_image_protocol = (char *) "auto";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);

    // garbage falls back to autodetection
    mcview_image_protocol = (char *) "bogus";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);

    mcview_image_protocol = (char *) "none";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);

    // explicit value overrides detection, case insensitive
    mcview_image_protocol = (char *) "Kitty";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_KITTY);

    // ... and works in a terminal we know nothing about
    g_unsetenv ("TERM_PROGRAM");
    mcview_image_protocol = (char *) "iterm2";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_ITERM2);
    mcview_image_protocol = (char *) "auto";
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);

    // but never inside a multiplexer
    mcview_image_protocol = (char *) "kitty";
    g_setenv ("TMUX", "/tmp/tmux-1000/default,1,0", TRUE);
    ck_assert_int_eq (mcview_image_detect_protocol (), MCVIEW_IMAGE_NONE);

    mcview_image_protocol = NULL;
    restore_term_env (saved);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_png_size)
{
    int w = 0, h = 0;
    unsigned char bad_chunk[24];

    ck_assert (mcview_image_png_size (png_header, sizeof (png_header), &w, &h));
    ck_assert_int_eq (w, 640);
    ck_assert_int_eq (h, 480);

    // truncated
    ck_assert (!mcview_image_png_size (png_header, 23, &w, &h));

    // first chunk is not IHDR
    memcpy (bad_chunk, png_header, sizeof (bad_chunk));
    bad_chunk[12] = 'X';
    ck_assert (!mcview_image_png_size (bad_chunk, sizeof (bad_chunk), &w, &h));

    // zero width
    memcpy (bad_chunk, png_header, sizeof (bad_chunk));
    bad_chunk[18] = bad_chunk[19] = 0;
    ck_assert (!mcview_image_png_size (bad_chunk, sizeof (bad_chunk), &w, &h));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fit)
{
    WView view;
    int cols, lines;

    memset (&view, 0, sizeof (view));
    view.data_area.cols = 80;
    view.data_area.lines = 24;

    // Without pixel size of the terminal in the test, a cell is assumed to be 1:2, so the box
    // is 80x48 in "pixel" units.

    // wide image is limited by width: 80x40 -> 40 rows of 2 units
    view.image_width = 200;
    view.image_height = 100;
    mcview_image_fit (&view, &cols, &lines);
    ck_assert_int_le (cols, 80);
    ck_assert_int_le (lines, 24);
    ck_assert_int_ge (cols, 1);
    ck_assert_int_ge (lines, 1);

    // very tall image is limited by height and never exceeds the area
    view.image_width = 10;
    view.image_height = 10000;
    mcview_image_fit (&view, &cols, &lines);
    ck_assert_int_eq (lines, 24);
    ck_assert_int_eq (cols, 1);

    // very wide image is limited by width and keeps at least one row
    view.image_width = 10000;
    view.image_height = 10;
    mcview_image_fit (&view, &cols, &lines);
    ck_assert_int_eq (cols, 80);
    ck_assert_int_eq (lines, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("image_load_ds") */
/* *INDENT-OFF* */
static const struct image_load_ds
{
    const char *term_program;  // iTerm2 or kitty (ghostty)
    const char *content;
    gsize len;
    gboolean in_panel;
    gboolean expect_image;
} image_load_ds[] = {
    // PNG file in quick view panel
    { "iTerm.app", (const char *) png_header, sizeof (png_header), TRUE, TRUE },
    { "ghostty", (const char *) png_header, sizeof (png_header), TRUE, TRUE },
    // the same file in the full screen viewer: no preview
    { "iTerm.app", (const char *) png_header, sizeof (png_header), FALSE, FALSE },
    // JPEG: iTerm2 decodes it, kitty protocol can't
    { "iTerm.app", "\xFF\xD8\xFF\xE0\x00\x10JFIF", 10, TRUE, TRUE },
    { "ghostty", "\xFF\xD8\xFF\xE0\x00\x10JFIF", 10, TRUE, FALSE },
    // PNG signature without a valid IHDR: kitty needs the size
    { "ghostty", "\x89PNG\r\n\x1a\nxxxxxxxxxxxxxxxx", 24, TRUE, FALSE },
    // plain text
    { "iTerm.app", "hello world\n", 12, TRUE, FALSE },
    // empty file
    { "iTerm.app", "", 0, TRUE, FALSE },
};
/* *INDENT-ON* */

START_PARAMETRIZED_TEST (test_image_load, image_load_ds)
{
    // given
    char *saved[G_N_ELEMENTS (term_vars)];
    char *path = NULL;
    GError *error = NULL;
    int fd;
    WView view;

    save_and_clear_term_env (saved);
    g_setenv ("TERM_PROGRAM", data->term_program, TRUE);

    fd = g_file_open_tmp ("mc-viewer-image-XXXXXX", &path, &error);
    ck_assert_msg (fd != -1, "cannot create temp file");
    close (fd);
    ck_assert (g_file_set_contents (path, data->content, (gssize) data->len, &error));

    memset (&view, 0, sizeof (view));
    view.dpy_frame_size = data->in_panel ? 1 : 0;
    view.filename_vpath = vfs_path_from_str (path);
    view.image_b64 = g_strdup ("stale");  // must be dropped on reload

    // when
    mcview_image_load (&view);

    // then
    if (data->expect_image)
    {
        gchar *expected = g_base64_encode ((const guchar *) data->content, data->len);

        ck_assert_ptr_nonnull (view.image_b64);
        ck_assert_str_eq (view.image_b64, expected);
        g_free (expected);
        ck_assert_int_eq (view.image_proto,
                          strcmp (data->term_program, "iTerm.app") == 0 ? MCVIEW_IMAGE_ITERM2
                                                                        : MCVIEW_IMAGE_KITTY);
    }
    else
    {
        ck_assert_ptr_null (view.image_b64);
        ck_assert_int_eq (view.image_proto, MCVIEW_IMAGE_NONE);
    }

    ck_assert_int_eq (mcview_image_active (&view), data->expect_image);

    // in hex mode the raw bytes are shown instead of the picture
    view.mode_flags.hex = TRUE;
    ck_assert (!mcview_image_active (&view));

    // cleanup
    g_unlink (path);
    g_free (path);
    g_free (view.image_b64);
    vfs_path_free (view.filename_vpath, TRUE);
    restore_term_env (saved);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_image_load_unsupported_terminal)
{
    char *saved[G_N_ELEMENTS (term_vars)];
    char *path = NULL;
    GError *error = NULL;
    WView view;
    int fd;

    save_and_clear_term_env (saved);

    fd = g_file_open_tmp ("mc-viewer-image-XXXXXX", &path, &error);
    ck_assert_msg (fd != -1, "cannot create temp file");
    close (fd);
    ck_assert (g_file_set_contents (path, (const char *) png_header, sizeof (png_header), &error));

    memset (&view, 0, sizeof (view));
    view.dpy_frame_size = 1;
    view.filename_vpath = vfs_path_from_str (path);

    mcview_image_load (&view);

    ck_assert_ptr_null (view.image_b64);

    g_unlink (path);
    g_free (path);
    vfs_path_free (view.filename_vpath, TRUE);
    restore_term_env (saved);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_sniff_known_formats);
    tcase_add_test (tc_core, test_sniff_rejects_other_data);
    tcase_add_test (tc_core, test_sniff_truncated_data);
    tcase_add_test (tc_core, test_detect_protocol);
    tcase_add_test (tc_core, test_detect_protocol_setting);
    tcase_add_test (tc_core, test_png_size);
    tcase_add_test (tc_core, test_fit);
    mctest_add_parameterized_test (tc_core, test_image_load, image_load_ds);
    tcase_add_test (tc_core, test_image_load_unsupported_terminal);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
